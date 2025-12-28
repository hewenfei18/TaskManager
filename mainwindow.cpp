#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "databasemanager.h"
#include "pdfexporter.h"
#include "csvexporter.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_taskModel(new TaskTableModel(this))
    , m_proxyModel(new QSortFilterProxyModel(this))
{
    ui->setupUi(this);

    // 配置代理模型 - 修复顺序 + 新增关键配置
    m_proxyModel->setSourceModel(m_taskModel);
    m_proxyModel->setFilterKeyColumn(-1); // 过滤所有列
    m_proxyModel->setFilterRole(Qt::DisplayRole); // 先设置过滤角色（关键：移到这里，提前生效）
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive); // 忽略大小写，提高兼容性
    ui->tableViewTasks->setModel(m_proxyModel);

    // 设置表格属性
    ui->tableViewTasks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTasks->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewTasks->resizeColumnsToContents();
    ui->tableViewTasks->horizontalHeader()->setStretchLastSection(true);

    // 初始化筛选下拉框
    ui->comboCategoryFilter->addItems({"全部", "工作", "学习", "生活", "其他"});
    ui->comboPriorityFilter->addItems({"全部", "高", "中", "低"});
    ui->comboStatusFilter->addItems({"全部", "未完成", "已完成"});

    // 更新统计面板
    updateStatisticPanel();

    // 连接模型数据变化信号到统计面板更新
    connect(m_taskModel, &QAbstractTableModel::dataChanged, this, &MainWindow::updateStatisticPanel);
    connect(m_taskModel, &QAbstractTableModel::modelReset, this, &MainWindow::updateStatisticPanel);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnAdd_clicked()
{
    Task task;
    if (showTaskDialog(task, false)) {
        if (DatabaseManager::instance().addTask(task)) {
            m_taskModel->refreshTasks();
            QMessageBox::information(this, "成功", "任务添加成功！");
        } else {
            QMessageBox::critical(this, "失败", "任务添加失败，请查看日志！");
        }
    }
}

void MainWindow::on_btnEdit_clicked()
{
    // 获取选中行
    QModelIndex proxyIndex = ui->tableViewTasks->currentIndex();
    if (!proxyIndex.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要编辑的任务！");
        return;
    }

    // 转换为源模型索引
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    Task task = m_taskModel->getTaskAt(sourceIndex.row());
    if (task.id == -1) {
        QMessageBox::warning(this, "提示", "无效的任务！");
        return;
    }

    if (showTaskDialog(task, true)) {
        if (DatabaseManager::instance().updateTask(task)) {
            m_taskModel->refreshTasks();
            QMessageBox::information(this, "成功", "任务编辑成功！");
        } else {
            QMessageBox::critical(this, "失败", "任务编辑失败，请查看日志！");
        }
    }
}

void MainWindow::on_btnDelete_clicked()
{
    // 获取选中行
    QModelIndex proxyIndex = ui->tableViewTasks->currentIndex();
    if (!proxyIndex.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要编辑的任务！");
        return;
    }

    // 转换为源模型索引
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    Task task = m_taskModel->getTaskAt(sourceIndex.row());
    if (task.id == -1) {
        QMessageBox::warning(this, "提示", "无效的任务！");
        return;
    }

    // 二次确认
    int ret = QMessageBox::question(this, "确认删除", QString("是否确定删除任务「%1」？").arg(task.title),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (DatabaseManager::instance().deleteTask(task.id)) {
        m_taskModel->refreshTasks();
        QMessageBox::information(this, "成功", "任务删除成功！");
    } else {
        QMessageBox::critical(this, "失败", "任务删除失败，请查看日志！");
    }
}

void MainWindow::on_btnRefreshFilter_clicked()
{
    applyFilter(); // 此处信号槽绑定正常，按钮点击会触发
}

void MainWindow::on_btnExportPdf_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出PDF", QDir::homePath(), "PDF文件 (*.pdf)");
    if (filePath.isEmpty()) {
        return;
    }

    QList<Task> tasks = DatabaseManager::instance().getAllTasks();
    PdfExporter::exportToPdf(tasks, filePath);
    QMessageBox::information(this, "成功", "PDF导出成功！");
}

void MainWindow::on_btnExportCsv_clicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出CSV", QDir::homePath(), "CSV文件 (*.csv)");
    if (filePath.isEmpty()) {
        return;
    }

    QList<Task> tasks = DatabaseManager::instance().getAllTasks();
    CsvExporter::exportToCsv(tasks, filePath);
    QMessageBox::information(this, "成功", "CSV导出成功！");
}

