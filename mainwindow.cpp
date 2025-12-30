#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "databasemanager.h"
#include "tasktablemodel.h"
#include "archivedialog.h"
#include "statisticdialog.h"
#include <QMessageBox>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QHeaderView>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_taskModel(new TaskTableModel(this))
{
    ui->setupUi(this);

    // 初始化数据库
    if (!DatabaseManager::instance().init()) {
        QMessageBox::critical(this, "错误", "数据库初始化失败！");
        this->close();
        return;
    }

    // 绑定表格模型
    ui->tableViewTasks->setModel(m_taskModel);
    // 调整表格列宽
    QHeaderView* header = ui->tableViewTasks->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->resizeSection(3, 150); // 截止时间列
    header->resizeSection(4, 80);  // 状态列
    header->setSectionResizeMode(header->count() - 1, QHeaderView::Stretch);

    // 初始化筛选下拉框
    initFilterComboBoxes();
    // 初始化标签筛选下拉框
    initTagFilter();

    // 启动时主动刷新任务列表
    m_taskModel->refreshTasks();

    // 更新统计面板
    updateStatisticPanel();

    // 初始化已有任务的提醒
    initTaskReminders();

    // 原有功能信号槽（全部保留）
    connect(ui->btnAdd, &QPushButton::clicked, this, &MainWindow::onBtnAddClicked);
    connect(ui->btnEdit, &QPushButton::clicked, this, &MainWindow::onBtnEditClicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &MainWindow::onBtnDeleteClicked);
    connect(ui->btnExportPdf, &QPushButton::clicked, this, &MainWindow::onBtnExportPdfClicked);
    connect(ui->btnExportCsv, &QPushButton::clicked, this, &MainWindow::onBtnExportCsvClicked);

    // 筛选相关信号槽
    connect(ui->comboCategoryFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboPriorityFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboStatusFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboTagFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->btnRefreshFilter, &QPushButton::clicked, this, &MainWindow::onBtnRefreshFilterClicked);

    // 归档、报表、搜索信号槽
    connect(ui->btnArchiveCompleted, &QPushButton::clicked, this, &MainWindow::on_btnArchiveCompleted_clicked);
    connect(ui->btnViewArchive, &QPushButton::clicked, this, &MainWindow::on_btnViewArchive_clicked);
    connect(ui->btnSearch, &QPushButton::clicked, this, &MainWindow::on_btnSearch_clicked);
    connect(ui->btnGenerateReport, &QPushButton::clicked, this, &MainWindow::on_btnGenerateReport_clicked);
}

MainWindow::~MainWindow()
{
    // 销毁所有任务提醒定时器，防止内存泄漏
    QMap<int, TaskReminder>::iterator it = m_taskReminders.begin();
    while (it != m_taskReminders.end()) {
        if (it.value().reminderTimer) {
            it.value().reminderTimer->stop();
            delete it.value().reminderTimer;
        }
        ++it;
    }
    m_taskReminders.clear();

    DatabaseManager::instance().close();
    delete ui;
}

// 初始化已有任务的提醒
void MainWindow::initTaskReminders()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    for (const Task& task : allTasks) {
        // 仅为有效提醒时间（未过期、未完成）的任务设置提醒
        if (task.remindTime.isValid() && task.remindTime > QDateTime::currentDateTime() && task.status == 0) {
            setTaskReminder(task);
        }
    }
}

// 为任务设置提醒
void MainWindow::setTaskReminder(const Task &task)
{
    // 若任务已有提醒，先移除旧提醒
    if (m_taskReminders.contains(task.id)) {
        removeTaskReminder(task.id);
    }

    // 无效提醒条件：提醒时间无效/已过期/任务已完成
    if (!task.remindTime.isValid() || task.remindTime <= QDateTime::currentDateTime() || task.status != 0) {
        return;
    }

    // 计算提醒延时（毫秒）
    qint64 delayMs = QDateTime::currentDateTime().msecsTo(task.remindTime);
    if (delayMs <= 0) {
        return;
    }

    // 创建单次定时器
    QTimer* reminderTimer = new QTimer(this);
    reminderTimer->setSingleShot(true);
    reminderTimer->setInterval(delayMs);

    // 直接插入映射，避免单独定义变量
    m_taskReminders.insert(task.id, {
                                        task.id,
                                        reminderTimer,
                                        task.title
                                    });

    // 绑定定时器触发信号
    connect(reminderTimer, &QTimer::timeout, this, [=]() {
        onTaskReminderTriggered(task.id);
    });

    // 启动定时器
    reminderTimer->start();
    qDebug() << "任务[" << task.title << "]提醒已设置，触发时间：" << task.remindTime.toString("yyyy-MM-dd HH:mm:ss");
}

