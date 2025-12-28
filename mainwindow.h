#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QTimer>
#include <QSet>
#include "databasemanager.h" // 包含这个头文件，直接使用其中的Task结构体

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TaskTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_btnAdd_clicked();
    void on_btnEdit_clicked();
    void on_btnDelete_clicked();
    void on_btnRefreshFilter_clicked();
    void on_btnExportPdf_clicked();
    void on_btnExportCsv_clicked();
    void handleOverdueTasks(const QList<Task>& overdueTasks);
    void updateStatisticPanel();
    void checkRemindTasks();
    void on_checkBoxRemindEnable_toggled(bool checked);
    void on_spinBoxCheckInterval_valueChanged(int value);

private:
    Ui::MainWindow *ui;
    TaskTableModel* m_taskModel;
    QSortFilterProxyModel* m_proxyModel;
    QTimer* m_remindTimer;
    QSet<int> m_remindedTaskIds;

    bool showTaskDialog(Task& task, bool isEdit = false);
    void applyFilter();
    void showRemindDialog(const QList<Task>& remindTasks);
};

#endif // MAINWINDOW_H
