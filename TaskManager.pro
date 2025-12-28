QT += core gui sql printsupport widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

SOURCES += \
    csvexporter.cpp \
    databasemanager.cpp \
    main.cpp \
    mainwindow.cpp \
    pdfexporter.cpp \
    reminderworker.cpp \
    tasktablemodel.cpp

HEADERS += \
    csvexporter.h \
    databasemanager.h \
    mainwindow.h \
    pdfexporter.h \
    reminderworker.h \
    tasktablemodel.h

FORMS += \
    mainwindow.ui
