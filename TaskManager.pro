QT += core gui sql printsupport widgets
QT += charts
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

SOURCES += \
    archivedialog.cpp \
    csvexporter.cpp \
    databasemanager.cpp \
    main.cpp \
    mainwindow.cpp \
    pdfexporter.cpp \
    reminderworker.cpp \
    statisticdialog.cpp \
    tasktablemodel.cpp

HEADERS += \
    archivedialog.h \
    csvexporter.h \
    databasemanager.h \
    mainwindow.h \
    pdfexporter.h \
    reminderworker.h \
    statisticdialog.h \
    tasktablemodel.h

FORMS += \
    archivedialog.ui \
    mainwindow.ui \
    statisticdialog.ui
