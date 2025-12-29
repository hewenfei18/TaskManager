#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "tasktablemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    // 任务更新信号（用于重置提醒记录）
    void taskUpdated();
    // 提醒开关状态变更信号
    void reminderEnabledChanged(bool enabled);

private slots:
    void onBtnAddClicked();
    void onBtnEditClicked();
    void onBtnDeleteClicked();
    void onBtnExportPdfClicked();
    void onBtnExportCsvClicked();
    void onFilterChanged();
    void onBtnRefreshFilterClicked();
    void on_btnArchiveCompleted_clicked();
    void on_btnViewArchive_clicked();
    void on_btnSearch_clicked();
    void on_btnGenerateReport_clicked();
    // 提醒开关状态变更槽函数
    void on_reminderEnabledChanged(int state);
    // 提醒提前时间变更槽函数
    void on_reminderMinutesChanged(int minutes);

private:
    Ui::MainWindow *ui;
    TaskTableModel *m_taskModel;
    // 记录提醒开关状态
    bool m_reminderEnabled = false;
    // 记录提醒提前时间
    int m_reminderMinutes = 30;

    void initFilterComboBoxes();
    void initTagFilter();
    void updateStatisticPanel();
    bool showTaskDialog(Task &task, bool isEdit);
};

#endif // MAINWINDOW_H
