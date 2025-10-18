#include "mainwindow.h"
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(520, 160);

    selectFileLineEdit.setParent(this);
    selectFileLineEdit.move(10,10);
    selectFileLineEdit.resize(500, 50);
    selectFileLineEdit.show();

    connect(&selectFileLineEdit, &QLineEdit::textChanged, this,[=]()
    {
        s.filePath = selectFileLineEdit.text();
    });
    connect(&openFileManager, &QPushButton::clicked, this, [=]()//Вручную проставить путь к папке
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Выбор папки", QDir::homePath());
        if (!dir.isEmpty())
        {
            selectFileLineEdit.setText(dir);
        }
    });
}

MainWindow::~MainWindow()
{

}
