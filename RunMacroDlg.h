#ifndef RUNMACRODLG_H
#define RUNMACRODLG_H

#include <QDialog>
#include <QDate>

namespace Ui {
class RunMacroDlg;
}

class QStandardItemModel;
class RunMacroMgr;
class QStandardItem;

class RunMacroDlg : public QDialog
{
    Q_OBJECT

public:
    explicit RunMacroDlg(RunMacroMgr *manager, QWidget *parent = 0);
    ~RunMacroDlg();

public slots:
    //переопределяем действие на при клике на OK
    virtual void accept();

private slots:
    //слот вызывается при смене начальной даты в UI
    //нужен для установки минимальный даты для редактора конечной даты
    //чтобы начальная дата не могла быть больше конечной
    void onStartDateChanged(const QDate &startDate);

    //обновление TableView с учетом фильтрации
    void updateView();

    void checkAll();
    void checkAll(QStandardItemModel *model);
    void uncheckAll();
    void uncheckAll(QStandardItemModel *model);

    QMultiHash<QString /*путь к архиву*/, QString /*путь к сценарию*/> archiveMacroPairs(QStandardItemModel *model, bool joinMacros) const;

private:
    //инициализация диалога при его показе
    void init();

    void connectSignals();

    //заполняет модель заданными архивами и сценариями
    void fillModel(const QStringList &archivePaths, const QStringList &macroPaths);

    QStandardItemModel *m_asvTableModel;
    RunMacroMgr *m_runMacroMgr;

    //хранят даты начала и конца фильтрации
    //нужны для сохранения дат при закрыти диалога
    QDate m_startDate;
    QDate m_endDate;

    Ui::RunMacroDlg *ui;
};

#endif // RUNMACRODLG_H
