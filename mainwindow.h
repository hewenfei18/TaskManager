#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>

// 前置声明
struct Task;
namespace Ui { class MainWindow; }
class TaskTableModel;
class StatisticDialog; // 前置声明统计报表对话框

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void taskUpdated();

private slots:
    // 原有槽函数
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
    // 新增：全局任务监测槽函数
    void onGlobalTaskMonitorTriggered();

private:
    // 内部结构体
    struct TaskReminder {
        int taskId;
        QTimer* reminderTimer;
        QString taskTitle;
    };

    Ui::MainWindow *ui;
    TaskTableModel *m_taskModel;
    QMap<int, TaskReminder> m_taskReminders;
    // 新增：成员变量声明
    StatisticDialog* m_reportDialog; // 统计报表对话框指针
    QTimer* m_globalTaskMonitorTimer; // 全局任务监测定时器

    // 原有私有函数
    void initFilterComboBoxes();
    void initTagFilter();
    void updateStatisticPanel();
    void initTaskReminders();
    void setTaskReminder(const Task &task);
    void removeTaskReminder(int taskId);
    void onTaskReminderTriggered(int taskId);
    bool showTaskDialog(Task &task, bool isEdit);
};

#endif // MAINWINDOW_H
