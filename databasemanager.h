#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QList>
#include <QString>
#include <QDateTime>
#include <QMutex>

// 任务结构体（包含进度、归档标记字段）
struct Task {
    int id = -1;
    QString title;
    QString category;
    QString priority;
    QDateTime dueTime;
    int status = 0; // 0:未完成 1:已完成
    QString description;
    int progress = 0; // 任务进度 0~100
    int is_archived = 0; // 0:未归档 1:已归档
};

class DatabaseManager
{
public:
    // 单例模式：全局唯一实例
    static DatabaseManager& instance() {
        static DatabaseManager instance;
        return instance;
    }

    // 初始化数据库（创建表、新增字段）
    bool init();
    // 关闭数据库连接
    void close();
    // 获取线程安全数据库连接（多线程操作必备）
    QSqlDatabase getThreadSafeDatabase();

    // 原有核心任务操作方法
    bool addTask(const Task& task);
    bool updateTask(const Task& task);
    bool deleteTask(int taskId);
    QList<Task> getAllTasks(); // 仅返回未归档任务

    // 归档相关方法
    bool archiveCompletedTasks(); // 归档所有已完成且未归档的任务
    QList<Task> getAllArchivedTasks(); // 获取所有归档任务
    bool restoreTaskFromArchive(int taskId); // 从归档恢复任务（取消归档标记）
    bool deleteTaskPermanently(int taskId); // 永久删除归档任务（不可恢复）

    // 标签相关方法
    bool addTagsForTask(int taskId, const QStringList& tagNames); // 给任务添加标签（先删旧标签再新增）
    QStringList getTagsForTask(int taskId); // 获取指定任务的所有标签
    QStringList getAllDistinctTags(); // 获取系统中所有不重复的标签
    QList<Task> getTasksByTag(const QString& tagName); // 根据标签筛选未归档任务

    // 补充缺失的方法（完整新增）
    QList<Task> getOverdueUncompletedTasks(); // 获取所有逾期未完成的未归档任务
    int getTotalTaskCount(); // 获取未归档任务总数
    int getCompletedTaskCount(); // 获取未归档的已完成任务数
    int getOverdueUncompletedCount(); // 获取逾期未完成的任务数（快捷方法）
    double getCompletionRate(); // 计算未归档任务的完成率（百分比，保留1位小数）

private:
    // 私有构造函数/析构函数（单例模式，禁止外部实例化）
    DatabaseManager();
    ~DatabaseManager();
    // 禁止拷贝构造和赋值运算符（确保单例唯一性）
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QSqlDatabase m_db; // 主数据库连接
    QMutex m_mutex; // 线程安全锁（保护数据库连接创建）
    QString m_connectionName; // 主连接名称
    QString m_dbPath; // 固定数据库文件路径
};

#endif // DATABASEMANAGER_H
