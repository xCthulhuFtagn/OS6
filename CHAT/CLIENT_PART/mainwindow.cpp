#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <string.h>
#include <QTableView>
#include <QPushButton>
#include <QDebug>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    c_message = new client_data_t;
    s_message = new server_data_t;
    StartConnection();
}

void MainWindow::StartConnection(){
    socket = new QTcpSocket(this);
    socket->connectToHost("127.0.0.1", 5000);
    if(socket->waitForConnected(3000)){
        qDebug() << "Connected!";
    }
    else{
        qDebug() << "Not connected!";
    }
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

MainWindow::~MainWindow()
{
    delete s_message;
    delete c_message;
    delete ui;
}

void MainWindow::SendToServer(client_data_t* c_d){
    int written = socket->write(QByteArray::fromRawData((char*)(c_d), sizeof(client_data_t))); //writes by using this bullshit interface
    qDebug() << "Written a block of data sized " << written;
}


void MainWindow::on_NameLine_returnPressed()
{
    if(ui->NameLine->text().size() > 32){
        if(!ErrorBox){
            ErrorBox = new QMessageBox();
            ui->verticalLayout->insertWidget(2,ErrorBox);
        }
        ErrorBox->setText("Your name is too big");
    }
    else{
        strncpy(c_message->message_text, ui->NameLine->text().toStdString().c_str(), MAX_NAME_LEN);
        SendToServer(c_message);
        //add reading from server or smth
        int expected = sizeof(server_data_t);
//        if(socket->bytesAvailable() >= expected){
            socket->read((char*)s_message, expected);
//        }
        if(s_message->responce == s_failure){
            if(!ErrorBox){
                ErrorBox = new QMessageBox();
                ErrorBox->setText("Your name is already used");
                ui->verticalLayout->insertWidget(2,ErrorBox);
            }
            ErrorBox->setText("Your name is already used");
        }
        else{
            //deleting start ui
//            qDeleteAll(ui->verticalLayout->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));
            QLayoutItem *wItem;
            while ((wItem = ui->verticalLayout->layout()->takeAt(0)) != 0) {
                delete wItem->widget();
                delete wItem;
            }
            //creating mid ui
            QLineEdit* NewChatLine = new QLineEdit();
            ui->verticalLayout->insertWidget(0, NewChatLine);
            QTableView* ChatsTable = new QTableView;
            //need to get chats' names
            c_message->request = c_get_available_chats;
            SendToServer(c_message);
            int expected = sizeof(server_data_t);
            int row = 1;
            do{
                socket->waitForReadyRead();
                socket->read((char*)s_message, expected);
                if(s_message->responce == s_success){
                    //got a name of a chat
                    QPushButton* chat = new QPushButton;
                    connect(chat, &QPushButton::clicked, this, &MainWindow::on_ChatButton_Clicked);
                    ChatsTable->setIndexWidget(ChatsTable->model()->index(row, 1), chat);
                    ++row;
                }
            }while(s_message->responce == s_success);
            //if(s_message->responce == s_failure) oh fuck
            ui->verticalLayout->insertWidget(1, ChatsTable);

        }
    }
}

void MainWindow::on_ChatButton_Clicked(){
    //QPushButton* chat = qobject_cast<QPushButton*>(sender()); // get QObject that emitted the signal
    //use it's name to connect
}
