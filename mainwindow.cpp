#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "databasemanager.h"
#include "pdfexporter.h"
#include "csvexporter.h"
#include "tasktablemodel.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_taskModel(new TaskTableModel(this))
    , m_proxyModel(new QSortFilterProxyModel(this))
    , m_remindTimer(new QTimer(this))
{
    ui->setupUi(this);

    // 代理模型配置
    m_proxyModel->setSourceModel(m_taskModel);
    m_proxyModel->setFilterKeyColumn(-1);
    m_proxyModel->setFilterRole(Qt::DisplayRole);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    ui->tableViewTasks->setModel(m_proxyModel);

    // 表格属性设置
    ui->tableViewTasks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewTasks->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewTasks->resizeColumnsToContents();
    ui->tableViewTasks->horizontalHeader()->setStretchLastSection(true);

    // 筛选下拉框初始化
    ui->comboCategoryFilter->addItems({"全部", "工作", "学习", "生活", "其他"});
    ui->comboPriorityFilter->addItems({"全部", "高", "中", "低"});
    ui->comboStatusFilter->addItems({"全部", "未完成", "已完成"});

    // 定时提醒控件初始化
    ui->spinBoxRemindMinutes->setRange(0, 1440);
    ui->spinBoxRemindMinutes->setValue(30);
    ui->spinBoxCheckInterval->setRange(10, 300);
    ui->spinBoxCheckInterval->setValue(30);
    ui->checkBoxRemindEnable->setChecked(true);

    // 定时器绑定与启动
    connect(m_remindTimer, &QTimer::timeout, this, &MainWindow::checkRemindTasks);
    m_remindTimer->setInterval(ui->spinBoxCheckInterval->value() * 1000);
    m_remindTimer->start();

    // 统计面板更新绑定
    updateStatisticPanel();
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
    QModelIndex proxyIndex = ui->tableViewTasks->currentIndex();
    if (!proxyIndex.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要编辑的任务！");
        return;
    }

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
            // 编辑任务后，清除该任务的提醒标记，以便重新判断是否需要提醒
            m_remindedTaskIds.remove(task.id);
        } else {
            QMessageBox::critical(this, "失败", "任务编辑失败，请查看日志！");
        }
    }
}