// 移除指定任务的提醒
void MainWindow::removeTaskReminder(int taskId)
{
    if (!m_taskReminders.contains(taskId)) {
        return;
    }

    // 停止并删除定时器
    TaskReminder &taskReminder = m_taskReminders[taskId];
    if (taskReminder.reminderTimer) {
        taskReminder.reminderTimer->stop();
        delete taskReminder.reminderTimer;
        taskReminder.reminderTimer = nullptr;
    }

    // 从映射中移除
    m_taskReminders.remove(taskId);
    qDebug() << "任务ID[" << taskId << "]提醒已移除";
}

// 任务提醒触发
void MainWindow::onTaskReminderTriggered(int taskId)
{
    if (!m_taskReminders.contains(taskId)) {
        return;
    }

    TaskReminder &taskReminder = m_taskReminders[taskId];
    // 查询最新任务信息
    Task task = DatabaseManager::instance().getTaskById(taskId);
    if (!task.isValid() || task.status == 1) {
        removeTaskReminder(taskId);
        return;
    }

    // 弹出提醒窗口
    QString tipText = QString("【任务提醒】\n任务名称：%1\n任务分类：%2\n优先级：%3\n截止时间：%4\n提醒时间：%5")
                          .arg(task.title)
                          .arg(task.category)
                          .arg(task.priority)
                          .arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"))
                          .arg(task.remindTime.toString("yyyy-MM-dd HH:mm:ss"));
    QMessageBox::information(this, "任务提醒", tipText);

    // 触发后移除该任务提醒
    removeTaskReminder(taskId);
}

// 初始化筛选下拉框
void MainWindow::initFilterComboBoxes()
{
    // 分类筛选：comboCategoryFilter
    ui->comboCategoryFilter->clear();
    ui->comboCategoryFilter->addItem("全部分类");
    ui->comboCategoryFilter->addItems({"工作", "学习", "生活", "其他"});

    // 优先级筛选：comboPriorityFilter
    ui->comboPriorityFilter->clear();
    ui->comboPriorityFilter->addItem("全部优先级");
    ui->comboPriorityFilter->addItems({"高", "中", "低"});

    // 状态筛选：comboStatusFilter
    ui->comboStatusFilter->clear();
    ui->comboStatusFilter->addItem("全部状态");
    ui->comboStatusFilter->addItems({"未完成", "已完成", "未完成（已超期）"});
}

// 初始化标签筛选下拉框
void MainWindow::initTagFilter()
{
    // 标签筛选：comboTagFilter
    ui->comboTagFilter->clear();
    ui->comboTagFilter->addItem("全部标签");
    ui->comboTagFilter->addItems(DatabaseManager::instance().getAllDistinctTags());
}

// 更新统计面板
void MainWindow::updateStatisticPanel()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    int total = allTasks.count();
    int completed = 0;
    int overdue = 0;

    for (const Task& task : allTasks) {
        if (task.status == 1) {
            completed++;
        }
        if (task.dueTime < QDateTime::currentDateTime() && task.status == 0) {
            overdue++;
        }
    }

    // 统计标签：labelTotal、labelCompleted、labelOverdue
    ui->labelTotal->setText(QString("总任务：%1").arg(total));
    ui->labelCompleted->setText(QString("已完成：%1").arg(completed));
    ui->labelOverdue->setText(QString("逾期：%1").arg(overdue));
}

// 添加任务按钮点击事件
void MainWindow::onBtnAddClicked()
{
    Task task;
    if (showTaskDialog(task, false)) {
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        // 为新任务设置提醒
        setTaskReminder(task);
        emit taskUpdated();
    }
}

// 编辑任务按钮点击事件
void MainWindow::onBtnEditClicked()
{
    QModelIndex index = ui->tableViewTasks->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要编辑的任务！");
        return;
    }

    Task task = m_taskModel->getTaskAt(index.row());
    if (showTaskDialog(task, true)) {
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        // 更新任务提醒
        setTaskReminder(task);
        emit taskUpdated();
    }
}

// 删除任务按钮点击事件
void MainWindow::onBtnDeleteClicked()
{
    QModelIndex index = ui->tableViewTasks->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要删除的任务！");
        return;
    }

    int taskId = m_taskModel->getTaskAt(index.row()).id;
    int ret = QMessageBox::question(this, "确认删除", "确定要删除该任务吗？",
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (DatabaseManager::instance().deleteTask(taskId)) {
        // 删除任务时移除对应提醒
        removeTaskReminder(taskId);
        QMessageBox::information(this, "成功", "任务删除成功！");
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        emit taskUpdated();
    } else {
        QMessageBox::critical(this, "失败", "任务删除失败！");
    }
}

