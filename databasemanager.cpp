#include "databasemanager.h"
#include <QDebug>
#include <QSqlError>
#include <QDateTime>
#include <QThread>

const QString DB_PATH = "./task_database.db";

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
    // 初始化m_db
    m_db = QSqlDatabase::addDatabase("QSQLITE", "MainConnection");
    m_db.setDatabaseName(DB_PATH);
}

// 修正：每个线程独立的数据库连接
QSqlDatabase DatabaseManager::getThreadSafeDatabase()
{
    QString connectionName = QString("TaskDB_%1").arg((quintptr)QThread::currentThreadId());
    if (QSqlDatabase::contains(connectionName)) {
        return QSqlDatabase::database(connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(DB_PATH);
    if (!db.open()) {
        qCritical() << "线程" << QThread::currentThreadId() << "数据库打开失败：" << db.lastError().text();
    }
    return db;
}

bool DatabaseManager::init()
{
    if (!m_db.open()) {
        qDebug() << "数据库打开失败：" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            category TEXT NOT NULL CHECK(category IN ('工作', '学习', '生活', '其他')),
            priority TEXT NOT NULL CHECK(priority IN ('高', '中', '低')),
            due_time DATETIME NOT NULL,
            status INTEGER NOT NULL DEFAULT 0 CHECK(status IN (0, 1)),
            description TEXT
        )
    )";

    if (!query.exec(createTableSql)) {
        qDebug() << "创建表失败：" << query.lastError().text();
        m_db.close();
        return false;
    }
    return true;
}

QList<Task> DatabaseManager::getAllTasks()
{
    QList<Task> taskList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return taskList;

    QSqlQuery query("SELECT id, title, category, priority, due_time, status, description FROM tasks ORDER BY id DESC", db);
    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.status = query.value(5).toInt();
        task.description = query.value(6).toString();
        taskList.append(task);
    }
    return taskList;
}

QList<Task> DatabaseManager::getOverdueUncompletedTasks()
{
    QList<Task> taskList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return taskList;

    QSqlQuery query(db);
    query.prepare(R"(
        SELECT id, title, category, priority, due_time, status, description
        FROM tasks
        WHERE due_time <= :now AND status = 0
        ORDER BY due_time ASC
    )");
    query.bindValue(":now", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qDebug() << "获取逾期任务失败：" << query.lastError().text();
        return taskList;
    }

    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.status = query.value(5).toInt();
        task.description = query.value(6).toString();
        taskList.append(task);
    }
    return taskList;
}

int DatabaseManager::getTotalTaskCount()
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return 0;

    QSqlQuery query("SELECT COUNT(*) FROM tasks", db);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int DatabaseManager::getCompletedTaskCount()
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return 0;

    QSqlQuery query("SELECT COUNT(*) FROM tasks WHERE status = 1", db);
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int DatabaseManager::getOverdueUncompletedCount()
{
    return getOverdueUncompletedTasks().count();
}

// 补充getCompletionRate方法实现
QString DatabaseManager::getCompletionRate()
{
    int total = getTotalTaskCount();
    if (total == 0) {
        return "0.0%";
    }

    int completed = getCompletedTaskCount();
    double rate = (static_cast<double>(completed) / total) * 100.0;
    return QString("%1%").arg(rate, 0, 'f', 1);
}

bool DatabaseManager::addTask(const Task& task)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO tasks (title, category, priority, due_time, status, description)
        VALUES (:title, :category, :priority, :due_time, :status, :description)
    )");
    query.bindValue(":title", task.title);
    query.bindValue(":category", task.category);
    query.bindValue(":priority", task.priority);
    query.bindValue(":due_time", task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":status", task.status);
    query.bindValue(":description", task.description);

    if (!query.exec()) {
        qDebug() << "添加任务失败：" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updateTask(const Task& task)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || task.id == -1) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE tasks
        SET title = :title,
            category = :category,
            priority = :priority,
            due_time = :due_time,
            status = :status,
            description = :description
        WHERE id = :id
    )");
    query.bindValue(":title", task.title);
    query.bindValue(":category", task.category);
    query.bindValue(":priority", task.priority);
    query.bindValue(":due_time", task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":status", task.status);
    query.bindValue(":description", task.description);
    query.bindValue(":id", task.id);

    if (!query.exec()) {
        qDebug() << "更新任务失败：" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::deleteTask(int taskId)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || taskId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("DELETE FROM tasks WHERE id = :id");
    query.bindValue(":id", taskId);

    if (!query.exec()) {
        qDebug() << "删除任务失败：" << query.lastError().text();
        return false;
    }
    return true;
}
