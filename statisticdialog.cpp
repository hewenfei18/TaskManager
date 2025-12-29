#include "statisticdialog.h"
#include "ui_statisticdialog.h"
#include "databasemanager.h"
#include "tasktablemodel.h"
// 补充必要头文件
#include <QChartView>
#include <QPieSeries>
#include <QPieSlice>
#include <QLineSeries>
#include <QValueAxis>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QMap>
#include <QDir>

StatisticDialog::StatisticDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StatisticDialog)
    , m_pieChart(new QChart())
    , m_lineChart(new QChart())
{
    ui->setupUi(this);
    this->setModal(true); // 模态弹窗，阻塞主窗口操作

    // 代码绑定信号槽（替代UI文件的错误绑定，解决编译报错）
    connect(ui->btnGenerate, &QPushButton::clicked, this, &StatisticDialog::generateReport);
    connect(ui->btnExportPng, &QPushButton::clicked, this, &StatisticDialog::exportReportAsPng);
    connect(ui->radioBtnToday, &QRadioButton::clicked, this, &StatisticDialog::generateReport);
    connect(ui->radioBtnWeek, &QRadioButton::clicked, this, &StatisticDialog::generateReport);

    // 初始化图表
    ui->chartViewPie->setChart(m_pieChart);
    ui->chartViewLine->setChart(m_lineChart);
    // 开启抗锯齿，优化图表显示效果
    ui->chartViewPie->setRenderHint(QPainter::Antialiasing);
    ui->chartViewLine->setRenderHint(QPainter::Antialiasing);

    // 设置图表初始属性
    m_pieChart->setTitle("任务分类占比");
    m_lineChart->setTitle("任务完成率趋势");
    m_pieChart->legend()->setAlignment(Qt::AlignRight);
    m_lineChart->legend()->setAlignment(Qt::AlignBottom);

    // 初始生成报表
    generateReport();
}

StatisticDialog::~StatisticDialog()
{
    delete ui;
    delete m_pieChart;
    delete m_lineChart;
}

