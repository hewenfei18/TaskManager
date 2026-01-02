#include "archivedialog.h"
#include "ui_archivedialog.h"
#include "databasemanager.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QModelIndex>

ArchiveDialog::ArchiveDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ArchiveDialog)
    , m_archivedTableModel(new ArchivedTableModel(this)) // 初始化内部模型
{
    ui->setupUi(this);

    // 绑定内部模型到UI表格 tableViewArchived
    if (ui->tableViewArchived) {
        ui->tableViewArchived->setModel(m_archivedTableModel);
        m_archivedTableModel->loadArchivedTasks(); // 加载归档数据

        // 调整表格列宽，确保内容完整显示
        QHeaderView *header = ui->tableViewArchived->horizontalHeader();
        if (header) {
            header->setSectionResizeMode(QHeaderView::ResizeToContents);
            if (header->count() >= 5) {
                header->resizeSection(3, 150); // 截止时间列
                header->resizeSection(4, 80);  // 状态列
            }
            header->setSectionResizeMode(header->count() - 1, QHeaderView::Stretch); // 标签列拉伸
        }
    }
}

ArchiveDialog::~ArchiveDialog()
{
    delete ui; // 释放UI对象
    if (m_archivedTableModel) {
        delete m_archivedTableModel;
        m_archivedTableModel = nullptr;
    }
}

// 恢复选中任务
void ArchiveDialog::on_btnRestore_clicked()
{
    if (!ui || !ui->tableViewArchived || !m_archivedTableModel) {
        QMessageBox::critical(this, "错误", "控件初始化失败！");
        return;
    }

    QModelIndex index = ui->tableViewArchived->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要恢复的归档任务！");
        return;
    }

    Task task = m_archivedTableModel->getTaskAt(index.row());
    if (task.id <= 0) {
        QMessageBox::warning(this, "提示", "无效的归档任务！");
        return;
    }

    if (DatabaseManager::instance().restoreTaskFromArchive(task.id)) {
        QMessageBox::information(this, "成功", "任务恢复成功！");
        m_archivedTableModel->loadArchivedTasks(); // 刷新归档列表
        this->accept();
    } else {
        QMessageBox::critical(this, "失败", "任务恢复失败！");
    }
}

// 永久删除任务（匹配UI btnPermanentDelete）
void ArchiveDialog::on_btnPermanentDelete_clicked()
{
    if (!ui || !ui->tableViewArchived || !m_archivedTableModel) {
        QMessageBox::critical(this, "错误", "控件初始化失败！");
        return;
    }

    QModelIndex index = ui->tableViewArchived->currentIndex();
    if (!index.isValid()) {
        QMessageBox::warning(this, "提示", "请先选中要永久删除的归档任务！");
        return;
    }

    Task task = m_archivedTableModel->getTaskAt(index.row());
    if (task.id <= 0) {
        QMessageBox::warning(this, "提示", "无效的归档任务！");
        return;
    }

    int ret = QMessageBox::question(this, "危险提示", "确定要永久删除该归档任务吗？此操作不可恢复！",
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }

    if (DatabaseManager::instance().deleteTaskPermanently(task.id)) {
        QMessageBox::information(this, "成功", "任务永久删除成功！");
        m_archivedTableModel->loadArchivedTasks(); // 刷新归档列表
    } else {
        QMessageBox::critical(this, "失败", "任务永久删除失败！");
    }
}

// 关闭对话框（匹配UI btnClose）
void ArchiveDialog::on_btnClose_clicked()
{
    this->reject(); // 关闭对话框，不触发额外的操作
}
