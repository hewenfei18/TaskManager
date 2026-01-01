#include "databasemanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSqlError>
#include <QThread>
#include <QDir>

DatabaseManager::DatabaseManager()
    : m_connectionName("TaskManagerConnection")
{

    // 项目根目录路径：D:\Qt\Qt project\TaskManager
    QString projectRootPath = "D:/Qt/Qt project/TaskManager";
    QDir projectDir(projectRootPath);

    // 确保项目根目录存在（防止路径不存在导致创建数据库失败）
    if (!projectDir.exists()) {
        projectDir.mkpath(projectRootPath);
        qDebug() << "项目根目录不存在，已自动创建：" << projectRootPath;
    }

    // 绑定项目根目录下的 task_database.db
    m_dbPath = projectDir.filePath("task_database.db");

    // 初始化数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);
    qDebug() << "已绑定数据库路径（项目根目录）：" << m_dbPath;
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::init()
{
    if (!m_db.open()) {
        qDebug() << "数据库打开失败：" << m_db.lastError().text();
        qDebug() << "当前数据库路径：" << m_dbPath;
        return false;
    }

    QSqlQuery query(m_db);

    // 检查并新增is_archived字段（兼容旧表，避免语法错误）
    query.exec("PRAGMA table_info(tasks)");
    bool hasArchivedField = false;
    while (query.next()) {
        if (query.value(1).toString() == "is_archived") {
            hasArchivedField = true;
            break;
        }
    }
    if (!hasArchivedField) {
        QString addArchivedFieldSql = "ALTER TABLE tasks ADD COLUMN is_archived INTEGER DEFAULT 0";
        if (!query.exec(addArchivedFieldSql)) {
            qDebug() << "新增is_archived字段失败：" << query.lastError().text();
        }
    }

    // 检查并新增progress字段
    query.exec("PRAGMA table_info(tasks)");
    bool hasProgressField = false;
    while (query.next()) {
        if (query.value(1).toString() == "progress") {
            hasProgressField = true;
            break;
        }
    }
    if (!hasProgressField) {
        QString addProgressFieldSql = "ALTER TABLE tasks ADD COLUMN progress INTEGER DEFAULT 0";
        if (!query.exec(addProgressFieldSql)) {
            qDebug() << "新增progress字段失败：" << query.lastError().text();
        }
    }

    // 检查并新增remind_time字段（新增：任务提醒时间）
    query.exec("PRAGMA table_info(tasks)");
    bool hasRemindField = false;
    while (query.next()) {
        if (query.value(1).toString() == "remind_time") {
            hasRemindField = true;
            break;
        }
    }
    if (!hasRemindField) {
        QString addRemindFieldSql = "ALTER TABLE tasks ADD COLUMN remind_time DATETIME";
        if (!query.exec(addRemindFieldSql)) {
            qDebug() << "新增remind_time字段失败：" << query.lastError().text();
        }
    }

    // 创建tags表（任务标签表，关联tasks表，不存在则创建）
    QString createTagsTableSql = R"(
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            task_id INTEGER NOT NULL,
            tag_name TEXT NOT NULL,
            FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE
        )
    )";
    if (!query.exec(createTagsTableSql)) {
        qDebug() << "创建tags表失败：" << query.lastError().text();
        m_db.close();
        return false;
    }

    // 创建tasks表（项目根目录下的核心表，不存在则创建）
    QString createTableSql = R"(
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            category TEXT NOT NULL CHECK(category IN ('工作', '学习', '生活', '其他')),
            priority TEXT NOT NULL CHECK(priority IN ('高', '中', '低')),
            due_time DATETIME NOT NULL,
            remind_time DATETIME, -- 新增：提醒时间字段
            status INTEGER NOT NULL DEFAULT 0 CHECK(status IN (0, 1)),
            description TEXT,
            progress INTEGER DEFAULT 0,
            is_archived INTEGER DEFAULT 0
        )
    )";
    if (!query.exec(createTableSql)) {
        qDebug() << "创建tasks表失败：" << query.lastError().text();
        m_db.close();
        return false;
    }

    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen()) {
        m_db.commit(); // 强制提交所有事务，确保数据写入磁盘
        m_db.close();
        qDebug() << "数据库已关闭，数据已持久化到：" << m_dbPath;
    }
}

