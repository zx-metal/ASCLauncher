QT       += core gui printsupport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

DESTDIR =      $${PWD}/build
OBJECTS_DIR =  $${DESTDIR}/obj/
MOC_DIR =      $${DESTDIR}/moc/
UI_DIR =       $${DESTDIR}/ui/
RCC_DIR =      $${DESTDIR}/rcc/

DESTDIR =      $${PWD}

TARGET = ASCLauncher
TEMPLATE = app

SOURCES += \
    RunMacroDlg.cpp \
    RunMacroMgr.cpp \
    main.cpp \
    WNDASCLauncherMain.cpp \
    ConfigManager.cpp

HEADERS += \
    RunMacroDlg.h \
    RunMacroMgr.h \
    WNDASCLauncherMain.h \
    ConfigManager.h

FORMS += \
    RunMacroDlg.ui \
    WNDASCLauncherMain.ui

TRANSLATIONS += \
    ASCLauncher_ru.ts
