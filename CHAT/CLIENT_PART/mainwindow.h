#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include "client_interface.h"
#include "chatroutine.h"
#include <QMessageBox>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

// public slots:
private slots:
    void on_NameLine_returnPressed();
    void on_ChatButton_Clicked();
    void on_newMessage(std::string&);

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    ChatRoutineHandler* CRH;
    void GetAvailableChats();
    void StartConnection();
    client_data_t* c_message;
    server_data_t* s_message;
};
#endif // MAINWINDOW_H
