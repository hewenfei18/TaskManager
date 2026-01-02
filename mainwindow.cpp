#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "databasemanager.h"
#include "tasktablemodel.h"
#include "archivedialog.h"
#include "statisticdialog.h"
#include "pdfexporter.h"
#include "csvexporter.h"
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
#include <QFileDialog>
#include <QDir>
#include <QTimer>
#include <QMap>
#include <QDebug>
#include <QSet>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_taskModel(new TaskTableModel(this))
    , m_reportDialog(nullptr)
    , m_globalTaskMonitorTimer(new QTimer(this))
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

    // 调用私有函数
    initFilterComboBoxes();
    initTagFilter();
    m_taskModel->refreshTasks();
    updateStatisticPanel();
    initTaskReminders();

    // 绑定信号槽
    connect(ui->btnAdd, &QPushButton::clicked, this, &MainWindow::onBtnAddClicked);
    connect(ui->btnEdit, &QPushButton::clicked, this, &MainWindow::onBtnEditClicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &MainWindow::onBtnDeleteClicked);
    connect(ui->btnExportPdf, &QPushButton::clicked, this, &MainWindow::onBtnExportPdfClicked);
    connect(ui->btnExportCsv, &QPushButton::clicked, this, &MainWindow::onBtnExportCsvClicked);
    connect(ui->btnGenerateReport, &QPushButton::clicked, this, &MainWindow::on_btnGenerateReport_clicked);

    connect(ui->comboCategoryFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboPriorityFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboStatusFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->comboTagFilter, &QComboBox::currentTextChanged, this, &MainWindow::onFilterChanged);
    connect(ui->btnRefreshFilter, &QPushButton::clicked, this, &MainWindow::onBtnRefreshFilterClicked);

    connect(ui->btnArchiveCompleted, &QPushButton::clicked, this, &MainWindow::on_btnArchiveCompleted_clicked);
    connect(ui->btnViewArchive, &QPushButton::clicked, this, &MainWindow::on_btnViewArchive_clicked);
    connect(ui->btnSearch, &QPushButton::clicked, this, &MainWindow::on_btnSearch_clicked);

    // 全局后台实时监测配置
    connect(m_globalTaskMonitorTimer, &QTimer::timeout, this, &MainWindow::onGlobalTaskMonitorTriggered);
    m_globalTaskMonitorTimer->start(30000); // 30秒监测一次
    onGlobalTaskMonitorTriggered(); // 初始化时立即监测
    qDebug() << "全局任务后台监测已启动，监测间隔：30秒";
}


// 2. 析构函数
MainWindow::~MainWindow()
{
    // 销毁单个任务提醒定时器
    QMap<int, TaskReminder>::iterator it = m_taskReminders.begin();
    while (it != m_taskReminders.end()) {
        if (it.value().reminderTimer) {
            it.value().reminderTimer->stop();
            delete it.value().reminderTimer;
        }
        ++it;
    }
    m_taskReminders.clear();

    // 销毁全局监测定时器
    if (m_globalTaskMonitorTimer) {
        m_globalTaskMonitorTimer->stop();
        delete m_globalTaskMonitorTimer;
        m_globalTaskMonitorTimer = nullptr;
    }

    // 销毁报表对话框
    if (m_reportDialog) {
        delete m_reportDialog;
        m_reportDialog = nullptr;
    }

    DatabaseManager::instance().close();
    delete ui;
}

// 3. 私有函数：initFilterComboBoxes
void MainWindow::initFilterComboBoxes()
{
    // 分类筛选
    ui->comboCategoryFilter->clear();
    ui->comboCategoryFilter->addItem("全部分类");
    ui->comboCategoryFilter->addItems({"工作", "学习", "生活", "其他"});

    // 优先级筛选
    ui->comboPriorityFilter->clear();
    ui->comboPriorityFilter->addItem("全部优先级");
    ui->comboPriorityFilter->addItems({"高", "中", "低"});

    // 状态筛选
    ui->comboStatusFilter->clear();
    ui->comboStatusFilter->addItem("全部状态");
    ui->comboStatusFilter->addItems({"未完成", "已完成", "未完成（已超期）"});
}


// 4. 私有函数：initTagFilter
void MainWindow::initTagFilter()
{
    ui->comboTagFilter->clear();
    ui->comboTagFilter->addItem("全部标签");
    ui->comboTagFilter->addItems(DatabaseManager::instance().getAllDistinctTags());
}

