#include "RunMacroMgr.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QtConcurrent/QtConcurrent>
#include <QMutexLocker>
#include <QTextCodec>
#include <QProcess>
#include <QtPrintSupport/QPrinter>
#include <QPainter>
#include <QImage>

#include <ConfigManager.h>
#include <unistd.h>

#define COMPLETED_CONFIG ConfigManager::getInstance("Completed", "macros.ini")

RunMacroMgr::RunMacroMgr(RunMacroMgrSettings *settings, QObject *parent) :
    QObject(parent)
  , m_isActive(false)
{
    m_NPPUnitID = settings->NPPUnitID;
    m_VMSTypeID = settings->VMSTypeID;

    m_OutputSizeLimitMB = settings->OutputSizeLimitMB;

    m_strArchivesPath = settings->strArchivesPath;
    m_strPlayerPath = settings->strPlayerPath;
    m_strBorlandPath = settings->strBorlandPath;
    m_strPlayerOutputRoot = settings->strPlayerOutputRoot;
}

QString RunMacroMgr::playerOutputRoot()
{
    return m_strPlayerOutputRoot;
}

QString RunMacroMgr::dataOutputPath()
{
    return m_strPlayerOutputRoot + "/output_db/";
}

QString RunMacroMgr::reportsOutputPath()
{
    return m_strPlayerOutputRoot + "/output_reports/";
}

QString RunMacroMgr::unpackArchivesPath()
{
    return m_strPlayerOutputRoot + "/output_tmp/";
}

QString RunMacroMgr::macrosRoot()
{
    return QDir(m_strPlayerPath + "/../scenar").absolutePath();
}

QString RunMacroMgr::macrosPath(ScenarioType type)
{
    static const QMap<ScenarioType, QString> paths =
    {
        {VIBRO, "/vibro/"},
        {TERMO, "/termo/"}
    };
    return macrosRoot() + paths.value(type);
}


QStringList RunMacroMgr::getArchivePaths(const QDate &startDate, const QDate &endDate)
{
    QFileInfoList infoList;
    QDir dir(m_strArchivesPath);
    infoList = dir.entryInfoList(QStringList() << "ASV*.tar*", QDir::Files);

    QStringList filePaths;
    foreach (QFileInfo info, infoList) {
        //выделяем дату из названия файла с архивом
        QDate archiveDate = QDate::fromString(info.fileName().mid(4, 6).prepend("20"), "yyyyMMdd");

        //фильтруем по дате
        if (archiveDate.isValid() && archiveDate >= startDate && archiveDate <= endDate)
            filePaths.append(info.absoluteFilePath());
    }

    return filePaths;
}

QStringList RunMacroMgr::getMacroPaths()
{
    QStringList filePaths;

    auto addMacros = [&filePaths](const QString &path, const QString &mask){
        QDir dir(path);
        QFileInfoList infoList = dir.entryInfoList(QStringList() << mask, QDir::Files);
        foreach (QFileInfo info, infoList) {
            filePaths.append(info.absoluteFilePath());
        }
    };

    addMacros(macrosPath(VIBRO), "*.asc*");
    // addMacros(macrosPath(TERMO), "*.asc*");

    return filePaths;
}

void RunMacroMgr::verifyFinishedItemsHash(const QStringList &macroPaths)
{
    QMultiHash<QString, QString> finishedItemsHash = getFinishedItemsHash();

    foreach (QString macroPath, macroPaths) {
        QString macroName = QFileInfo(macroPath).baseName();
        QStringList fileNames = COMPLETED_CONFIG->getStringListValue(macroName);
        if (!fileNames.isEmpty()) {
            QDir dir(dataOutputPath() + macroName);
            QStringList files = dir.entryList(QDir::Files);

            //проверяем содержит ли директория сгенерированные файлы
            foreach (QString fileName, fileNames) {
                if (!files.contains(fileName)) {
                    //удаляем содержимое директории
                    QProcess process;
                    process.start("sh", QStringList() << "-c" << QString("echo alpha3 | sudo -S sh -c \"rm -rf %1\"").arg(dataOutputPath() + macroName));
                    process.waitForFinished();

                    //чистим finishedItemsHash
                    foreach (QString archiveName, finishedItemsHash.uniqueKeys()) {
                        finishedItemsHash.remove(archiveName, macroName);
                    }
                    break;

                    //чистим сеттинги
                    COMPLETED_CONFIG->setValue(macroName, QVariant());
                }
            }
        }
    }

    saveFinishedItemsHash(finishedItemsHash);
}

