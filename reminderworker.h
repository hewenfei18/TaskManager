#ifndef REMINDERWORKER_H
#define REMINDERWORKER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QSet>
#include "databasemanager.h"

// 提醒工作类（Worker + moveToThread模式）
class ReminderWorker : public QObject
{
    Q_OBJECT
public:
    explicit ReminderWorker(QObject *parent = nullptr);
    ~ReminderWorker() override;

    // 设置即将到期提醒阈值（提前X分钟）
    void setUpcomingReminderThreshold(int minutes = 30);
    // 设置检查间隔（毫秒）
    void setCheckInterval(int intervalMs);

signals:
    // 逾期任务信号
    void taskOverdue(const QList<Task>& overdueTasks);
    // 即将到期任务信号
    void taskUpcoming(const QList<Task>& upcomingTasks);

public slots:
    // 启动检查定时器
    void startChecking();
    // 停止检查定时器
    void stop();
    // 重置已提醒任务记录（任务状态变更后调用）
    void resetRemindedTasks();

private slots:
    // 统一检查逾期任务 + 即将到期任务
    void checkTasks();

private:
    // 检查定时器
    QTimer* m_checkTimer;
    // 即将到期提醒阈值（分钟）
    int m_upcomingMinutes;
    // 已提醒的逾期任务ID集合（避免重复提醒）
    QSet<int> m_remindedOverdueTaskIds;
    // 已提醒的即将到期任务ID集合（避免重复提醒）
    QSet<int> m_remindedUpcomingTaskIds;
};

#endif // REMINDERWORKER_H
