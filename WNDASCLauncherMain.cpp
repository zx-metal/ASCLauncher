#include "WNDASCLauncherMain.h"
#include "ui_WNDASCLauncherMain.h"
#include <QtGui>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>
#include <QStringList>

#include <RunMacroDlg.h>
#include <RunMacroMgr.h>
#include <ConfigManager.h>

#define SYSTEM_CONFIG ConfigManager::getInstance("System", "ASCLauncher.ini")

QString strSettingsFileName;

RunMacroMgrSettings *settings;

WNDASCLauncherMain::WNDASCLauncherMain(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::WNDASCLauncherMain)
{
    ui->setupUi(this);

    move(QGuiApplication::screens().at(0)->geometry().center() - frameGeometry().center());

    ui->pbCompleted->setVisible(true);
    ui->pbCompleted->setValue(0);
    ui->btnRunMacro->setEnabled(true);

    settings = new RunMacroMgrSettings;

    settings->NPPUnitID = SYSTEM_CONFIG->getIntegerValue("NPPUnitID", 1);
    settings->VMSTypeID = SYSTEM_CONFIG->getIntegerValue("VMSTypeID", 1);

    settings->OutputSizeLimitMB = SYSTEM_CONFIG->getIntegerValue("OutputSizeLimitMB", 512);
    settings->strPlayerPath = SYSTEM_CONFIG->getStringValue("PlayerPath");
    settings->strBorlandPath = SYSTEM_CONFIG->getStringValue("BorlandPath");
    settings->strArchivesPath = SYSTEM_CONFIG->getStringValue("ArchivesPath");
    settings->strPlayerOutputRoot = SYSTEM_CONFIG->getStringValue("PlayerOutputRoot", "/home/diaprom/SKW");

    m_RunMacroMgr = new RunMacroMgr(settings, this);

    bool ConnRes = connect(m_RunMacroMgr, SIGNAL(sigPlayerFinished(int)), this, SLOT(showMacrosProgress(int)));
    Q_ASSERT(ConnRes);

    ConnRes = connect(m_RunMacroMgr, SIGNAL(sigPlayerError(QString)), this, SLOT(showMacrosErrorMessageBox(QString)));
    Q_ASSERT(ConnRes);

    ConnRes = connect(m_RunMacroMgr, SIGNAL(sigPlayerMessage(QString)), this, SLOT(showMacrosMessage(QString)));
    Q_ASSERT(ConnRes);
}

WNDASCLauncherMain::~WNDASCLauncherMain()
{
    delete ui;
}

void WNDASCLauncherMain::showMacrosProgress(const int &progress)
{
    ui->pbCompleted->setValue(progress);
    if (progress == 100)
    {
        QMessageBox::information(this, tr("Macro execution"), tr("Macro execution completed successfully"));
    }
}

void WNDASCLauncherMain::showMacrosMessage(const QString &messageText)
{
    QString str = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->edProtocol->append(QString("[") + str + QString("] ") + messageText);
}

void WNDASCLauncherMain::showMacrosErrorMessageBox(const QString &messageText)
{
    QMessageBox::critical(this, tr("Macro execution error"), tr("Macro execution error") + messageText);
}

void WNDASCLauncherMain::on_btnRunMacro_clicked()
{
    ui->edProtocol->clear();
    RunMacroDlg dlg(m_RunMacroMgr);
    dlg.exec();
}
