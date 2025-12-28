#ifndef PDFEXPORTER_H
#define PDFEXPORTER_H

#include <QList>
#include "databasemanager.h"

// PDF导出静态工具类
class PdfExporter
{
public:
    // 导出任务到PDF
    static void exportToPdf(const QList<Task>& tasks, const QString& filePath);

private:
    // 私有构造函数（静态类）
    PdfExporter() = delete;
};

#endif // PDFEXPORTER_H