QSqlDatabase DatabaseManager::getThreadSafeDatabase()
{
    QMutexLocker locker(&m_mutex);
    QString threadConnectionName = m_connectionName + "_" + QString::number((qlonglong)QThread::currentThreadId());
    if (!QSqlDatabase::contains(threadConnectionName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", threadConnectionName);
        db.setDatabaseName(m_dbPath); // 线程连接也绑定项目根目录的数据库
        if (!db.open()) {
            qDebug() << "线程安全数据库连接失败：" << db.lastError().text();
        }
    }
    return QSqlDatabase::database(threadConnectionName);
}

bool DatabaseManager::addTask(const Task& task)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO tasks (title, category, priority, due_time, remind_time, status, description, progress, is_archived)
        VALUES (:title, :category, :priority, :due_time, :remind_time, :status, :description, :progress, :is_archived)
    )");
    query.bindValue(":title", task.title);
    query.bindValue(":category", task.category);
    query.bindValue(":priority", task.priority);
    query.bindValue(":due_time", task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":remind_time", task.remindTime.toString("yyyy-MM-dd HH:mm:ss")); // 绑定提醒时间
    query.bindValue(":status", task.status);
    query.bindValue(":description", task.description);
    query.bindValue(":progress", task.progress);
    query.bindValue(":is_archived", task.is_archived);

    if (!query.exec()) {
        qDebug() << "添加任务失败：" << query.lastError().text();
        return false;
    }

    db.commit(); // 立即写入磁盘，确保数据不丢失
    return true;
}

bool DatabaseManager::updateTask(const Task& task)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || task.id <= 0) return false;

    QSqlQuery query(db);
    query.prepare(R"(
        UPDATE tasks
        SET title = :title, category = :category, priority = :priority, due_time = :due_time,
            remind_time = :remind_time, status = :status, description = :description,
            progress = :progress, is_archived = :is_archived
        WHERE id = :id
    )");
    query.bindValue(":id", task.id);
    query.bindValue(":title", task.title);
    query.bindValue(":category", task.category);
    query.bindValue(":priority", task.priority);
    query.bindValue(":due_time", task.dueTime.toString("yyyy-MM-dd HH:mm:ss"));
    query.bindValue(":remind_time", task.remindTime.toString("yyyy-MM-dd HH:mm:ss")); // 绑定提醒时间
    query.bindValue(":status", task.status);
    query.bindValue(":description", task.description);
    query.bindValue(":progress", task.progress);
    query.bindValue(":is_archived", task.is_archived);

    if (!query.exec()) {
        qDebug() << "更新任务失败：" << query.lastError().text();
        return false;
    }

    db.commit(); // 立即写入磁盘，确保数据同步
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

    db.commit(); // 立即写入磁盘，确保数据同步
    return true;
}

QList<Task> DatabaseManager::getAllTasks()
{
    QList<Task> taskList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return taskList;

    // 仅查询未归档任务，按ID倒序排列
    QSqlQuery query("SELECT id, title, category, priority, due_time, remind_time, status, description, progress, is_archived FROM tasks WHERE is_archived = 0 ORDER BY id DESC", db);
    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.remindTime = QDateTime::fromString(query.value(5).toString(), "yyyy-MM-dd HH:mm:ss"); // 读取提醒时间
        task.status = query.value(6).toInt();
        task.description = query.value(7).toString();
        task.progress = query.value(8).toInt();
        task.is_archived = query.value(9).toInt();
        taskList.append(task);
    }

    return taskList;
}


