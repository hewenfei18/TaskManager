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
#include "tasktablemodel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 按钮点击槽函数
    void on_btnAdd_clicked();
    void on_btnEdit_clicked();
    void on_btnDelete_clicked();
    void on_btnRefreshFilter_clicked();
    void on_btnExportPdf_clicked();
    void on_btnExportCsv_clicked();

    // 逾期任务提醒槽函数
    void handleOverdueTasks(const QList<Task>& overdueTasks);

    // 更新统计面板
    void updateStatisticPanel();

private:
    Ui::MainWindow *ui;
    // 任务表格模型
    TaskTableModel* m_taskModel;
    // 筛选代理模型
    QSortFilterProxyModel* m_proxyModel;

    // 显示添加/编辑任务对话框
    bool showTaskDialog(Task& task, bool isEdit = false);

    // 应用筛选条件
    void applyFilter();
};

#endif // MAINWINDOW_H