void StatisticDialog::generateReport()
{
    // 1. 确定报表时间范围
    if (ui->radioBtnToday->isChecked()) {
        // 今日：当天00:00 ~ 23:59
        m_startTime = QDateTime::currentDateTime().date().startOfDay();
        m_endTime = QDateTime::currentDateTime().date().endOfDay();
    } else {
        // 本周：周一00:00 ~ 周日23:59
        int weekDay = QDateTime::currentDateTime().date().dayOfWeek();
        m_startTime = QDateTime::currentDateTime().date().addDays(-(weekDay - 1)).startOfDay();
        m_endTime = QDateTime::currentDateTime().date().addDays(7 - weekDay).endOfDay();
    }

    // 2. 获取时间范围内的所有任务
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    QList<Task> timeRangeTasks;
    for (const Task& task : allTasks) {
        if (task.dueTime >= m_startTime && task.dueTime <= m_endTime) {
            timeRangeTasks.append(task);
        }
    }

    // 3. 生成分类占比饼图
    m_pieChart->removeAllSeries(); // 清空原有数据
    QPieSeries* pieSeries = new QPieSeries();
    QMap<QString, int> categoryCountMap;
    // 初始化任务分类
    categoryCountMap["工作"] = 0;
    categoryCountMap["学习"] = 0;
    categoryCountMap["生活"] = 0;
    categoryCountMap["其他"] = 0;

    // 统计各分类任务数量
    for (const Task& task : timeRangeTasks) {
        if (categoryCountMap.contains(task.category)) {
            categoryCountMap[task.category]++;
        }
    }

    // 填充饼图数据
    for (auto it = categoryCountMap.begin(); it != categoryCountMap.end(); ++it) {
        if (it.value() > 0) {
            QString sliceLabel = QString("%1（%2个）").arg(it.key()).arg(it.value());
            QPieSlice* slice = pieSeries->append(sliceLabel, it.value());
            slice->setLabelVisible(true);
            // 给不同分类设置不同颜色
            if (it.key() == "工作") slice->setColor(QColor(255, 107, 107));
            else if (it.key() == "学习") slice->setColor(QColor(107, 185, 255));
            else if (it.key() == "生活") slice->setColor(QColor(129, 207, 129));
            else slice->setColor(QColor(255, 204, 128));
        }
    }

    m_pieChart->addSeries(pieSeries);
    m_pieChart->setTitle(QString("任务分类占比（%1 至 %2）").arg(m_startTime.toString("yyyy-MM-dd")).arg(m_endTime.toString("yyyy-MM-dd")));
    m_pieChart->createDefaultAxes();

    // 4. 生成完成率趋势折线图
    m_lineChart->removeAllSeries(); // 清空原有数据
    QLineSeries* lineSeries = new QLineSeries();
    QList<QDateTime> timeNodeList;

    // 生成时间节点（今日按小时，本周按天）
    if (ui->radioBtnToday->isChecked()) {
        for (int hour = 0; hour < 24; hour++) {
            timeNodeList.append(m_startTime.addSecs(hour * 3600)); // 兼容旧Qt版本，替代addHours
        }
    } else {
        for (int day = 0; day < 7; day++) {
            timeNodeList.append(m_startTime.addDays(day));
        }
    }

    // 计算每个时间节点的完成率
    for (const QDateTime& timeNode : timeNodeList) {
        int totalTaskCount = 0;
        int completedTaskCount = 0;

        for (const Task& task : timeRangeTasks) {
            if (task.dueTime <= timeNode) {
                totalTaskCount++;
                if (task.status == 1) {
                    completedTaskCount++;
                }
            }
        }

        double completionRate = (totalTaskCount > 0) ? (static_cast<double>(completedTaskCount) / totalTaskCount) * 100 : 0.0;
        lineSeries->append(timeNode.toMSecsSinceEpoch(), completionRate);
    }

    m_lineChart->addSeries(lineSeries);
    m_lineChart->setTitle(QString("任务完成率趋势（%1 至 %2）").arg(m_startTime.toString("yyyy-MM-dd")).arg(m_endTime.toString("yyyy-MM-dd")));
    m_lineChart->createDefaultAxes();

    // 设置Y轴范围（0~100%）
    QValueAxis* yAxis = qobject_cast<QValueAxis*>(m_lineChart->axes(Qt::Vertical).first());
    if (yAxis) {
        yAxis->setRange(0, 100);
        yAxis->setTitleText("完成率（%）");
        yAxis->setTickCount(11); // 0、10、20...100，共11个刻度
    }

    // 设置X轴范围
    QValueAxis* xAxis = qobject_cast<QValueAxis*>(m_lineChart->axes(Qt::Horizontal).first());
    if (xAxis) {
        xAxis->setRange(timeNodeList.first().toMSecsSinceEpoch(), timeNodeList.last().toMSecsSinceEpoch());
        xAxis->setTitleText("时间");
    }

    // 5. 更新报表信息标签
    int totalTask = timeRangeTasks.count();
    int completedTask = 0;
    int overdueTask = 0;
    for (const Task& task : timeRangeTasks) {
        if (task.status == 1) {
            completedTask++;
        }
        if (task.dueTime < QDateTime::currentDateTime() && task.status == 0) {
            overdueTask++;
        }
    }
    double completionRate = (totalTask > 0) ? (static_cast<double>(completedTask) / totalTask) * 100 : 0.0;
    QString infoText = QString("报表时间范围：%1 ~ %2\n")
                           .arg(m_startTime.toString("yyyy-MM-dd HH:mm"))
                           .arg(m_endTime.toString("yyyy-MM-dd HH:mm")) +
                       QString("总任务数：%1 | 已完成数：%2 | 完成率：%3%\n")
                           .arg(totalTask).arg(completedTask).arg(completionRate, 0, 'f', 1) +
                       QString("逾期任务数：%1").arg(overdueTask);
    ui->labelInfo->setText(infoText);
}

void StatisticDialog::exportReportAsPng()
{
    // 弹出保存文件对话框
    QString filePath = QFileDialog::getSaveFileName(
        this,
        "导出统计报表为PNG",
        QDir::homePath() + QString("/任务统计报表_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss")),
        "PNG图片 (*.png);;所有文件 (*.*)"
        );

    if (filePath.isEmpty()) {
        return; // 用户取消保存
    }

    // 生成弹窗截图
    QPixmap pixmap(this->size());
    this->render(&pixmap);

    // 保存截图
    bool saveSuccess = pixmap.save(filePath);
    if (saveSuccess) {
        QMessageBox::information(this, "导出成功", QString("报表已成功导出至：\n%1").arg(filePath));
    } else {
        QMessageBox::critical(this, "导出失败", "报表导出失败，请检查文件路径是否可写入！");
    }
}

void StatisticDialog::on_radioBtnToday_clicked()
{
    generateReport();
}

void StatisticDialog::on_radioBtnWeek_clicked()
{
    generateReport();
}
