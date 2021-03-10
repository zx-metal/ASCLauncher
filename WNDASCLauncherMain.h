#ifndef WNDASCLAUNCHERMAIN_H
#define WNDASCLAUNCHERMAIN_H

#include <QMainWindow>
#include <QSettings>

namespace Ui {
class WNDASCLauncherMain;
}

class RunMacroMgr;

class WNDASCLauncherMain : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit WNDASCLauncherMain(QWidget *parent = 0);
    ~WNDASCLauncherMain();

private slots:
    void on_btnRunMacro_clicked();

    void showMacrosProgress(const int &progress);
    void showMacrosMessage(const QString &messageText);
    void showMacrosErrorMessageBox(const QString &messageText);
private:
    Ui::WNDASCLauncherMain *ui;
    RunMacroMgr *m_RunMacroMgr;
};

#endif // WNDASCLAUNCHERMAIN_H
