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

    // 原有功能信号槽
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
    DatabaseManager::instance().close();
    delete ui;
}

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
    ui->comboStatusFilter->addItems({"未完成", "已完成"});
}

void MainWindow::initTagFilter()
{
    ui->comboTagFilter->clear();
    ui->comboTagFilter->addItem("全部标签");
    ui->comboTagFilter->addItems(DatabaseManager::instance().getAllDistinctTags());
}

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

    ui->labelTotal->setText(QString("总任务：%1").arg(total));
    ui->labelCompleted->setText(QString("已完成：%1").arg(completed));
    ui->labelOverdue->setText(QString("逾期：%1").arg(overdue));
}

void MainWindow::onBtnAddClicked()
{
    Task task;
    if (showTaskDialog(task, false)) {
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
    }
}

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
    }
}

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
    } else {
        QMessageBox::critical(this, "失败", "任务删除失败！");
    }
}

void MainWindow::onBtnRefreshFilterClicked()
{
    m_taskModel->refreshTasks();
    updateStatisticPanel();
    initTagFilter();
}

void MainWindow::onBtnExportPdfClicked()
{
    QMessageBox::information(this, "提示", "PDF导出功能待实现");
}

void MainWindow::onBtnExportCsvClicked()
{
    QMessageBox::information(this, "提示", "CSV导出功能待实现");
}

// ========== 修复1：标签筛选失效问题（重构筛选逻辑，确保标签筛选生效） ==========
void MainWindow::onFilterChanged()
{
    QString category = ui->comboCategoryFilter->currentText();
    QString priority = ui->comboPriorityFilter->currentText();
    QString status = ui->comboStatusFilter->currentText();
    QString tag = ui->comboTagFilter->currentText();

    // 直接传递筛选条件给模型，确保标签筛选逻辑被正确执行
    m_taskModel->setFilterConditions(category, priority, status, tag);
    // 刷新统计面板
    updateStatisticPanel();
}

// ========== 修复2：归档弹窗重复弹出问题（避免信号重复绑定/重复执行） ==========
void MainWindow::on_btnArchiveCompleted_clicked()
{
    // 1. 先判断是否有已完成任务，无任务则直接提示，避免无效操作
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

    // 2. 仅弹出一次确认弹窗，通过返回值直接判断，避免重复执行
    int ret = QMessageBox::question(this, "确认归档", "是否确定归档所有已完成任务？",
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return; // 取消则直接返回，不执行后续逻辑
    }

    // 3. 执行归档操作，仅弹出一次结果弹窗
    if (DatabaseManager::instance().archiveCompletedTasks()) {
        QMessageBox::information(this, "成功", QString("已成功归档%1个已完成任务！").arg(completedCount));
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
    } else {
        QMessageBox::critical(this, "失败", "任务归档失败！");
    }
}

// ========== 修复3：查看归档无数据问题（确保归档对话框正确加载归档任务） ==========
void MainWindow::on_btnViewArchive_clicked()
{
    // 每次创建新的归档对话框，确保数据重新加载
    ArchiveDialog* dialog = new ArchiveDialog(this);
    // 仅绑定一次accepted信号，避免重复刷新
    connect(dialog, &ArchiveDialog::accepted, this, [=]() {
        m_taskModel->refreshTasks();
        updateStatisticPanel();
        initTagFilter();
    });
    // 显示对话框（非模态改为模态，避免多次打开）
    dialog->exec(); // 使用exec()替代show()，确保对话框独占焦点，避免重复点击
}

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

void MainWindow::on_btnGenerateReport_clicked()
{
    StatisticDialog* dialog = new StatisticDialog(this);
    dialog->show();
}

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
