#include <QApplication>
#include <QTranslator>
#include <QDir>
#include <QTextCodec>


#include "WNDASCLauncherMain.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QDir dataDir(qApp->applicationDirPath());
    const QString &dataPath = dataDir.cleanPath(dataDir.path());
    translator.load("ASCLauncher_ru.qm", dataPath);
    a.installTranslator(&translator);

    WNDASCLauncherMain w;
    w.show();
    
    return a.exec();
}
