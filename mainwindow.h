#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QMap>
#include "tasktablemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 任务提醒结构体：存储任务ID、定时器、任务标题
struct TaskReminder
{
    int taskId;
    QTimer* reminderTimer;
    QString taskTitle;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void taskUpdated(); // 仅保留任务更新信号，移除全局提醒相关信号

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
    void onTaskReminderTriggered(int taskId); // 任务提醒触发槽函数

private:
    Ui::MainWindow *ui;
    TaskTableModel *m_taskModel;
    QMap<int, TaskReminder> m_taskReminders; // 存储所有任务的提醒定时器

    void initFilterComboBoxes();
    void initTagFilter();
    void updateStatisticPanel();
    bool showTaskDialog(Task &task, bool isEdit);
    void initTaskReminders(); // 初始化已有任务的提醒
    void removeTaskReminder(int taskId); // 移除指定任务的提醒
    void setTaskReminder(const Task &task); // 为任务设置提醒
};

#endif // MAINWINDOW_H
