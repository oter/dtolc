#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPortInfo>

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

void MainWindow::on_pbRefresh_clicked()
{
   auto ports = QSerialPortInfo::availablePorts();
   ui->cbPorts->clear();
   for (auto& port : ports)
   {
       ui->cbPorts->addItem(port.portName());
   }
}
