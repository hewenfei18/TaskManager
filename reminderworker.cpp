#include "reminderworker.h"
#include <QDebug>
#include <QDateTime>

ReminderWorker::ReminderWorker(QObject *parent)
    : QObject(parent)
    , m_checkTimer(new QTimer(this))
{
    // 设置定时器间隔为10秒
    m_checkTimer->setInterval(10000);
    // 连接定时器超时信号到检查槽
    connect(m_checkTimer, &QTimer::timeout, this, &ReminderWorker::checkOverdueTasks);
}

ReminderWorker::~ReminderWorker()
{
    stop();
}

void ReminderWorker::startChecking()
{
    qDebug() << "提醒工作线程开始检查逾期任务";
    // 立即执行一次检查
    checkOverdueTasks();
    // 启动定时器
    m_checkTimer->start();
}

void ReminderWorker::stop()
{
    if (m_checkTimer->isActive()) {
        m_checkTimer->stop();
        qDebug() << "提醒工作线程停止检查逾期任务";
    }
}

void ReminderWorker::checkOverdueTasks()
{
    qDebug() << "正在检查逾期未完成任务";
    // 获取逾期未完成任务
    QList<Task> overdueTasks = DatabaseManager::instance().getOverdueUncompletedTasks();
    if (!overdueTasks.isEmpty()) {
        // 发送逾期任务信号
        emit taskOverdue(overdueTasks);
    }
}
