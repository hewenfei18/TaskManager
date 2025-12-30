#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QTimer>

struct Task;
namespace Ui { class MainWindow; }
class TaskTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void taskUpdated(); // 信号声明

private slots:
    // 所有按钮/筛选的槽函数声明
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
    void on_btnGenerateReport_clicked(); // 新增：生成统计报表的槽函数
private:
    struct TaskReminder; // 内部结构体声明
    Ui::MainWindow *ui;
    TaskTableModel *m_taskModel;
    QMap<int, TaskReminder> m_taskReminders;

    // 私有成员函数声明
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
