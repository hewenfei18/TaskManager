#ifndef ARCHIVEDIALOG_H
#define ARCHIVEDIALOG_H

#include <QDialog>
#include <QAbstractTableModel>
#include <QList>
#include <QHeaderView>
#include <QModelIndex>
#include "databasemanager.h" // 包含Task结构体

namespace Ui {
class ArchiveDialog;
}

// 直接在对话框内定义表格模型（无需独立头文件）
class ArchivedTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ArchivedTableModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    // 重写模型核心方法
    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return m_archivedTasks.count();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 7; // 标题、分类、优先级、截止时间、状态、进度、标签
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_archivedTasks.count()) {
            return QVariant();
        }

        const Task& task = m_archivedTasks.at(index.row());
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0: return task.title;
            case 1: return task.category;
            case 2: return task.priority;
            case 3: return task.dueTime.toString("yyyy-MM-dd HH:mm:ss");
            case 4: return task.status == 0 ? "未完成" : "已完成";
            case 5: return QString("%1%").arg(task.progress);
            case 6: return DatabaseManager::instance().getTagsForTask(task.id).join(", ");
            default: return QVariant();
            }
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
            switch (section) {
            case 0: return "任务标题";
            case 1: return "任务分类";
            case 2: return "任务优先级";
            case 3: return "截止时间";
            case 4: return "任务状态";
            case 5: return "任务进度";
            case 6: return "任务标签";
            default: return QVariant();
            }
        }
        return QVariant();
    }

    // 加载归档任务
    void loadArchivedTasks() {
        beginResetModel();
        m_archivedTasks = DatabaseManager::instance().getAllArchivedTasks();
        endResetModel();
    }

    // 获取指定行任务
    Task getTaskAt(int row) const {
        if (row >= 0 && row < m_archivedTasks.count()) {
            return m_archivedTasks.at(row);
        }
        Task emptyTask;
        emptyTask.id = -1;
        return emptyTask;
    }

private:
    QList<Task> m_archivedTasks; // 归档任务列表
};

// 归档对话框（整合模型，无需独立头文件）
class ArchiveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ArchiveDialog(QWidget *parent = nullptr);
    ~ArchiveDialog();

private slots:
    void on_btnRestore_clicked(); // 匹配UI btnRestore
    void on_btnPermanentDelete_clicked(); // 匹配UI btnPermanentDelete
    void on_btnClose_clicked(); // 匹配UI btnClose

private:
    Ui::ArchiveDialog *ui;
    ArchivedTableModel *m_archivedTableModel; // 内部模型对象，无需独立文件
};

#endif // ARCHIVEDIALOG_H
