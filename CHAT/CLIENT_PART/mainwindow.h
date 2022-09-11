#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include "client_interface.h"
#include "readroutine.h"
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
    std::atomic_int client_state;

public slots:
    void on_nameLine_returnPressed2nd(server_data_t);
    void on_chatButton_Clicked2nd(server_data_t);
    void on_createChat2nd(server_data_t);
    void on_newMessage(std::string&);
//    void on_leaveChat2nd(server_data_t);
    void list_of_chats(server_data_t s_message);
    void show_chats(const std::string&);

private slots:
    void on_nameLine_returnPressed1st();
    void on_chatButton_Clicked1st();
    void on_createChat1st();
    void on_leaveChat1st();
    void on_sendMessage();

private:
    Ui::MainWindow *ui;
    QTcpSocket *socket;
    ReadRoutineHandler* CRH;
    QStandardItemModel* model;
    bool StartConnection();
    server_data_t s_message;
    client_data_t c_message;
};
#endif // MAINWINDOW_H
