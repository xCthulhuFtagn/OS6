#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include "client_interface.h"
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:

private slots:
    void on_NameLine_returnPressed();
    void on_ChatButton_Clicked();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    QMessageBox* ErrorBox = nullptr;
    void GetAvailableChats();
    void StartConnection();
    void SendToServer(client_data_t*);
    client_data_t* c_message;
    server_data_t* s_message;
};
#endif // MAINWINDOW_H
