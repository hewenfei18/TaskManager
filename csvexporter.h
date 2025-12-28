#ifndef CSVEXPORTER_H
#define CSVEXPORTER_H

#include <QList>
#include "databasemanager.h"

// CSV导出静态工具类
class CsvExporter
{
public:
    // 导出任务到CSV（UTF-8 with BOM）
    static void exportToCsv(const QList<Task>& tasks, const QString& filePath);

private:
    // 私有构造函数（静态类）
    CsvExporter() = delete;
    // 处理CSV字段（含逗号时用双引号包裹）
    static QString processCsvField(const QString& field);
};

#endif // CSVEXPORTER_H
