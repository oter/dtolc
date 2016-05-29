#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPort>
#include <QIODevice>
#include <QString>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    m_proto.Connect();
}

void MainWindow::on_pushButton_3_clicked()
{
    QString str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    QByteArray data;
    auto size = rand() % 8000;
    for (int i = 0; i < size; ++i)
    {
        data.append(rand() % 255);
    }

    qDebug() << "Send length: " << size;

    m_proto.Send(data);
}

void MainWindow::on_pushButton_2_clicked()
{
    m_proto.Disconnect();
}
