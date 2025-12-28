#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QDateTime>

// 任务结构体
struct Task
{
    int id = -1;
    QString title;
    QString category;
    QString priority;
    QDateTime dueTime;
    int status = 0; // 0=未完成，1=已完成
    QString description;
};

// 单例数据库管理类
class DatabaseManager
{
public:
    // 获取单例实例
    static DatabaseManager& instance();

    // 初始化数据库（创建表）
    bool init();

    // CRUD接口
    bool addTask(const Task& task);
    bool updateTask(const Task& task);
    bool deleteTask(int taskId);
    QList<Task> getAllTasks();
    QList<Task> getOverdueUncompletedTasks();

    // 统计接口
    int getTotalTaskCount();
    int getCompletedTaskCount();
    int getOverdueUncompletedCount();
    QString getCompletionRate();

private:
    // 私有构造函数（单例模式）
    DatabaseManager();
    // 禁止拷贝构造和赋值
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // 数据库对象
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
