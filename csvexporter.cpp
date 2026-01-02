#include "csvexporter.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

QString CsvExporter::processCsvField(const QString &field)
{
    return field.contains(',') ? QString("\"%1\"").arg(field) : field;
}

void CsvExporter::exportToCsv(const QList<Task> &tasks, const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "CSV打开失败：" << file.errorString();
        return;
    }


    QTextStream stream(&file);
    file.write("\xEF\xBB\xBF");

    stream << "序号,标题,分类,优先级,截止时间,状态,备注\n";
    for (int i=0; i<tasks.count(); i++) {
        const Task& t = tasks.at(i);
        QStringList fields;
        fields << QString::number(i+1)
               << processCsvField(t.title)
               << processCsvField(t.category)
               << processCsvField(t.priority)
               << processCsvField(t.dueTime.toString("yyyy-MM-dd HH:mm:ss"))
               << processCsvField(t.status == 1 ? "已完成" : "未完成")
               << processCsvField(t.description);
        stream << fields.join(',') << "\n";
    }

    file.close();
    qDebug() << "CSV导出成功：" << filePath;
}