void MainWindow::on_btnDelete_clicked()
{
    QModelIndex proxyIndex = ui->tableViewTasks->currentIndex();
    if (!proxyIndex.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要删除的任务！");
        return;
    }

    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    Task task = m_taskModel->getTaskAt(sourceIndex.row());
    if (task.id == -1) {
        QMessageBox::warning(this, "提示", "无效的任务！");
        return;
    }

    int ret = QMessageBox::question(this, "确认删除", QString("是否确定删除任务「%1」？").arg(task.title),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (DatabaseManager::instance().deleteTask(task.id)) {
        m_taskModel->refreshTasks();
        QMessageBox::information(this, "成功", "任务删除成功！");
        // 删除任务后，清除该任务的提醒标记
        m_remindedTaskIds.remove(task.id);
    } else {
        QMessageBox::critical(this, "失败", "任务删除失败，请查看日志！");
    }
}

void MainWindow::on_btnRefreshFilter_clicked()
{
    applyFilter();
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

    QFormLayout* layout = new QFormLayout(&dialog);
    layout->addRow("标题：", editTitle);
    layout->addRow("分类：", comboCategory);
    layout->addRow("优先级：", comboPriority);
    layout->addRow("截止时间：", dtEditDueTime);
    layout->addRow("状态：", chkStatus);
    layout->addRow("备注：", editDescription);
    layout->addWidget(buttonBox);

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

void MainWindow::applyFilter()
{
    QString categoryFilter = ui->comboCategoryFilter->currentText();
    QString priorityFilter = ui->comboPriorityFilter->currentText();
    QString statusFilter = ui->comboStatusFilter->currentText();

    QStringList filterParts;
    if (categoryFilter != "全部") {
        filterParts.append(categoryFilter);
    }
    if (priorityFilter != "全部") {
        filterParts.append(priorityFilter);
    }
    if (statusFilter != "全部") {
        filterParts.append(statusFilter);
    }

    QString filterText = filterParts.join("*");
    m_proxyModel->setFilterWildcard(QString("*%1*").arg(filterText));
    m_proxyModel->setFilterRole(Qt::DisplayRole);
    ui->tableViewTasks->resizeColumnsToContents();
}

// 核心修正：仅提醒即将到期任务，完全过滤已逾期任务
void MainWindow::checkRemindTasks()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    QList<Task> needRemindTasks; // 仅存储即将到期的任务（不包含已逾期）
    QDateTime currentTime = QDateTime::currentDateTime();
    int remindMinutes = ui->spinBoxRemindMinutes->value();
    qint64 remindSeconds = (qint64)remindMinutes * 60; // 提前提醒时长（秒）

    // 先清除已到期/已完成任务的提醒标记（避免标记残留）
    QList<int> needRemoveIds;
    for (int taskId : m_remindedTaskIds) {
        bool isTaskValid = false;
        bool isExpired = false;
        bool isCompleted = false;

        for (const Task& task : allTasks) {
            if (task.id == taskId) {
                isTaskValid = true;
                isExpired = (currentTime > task.dueTime);
                isCompleted = (task.status == 1);
                break;
            }
        }

        // 任务无效（已删除）、已逾期、已完成，都清除提醒标记
        if (!isTaskValid || isExpired || isCompleted) {
            needRemoveIds.append(taskId);
        }
    }
    // 批量移除无效标记
    for (int taskId : needRemoveIds) {
        m_remindedTaskIds.remove(taskId);
    }

    // 筛选即将到期的任务
    for (const Task& task : allTasks) {
        // 过滤条件：1. 未完成 2. 未被重复提醒 3. 未逾期（核心：只处理即将到期任务）
        if (task.status == 1 || m_remindedTaskIds.contains(task.id)) {
            continue;
        }

        // 计算当前时间到任务截止时间的差值（秒）：currentTime -> task.dueTime
        qint64 timeDiffSeconds = currentTime.secsTo(task.dueTime);

        // 仅保留「即将到期」条件：0 < 时间差 ≤ 提前提醒时长（例如：剩余1~30分钟）
        // 完全排除 timeDiffSeconds ≤ 0（已逾期）的任务
        bool willExpireSoon = (timeDiffSeconds > 0 && timeDiffSeconds <= remindSeconds);
        if (willExpireSoon) {
            needRemindTasks.append(task);
            m_remindedTaskIds.insert(task.id); // 标记为已提醒，避免重复弹窗
        }
    }

    // 有即将到期任务时，显示弹窗
    if (!needRemindTasks.isEmpty()) {
        showRemindDialog(needRemindTasks);
    }
}

// 优化：仅显示即将到期任务的提醒信息，无已逾期相关描述
void MainWindow::showRemindDialog(const QList<Task>& remindTasks)
{
    QString remindTitle = "即将到期任务提醒"; // 标题明确为即将到期
    QString remindContent = "发现以下任务即将到期，请及时处理：\n\n";

    for (const Task& task : remindTasks) {
        QDateTime currentTime = QDateTime::currentDateTime();
        qint64 timeDiffSeconds = currentTime.secsTo(task.dueTime);
        QString taskStatus;

        // 此时必然是即将到期（已过滤已逾期），计算剩余时长
        qint64 remainMinutes = timeDiffSeconds / 60;
        qint64 remainSeconds = timeDiffSeconds % 60;

        // 精准显示剩余时长（分+秒）
        if (remainMinutes <= 0) {
            taskStatus = QString("即将到期，剩余 %1 秒").arg(timeDiffSeconds);
        } else {
            taskStatus = QString("即将到期，剩余 %1 分 %2 秒").arg(remainMinutes).arg(remainSeconds);
        }

        // 拼接任务信息，仅展示即将到期状态
        remindContent += QString("• 标题：%1\n  分类：%2\n  截止时间：%3\n  状态：%4\n\n")
                             .arg(task.title)
                             .arg(task.category)
                             .arg(task.dueTime.toString("yyyy-MM-dd HH:mm"))
                             .arg(taskStatus);
    }

    QMessageBox::information(this, remindTitle, remindContent);
}

void MainWindow::on_checkBoxRemindEnable_toggled(bool checked)
{
    if (checked) {
        int interval = ui->spinBoxCheckInterval->value() * 1000;
        m_remindTimer->setInterval(interval);
        m_remindTimer->start();
    } else {
        m_remindTimer->stop();
        m_remindedTaskIds.clear(); // 禁用提醒时，清空所有提醒标记
    }
}

void MainWindow::on_spinBoxCheckInterval_valueChanged(int value)
{
    if (ui->checkBoxRemindEnable->isChecked()) {
        m_remindTimer->setInterval(value * 1000);
    }
}
