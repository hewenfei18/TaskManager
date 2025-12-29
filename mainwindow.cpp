#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "databasemanager.h"
#include "tasktablemodel.h"
#include "archivedialog.h"
#include "statisticdialog.h"
#include "reminderworker.h"
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
#include <QThread>

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

    // ========== 初始化提醒相关参数（严格匹配UI组件名称） ==========
    // 提醒开关：checkBoxRemindEnable
    m_reminderEnabled = (ui->checkBoxRemindEnable->checkState() == Qt::Checked);
    // 提前提醒分钟数：spinBoxRemindMinutes
    m_reminderMinutes = ui->spinBoxRemindMinutes->value();
    // 检查频率（秒）：spinBoxCheckInterval
    int checkInterval = ui->spinBoxCheckInterval->value();

    // 绑定提醒开关状态变更槽函数
    connect(ui->checkBoxRemindEnable, &QCheckBox::checkStateChanged, this, &MainWindow::on_reminderEnabledChanged);
    // 绑定提前提醒时间变更槽函数
    connect(ui->spinBoxRemindMinutes, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::on_reminderMinutesChanged);
    // 绑定检查频率变更槽函数
    connect(ui->spinBoxCheckInterval, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int interval) {
        // 获取提醒工作线程中的定时器并更新间隔
        QThread* reminderThread = nullptr;
        ReminderWorker* reminderWorker = nullptr;
        // 遍历子对象找到ReminderWorker
        for (QObject* obj : children()) {
            if (obj->isWidgetType()) continue;
            QThread* t = qobject_cast<QThread*>(obj);
            if (t) {
                for (QObject* workerObj : t->children()) {
                    reminderWorker = qobject_cast<ReminderWorker*>(workerObj);
                    if (reminderWorker) {
                        reminderThread = t;
                        break;
                    }
                }
            }
            if (reminderWorker) break;
        }
        if (reminderWorker) {
            reminderWorker->setCheckInterval(interval * 1000); // 秒转毫秒
        }
    });

    // ========== 初始化定时提醒功能（线程安全模式） ==========
    QThread* reminderThread = new QThread(this);
    ReminderWorker* reminderWorker = new ReminderWorker();
    reminderWorker->setUpcomingReminderThreshold(m_reminderMinutes);
    reminderWorker->setCheckInterval(checkInterval * 1000); // 设置检查频率（毫秒）
    reminderWorker->moveToThread(reminderThread);
    // 线程信号绑定
    connect(reminderThread, &QThread::started, reminderWorker, &ReminderWorker::startChecking);
    connect(this, &MainWindow::destroyed, reminderWorker, &ReminderWorker::stop);
    connect(this, &MainWindow::destroyed, reminderThread, &QThread::quit);
    connect(reminderThread, &QThread::finished, reminderWorker, &ReminderWorker::deleteLater);
    connect(reminderThread, &QThread::finished, reminderThread, &QThread::deleteLater);
    // 绑定任务更新信号到重置提醒记录
    connect(this, &MainWindow::taskUpdated, reminderWorker, &ReminderWorker::resetRemindedTasks);
    // 绑定提醒开关状态变更信号
    connect(this, &MainWindow::reminderEnabledChanged, reminderWorker, [=](bool enabled) {
        if (enabled) {
            reminderWorker->startChecking();
        } else {
            reminderWorker->stop();
            reminderWorker->resetRemindedTasks(); // 关闭时重置记录
        }
    });
    // 绑定提醒提前时间变更信号
    connect(this, &MainWindow::reminderEnabledChanged, reminderWorker, [=](bool enabled) {
        Q_UNUSED(enabled);
        reminderWorker->setUpcomingReminderThreshold(m_reminderMinutes);
    });

    // 绑定逾期任务提醒信号（新增开关判断）
    connect(reminderWorker, &ReminderWorker::taskOverdue, this, [=](const QList<Task>& overdueTasks) {
        if (!m_reminderEnabled) return; // 未启用提醒则不弹出
        QString tipText = QString("检测到%1个逾期未完成任务：\n").arg(overdueTasks.count());
        for (const Task& task : overdueTasks) {
            tipText += QString("- %1（截止时间：%2）\n").arg(task.title).arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
        }
        QMessageBox::warning(this, "逾期任务提醒", tipText);
    });

    // 绑定即将到期任务提醒信号（新增开关判断）
    connect(reminderWorker, &ReminderWorker::taskUpcoming, this, [=](const QList<Task>& upcomingTasks) {
        if (!m_reminderEnabled) return; // 未启用提醒则不弹出
        QString tipText = QString("检测到%1个即将到期任务：\n").arg(upcomingTasks.count());
        for (const Task& task : upcomingTasks) {
            tipText += QString("- %1（优先级：%2，截止时间：%3）\n")
                           .arg(task.title)
                           .arg(task.priority)
                           .arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
        }
        QMessageBox::information(this, "即将到期任务提醒", tipText);
    });

    // 启动子线程（初始状态：若开关未启用则不检查）
    reminderThread->start();
    if (!m_reminderEnabled) {
        reminderWorker->stop();
    }

    // 原有功能信号槽（全部匹配UI组件名称）
    connect(ui->btnAdd, &QPushButton::clicked, this, &MainWindow::onBtnAddClicked);
    connect(ui->btnEdit, &QPushButton::clicked, this, &MainWindow::onBtnEditClicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &MainWindow::onBtnDeleteClicked);
    connect(ui->btnExportPdf, &QPushButton::clicked, this, &MainWindow::onBtnExportPdfClicked);
    connect(ui->btnExportCsv, &QPushButton::clicked, this, &MainWindow::onBtnExportCsvClicked);

    // 筛选相关信号槽（全部匹配UI组件名称）
    connect(ui->comboCategoryFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboPriorityFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboStatusFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboTagFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->btnRefreshFilter, &QPushButton::clicked, this, &MainWindow::onBtnRefreshFilterClicked);

    // 归档、报表、搜索信号槽（全部匹配UI组件名称）
    connect(ui->btnArchiveCompleted, &QPushButton::clicked, this, &MainWindow::on_btnArchiveCompleted_clicked);
    connect(ui->btnViewArchive, &QPushButton::clicked, this, &MainWindow::on_btnViewArchive_clicked);
    connect(ui->btnSearch, &QPushButton::clicked, this, &MainWindow::on_btnSearch_clicked);
    connect(ui->btnGenerateReport, &QPushButton::clicked, this, &MainWindow::on_btnGenerateReport_clicked);
}