// ------------------------------
// 5. 私有函数：updateStatisticPanel
// ------------------------------
void MainWindow::updateStatisticPanel()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    int total = allTasks.count();
    int completed = 0;
    int overdue = 0;

    for (const Task& task : allTasks) {
        if (task.status == 1) completed++;
        if (task.dueTime < QDateTime::currentDateTime() && task.status == 0) overdue++;
    }

    ui->labelTotal->setText(QString("总任务：%1").arg(total));
    ui->labelCompleted->setText(QString("已完成：%1").arg(completed));
    ui->labelOverdue->setText(QString("逾期：%1").arg(overdue));
}

// ------------------------------------------
// 6. 私有函数：initTaskReminders
// ------------------------------------------
void MainWindow::initTaskReminders()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    for (const Task& task : allTasks) {
        if (task.remindTime.isValid() && task.remindTime > QDateTime::currentDateTime() && task.status == 0) {
            setTaskReminder(task);
        }
    }
}


// 7. 私有函数：setTaskReminder
// ---------------------------------------
void MainWindow::setTaskReminder(const Task &task)
{
    if (m_taskReminders.contains(task.id)) {
        removeTaskReminder(task.id);
    }

    if (!task.remindTime.isValid() || task.remindTime <= QDateTime::currentDateTime() || task.status != 0) {
        return;
    }

    qint64 delayMs = QDateTime::currentDateTime().msecsTo(task.remindTime);
    if (delayMs <= 0) return;

    QTimer* reminderTimer = new QTimer(this);
    reminderTimer->setSingleShot(true);
    reminderTimer->setInterval(delayMs);

    m_taskReminders.insert(task.id, {task.id, reminderTimer, task.title});

    connect(reminderTimer, &QTimer::timeout, this, [=]() {
        onTaskReminderTriggered(task.id);
    });

    reminderTimer->start();
    qDebug() << "任务[" << task.title << "]提醒已设置";
}

// --------------------------------------------
// 8. 私有函数：removeTaskReminder
// -------------------------------------------
void MainWindow::removeTaskReminder(int taskId)
{
    if (!m_taskReminders.contains(taskId)) return;

    TaskReminder &taskReminder = m_taskReminders[taskId];
    if (taskReminder.reminderTimer) {
        taskReminder.reminderTimer->stop();
        delete taskReminder.reminderTimer;
        taskReminder.reminderTimer = nullptr;
    }

    m_taskReminders.remove(taskId);
}


// 9. 私有函数：onTaskReminderTriggered
void MainWindow::onTaskReminderTriggered(int taskId)
{
    if (!m_taskReminders.contains(taskId)) return;

    Task task = DatabaseManager::instance().getTaskById(taskId);
    if (!task.isValid() || task.status == 1) {
        removeTaskReminder(taskId);
        return;
    }

    QString tipText = QString("【任务自定义提醒】\n任务名称：%1\n分类：%2\n优先级：%3\n截止时间：%4")
                          .arg(task.title)
                          .arg(task.category)
                          .arg(task.priority)
                          .arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
    QMessageBox::information(this, "任务提醒", tipText);

    removeTaskReminder(taskId);
}

// ------------------------------
// 新增：全局后台任务监测槽函数
// ------------------------------
void MainWindow::onGlobalTaskMonitorTriggered()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    QSet<int> notifiedOverdueTasks;
    QSet<int> notifiedUpcomingTasks;

    // 监测逾期任务
    for (const Task& task : allTasks) {
        if (task.status == 0 && task.dueTime < QDateTime::currentDateTime()) {
            if (!notifiedOverdueTasks.contains(task.id)) {
                QString overdueTip = QString("【任务逾期提醒】\n任务名称：%1\n分类：%2\n优先级：%3\n原定截止时间：%4\n当前状态：未完成（已逾期）")
                                         .arg(task.title)
                                         .arg(task.category)
                                         .arg(task.priority)
                                         .arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
                QMessageBox::warning(this, "任务逾期警告", overdueTip);
                notifiedOverdueTasks.insert(task.id);
                qDebug() << "监测到逾期任务：" << task.title;
            }
        }

        // 监测即将到期任务（30分钟内）
        qint64 secsToDue = QDateTime::currentDateTime().secsTo(task.dueTime);
        if (task.status == 0 && task.dueTime > QDateTime::currentDateTime() && secsToDue > 0 && secsToDue <= 1800) {
            if (!notifiedUpcomingTasks.contains(task.id)) {
                QString upcomingTip = QString("【任务即将到期提醒】\n任务名称：%1\n分类：%2\n优先级：%3\n截止时间：%4\n剩余时间：约%5分钟")
                                          .arg(task.title)
                                          .arg(task.category)
                                          .arg(task.priority)
                                          .arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"))
                                          .arg(qRound(secsToDue / 60.0));
                QMessageBox::information(this, "任务即将到期", upcomingTip);
                notifiedUpcomingTasks.insert(task.id);
                qDebug() << "监测到即将到期任务：" << task.title;
            }
        }
    }

    // 刷新UI
    m_taskModel->refreshTasks();
    updateStatisticPanel();
}