Task DatabaseManager::getTaskById(int taskId)
{
    Task task;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || taskId <= 0) return task;

    QSqlQuery query(db);
    query.prepare("SELECT id, title, category, priority, due_time, remind_time, status, description, progress, is_archived FROM tasks WHERE id = :id");
    query.bindValue(":id", taskId);
    if (!query.exec()) {
        qDebug() << "根据ID获取任务失败：" << query.lastError().text();
        return task;
    }

    if (query.next()) {
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.remindTime = QDateTime::fromString(query.value(5).toString(), "yyyy-MM-dd HH:mm:ss");
        task.status = query.value(6).toInt();
        task.description = query.value(7).toString();
        task.progress = query.value(8).toInt();
        task.is_archived = query.value(9).toInt();
    }

    return task;
}

bool DatabaseManager::archiveCompletedTasks()
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE tasks SET is_archived = 1 WHERE status = 1 AND is_archived = 0");
    if (!query.exec()) {
        qDebug() << "归档已完成任务失败：" << query.lastError().text();
        return false;
    }

    db.commit();
    return true;
}

QList<Task> DatabaseManager::getAllArchivedTasks()
{
    QList<Task> taskList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return taskList;

    // 仅查询归档任务，按ID倒序排列
    QSqlQuery query("SELECT id, title, category, priority, due_time, remind_time, status, description, progress, is_archived FROM tasks WHERE is_archived = 1 ORDER BY id DESC", db);
    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.remindTime = QDateTime::fromString(query.value(5).toString(), "yyyy-MM-dd HH:mm:ss");
        task.status = query.value(6).toInt();
        task.description = query.value(7).toString();
        task.progress = query.value(8).toInt();
        task.is_archived = query.value(9).toInt();
        taskList.append(task);
    }

    return taskList;
}

bool DatabaseManager::restoreTaskFromArchive(int taskId)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || taskId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("UPDATE tasks SET is_archived = 0 WHERE id = :id");
    query.bindValue(":id", taskId);
    if (!query.exec()) {
        qDebug() << "恢复归档任务失败：" << query.lastError().text();
        return false;
    }

    db.commit();
    return true;
}

bool DatabaseManager::deleteTaskPermanently(int taskId)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || taskId <= 0) return false;

    QSqlQuery query(db);
    query.prepare("DELETE FROM tasks WHERE id = :id");
    query.bindValue(":id", taskId);
    if (!query.exec()) {
        qDebug() << "永久删除任务失败：" << query.lastError().text();
        return false;
    }

    db.commit();
    return true;
}

bool DatabaseManager::addTagsForTask(int taskId, const QStringList& tagNames)
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || taskId <= 0 || tagNames.isEmpty()) return false;

    // 先删除该任务原有标签，避免重复
    QSqlQuery delQuery(db);
    delQuery.prepare("DELETE FROM tags WHERE task_id = :task_id");
    delQuery.bindValue(":task_id", taskId);
    if (!delQuery.exec()) {
        qDebug() << "删除任务原有标签失败：" << delQuery.lastError().text();
        return false;
    }

    // 批量添加新标签
    QSqlQuery addQuery(db);
    addQuery.prepare("INSERT INTO tags (task_id, tag_name) VALUES (:task_id, :tag_name)");
    for (const QString& tag : tagNames) {
        QString tagTrimmed = tag.trimmed();
        if (tagTrimmed.isEmpty()) continue; // 跳过空标签
        addQuery.bindValue(":task_id", taskId);
        addQuery.bindValue(":tag_name", tagTrimmed);
        if (!addQuery.exec()) {
            qDebug() << "添加标签失败：" << addQuery.lastError().text();
            return false;
        }
    }

    db.commit();
    return true;
}

