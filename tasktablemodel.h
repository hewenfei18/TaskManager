#ifndef TASKTABLEMODEL_H
#define TASKTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "databasemanager.h"

// 任务表格模型（继承QAbstractTableModel，遵循Model/View架构）
class TaskTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // 列索引枚举
    enum TaskColumn
    {
        ColumnTitle = 0,
        ColumnCategory,
        ColumnPriority,
        ColumnDueTime,
        ColumnStatus,
        ColumnDescription,
        ColumnCount // 列总数
    };

    // 构造函数
    explicit TaskTableModel(QObject *parent = nullptr);

    // 重写QAbstractTableModel接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // 刷新模型数据
    void refreshTasks();

    // 根据索引获取任务
    Task getTaskAt(int row) const;

private:
    // 任务列表
    QList<Task> m_taskList;

    // 更新单条任务
    bool updateTaskData(const Task& task);
};

#endif // TASKTABLEMODEL_H