void MainWindow::handleOverdueTasks(const QList<Task> &overdueTasks)
{
    QString message = "发现以下逾期未完成任务：\n\n";
    for (const Task& task : overdueTasks) {
        message += QString("• 标题：%1（分类：%2，截止时间：%3）\n").arg(
            task.title, task.category, task.dueTime.toString("yyyy-MM-dd HH:mm"));
    }

    QMessageBox::warning(this, "逾期提醒", message);
}

void MainWindow::updateStatisticPanel()
{
    DatabaseManager& dbMgr = DatabaseManager::instance();
    ui->labelTotal->setText(QString("总任务数：%1").arg(dbMgr.getTotalTaskCount()));
    ui->labelCompleted->setText(QString("已完成任务数：%1").arg(dbMgr.getCompletedTaskCount()));
    ui->labelOverdue->setText(QString("逾期未完成任务数：%1").arg(dbMgr.getOverdueUncompletedCount()));
}

bool MainWindow::showTaskDialog(Task &task, bool isEdit)
{
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "编辑任务" : "添加任务");
    dialog.setModal(true);
    dialog.resize(400, 350);

    // 创建控件
    QLineEdit* editTitle = new QLineEdit(&dialog);
    editTitle->setText(task.title);

    QComboBox* comboCategory = new QComboBox(&dialog);
    comboCategory->addItems({"工作", "学习", "生活", "其他"});
    int categoryIndex = comboCategory->findText(task.category);
    if (categoryIndex != -1) {
        comboCategory->setCurrentIndex(categoryIndex);
    }

    QComboBox* comboPriority = new QComboBox(&dialog);
    comboPriority->addItems({"高", "中", "低"});
    int priorityIndex = comboPriority->findText(task.priority);
    if (priorityIndex != -1) {
        comboPriority->setCurrentIndex(priorityIndex);
    }

    QDateTimeEdit* dtEditDueTime = new QDateTimeEdit(&dialog);
    dtEditDueTime->setDateTime(task.dueTime.isValid() ? task.dueTime : QDateTime::currentDateTime().addDays(1));
    dtEditDueTime->setDisplayFormat("yyyy-MM-dd HH:mm");
    dtEditDueTime->setCalendarPopup(true);

    QCheckBox* chkStatus = new QCheckBox("已完成", &dialog);
    chkStatus->setChecked(task.status == 1);

    QTextEdit* editDescription = new QTextEdit(&dialog);
    editDescription->setText(task.description);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 布局
    QFormLayout* layout = new QFormLayout(&dialog);
    layout->addRow("标题：", editTitle);
    layout->addRow("分类：", comboCategory);
    layout->addRow("优先级：", comboPriority);
    layout->addRow("截止时间：", dtEditDueTime);
    layout->addRow("状态：", chkStatus);
    layout->addRow("备注：", editDescription);
    layout->addWidget(buttonBox);

    // 验证标题非空
    if (dialog.exec() == QDialog::Accepted) {
        if (editTitle->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "提示", "任务标题不能为空！");
            return false;
        }

        task.title = editTitle->text().trimmed();
        task.category = comboCategory->currentText();
        task.priority = comboPriority->currentText();
        task.dueTime = dtEditDueTime->dateTime();
        task.status = chkStatus->isChecked() ? 1 : 0;
        task.description = editDescription->toPlainText();

        return true;
    }

    return false;
}

// ########### 核心修复：彻底重构applyFilter函数 ###########
void MainWindow::applyFilter()
{
    QString categoryFilter = ui->comboCategoryFilter->currentText();
    QString priorityFilter = ui->comboPriorityFilter->currentText();
    QString statusFilter = ui->comboStatusFilter->currentText();

    // 1. 修复：状态筛选不再转换为"0"/"1"，直接使用中文字符串（与模型DisplayRole一致）
    QString statusText = statusFilter; // 直接获取"未完成"/"已完成"

    // 2. 构建过滤文本（兼容多条件同时筛选）
    QStringList filterParts;
    if (categoryFilter != "全部") {
        filterParts.append(categoryFilter);
    }
    if (priorityFilter != "全部") {
        filterParts.append(priorityFilter);
    }
    if (statusFilter != "全部") {
        filterParts.append(statusText); // 添加状态中文文本
    }

    // 3. 修复：使用模糊匹配（*表示任意字符），解决精确匹配导致的筛选失效
    QString filterText = filterParts.join("*"); // 多条件用*连接，支持模糊匹配
    m_proxyModel->setFilterWildcard(QString("*%1*").arg(filterText));

    // 4. 确保过滤角色生效（此处可再次设置，双重保障）
    m_proxyModel->setFilterRole(Qt::DisplayRole);

    // 刷新表格
    ui->tableViewTasks->resizeColumnsToContents();
}
