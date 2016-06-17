#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

    enum class ConnectionStatus
    {
        kConnected,
        kDisconnected
    };

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_pbRefresh_clicked();

private:
    Ui::MainWindow *ui;
    ConnectionStatus connectionStatus;
};

#endif // MAINWINDOW_H