QMultiHash<QString, QString> RunMacroMgr::getFinishedItemsHash() const
{  
    QMultiHash<QString, QString> finishedItemsHash;

    COMPLETED_CONFIG->reloadConfigFile();
    QByteArray finishedItemsData = COMPLETED_CONFIG->getByteArrayValue("Completed");
    QDataStream stream(&finishedItemsData, QIODevice::ReadOnly);
    stream >> finishedItemsHash;

    return finishedItemsHash;
}

bool RunMacroMgr::startPlay(const QMultiHash<QString, QString> &data)
{
    if (isActive())
        return false;

    QtConcurrent::run(this, &RunMacroMgr::runPlayerExecutable, data);

    return true;
}

bool RunMacroMgr::isActive() const
{
    QMutexLocker locker(&m_mutex);
    return m_isActive;
}

void RunMacroMgr::clearOversizedOutput()
{
    QProcess process;
    process.start("du", QStringList() << "-s" << dataOutputPath() << reportsOutputPath() << unpackArchivesPath());
    bool result = process.waitForFinished(10000);
    if (result) {
        const QByteArray output = process.readAllStandardOutput();

        QTextStream stream(output);

        qlonglong totalSizeKB = 0;

        QString line;
        while (stream.readLineInto(&line)) {
            QStringList data = line.simplified().split(" ");
            if (!data.isEmpty()) {
                bool isOk = false;
                const auto size = data.at(0).toLongLong(&isOk);
                if (isOk) {
                    totalSizeKB += size;
                }
            }
        }

        const auto sizeLimitMB = m_OutputSizeLimitMB;

        if (totalSizeKB / 1024 >= sizeLimitMB) {
            QProcess process;

            process.start("rm", QStringList() << "-rf" << dataOutputPath() << reportsOutputPath() << unpackArchivesPath());
            bool result = process.waitForFinished();

            if (result) {
                // Cleaning the completed macros data in *.ini file
                COMPLETED_CONFIG->setValue("Completed", QVariant());

                foreach (QString macroPath, getMacroPaths()) {
                    if (!macroPath.contains(";")) {
                        QString macroName = QFileInfo(macroPath).baseName();
                        COMPLETED_CONFIG->setValue(macroName, QVariant());
                    }
                }
            }
        }
    } else {
        qWarning() << "Proceess du finished with error:" << process.readAllStandardError();
    }
}

