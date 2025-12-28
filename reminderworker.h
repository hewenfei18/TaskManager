#ifndef REMINDERWORKER_H
#define REMINDERWORKER_H

#include <QObject>
#include <QTimer>
#include <QList>
#include "databasemanager.h"

// 提醒工作类（Worker + moveToThread模式）
class ReminderWorker : public QObject
{
    Q_OBJECT
public:
    explicit ReminderWorker(QObject *parent = nullptr);
    ~ReminderWorker() override;

signals:
    // 发送逾期任务信号
    void taskOverdue(const QList<Task>& overdueTasks);

public slots:
    // 启动检查定时器
    void startChecking();
    // 停止检查定时器
    void stop();

private slots:
    // 定时检查逾期任务
    void checkOverdueTasks();

private:
    // 检查定时器
    QTimer* m_checkTimer;
};

#endif // REMINDERWORKER_H
