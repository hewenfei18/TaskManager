#include "tasktablemodel.h"
#include "databasemanager.h"
#include <QDateTime>
#include <QBrush>
#include <QColor>

// 构造函数实现
TaskTableModel::TaskTableModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_filterCategory("全部分类")
    , m_filterPriority("全部优先级")
    , m_filterStatus("全部状态")
    , m_filterTag("全部标签")
{
}

int TaskTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_filteredTaskList.count();
}

int TaskTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 7;
}

QVariant TaskTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_filteredTaskList.count()) {
        return QVariant();
    }

    const Task& task = m_filteredTaskList.at(index.row());
    QDateTime currentTime = QDateTime::currentDateTime();
    // 超期判断（兼容原有统计逻辑）
    bool isOverdue = (task.status == 0 && task.dueTime < currentTime);

    // 1. 原有显示逻辑
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return task.title;
        case 1: return task.category;
        case 2: return task.priority;
        case 3:
            // 超期任务追加标识
            if (isOverdue) {
                return QString("%1 （已超期）").arg(task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
            }
            return task.dueTime.toString("yyyy-MM-dd HH:mm:ss");
        case 4:
            if (isOverdue) {
                return "未完成（已超期）";
            }
            return task.status == 0 ? "未完成" : "已完成";
        case 5: return QString("%1%").arg(task.progress);
        case 6: return DatabaseManager::instance().getTagsForTask(task.id).join(", ");
        default: return QVariant();
        }
    }

    // 2. 优先级颜色标识（仅优先级列：索引2）
    if (role == Qt::ForegroundRole && index.column() == 2) {
        QString priority = task.priority.trimmed().toUpper();
        if (priority == "高" || priority == "HIGH") {
            return QBrush(Qt::red); // 高优先级：红色文字
        } else if (priority == "中" || priority == "MIDDLE" || priority == "MEDIUM") {
            return QBrush(QColor(255, 140, 0)); // 中优先级：自定义深橙色
        } else if (priority == "低" || priority == "LOW") {
            return QBrush(Qt::darkGreen); // 低优先级：深绿色文字
        }
        return QBrush(Qt::black); // 默认：黑色文字
    }

    // 3. 超期任务整行背景色标识（不影响优先级颜色）
    if (isOverdue && role == Qt::BackgroundRole) {
        return QBrush(QColor(255, 220, 220)); // 浅红背景，醒目不刺眼
    }

    return QVariant();
}

QVariant TaskTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
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

// 实现 setTaskList（匹配头文件声明）
void TaskTableModel::setTaskList(const QList<Task> &taskList)
{
    beginResetModel();
    m_filteredTaskList = taskList;
    endResetModel();
}

void TaskTableModel::refreshTasks()
{
    beginResetModel();
    m_taskList = DatabaseManager::instance().getAllTasks();
    setFilterConditions(m_filterCategory, m_filterPriority, m_filterStatus, m_filterTag);
    endResetModel();
}

Task TaskTableModel::getTaskAt(int row) const
{
    if (row >= 0 && row < m_filteredTaskList.count()) {
        return m_filteredTaskList.at(row);
    }
    return Task();
}

void TaskTableModel::setFilterConditions(const QString &category, const QString &priority,
                                         const QString &status, const QString &tag)
{
    m_filterCategory = category;
    m_filterPriority = priority;
    m_filterStatus = status;
    m_filterTag = tag;

    m_filteredTaskList.clear();
    QDateTime currentTime = QDateTime::currentDateTime();
    for (const Task& task : m_taskList) {
        if (category != "全部分类" && task.category != category) continue;
        if (priority != "全部优先级" && task.priority != priority) continue;
        QString taskStatusText = task.status == 0 ? "未完成" : "已完成";
        // 兼容超期状态筛选
        bool isOverdue = (task.status == 0 && task.dueTime < currentTime);
        QString actualStatusText = isOverdue ? "未完成（已超期）" : taskStatusText;
        if (status != "全部状态" && actualStatusText != status) continue;
        if (tag != "全部标签") {
            QStringList taskTags = DatabaseManager::instance().getTagsForTask(task.id);
            if (!taskTags.contains(tag, Qt::CaseInsensitive)) continue;
        }
        m_filteredTaskList.append(task);
    }

    beginResetModel();
    endResetModel();
}

bool TaskTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(value); // 消除未使用参数警告
    if (!index.isValid() || index.row() >= m_filteredTaskList.count() || role != Qt::EditRole) {
        return false;
    }
    return false;
}

Qt::ItemFlags TaskTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}