void RunMacroMgr::runPlayerExecutable(const QMultiHash<QString, QString> &data)
{
    clearOversizedOutput();

    QDir rootDir("/");
    rootDir.mkpath(dataOutputPath());
    rootDir.mkpath(reportsOutputPath());
    rootDir.mkpath(unpackArchivesPath());
    rootDir.mkpath(macrosRoot());

    //проверка возможности запуска
    const QString skwExecFile = m_strPlayerPath + "/" + playerExecutable;
    if (!QFile::exists(skwExecFile)) {
        emit sigPlayerError(trUtf8("No Player executable in %1 dircetory").arg(m_strPlayerPath)); // "Проигрыватель не обнаружен в директории %1"
        return;
    }

    if (!QFileInfo(unpackArchivesPath()).isWritable()) {
        emit sigPlayerError(trUtf8("Path %1 is write-protected").arg(unpackArchivesPath())); // "Путь %1 недоступен для записи"
        return;
    }

    if (!QFileInfo(reportsOutputPath()).isWritable()) {
        emit sigPlayerError(trUtf8("Path %1 is write-protected").arg(reportsOutputPath())); // "Путь %1 недоступен для записи"
        return;
    }

    if (!QFileInfo(dataOutputPath()).isWritable()) {
        emit sigPlayerError(trUtf8("Path %1 is write-protected").arg(dataOutputPath())); // "Путь %1 недоступен для записи"
        return;
    }

    if (!QFileInfo(macrosRoot()).isReadable()) {
        emit sigPlayerError(trUtf8("Path %1 is write-protected").arg(macrosRoot())); // "Путь %1 недоступен для записи"
        return;    }

    // Macro execution begin

    setActive(true);
    emit sigPlayerStarted();
    emit sigPlayerFinished(0);

    //список завершенных сценариев
    QMultiHash<QString /*путь к архиву*/, QString /*путь к сценарию*/> finishedMacrosHash = getFinishedItemsHash();

    QSet<QString> processedArchives;

    QString targetImagePath = m_strPlayerPath + "/../images/Skw1.bmp";
    if (QFile(targetImagePath).exists()) {
        if (!QFile(targetImagePath).remove()) {
            qWarning() << trUtf8("Unable to delete Skw1.bmp file for replace. The report can be saved incorrectly.");
        }
    }

    if (!QFile::copy(QString("%1/../images/Skw1_%2.bmp").arg(m_strPlayerPath).arg(m_VMSTypeID),
                     QString("%1/../images/Skw1.bmp").arg(m_strPlayerPath))) {
        qWarning() << trUtf8("Skw1.bmp file was not copied. The report can be saved incorrectly.");
    }

    // Count the number of player launches to do
    int totalLaunches = 0, progress = 0, progressStep;

    foreach (QString archivePath, data.keys()) {
        if (processedArchives.contains(archivePath))
            continue;

        processedArchives.insert(archivePath);
        foreach (QString macroPath, data.values(archivePath)) {
            totalLaunches++;
        }
    }

    processedArchives.clear();
    progressStep = 100 / (totalLaunches + 1);


    foreach (QString archivePath, data.keys()) {
        if (processedArchives.contains(archivePath))
            continue;

        processedArchives.insert(archivePath);

        bool archiveUnpacked = false; // is archive already unpacked?

        foreach (QString macroPath, data.values(archivePath)) {
            if (!archiveUnpacked) {
                //распаковываем текущий архив
                QProcess::execute("tar", QStringList() << "xfz" << archivePath << "-C" << unpackArchivesPath());
                archiveUnpacked = true;
            }

            emit sigPlayerMessage(tr("Archive ") + archivePath + tr(" unpacked"));

            // Write ini for player
            QDir dir(m_strPlayerPath + "/../config/");
            const QString skwConfigFile = dir.absolutePath() + "/Skw.ini";
            QFile file(skwConfigFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                emit sigPlayerError(trUtf8("Unable to open SKW player config file %1").arg(skwConfigFile)); // "Ошибка открытия конфигурационного файла %1"
                return;
            }

            QTextStream stream(&file);
            stream.setCodec(QTextCodec::codecForName("KOI8-R"));
            if (!macroPath.contains(";")) {
                stream << QFileInfo(macroPath).fileName() << "\n";
            } else {
                QStringList macrosesList = macroPath.split(";", QString::SkipEmptyParts);
                foreach (QString macro, macrosesList) {
                    stream << QFileInfo(macro).fileName() << "\n";
                }
                stream << QString("5%1%2").arg(QLocale::system().decimalPoint()).arg(0) << "\n";
            }

            QString scenariosPath;

            QString firstMacro = macroPath.split(';').first();
            firstMacro = firstMacro.split("/").last();
            if (firstMacro.startsWith("v_")) {
                scenariosPath = this->macrosPath(VIBRO);
            } else {
                emit sigPlayerError(tr("Incorrect macro name %1").arg(firstMacro)); // "Некорректное имя макроса %1"
                continue;
            }

            stream << unpackArchivesPath() << "\n";
            stream << scenariosPath << "\n";
            stream << reportsOutputPath() << "\n";
            stream << dataOutputPath() << "\n";
            stream << QString::number(m_NPPUnitID) << "\n";
            stream << QString::number(m_VMSTypeID) << "\n";
            file.close();

            emit sigPlayerMessage(tr("Processing: ") + QFileInfo(macroPath).baseName() + QString(":") + archivePath);

            // Starting player
            QProcess playProcess;
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            if (env.contains("LD_LIBRARY_PATH"))
            {
                QString val = env.value("LD_LIBRARY_PATH");
                if (!val.contains(m_strBorlandPath, Qt::CaseSensitive))
                {
                    val = val + QString(":") + m_strBorlandPath;
                    env.insert("LD_LIBRARY_PATH", val);
                }
            }
            else
            {
                env.insert("LD_LIBRARY_PATH", m_strBorlandPath); // Add an environment variable
            }

            playProcess.setProcessEnvironment(env);
            playProcess.setWorkingDirectory(m_strPlayerPath);

            if (geteuid() == 0) // Если мы запущены от рута
                playProcess.start("su", QStringList() << "diaprom" << "-c" << skwExecFile);
            // Запуск от рута дает проблемы при подключении к X11
            else
                playProcess.start("sh", QStringList() << "-c" << skwExecFile);
            // playProcess.start(skwExecFile);

            if (playProcess.waitForStarted()) {
                //понижаем приоритет Skw_noice
                QProcess::execute("renice", QStringList() << "10" << QString::number(playProcess.processId())); // .pid()));

                //высылаем ошибку в случае возникновения
                if (!playProcess.waitForFinished(180000)) {
                    emit sigPlayerError(playProcess.errorString());
                    continue;
                }

                emit sigPlayerMessage("Player finished");

                if (!macroPath.contains(";")) {
                    QString macroName = QFileInfo(macroPath).baseName();
                    QString refMacroPath = QString("%1%2_ref.asc").arg(scenariosPath).arg(macroName);
                    if (QFileInfo(refMacroPath).exists()) {
                        if (!QFile(refMacroPath).remove()) {
                            qWarning() << "Can't remove 'ref' scenario";
                        }
                    }
                    //сохраняем обработанную пару
                    finishedMacrosHash.insert(QFileInfo(archivePath).baseName(), macroName);

                    //сохраняем список файлов, которые были созданы в результате прогонки сценария
                    QDir dir(dataOutputPath() + macroName);
                    QStringList files = dir.entryList(QDir::Files);

                    //формируем pdf файл из сгенерированных bmp
                    QDir reportsDir(reportsOutputPath() + macroName);
                    QString pdfFilePath = reportsOutputPath() + macroName + "/" + QFileInfo(archivePath).baseName() + ".pdf";
                    QFileInfoList bmpFiles = reportsDir.entryInfoList(QStringList() << "*.bmp", QDir::Files);
                    if (createPdf(pdfFilePath, bmpFiles)) {
                        //удаляем bmp файлы
                        foreach (QFileInfo bmpFile, bmpFiles) {
                            QFile::remove(bmpFile.absoluteFilePath());
                        }
                    }
                    COMPLETED_CONFIG->setValue(macroName, files);
                    progress += progressStep;
                    emit sigPlayerFinished(progress);
                    emit sigPlayerMessage(QString("PDF completed: ") + pdfFilePath);
                }
            }
        }

        // удаляем данные архива
        QDir dir(unpackArchivesPath());
        dir.setFilter(QDir::NoDotAndDotDot | QDir::Files);
        foreach(QString dirItem, dir.entryList())
            dir.remove(dirItem);
    }

    saveFinishedItemsHash(finishedMacrosHash);

    setActive(false);
    emit sigPlayerFinished(100);
}

void RunMacroMgr::setActive(bool active)
{
    QMutexLocker locker(&m_mutex);
    m_isActive = active;
}

void RunMacroMgr::saveFinishedItemsHash(const QMultiHash<QString, QString> &data)
{
    //запись хэша с завершенными сценариями
    //сначала переводим в byte array, а затем пишем в ini
    //работаем через byte array, т.к. QMultiHash не читается из ini после перезапуска
    QByteArray finishedItemsData;
    QDataStream stream(&finishedItemsData, QIODevice::WriteOnly);
    stream << data;

    COMPLETED_CONFIG->setValue("Completed", finishedItemsData);
}

bool RunMacroMgr::createPdf(const QString &filePath, const QFileInfoList &bmpFiles)
{
    QPrinter printer;
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(filePath);

    QPainter painter;
    if (!painter.begin(&printer))
        return false;

    //рисуем каждый bmp файл на отдельной странице
    for (int i = 0; i < bmpFiles.count(); ++i) {
        QImage image(bmpFiles.at(i).absoluteFilePath());
        painter.drawImage(0, 0, image);

        if (i != bmpFiles.count() - 1) {
            if (!printer.newPage())
                return false;
        }
    }

    painter.end();

    return true;
}