// 刷新筛选按钮点击事件
void MainWindow::onBtnRefreshFilterClicked()
{
    m_taskModel->refreshTasks();
    updateStatisticPanel();
    initTagFilter();
}

// 导出PDF按钮点击事件
void MainWindow::onBtnExportPdfClicked()
{
    QMessageBox::information(this, "提示", "PDF导出功能待实现");
}

// 导出CSV按钮点击事件
void MainWindow::onBtnExportCsvClicked()
{
    QMessageBox::information(this, "提示", "CSV导出功能待实现");
}

// 筛选条件变更事件
void MainWindow::onFilterChanged()
{
    QString category = ui->comboCategoryFilter->currentText();
    QString priority = ui->comboPriorityFilter->currentText();
    QString status = ui->comboStatusFilter->currentText();
    QString tag = ui->comboTagFilter->currentText();

    m_taskModel->setFilterConditions(category, priority, status, tag);
    updateStatisticPanel();
}

// 归档已完成任务按钮点击事件
void MainWindow::on_btnArchiveCompleted_clicked()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    int completedCount = 0;
    QList<int> completedTaskIds;
    for (const Task& task : allTasks) {
        if (task.status == 1) {
            completedCount++;
            completedTaskIds.append(task.id);
        }
    }
    if (completedCount == 0) {
        QMessageBox::information(this, "提示", "当前无已完成任务，无需归档！");
        return;
    }

    int ret = QMessageBox::question(this, "确认归档", "是否确定归档所有已完成任务？",
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (DatabaseManager::instance().archiveCompletedTasks()) {
        // 移除已归档任务的提醒
        for (int taskId : completedTaskIds) {
            removeTaskReminder(taskId);
        }
        QMessageBox::information(this, "成功", QString("已成功归档%1个已完成任务！").arg(completedCount));
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        emit taskUpdated();
    } else {
        QMessageBox::critical(this, "失败", "任务归档失败！");
    }
}

// 查看归档任务按钮点击事件
void MainWindow::on_btnViewArchive_clicked()
{
    ArchiveDialog* dialog = new ArchiveDialog(this);
    connect(dialog, &ArchiveDialog::accepted, this, [=]() {
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        emit taskUpdated();
    });
    dialog->exec();
}

// 搜索按钮点击事件
void MainWindow::on_btnSearch_clicked()
{
    QString searchText = ui->lineEditSearch->text().trimmed();
    if (searchText.isEmpty()) {
        m_taskModel->refreshTasks();
        QMessageBox::information(this, "提示", "请输入搜索关键词！");
        updateStatisticPanel();
        return;
    }

    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    QList<Task> searchTasks;
    for (const Task& task : allTasks) {
        if (task.title.contains(searchText, Qt::CaseInsensitive)
            || task.description.contains(searchText, Qt::CaseInsensitive)
            || DatabaseManager::instance().getTagsForTask(task.id).contains(searchText, Qt::CaseInsensitive)) {
            searchTasks.append(task);
        }
    }

    m_taskModel->setTaskList(searchTasks);
    int total = searchTasks.count();
    int completed = 0;
    int overdue = 0;
    for (const Task& task : searchTasks) {
        if (task.status == 1) completed++;
        if (task.dueTime < QDateTime::currentDateTime() && task.status == 0) overdue++;
    }
    ui->labelTotal->setText(QString("搜索结果：%1").arg(total));
    ui->labelCompleted->setText(QString("已完成：%1").arg(completed));
    ui->labelOverdue->setText(QString("逾期：%1").arg(overdue));
}

// 生成统计报表按钮点击事件
void MainWindow::on_btnGenerateReport_clicked()
{
    StatisticDialog* dialog = new StatisticDialog(this);
    dialog->show();
}

