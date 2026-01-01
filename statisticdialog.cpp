#include "statisticdialog.h"
#include "ui_statisticdialog.h"
#include "databasemanager.h"
#include "tasktablemodel.h"
#include <QChartView>
#include <QPieSeries>
#include <QPieSlice>
#include <QLineSeries>
#include <QValueAxis>
#include <QCategoryAxis>  // 改用分类轴，手动控制标签
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QMap>
#include <QDir>


void StatisticDialog::on_radioBtnToday_clicked() { generateReport(); }
void StatisticDialog::on_radioBtnWeek_clicked() { generateReport(); }

StatisticDialog::StatisticDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StatisticDialog)
    , m_pieChart(new QChart())
    , m_lineChart(new QChart())
{
    ui->setupUi(this);
    this->setModal(true);

    connect(ui->btnGenerate, &QPushButton::clicked, this, &StatisticDialog::generateReport);
    connect(ui->btnExportPng, &QPushButton::clicked, this, &StatisticDialog::exportReportAsPng);
    connect(ui->radioBtnToday, &QRadioButton::clicked, this, &StatisticDialog::generateReport);
    connect(ui->radioBtnWeek, &QRadioButton::clicked, this, &StatisticDialog::generateReport);

    ui->chartViewPie->setChart(m_pieChart);
    ui->chartViewLine->setChart(m_lineChart);
    ui->chartViewPie->setRenderHint(QPainter::Antialiasing);
    ui->chartViewLine->setRenderHint(QPainter::Antialiasing);

    m_pieChart->setTitle("任务分类占比");
    m_lineChart->setTitle("任务完成率趋势");
    m_pieChart->legend()->setAlignment(Qt::AlignRight);
    m_lineChart->legend()->setAlignment(Qt::AlignBottom);

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
    // 1. 确定时间范围
    if (ui->radioBtnToday->isChecked()) {
        m_startTime = QDateTime::currentDateTime().date().startOfDay();
        m_endTime = QDateTime::currentDateTime().date().endOfDay();
    } else {
        int weekDay = QDateTime::currentDateTime().date().dayOfWeek();
        m_startTime = QDateTime::currentDateTime().date().addDays(-(weekDay - 1)).startOfDay();
        m_endTime = QDateTime::currentDateTime().date().addDays(7 - weekDay).endOfDay();
    }

    // 2. 获取时间范围内的任务
    QList<Task> allTasks = DatabaseManager::instance().getAllTasks();
    QList<Task> timeRangeTasks;
    for (const Task& task : allTasks) {
        if (task.dueTime >= m_startTime && task.dueTime <= m_endTime) {
            timeRangeTasks.append(task);
        }
    }

    // 3. 生成饼图（保持不变）
    m_pieChart->removeAllSeries();
    QPieSeries* pieSeries = new QPieSeries();
    QMap<QString, int> categoryCountMap{{"工作",0}, {"学习",0}, {"生活",0}, {"其他",0}};
    for (const Task& task : timeRangeTasks) {
        if (categoryCountMap.contains(task.category)) categoryCountMap[task.category]++;
    }
    for (auto it = categoryCountMap.begin(); it != categoryCountMap.end(); ++it) {
        if (it.value() > 0) {
            QPieSlice* slice = pieSeries->append(QString("%1（%2个）").arg(it.key()).arg(it.value()), it.value());
            slice->setLabelVisible(true);
            if (it.key() == "工作") slice->setColor(QColor(255, 107, 107));
            else if (it.key() == "学习") slice->setColor(QColor(107, 185, 255));
            else if (it.key() == "生活") slice->setColor(QColor(129, 207, 129));
            else slice->setColor(QColor(255, 204, 128));
        }
    }
    m_pieChart->addSeries(pieSeries);
    m_pieChart->setTitle(QString("任务分类占比（%1 至 %2）").arg(m_startTime.toString("yyyy-MM-dd")).arg(m_endTime.toString("yyyy-MM-dd")));
    m_pieChart->createDefaultAxes();

    // 4. 生成折线图（今日用“小时数字”，本周用“月-日”）
    m_lineChart->removeAllSeries();
    qDeleteAll(m_lineChart->axes());
    QLineSeries* lineSeries = new QLineSeries();
    QStringList xLabels;  // 存储X轴标签

    if (ui->radioBtnToday->isChecked()) {
        // 今日报表：每2小时1个节点，X轴标签直接用“小时数字”（如2、4、6...）
        for (int hour = 0; hour < 24; hour += 2) {
            xLabels.append(QString::number(hour));  // 直接存“小时数字”字符串
        }
    } else {
        // 本周报表：按天生成，X轴标签用“月-日”
        for (int day = 0; day < 7; day++) {
            QDateTime node = m_startTime.addDays(day);
            xLabels.append(node.toString("MM-dd"));
        }
    }

    // 计算完成率并填充数据（X轴用索引，后续绑定分类轴）
    for (int i = 0; i < xLabels.count(); ++i) {
        QDateTime node;
        if (ui->radioBtnToday->isChecked()) {
            // 今日：根据索引计算对应的时间节点
            int hour = i * 2;  // 每2小时一个节点
            node = m_startTime.addSecs(hour * 3600);
        } else {
            // 本周：根据索引计算对应的时间节点
            node = m_startTime.addDays(i);
        }

        // 统计该时间节点前的任务完成率
        int total = 0, completed = 0;
        for (const Task& task : timeRangeTasks) {
            if (task.dueTime <= node) {
                total++;
                if (task.status == 1) completed++;
            }
        }
        double rate = (total > 0) ? (static_cast<double>(completed)/total)*100 : 0.0;
        lineSeries->append(i, rate);  // X轴用索引
    }

    m_lineChart->addSeries(lineSeries);
    m_lineChart->setTitle(QString("任务完成率趋势（%1 至 %2）").arg(m_startTime.toString("yyyy-MM-dd")).arg(m_endTime.toString("yyyy-MM-dd")));

    // Y轴（保持不变）
    QValueAxis* yAxis = new QValueAxis();
    yAxis->setRange(0, 100);
    yAxis->setTitleText("完成率（%）");
    yAxis->setTickCount(11);
    m_lineChart->addAxis(yAxis, Qt::AlignLeft);
    lineSeries->attachAxis(yAxis);

    // X轴：改用QCategoryAxis，手动绑定“小时数字/月-日”标签
    QCategoryAxis* xAxis = new QCategoryAxis();
    xAxis->setTitleText(ui->radioBtnToday->isChecked() ? "小时" : "日期");
    xAxis->setLabelsAngle(0);  // 标签不旋转，直接水平显示
    // 绑定索引与标签
    for (int i = 0; i < xLabels.count(); ++i) {
        xAxis->append(xLabels[i], i);
    }
    xAxis->setRange(0, xLabels.count() - 1);
    m_lineChart->addAxis(xAxis, Qt::AlignBottom);
    lineSeries->attachAxis(xAxis);

    // 更新信息标签
    int totalTask = timeRangeTasks.count(), completedTask = 0, overdueTask = 0;
    for (const Task& task : timeRangeTasks) {
        if (task.status == 1) completedTask++;
        if (task.dueTime < QDateTime::currentDateTime() && task.status == 0) overdueTask++;
    }
    double completionRate = (totalTask > 0) ? (static_cast<double>(completedTask)/totalTask)*100 : 0.0;
    ui->labelInfo->setText(
        QString("报表时间范围：%1 ~ %2\n")
            .arg(m_startTime.toString("yyyy-MM-dd HH:mm"))
            .arg(m_endTime.toString("yyyy-MM-dd HH:mm")) +
        QString("总任务数：%1 | 已完成数：%2 | 完成率：%3%\n")
            .arg(totalTask).arg(completedTask).arg(completionRate, 0, 'f', 1) +
        QString("逾期任务数：%1").arg(overdueTask)
        );
}

void StatisticDialog::exportReportAsPng()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, "导出统计报表为PNG",
        QDir::homePath() + QString("/任务统计报表_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmss")),
        "PNG图片 (*.png);;所有文件 (*.*)"
        );
    if (filePath.isEmpty()) return;

    QPixmap pixmap(this->size());
    this->render(&pixmap);
    if (pixmap.save(filePath)) {
        QMessageBox::information(this, "导出成功", QString("报表已成功导出至：\n%1").arg(filePath));
    } else {
        QMessageBox::critical(this, "导出失败", "报表导出失败，请检查文件路径是否可写入！");
    }
}

