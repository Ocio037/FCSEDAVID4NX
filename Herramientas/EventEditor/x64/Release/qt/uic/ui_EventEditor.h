/********************************************************************************
** Form generated from reading UI file 'EventEditor.ui'
**
** Created by: Qt User Interface Compiler version 6.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EVENTEDITOR_H
#define UI_EVENTEDITOR_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_EventEditorClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *EventEditorClass)
    {
        if (EventEditorClass->objectName().isEmpty())
            EventEditorClass->setObjectName("EventEditorClass");
        EventEditorClass->resize(600, 400);
        menuBar = new QMenuBar(EventEditorClass);
        menuBar->setObjectName("menuBar");
        EventEditorClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(EventEditorClass);
        mainToolBar->setObjectName("mainToolBar");
        EventEditorClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(EventEditorClass);
        centralWidget->setObjectName("centralWidget");
        EventEditorClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(EventEditorClass);
        statusBar->setObjectName("statusBar");
        EventEditorClass->setStatusBar(statusBar);

        retranslateUi(EventEditorClass);

        QMetaObject::connectSlotsByName(EventEditorClass);
    } // setupUi

    void retranslateUi(QMainWindow *EventEditorClass)
    {
        EventEditorClass->setWindowTitle(QCoreApplication::translate("EventEditorClass", "EventEditor", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EventEditorClass: public Ui_EventEditorClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EVENTEDITOR_H