QStringList DatabaseManager::getTagsForTask(int taskId)
{
    QStringList tagList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || taskId <= 0) return tagList;

    QSqlQuery query(db);
    query.prepare("SELECT tag_name FROM tags WHERE task_id = :task_id");
    query.bindValue(":task_id", taskId);
    if (!query.exec()) {
        qDebug() << "获取任务标签失败：" << query.lastError().text();
        return tagList;
    }

    while (query.next()) {
        tagList.append(query.value(0).toString());
    }

    return tagList;
}

QStringList DatabaseManager::getAllDistinctTags()
{
    QStringList tagList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return tagList;

    // 获取所有不重复的标签，按名称排序
    QSqlQuery query("SELECT DISTINCT tag_name FROM tags ORDER BY tag_name", db);
    while (query.next()) {
        tagList.append(query.value(0).toString());
    }

    return tagList;
}

QList<Task> DatabaseManager::getTasksByTag(const QString& tagName)
{
    QList<Task> taskList;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen() || tagName.trimmed().isEmpty()) return taskList;

    // 关联查询标签对应的未归档任务（新增查询remind_time）
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT t.id, t.title, t.category, t.priority, t.due_time, t.remind_time, t.status, t.description, t.progress, t.is_archived
        FROM tasks t
        JOIN tags g ON t.id = g.task_id
        WHERE g.tag_name = :tag_name AND t.is_archived = 0
        ORDER BY t.id DESC
    )");
    query.bindValue(":tag_name", tagName.trimmed());
    if (!query.exec()) {
        qDebug() << "根据标签筛选任务失败：" << query.lastError().text();
        return taskList;
    }

    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.remindTime = QDateTime::fromString(query.value(5).toString(), "yyyy-MM-dd HH:mm:ss");
        task.status = query.value(6).toInt();
        task.description = query.value(7).toString();
        task.progress = query.value(8).toInt();
        task.is_archived = query.value(9).toInt();
        taskList.append(task);
    }

    return taskList;
}

QList<Task> DatabaseManager::getOverdueUncompletedTasks()
{
    QList<Task> tasks;
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return tasks;

    // 查询逾期未完成的未归档任务
    QSqlQuery query(db);
    query.prepare("SELECT id, title, category, priority, due_time, remind_time, status, description, progress, is_archived "
                  "FROM tasks WHERE status=0 AND due_time < datetime('now') AND is_archived=0 ORDER BY id DESC");
    if (!query.exec()) {
        qDebug() << "获取逾期未完成任务失败：" << query.lastError().text();
        return tasks;
    }

    while (query.next()) {
        Task task;
        task.id = query.value(0).toInt();
        task.title = query.value(1).toString();
        task.category = query.value(2).toString();
        task.priority = query.value(3).toString();
        task.dueTime = QDateTime::fromString(query.value(4).toString(), "yyyy-MM-dd HH:mm:ss");
        task.remindTime = QDateTime::fromString(query.value(5).toString(), "yyyy-MM-dd HH:mm:ss");
        task.status = query.value(6).toInt();
        task.description = query.value(7).toString();
        task.progress = query.value(8).toInt();
        task.is_archived = query.value(9).toInt();
        tasks.append(task);
    }
    return tasks;
}

int DatabaseManager::getTotalTaskCount()
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return 0;

    QSqlQuery query(db);
    query.exec("SELECT COUNT(*) FROM tasks WHERE is_archived=0");
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int DatabaseManager::getCompletedTaskCount()
{
    QSqlDatabase db = getThreadSafeDatabase();
    if (!db.isOpen()) return 0;

    QSqlQuery query(db);
    query.exec("SELECT COUNT(*) FROM tasks WHERE status=1 AND is_archived=0");
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int DatabaseManager::getOverdueUncompletedCount()
{
    return getOverdueUncompletedTasks().count();
}

double DatabaseManager::getCompletionRate()
{
    int total = getTotalTaskCount();
    if (total == 0) return 0.0;
    // 计算完成率（百分比）
    return (static_cast<double>(getCompletedTaskCount()) / total) * 100;
}
