#include "databasemanager.h"
#include <QDebug>
#include <QSqlError>
#include <QDateTime>

// 数据库路径（直接指定，避免宏冲突）
const QString DB_PATH = "./task_database.db";

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager()
{
    // 初始化SQLite数据库
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(DB_PATH);
}

// 仅保留一个init函数实现
bool DatabaseManager::init()
{
    if (!m_db.open()) {
        qDebug() << "数据库打开失败：" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query;
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
        return false;
    }

    qDebug() << "数据库初始化成功";
    return true;
}

bool DatabaseManager::addTask(const Task& task)
{
    if (!m_db.isOpen()) {
        qDebug() << "数据库未打开，添加任务失败";
        return false;
    }

    QSqlQuery query;
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

    qDebug() << "任务添加成功，ID：" << query.lastInsertId().toInt();
    return true;
}

bool DatabaseManager::updateTask(const Task& task)
{
    if (task.id == -1 || !m_db.isOpen()) {
        qDebug() << "任务ID无效或数据库未打开，更新任务失败";
        return false;
    }

    QSqlQuery query;
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

    if (query.numRowsAffected() == 0) {
        qDebug() << "更新任务失败：未找到对应ID的任务";
        return false;
    }

    qDebug() << "任务更新成功，ID：" << task.id;
    return true;
}

bool DatabaseManager::deleteTask(int taskId)
{
    if (taskId <= 0 || !m_db.isOpen()) {
        qDebug() << "任务ID无效或数据库未打开，删除任务失败";
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM tasks WHERE id = :id");
    query.bindValue(":id", taskId);

    if (!query.exec()) {
        qDebug() << "删除任务失败：" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        qDebug() << "删除任务失败：未找到对应ID的任务";
        return false;
    }

    qDebug() << "任务删除成功，ID：" << taskId;
    return true;
}

QList<Task> DatabaseManager::getAllTasks()
{
    QList<Task> taskList;
    if (!m_db.isOpen()) {
        qDebug() << "数据库未打开，获取所有任务失败";
        return taskList;
    }

    QSqlQuery query("SELECT id, title, category, priority, due_time, status, description FROM tasks ORDER BY id DESC");
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

    qDebug() << "获取所有任务成功，数量：" << taskList.count();
    return taskList;
}

QList<Task> DatabaseManager::getOverdueUncompletedTasks()
{
    QList<Task> taskList;
    if (!m_db.isOpen()) {
        qDebug() << "数据库未打开，获取逾期未完成任务失败";
        return taskList;
    }

    QSqlQuery query;
    query.prepare(R"(
        SELECT id, title, category, priority, due_time, status, description
        FROM tasks
        WHERE due_time <= :now AND status = 0
        ORDER BY due_time ASC
    )");
    query.bindValue(":now", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    if (!query.exec()) {
        qDebug() << "获取逾期未完成任务失败：" << query.lastError().text();
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

    qDebug() << "获取逾期未完成任务成功，数量：" << taskList.count();
    return taskList;
}

int DatabaseManager::getTotalTaskCount()
{
    if (!m_db.isOpen()) {
        qDebug() << "数据库未打开，获取总任务数失败";
        return 0;
    }

    QSqlQuery query("SELECT COUNT(*) FROM tasks");
    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

int DatabaseManager::getCompletedTaskCount()
{
    if (!m_db.isOpen()) {
        qDebug() << "数据库未打开，获取已完成任务数失败";
        return 0;
    }

    QSqlQuery query("SELECT COUNT(*) FROM tasks WHERE status = 1");
    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

int DatabaseManager::getOverdueUncompletedCount()
{
    return getOverdueUncompletedTasks().count();
}

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