// 任务添加/编辑对话框（新增提醒时间设置）
bool MainWindow::showTaskDialog(Task &task, bool isEdit)
{
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "编辑任务" : "添加任务");
    dialog.setModal(true);
    dialog.resize(400, 550);

    QFormLayout* layout = new QFormLayout(&dialog);

    // 任务标题
    QLineEdit* editTitle = new QLineEdit(&dialog);
    editTitle->setText(task.title);
    layout->addRow("任务标题：", editTitle);

    // 任务分类
    QComboBox* comboCategory = new QComboBox(&dialog);
    comboCategory->addItems({"工作", "学习", "生活", "其他"});
    int categoryIndex = comboCategory->findText(task.category);
    if (categoryIndex >= 0) {
        comboCategory->setCurrentIndex(categoryIndex);
    }
    layout->addRow("任务分类：", comboCategory);

    // 任务优先级
    QComboBox* comboPriority = new QComboBox(&dialog);
    comboPriority->addItems({"高", "中", "低"});
    int priorityIndex = comboPriority->findText(task.priority);
    if (priorityIndex >= 0) {
        comboPriority->setCurrentIndex(priorityIndex);
    }
    layout->addRow("任务优先级：", comboPriority);

    // 截止时间（修复addHours问题）
    QDateTimeEdit* dtEditDue = new QDateTimeEdit(&dialog);
    QDateTime defaultDueTime;
    if (task.dueTime.isValid()) {
        defaultDueTime = task.dueTime;
    } else {
        // 1小时=3600秒，兼容所有Qt版本
        defaultDueTime = QDateTime::currentDateTime().addSecs(3600);
    }
    dtEditDue->setDateTime(defaultDueTime);
    dtEditDue->setCalendarPopup(true);
    layout->addRow("截止时间：", dtEditDue);

    // 新增：提醒时间
    QDateTimeEdit* dtEditRemind = new QDateTimeEdit(&dialog);
    // 默认值：若任务已有提醒时间则使用，否则使用当前时间（不晚于截止时间）
    QDateTime defaultRemindTime = task.remindTime.isValid() ? task.remindTime : QDateTime::currentDateTime();
    if (defaultRemindTime > dtEditDue->dateTime()) {
        defaultRemindTime = dtEditDue->dateTime();
    }
    dtEditRemind->setDateTime(defaultRemindTime);
    dtEditRemind->setCalendarPopup(true);

    // 绑定：截止时间变更时，若提醒时间晚于截止时间，自动同步为截止时间
    connect(dtEditDue, &QDateTimeEdit::dateTimeChanged, dtEditRemind, [=](const QDateTime& dueTime) {
        if (dtEditRemind->dateTime() > dueTime) {
            dtEditRemind->setDateTime(dueTime);
        }
    });
    layout->addRow("提醒时间：", dtEditRemind);

    // 任务状态
    QComboBox* comboStatus = new QComboBox(&dialog);
    comboStatus->addItems({"未完成", "已完成"});
    comboStatus->setCurrentIndex(task.status);
    layout->addRow("任务状态：", comboStatus);

    // 任务进度
    QSpinBox* spinProgress = new QSpinBox(&dialog);
    spinProgress->setRange(0, 100);
    spinProgress->setValue(task.progress);
    layout->addRow("任务进度(%)：", spinProgress);

    // 任务描述
    QTextEdit* editDesc = new QTextEdit(&dialog);
    editDesc->setText(task.description);
    layout->addRow("任务描述：", editDesc);

    // 任务标签
    QLineEdit* editTags = new QLineEdit(&dialog);
    if (isEdit) {
        QStringList tags = DatabaseManager::instance().getTagsForTask(task.id);
        editTags->setText(tags.join(","));
    }
    layout->addRow("标签（逗号分隔）：", editTags);

    // 按钮布局
    QPushButton* btnOk = new QPushButton("确认", &dialog);
    QPushButton* btnCancel = new QPushButton("取消", &dialog);
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    layout->addRow(btnLayout);

    // 绑定按钮信号
    connect(btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        // 赋值任务属性
        task.title = editTitle->text().trimmed();
        task.category = comboCategory->currentText();
        task.priority = comboPriority->currentText();
        task.dueTime = dtEditDue->dateTime();
        task.remindTime = dtEditRemind->dateTime(); // 新增：赋值提醒时间
        task.status = comboStatus->currentIndex();
        task.progress = spinProgress->value();
        task.description = editDesc->toPlainText().trimmed();
        task.is_archived = 0;

        // 校验任务标题
        if (task.title.isEmpty()) {
            QMessageBox::warning(this, "提示", "任务标题不能为空！");
            return false;
        }

        // 校验：提醒时间不能晚于截止时间
        if (task.remindTime > task.dueTime) {
            QMessageBox::warning(this, "提示", "提醒时间不能晚于截止时间！");
            return false;
        }

        bool success = false;
        if (isEdit) {
            success = DatabaseManager::instance().updateTask(task);
        } else {
            success = DatabaseManager::instance().addTask(task);
            QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
            if (!allTasks.isEmpty()) {
                task.id = allTasks.first().id;
            }
        }

        // 保存标签
        if (success && task.id != -1) {
            QStringList tagNames = editTags->text().split(",", Qt::SkipEmptyParts);
            DatabaseManager::instance().addTagsForTask(task.id, tagNames);
        }

        return success;
    }

    return false;
}
