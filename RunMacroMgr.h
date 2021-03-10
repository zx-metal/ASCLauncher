#ifndef RUNMACROMGR_H
#define RUNMACROMGR_H

#include <QObject>
#include <QDate>
#include <QMultiHash>
#include <QMutex>
#include <QProcess>
#include <QFileInfoList>

const QString playerExecutable = "Skw_noise";

typedef struct tagRunMacroMgrSettings
{
    QString strArchivesPath;
    QString strPlayerPath;
    int NPPUnitID;
    int VMSTypeID;
    int OutputSizeLimitMB;
    QString strBorlandPath;
    QString strPlayerOutputRoot;
} RunMacroMgrSettings;

class RunMacroMgr : public QObject
{
    Q_OBJECT
public:
    enum ScenarioType {
        VIBRO,
        TERMO,
        ScenarioTypesCount
    };

    explicit RunMacroMgr(RunMacroMgrSettings *settings, QObject *parent = 0);

    QString playerOutputRoot();
    QString dataOutputPath();
    QString reportsOutputPath();
    QString unpackArchivesPath();
    QString macrosPath(ScenarioType type);
    QString macrosRoot();

    //выдает список путей к архивам (фильтруется по заданному промежутку времени)
    QStringList getArchivePaths(const QDate &startDate, const QDate &endDate);

    //выдает список путей к сценариям
    QStringList getMacroPaths();

    //проверяет не были ли удалены сгенерированные сценариями файла
    //если да, то удаляет сгенерированный сценарием каталог полностью
    //и удаляет сценарий из списка прогнанных
    void verifyFinishedItemsHash(const QStringList &macroPaths);

    //получаем хэш с обработанными архивами/сценариями
    QMultiHash<QString /*имя архива*/, QString /*имя сценария*/> getFinishedItemsHash() const;

    //запуск Play для заданных архивов в отдельном потоке
    bool startPlay(const QMultiHash<QString /*имя архива*/, QString /*имя сценария*/> &data);

    //true если Play сейчас в работе
    bool isActive() const;

signals:
    //высылается когда Play запущен и начат парсинг
    void sigPlayerStarted();

    //высылается после завершения работы Play
    void sigPlayerFinished(const int &progress);

    //высылается в случае возникновения ошибки при работе Play
    void sigPlayerError(const QString &errorText);

    //высылается в поцессе работы Play
    void sigPlayerMessage(const QString &messageText);

private:
    //функция запуска play. Работает в отдельном треде
    void runPlayerExecutable(const QMultiHash<QString /*имя архива*/, QString /*имя сценария*/> &data);

    //изменяет флаг активности Play
    void setActive(bool active);

    //сохраняет хэш прогнанных сценариев
    void saveFinishedItemsHash(const QMultiHash<QString /*имя архива*/, QString /*имя сценария*/> &data);

    //сохраняет PDF файл из bmpFiles
    bool createPdf(const QString &filePath, const QFileInfoList &bmpFiles);

    //удаляет результат работы play при превышении заданного объема
    void clearOversizedOutput();

    int m_NPPUnitID;
    int m_VMSTypeID;
    int m_OutputSizeLimitMB;

    QString m_strArchivesPath;
    QString m_strPlayerPath;
    QString m_strBorlandPath;
    QString m_strPlayerOutputRoot;

    bool m_isActive;
    mutable QMutex m_mutex;
};

#endif // RUNMACROMGR_H
