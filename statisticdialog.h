#ifndef STATISTICDIALOG_H
#define STATISTICDIALOG_H

#include <QDialog>
#include <QDateTime>
// 直接包含QtCharts的类头文件，不使用命名空间
#include <QChart>
#include <QPieSeries>
#include <QLineSeries>
#include <QValueAxis>
#include <QChartView>

namespace Ui {
class StatisticDialog;
}

class StatisticDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatisticDialog(QWidget *parent = nullptr);
    ~StatisticDialog();

private slots:
    void generateReport();
    void exportReportAsPng();
    void on_radioBtnToday_clicked();
    void on_radioBtnWeek_clicked();

private:
    Ui::StatisticDialog *ui;
    QChart* m_pieChart;       // 直接用QChart（无需命名空间）
    QChart* m_lineChart;
    QDateTime m_startTime;
    QDateTime m_endTime;
};

#endif // STATISTICDIALOG_H
