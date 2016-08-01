#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "comportinterface.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pbRefresh_clicked();

    void on_pbConnect_clicked();

    void OnComPortConnected();
    void OnComPortDisconnected();

private:
    Ui::MainWindow *ui;
    ComPortInterface m_com_interface;

};

#endif // MAINWINDOW_H
