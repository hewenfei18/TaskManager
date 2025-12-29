#include "reminderworker.h"
#include <QDebug>
#include <QDateTime>

ReminderWorker::ReminderWorker(QObject *parent)
    : QObject(parent)
    , m_checkTimer(new QTimer(this))
    , m_upcomingMinutes(30) // 默认提前30分钟提醒即将到期任务
{
    // 默认检查间隔10秒（可通过setCheckInterval修改）
    m_checkTimer->setInterval(10000);
    // 连接定时器超时信号到统一检查槽
    connect(m_checkTimer, &QTimer::timeout, this, &ReminderWorker::checkTasks);
}

ReminderWorker::~ReminderWorker()
{
    stop();
}

// 设置即将到期提醒阈值
void ReminderWorker::setUpcomingReminderThreshold(int minutes)
{
    if (minutes > 0) {
        m_upcomingMinutes = minutes;
    }
}

// 设置检查间隔（毫秒）
void ReminderWorker::setCheckInterval(int intervalMs)
{
    if (intervalMs > 0) {
        m_checkTimer->setInterval(intervalMs);
        qDebug() << "提醒检查间隔已更新为：" << intervalMs << "毫秒";
    }
}

void ReminderWorker::startChecking()
{
    qDebug() << "提醒工作线程开始检查任务（逾期 + 即将到期）";
    // 立即执行一次检查
    checkTasks();
    // 启动定时器
    m_checkTimer->start();
}

void ReminderWorker::stop()
{
    if (m_checkTimer->isActive()) {
        m_checkTimer->stop();
        qDebug() << "提醒工作线程停止检查任务";
    }
}

// 重置已提醒任务记录
void ReminderWorker::resetRemindedTasks()
{
    m_remindedOverdueTaskIds.clear();
    m_remindedUpcomingTaskIds.clear();
    qDebug() << "已重置所有任务提醒记录";
}

// 统一检查逾期任务 + 即将到期任务（过滤重复提醒）
void ReminderWorker::checkTasks()
{
    qDebug() << "正在检查任务（逾期 + 即将到期）";
    QDateTime currentTime = QDateTime::currentDateTime();
    QDateTime upcomingThreshold = currentTime.addSecs(m_upcomingMinutes * 60); // 分钟转秒，兼容所有Qt版本

    // 1. 逾期任务提醒（仅提醒未标记过的任务）
    QList<Task> overdueTasks = DatabaseManager::instance().getOverdueUncompletedTasks();
    QList<Task> newOverdueTasks; // 待发送提醒的新逾期任务
    for (const Task& task : overdueTasks) {
        // 未标记过已提醒，才加入待提醒列表
        if (!m_remindedOverdueTaskIds.contains(task.id)) {
            newOverdueTasks.append(task);
            m_remindedOverdueTaskIds.insert(task.id); // 标记为已提醒
        }
    }
    // 有新逾期任务才发送信号
    if (!newOverdueTasks.isEmpty()) {
        emit taskOverdue(newOverdueTasks);
        qDebug() << "检测到" << newOverdueTasks.count() << "个新逾期任务，发送提醒";
    }

    // 2. 即将到期任务提醒（仅提醒未标记过的任务）
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    QList<Task> newUpcomingTasks; // 待发送提醒的新即将到期任务
    for (const Task& task : allTasks) {
        // 筛选条件：未完成 + 未超期 + 截止时间在当前时间到阈值时间之间
        if (task.status != 0) {
            continue; // 跳过已完成任务
        }
        if (task.dueTime < currentTime) {
            continue; // 跳过已超期任务
        }
        if (task.dueTime > upcomingThreshold) {
            continue; // 跳过未到提醒时间的任务
        }

        // 未标记过已提醒，才加入待提醒列表
        if (!m_remindedUpcomingTaskIds.contains(task.id)) {
            newUpcomingTasks.append(task);
            m_remindedUpcomingTaskIds.insert(task.id); // 标记为已提醒
        }
    }
    // 有新即将到期任务才发送信号
    if (!newUpcomingTasks.isEmpty()) {
        emit taskUpcoming(newUpcomingTasks);
        qDebug() << "检测到" << newUpcomingTasks.count() << "个新即将到期任务，发送提醒";
    }
}
