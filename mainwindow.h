#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QDebug>
#include "server.h"

class MainWindow : public QMainWindow
{
private:
    Q_OBJECT
    Server s;
    QLineEdit selectFileLineEdit;
    QPushButton openFileManager;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
};
#endif // MAINWINDOW_H
