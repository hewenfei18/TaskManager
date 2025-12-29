#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include "tasktablemodel.h"
#include "databasemanager.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onBtnAddClicked();
    void onBtnEditClicked();
    void onBtnDeleteClicked();
    void onBtnRefreshFilterClicked();
    void onBtnExportPdfClicked();
    void onBtnExportCsvClicked();

    void on_btnArchiveCompleted_clicked();
    void on_btnViewArchive_clicked();
    void on_btnSearch_clicked();
    void on_btnGenerateReport_clicked();

    // 新增：筛选条件变化的槽函数
    void onFilterChanged();

private:
    Ui::MainWindow *ui;
    TaskTableModel* m_taskModel;

    void initFilterComboBoxes();
    void initTagFilter();
    void updateStatisticPanel();
    bool showTaskDialog(Task &task, bool isEdit);
};

#endif // MAINWINDOW_H
