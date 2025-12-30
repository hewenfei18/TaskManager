#include "pdfexporter.h"
#include "databasemanager.h"
#include <QPdfWriter>
#include <QTextDocument>
#include <QDateTime>
#include <QDebug>

void PdfExporter::exportToPdf(const QList<Task>& tasks, const QString& filePath)
{
    QPdfWriter pdfWriter(filePath);
    // 基础页面配置
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Portrait);
    pdfWriter.setPageMargins(QMargins(20, 20, 20, 20));

    // 用HTML构建文档内容（自动排版，避免坐标计算）
    QString htmlContent = R"(
        <html>
        <head>
            <meta charset="UTF-8">
            <style>
                /* 1. 全局统一边距，消除默认间隙 */
                * { margin: 0; padding: 0; box-sizing: border-box; }
                body {
                    font-family: SimHei;
                    font-size: 10pt;
                    margin: 20mm; /* 页面整体边距 */
                    line-height: 1.5; /* 行高更舒适 */
                }
                /* 2. 标题样式 */
                .title {
                    text-align: center;
                    font-size: 14pt;
                    font-weight: bold;
                    margin-bottom: 15px; /* 与下方信息区拉开距离 */
                }
                /* 3. 信息区样式（统计信息） */
                .info {
                    margin-bottom: 15px;
                    text-align: left; /* 左对齐，与表格对齐 */
                }
                /* 4. 表格样式（关键：宽度100%，消除溢出） */
                table {
                    width: 100%; /* 表格宽度占满容器 */
                    border-collapse: collapse;
                    margin: 0 auto; /* 自动居中（与上方内容对齐） */
                }
                th, td {
                    border: 1px solid #000;
                    padding: 6px 8px; /* 单元格内边距更均匀 */
                    text-align: left;
                    white-space: nowrap; /* 防止文字换行导致列宽混乱 */
                    overflow: hidden;
                    text-overflow: ellipsis; /* 超长文字省略号显示 */
                }
                th {
                    background-color: #f0f0f0;
                    font-weight: bold;
                }
                /* 优先级颜色 */
                .high { color: red; }
                .medium { color: orange; }
                .low { color: green; }
                .overdue { color: red; font-weight: bold; }
            </style>
        </head>
        <body>
    )";

    // 标题
    htmlContent += QString("<div class='title'>个人工作与任务管理系统 - 任务报告</div>");
    // 导出时间
    htmlContent += QString("<div class='info'>导出时间：%1</div>").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    // 统计信息
    DatabaseManager& dbMgr = DatabaseManager::instance();
    htmlContent += QString("<div class='info'>总任务数：%1 | 已完成：%2 | 逾期未完成：%3 | 完成率：%4</div>")
                       .arg(dbMgr.getTotalTaskCount())
                       .arg(dbMgr.getCompletedTaskCount())
                       .arg(dbMgr.getOverdueUncompletedCount())
                       .arg(dbMgr.getCompletionRate());

    // 表格（修复标签，确保与CSS对齐）
    htmlContent += R"(
        <table>
            <tr>
                <<th>标题</</th>
                <<th>分类</</th>
                <<th>优先级</</th>
                <<th>截止时间</</th>
                <<th>状态</</th>
                <<th>备注</</th>
            </tr>
    )";

    // 表格内容
    for (const Task& task : tasks) {
        // 优先级样式
        QString priorityClass;
        if (task.priority == "高") priorityClass = "high";
        else if (task.priority == "中") priorityClass = "medium";
        else if (task.priority == "低") priorityClass = "low";

        // 逾期样式
        QString titleClass = (task.status == 0 && task.dueTime <= QDateTime::currentDateTime()) ? "overdue" : "";

        htmlContent += QString("<tr>")
                           .append(QString("<td class='%1'>%2</td>").arg(titleClass).arg(task.title))
                           .append(QString("<td>%1</td>").arg(task.category))
                           .append(QString("<td class='%1'>%2</td>").arg(priorityClass).arg(task.priority))
                           .append(QString("<td>%1</td>").arg(task.dueTime.toString("yyyy-MM-dd HH:mm")))
                           .append(QString("<td>%1</td>").arg(task.status == 1 ? "已完成" : "未完成"))
                           .append(QString("<td>%1</td>").arg(task.description))
                           .append("</tr>");
    }

    htmlContent += R"(
        </table>
        </body>
        </html>
    )";

    // 渲染HTML到PDF
    QTextDocument doc;
    doc.setHtml(htmlContent);
    doc.print(&pdfWriter);

    qDebug() << "PDF导出成功：" << filePath;
}
