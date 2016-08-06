#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QtSerialPort/QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&this->m_com_interface, SIGNAL(Connected), this, SLOT(OnComPortConnected()));
    connect(&this->m_com_interface, SIGNAL(Disconnected), this, SLOT(OnComPortDisconnected()));
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

void MainWindow::on_pbConnect_clicked()
{
    if (ui->cbPorts->count() == 0 || ui->cbPorts->currentIndex() < 0)
    {
        return;
    }
    m_com_interface.Connect(ui->cbPorts->currentText());
}

void MainWindow::OnComPortConnected()
{

}

void MainWindow::OnComPortDisconnected()
{

}
