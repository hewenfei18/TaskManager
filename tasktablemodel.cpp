#include "tasktablemodel.h"
#include "databasemanager.h"
#include <QDateTime>
#include <QBrush>
#include <QFont>

TaskTableModel::TaskTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    refreshTasks();
}

int TaskTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_taskList.count();
}

int TaskTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant TaskTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_taskList.count()) {
        return QVariant();
    }

    const Task& task = m_taskList.at(index.row());
    QString statusText = task.status == 1 ? "已完成" : "未完成";

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColumnTitle: return task.title;
        case ColumnCategory: return task.category;
        case ColumnPriority: return task.priority;
        case ColumnDueTime: return task.dueTime.toString("yyyy-MM-dd HH:mm");
        case ColumnStatus: return statusText;
        case ColumnDescription: return task.description;
        default: return QVariant();
        }
    case Qt::EditRole:
        switch (index.column()) {
        case ColumnTitle: return task.title;
        case ColumnCategory: return task.category;
        case ColumnPriority: return task.priority;
        case ColumnDueTime: return task.dueTime.toString("yyyy-MM-dd HH:mm");
        case ColumnStatus: return statusText;
        case ColumnDescription: return task.description;
        default: return QVariant();
        }
    case Qt::ForegroundRole:
        if (index.column() == ColumnPriority) {
            if (task.priority == "高") return QBrush(Qt::red);
            if (task.priority == "中") return QBrush(QColor(255,165,0));
            if (task.priority == "低") return QBrush(Qt::green);
        }
        if (index.column() == ColumnTitle && task.status == 0 && task.dueTime <= QDateTime::currentDateTime()) {
            return QBrush(Qt::red);
        }
        return QVariant();
    case Qt::FontRole:
        if (index.column() == ColumnTitle && task.status == 0 && task.dueTime <= QDateTime::currentDateTime()) {
            QFont f; f.setBold(true); return f;
        }
        return QVariant();
    case Qt::TextAlignmentRole:
        return QVariant::fromValue(Qt::AlignVCenter | Qt::AlignLeft);
    default:
        return QVariant();
    }
}

QVariant TaskTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColumnTitle: return "标题";
        case ColumnCategory: return "分类";
        case ColumnPriority: return "优先级";
        case ColumnDueTime: return "截止时间";
        case ColumnStatus: return "状态";
        case ColumnDescription: return "备注";
        default: return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags TaskTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

bool TaskTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;

    Task task = m_taskList.at(index.row());
    bool changed = false;

    switch (index.column()) {
    case ColumnTitle:
        task.title = value.toString();
        changed = true;
        break;
    case ColumnCategory:
        task.category = value.toString();
        changed = true;
        break;
    case ColumnPriority:
        task.priority = value.toString();
        changed = true;
        break;
    case ColumnDueTime: {
        QDateTime dt = QDateTime::fromString(value.toString(), "yyyy-MM-dd HH:mm");
        if (dt.isValid()) {
            task.dueTime = dt;
            changed = true;
        }
        break;
    }
    case ColumnStatus: {
        QString input = value.toString().trimmed();
        if (input == "已完成" || input == "1") {
            task.status = 1;
            changed = true;
        } else if (input == "未完成" || input == "0") {
            task.status = 0;
            changed = true;
        }
        break;
    }
    case ColumnDescription:
        task.description = value.toString();
        changed = true;
        break;
    }

    if (changed && DatabaseManager::instance().updateTask(task)) {
        m_taskList[index.row()] = task;
        emit dataChanged(index, index);
        refreshTasks();
        return true;
    }
    return false;
}

void TaskTableModel::refreshTasks()
{
    beginResetModel();
    m_taskList = DatabaseManager::instance().getAllTasks();
    endResetModel();
}

Task TaskTableModel::getTaskAt(int row) const
{
    return (row >=0 && row < m_taskList.count()) ? m_taskList.at(row) : Task();
}

bool TaskTableModel::updateTaskData(const Task &task)
{
    for (int i=0; i<m_taskList.count(); i++) {
        if (m_taskList[i].id == task.id) {
            m_taskList[i] = task;
            emit dataChanged(index(i,0), index(i,ColumnCount-1));
            return true;
        }
    }
    return false;
}
