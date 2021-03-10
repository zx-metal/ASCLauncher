#include "RunMacroDlg.h"
#include "ui_RunMacroDlg.h"
#include "RunMacroMgr.h"

#include <QStandardItemModel>
#include <QFileInfo>

namespace {
static int PATH_ROLE = Qt::UserRole + 2;
}

RunMacroDlg::RunMacroDlg(RunMacroMgr *manager, QWidget *parent) :
    QDialog(parent)
  , m_asvTableModel(new QStandardItemModel(this))
  , m_runMacroMgr(manager)
  , ui(new Ui::RunMacroDlg)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));

    init();
    connectSignals();
}

RunMacroDlg::~RunMacroDlg()
{
    delete ui;
}

void RunMacroDlg::accept()
{
    QMultiHash<QString /*путь к архиву*/, QString /*путь к сценарию*/> resultHash;

    resultHash = archiveMacroPairs(m_asvTableModel, false);
    m_runMacroMgr->startPlay(resultHash);
    setResult(QDialog::Accepted);
    hide();
}

void RunMacroDlg::onStartDateChanged(const QDate &startDate)
{
    ui->deEnd->setMinimumDate(startDate);
}

void RunMacroDlg::init()
{
    ui->deStart->setDate(QDate(2000, 1, 1));
    ui->deEnd->setDate(QDate::currentDate());

    updateView();
    ui->asvTableView->setModel(m_asvTableModel);
}

void RunMacroDlg::connectSignals()
{
    bool connRes = connect(ui->pbUpdate, SIGNAL(clicked()), this, SLOT(updateView()));
    Q_ASSERT(connRes);

    connRes = connect(ui->deStart, SIGNAL(dateChanged(QDate)), this, SLOT(onStartDateChanged(QDate)));
    Q_ASSERT(connRes);

    connRes = connect(ui->pbCheckAll, SIGNAL(clicked()), this, SLOT(checkAll()));
    Q_ASSERT(connRes);

    connRes = connect(ui->pbUncheckAll, SIGNAL(clicked()), this, SLOT(uncheckAll()));
    Q_ASSERT(connRes);
}

void RunMacroDlg::updateView()
{
    m_asvTableModel->clear();
    m_startDate = ui->deStart->date();
    m_endDate = ui->deEnd->date();

    QStringList archivePaths = m_runMacroMgr->getArchivePaths(m_startDate, m_endDate);
    QStringList macroPaths = m_runMacroMgr->getMacroPaths();
    fillModel(archivePaths, macroPaths);

    ui->asvTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void RunMacroDlg::checkAll()
{
    checkAll(m_asvTableModel);
}

void RunMacroDlg::checkAll(QStandardItemModel *model)
{
    for (int i = 0; i < model->rowCount(); ++i) {
        for (int j = 1; j < model->columnCount(); ++j) {
            QStandardItem *item = model->item(i, j);
            if (item->isEnabled())
                item->setCheckState(Qt::Checked);
        }
    }
}

void RunMacroDlg::uncheckAll()
{
    uncheckAll(m_asvTableModel);
}

void RunMacroDlg::uncheckAll(QStandardItemModel *model)
{
    for (int i = 0; i < model->rowCount(); ++i) {
        for (int j = 1; j < model->columnCount(); ++j) {
            QStandardItem *item = model->item(i, j);
            if (item->isEnabled())
                item->setCheckState(Qt::Unchecked);
        }
    }
}

QMultiHash<QString /*путь к архиву*/, QString /*путь к сценарию*/> RunMacroDlg::archiveMacroPairs(QStandardItemModel *model, bool joinMacros) const
{
    QMultiHash<QString /*путь к архиву*/, QString /*путь к сценарию*/> resultHash;

    //проверяем чекбоксы
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem *archiveItem = model->item(i, 0);
        QString archivePath = archiveItem->data(PATH_ROLE).toString();

        QString macros;
        for (int j = 1; j < model->columnCount(); ++j) {
            QStandardItem *item = model->item(i, j);
            //если чекбокс включен и отмечен
            if (item->isEnabled() && item->checkState() == Qt::Checked) {
                //узнаем путь к сценарию
                QString macroPath = model->horizontalHeaderItem(j)->data(PATH_ROLE).toString();

                if (!joinMacros) {
                    //пишем пару в хэш
                    resultHash.insert(archivePath, macroPath);
                } else {
                    macros.append(macroPath + ";");
                }
            }
        }

        if (joinMacros && !macros.isEmpty()) {
            resultHash.insert(archivePath, macros);
        }
    }

    return resultHash;
}

void RunMacroDlg::fillModel(const QStringList &archivePaths, const QStringList &macroPaths)
{
    if (!archivePaths.isEmpty() && !macroPaths.isEmpty()) {
        m_asvTableModel->setHorizontalHeaderItem(0, new QStandardItem(tr("Archives")));

        int asvIndex = 1;
        foreach (QString path, macroPaths) {
            QFileInfo info(path);
            QStandardItem *item = new QStandardItem;
            item->setText(info.baseName());
            item->setData(path, PATH_ROLE);
            if (info.baseName().contains("v_")) {
                m_asvTableModel->setHorizontalHeaderItem(asvIndex++, item);
            }
        }

        m_runMacroMgr->verifyFinishedItemsHash(macroPaths);

        QMultiHash<QString, QString> finishedItems = m_runMacroMgr->getFinishedItemsHash();

        int asvCounter = 0;
        // задаем список архивов
        for (int i = 0; i < archivePaths.count(); ++i) {
            QFileInfo info(archivePaths.at(i));
            QStandardItem *item = new QStandardItem;
            item->setText(info.baseName());
            item->setData(archivePaths.at(i), PATH_ROLE);
            if (info.baseName().contains("ASV")) {
                m_asvTableModel->setItem(asvCounter, 0, item);
            }

            if (info.baseName().contains("ASV")) {
                // setting check boxes in ASV model
                for (int j = 1; j < m_asvTableModel->columnCount(); ++j) {
                    QStandardItem *item = new QStandardItem;
                    item->setCheckable(true);
                    item->setCheckState(Qt::Checked);

                    // disable checkbox if the pair { achive; macro } is in "Сompleted"
                    if (finishedItems.values(info.baseName()).contains(m_asvTableModel->horizontalHeaderItem(j)->text())) {
                        item->setEnabled(false);
                    }
                    m_asvTableModel->setItem(asvCounter, j, item);
                }
            }

            if (info.baseName().contains("ASV")) {
                asvCounter++;
            }
        }
    }
}