MainWindow::~MainWindow()
{
    DatabaseManager::instance().close();
    delete ui;
}

// 提醒开关状态变更槽函数
void MainWindow::on_reminderEnabledChanged(int state)
{
    m_reminderEnabled = (state == Qt::Checked);
    emit reminderEnabledChanged(m_reminderEnabled);
}

// 提醒提前时间变更槽函数
void MainWindow::on_reminderMinutesChanged(int minutes)
{
    m_reminderMinutes = minutes;
    emit reminderEnabledChanged(m_reminderEnabled); // 触发时间更新
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
        emit taskUpdated(); // 触发任务更新，重置提醒记录
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
        emit taskUpdated(); // 触发任务更新，重置提醒记录
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
        QMessageBox::information(this, "成功", "任务删除成功！");
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        emit taskUpdated(); // 触发任务更新，重置提醒记录
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
    for (const Task& task : allTasks) {
        if (task.status == 1) {
            completedCount++;
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
        QMessageBox::information(this, "成功", QString("已成功归档%1个已完成任务！").arg(completedCount));
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        emit taskUpdated(); // 触发任务更新，重置提醒记录
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
        emit taskUpdated(); // 触发任务更新，重置提醒记录
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

// 任务添加/编辑对话框
bool MainWindow::showTaskDialog(Task &task, bool isEdit)
{
    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? "编辑任务" : "添加任务");
    dialog.setModal(true);

    QFormLayout* layout = new QFormLayout(&dialog);

    QLineEdit* editTitle = new QLineEdit(&dialog);
    editTitle->setText(task.title);
    layout->addRow("任务标题：", editTitle);

    QComboBox* comboCategory = new QComboBox(&dialog);
    comboCategory->addItems({"工作", "学习", "生活", "其他"});
    int categoryIndex = comboCategory->findText(task.category);
    if (categoryIndex >= 0) {
        comboCategory->setCurrentIndex(categoryIndex);
    }
    layout->addRow("任务分类：", comboCategory);

    QComboBox* comboPriority = new QComboBox(&dialog);
    comboPriority->addItems({"高", "中", "低"});
    int priorityIndex = comboPriority->findText(task.priority);
    if (priorityIndex >= 0) {
        comboPriority->setCurrentIndex(priorityIndex);
    }
    layout->addRow("任务优先级：", comboPriority);

    QDateTimeEdit* dtEditDue = new QDateTimeEdit(&dialog);
    dtEditDue->setDateTime(task.dueTime.isValid() ? task.dueTime : QDateTime::currentDateTime());
    dtEditDue->setCalendarPopup(true);
    layout->addRow("截止时间：", dtEditDue);

    QComboBox* comboStatus = new QComboBox(&dialog);
    comboStatus->addItems({"未完成", "已完成"});
    comboStatus->setCurrentIndex(task.status);
    layout->addRow("任务状态：", comboStatus);

    QSpinBox* spinProgress = new QSpinBox(&dialog);
    spinProgress->setRange(0, 100);
    spinProgress->setValue(task.progress);
    layout->addRow("任务进度(%)：", spinProgress);

    QTextEdit* editDesc = new QTextEdit(&dialog);
    editDesc->setText(task.description);
    layout->addRow("任务描述：", editDesc);

    QLineEdit* editTags = new QLineEdit(&dialog);
    if (isEdit) {
        QStringList tags = DatabaseManager::instance().getTagsForTask(task.id);
        editTags->setText(tags.join(","));
    }
    layout->addRow("标签（逗号分隔）：", editTags);

    QPushButton* btnOk = new QPushButton("确认", &dialog);
    QPushButton* btnCancel = new QPushButton("取消", &dialog);
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    layout->addRow(btnLayout);

    connect(btnOk, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        task.title = editTitle->text().trimmed();
        task.category = comboCategory->currentText();
        task.priority = comboPriority->currentText();
        task.dueTime = dtEditDue->dateTime();
        task.status = comboStatus->currentIndex();
        task.progress = spinProgress->value();
        task.description = editDesc->toPlainText().trimmed();
        task.is_archived = 0;

        if (task.title.isEmpty()) {
            QMessageBox::warning(this, "提示", "任务标题不能为空！");
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

        if (success && task.id != -1) {
            QStringList tagNames = editTags->text().split(",", Qt::SkipEmptyParts);
            DatabaseManager::instance().addTagsForTask(task.id, tagNames);
        }

        return success;
    }

    return false;
}
