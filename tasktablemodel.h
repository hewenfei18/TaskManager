#ifndef TASKTABLEMODEL_H
#define TASKTABLEMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "databasemanager.h"

class TaskTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    // 构造函数声明
    explicit TaskTableModel(QObject *parent = nullptr);

    // 核心方法声明
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // 补全 setTaskList 方法声明
    void setTaskList(const QList<Task> &taskList);

    // 其他方法声明
    void refreshTasks();
    void setFilterConditions(const QString &category, const QString &priority, const QString &status, const QString &tag);
    Task getTaskAt(int row) const;

private:
    QList<Task> m_taskList;
    QList<Task> m_filteredTaskList;
    QString m_filterCategory;
    QString m_filterPriority;
    QString m_filterStatus;
    QString m_filterTag;
};

#endif // TASKTABLEMODEL_H
