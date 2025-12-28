#include <QApplication>
#include <QThread>
#include <QMessageBox> // 新增：包含QMessageBox头文件
#include "mainwindow.h"
#include "databasemanager.h"
#include "reminderworker.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 初始化数据库
    if (!DatabaseManager::instance().init()) {
        QMessageBox::critical(nullptr, "初始化失败", "数据库初始化失败，程序将退出！");
        return -1;
    }

    // 创建提醒线程和工作对象
    QThread* reminderThread = new QThread;
    ReminderWorker* reminderWorker = new ReminderWorker;
    reminderWorker->moveToThread(reminderThread);

    // 连接线程启动信号与工作对象的开始检查槽
    QObject::connect(reminderThread, &QThread::started, reminderWorker, &ReminderWorker::startChecking);

    // 启动线程
    reminderThread->start();

    // 创建并显示主窗口
    MainWindow w;
    w.show();

    // 应用程序退出时清理线程
    int ret = a.exec();
    reminderWorker->stop();
    reminderThread->quit();
    reminderThread->wait();
    delete reminderWorker;
    delete reminderThread;

    return ret;
}
