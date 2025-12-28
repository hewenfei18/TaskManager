#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QList>
#include <QString> // 补充QString头文件

struct Task {
    int id = -1;
    QString title;
    QString category;
    QString priority;
    QDateTime dueTime;
    int status = 0; // 0:未完成 1:已完成
    QString description;
};

class DatabaseManager : public QObject
{
    Q_OBJECT
private:
    explicit DatabaseManager(QObject *parent = nullptr);
    QSqlDatabase m_db; // 补充m_db成员（之前遗漏）
    static QSqlDatabase getThreadSafeDatabase();

public:
    static DatabaseManager& instance();
    DatabaseManager(const DatabaseManager&) = delete;
    void operator=(const DatabaseManager&) = delete;

    bool init();
    QList<Task> getAllTasks();
    QList<Task> getOverdueUncompletedTasks();
    int getTotalTaskCount();
    int getCompletedTaskCount();
    int getOverdueUncompletedCount();
    bool addTask(const Task& task);
    bool updateTask(const Task& task);
    bool deleteTask(int taskId);
    QString getCompletionRate(); // 补充getCompletionRate方法声明
};

#endif // DATABASEMANAGER_H