// ------------------------------
// 10. 槽函数：onBtnAddClicked
// ------------------------------
void MainWindow::onBtnAddClicked()
{
    Task task;
    if (showTaskDialog(task, false)) {
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        setTaskReminder(task);
        emit taskUpdated();
    }
}

// ------------------------------
// 11. 槽函数：onBtnEditClicked
// ------------------------------
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
        setTaskReminder(task);
        emit taskUpdated();
    }
}

// ------------------------------
// 12. 槽函数：onBtnDeleteClicked
// ------------------------------
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
    if (ret != QMessageBox::Yes) return;

    if (DatabaseManager::instance().deleteTask(taskId)) {
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

// ------------------------------
// 13. 槽函数：onBtnExportPdfClicked
// ------------------------------
void MainWindow::onBtnExportPdfClicked()
{
    QList<Task> exportTasks;
    for (int i = 0; i < m_taskModel->rowCount(); ++i) {
        exportTasks.append(m_taskModel->getTaskAt(i));
    }

    if (exportTasks.isEmpty()) {
        QMessageBox::warning(this, "提示", "当前无任务可导出！");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "导出PDF报表",
        QDir::homePath() + "/任务报表.pdf",
        "PDF文件 (*.pdf)"
        );
    if (filePath.isEmpty()) {
        qDebug() << "用户取消PDF导出";
        return;
    }

    PdfExporter::exportToPdf(exportTasks, filePath);
    QMessageBox::information(this, "成功", QString("PDF报表已成功导出至：\n%1").arg(filePath));
}

// ------------------------------
// 14. 槽函数：onBtnExportCsvClicked
// ------------------------------
void MainWindow::onBtnExportCsvClicked()
{
    QList<Task> exportTasks;
    for (int i = 0; i < m_taskModel->rowCount(); ++i) {
        exportTasks.append(m_taskModel->getTaskAt(i));
    }

    if (exportTasks.isEmpty()) {
        QMessageBox::warning(this, "提示", "当前无任务可导出！");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(
        this,
        "导出CSV报表",
        QDir::homePath() + "/任务报表.csv",
        "CSV文件 (*.csv)"
        );
    if (filePath.isEmpty()) {
        qDebug() << "用户取消CSV导出";
        return;
    }

    CsvExporter::exportToCsv(exportTasks, filePath);
    QMessageBox::information(this, "成功", QString("CSV报表已成功导出至：\n%1").arg(filePath));
}

// ------------------------------
// 15. 槽函数：on_btnGenerateReport_clicked
// ------------------------------
void MainWindow::on_btnGenerateReport_clicked()
{
    if (!m_reportDialog) {
        m_reportDialog = new StatisticDialog(this);
        m_reportDialog->setWindowTitle("任务统计报表");
        m_reportDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_reportDialog, &QObject::destroyed, this, [=]() {
            m_reportDialog = nullptr;
        });

        connect(m_reportDialog, &StatisticDialog::accepted, this, [=]() {
            m_taskModel->refreshTasks();
            updateStatisticPanel();
        });
        connect(m_reportDialog, &StatisticDialog::rejected, this, [=]() {
            m_taskModel->refreshTasks();
            updateStatisticPanel();
        });
    }
    m_reportDialog->show();
    m_reportDialog->raise();
    m_reportDialog->activateWindow();
}

// ------------------------------
// 16. 槽函数：onFilterChanged
// ------------------------------
void MainWindow::onFilterChanged()
{
    QString category = ui->comboCategoryFilter->currentText();
    QString priority = ui->comboPriorityFilter->currentText();
    QString status = ui->comboStatusFilter->currentText();
    QString tag = ui->comboTagFilter->currentText();

    m_taskModel->setFilterConditions(category, priority, status, tag);
    updateStatisticPanel();
}

// ------------------------------
// 17. 槽函数：onBtnRefreshFilterClicked
// ------------------------------
void MainWindow::onBtnRefreshFilterClicked()
{
    m_taskModel->refreshTasks();
    updateStatisticPanel();
    initTagFilter();
}

// ------------------------------
// 18. 槽函数：on_btnArchiveCompleted_clicked
// ------------------------------
void MainWindow::on_btnArchiveCompleted_clicked()
{
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    int completedCount = 0;
    QList<int> completedIds;
    for (const Task& task : allTasks) {
        if (task.status == 1) {
            completedCount++;
            completedIds.append(task.id);
        }
    }

    if (completedCount == 0) {
        QMessageBox::information(this, "提示", "无已完成任务可归档！");
        return;
    }

    int ret = QMessageBox::question(this, "确认归档", "确定归档所有已完成任务吗？",
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    if (DatabaseManager::instance().archiveCompletedTasks()) {
        for (int id : completedIds) removeTaskReminder(id);
        QMessageBox::information(this, "成功", QString("已归档%1个任务！").arg(completedCount));
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
        emit taskUpdated();
    } else {
        QMessageBox::critical(this, "失败", "归档失败！");
    }
}

// ------------------------------
// 19. 槽函数：on_btnViewArchive_clicked
// ------------------------------
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
    delete dialog;
}

// ------------------------------
// 20. 槽函数：on_btnSearch_clicked
// ------------------------------
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
            || task.description.contains(searchText, Qt::CaseInsensitive)) {
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

// ------------------------------
// 21. 私有函数：showTaskDialog
// ------------------------------
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

    // 分类
    QComboBox* comboCategory = new QComboBox(&dialog);
    comboCategory->addItems({"工作", "学习", "生活", "其他"});
    int cateIdx = comboCategory->findText(task.category);
    if (cateIdx >= 0) comboCategory->setCurrentIndex(cateIdx);
    layout->addRow("任务分类：", comboCategory);

    // 优先级
    QComboBox* comboPriority = new QComboBox(&dialog);
    comboPriority->addItems({"高", "中", "低"});
    int prioIdx = comboPriority->findText(task.priority);
    if (prioIdx >= 0) comboPriority->setCurrentIndex(prioIdx);
    layout->addRow("优先级：", comboPriority);

    // 截止时间
    QDateTimeEdit* dtDue = new QDateTimeEdit(&dialog);
    dtDue->setDateTime(task.dueTime.isValid() ? task.dueTime : QDateTime::currentDateTime().addSecs(3600));
    dtDue->setCalendarPopup(true);
    layout->addRow("截止时间：", dtDue);

    // 提醒时间
    QDateTimeEdit* dtRemind = new QDateTimeEdit(&dialog);
    QDateTime remindTime = task.remindTime.isValid() ? task.remindTime : QDateTime::currentDateTime();
    if (remindTime > dtDue->dateTime()) remindTime = dtDue->dateTime();
    dtRemind->setDateTime(remindTime);
    dtRemind->setCalendarPopup(true);
    connect(dtDue, &QDateTimeEdit::dateTimeChanged, dtRemind, [=](const QDateTime& due) {
        if (dtRemind->dateTime() > due) dtRemind->setDateTime(due);
    });
    layout->addRow("提醒时间：", dtRemind);

    // 状态
    QComboBox* comboStatus = new QComboBox(&dialog);
    comboStatus->addItems({"未完成", "已完成"});
    comboStatus->setCurrentIndex(task.status);
    layout->addRow("状态：", comboStatus);

    // 进度
    QSpinBox* spinProgress = new QSpinBox(&dialog);
    spinProgress->setRange(0, 100);
    spinProgress->setValue(task.progress);
    layout->addRow("进度(%)：", spinProgress);

    // 描述
    QTextEdit* editDesc = new QTextEdit(&dialog);
    editDesc->setText(task.description);
    layout->addRow("描述：", editDesc);

    // 标签
    QLineEdit* editTags = new QLineEdit(&dialog);
    if (isEdit) {
        QStringList tags = DatabaseManager::instance().getTagsForTask(task.id);
        editTags->setText(tags.join(","));
    }
    layout->addRow("标签（逗号分隔）：", editTags);

    // 按钮
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
        task.dueTime = dtDue->dateTime();
        task.remindTime = dtRemind->dateTime();
        task.status = comboStatus->currentIndex();
        task.progress = spinProgress->value();
        task.description = editDesc->toPlainText().trimmed();
        task.is_archived = 0;

        if (task.title.isEmpty()) {
            QMessageBox::warning(this, "提示", "任务标题不能为空！");
            return false;
        }
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
            if (!allTasks.isEmpty()) task.id = allTasks.first().id;
        }

        if (success && task.id != -1) {
            QStringList tags = editTags->text().split(",", Qt::SkipEmptyParts);
            DatabaseManager::instance().addTagsForTask(task.id, tags);
        }
        return success;
    }
    return false;
}

