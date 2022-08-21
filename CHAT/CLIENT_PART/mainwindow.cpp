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
    ChatRoutine = new QThread();
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
    delete ChatRoutine;
}

void MainWindow::SendToServer(client_data_t* c_d){
    int written = socket->write(QByteArray::fromRawData((char*)(&c_d->request), sizeof(client_request_e))); //writes by using this bullshit interface
    size_t tmp = c_d->message_text.size();
    written = socket->write(QByteArray::fromRawData((char*)(&tmp), sizeof(size_t))); //writes by using this bullshit interface
    written = socket->write(QByteArray::fromRawData((char*)(c_d->message_text.c_str()), c_d->message_text.size())); //writes by using this bullshit interface
    qDebug() << "Written a message to server";
}

void MainWindow::ReadFromServer(server_data_t* s_d){
    socket->waitForReadyRead();
    socket->read((char*)&s_d->responce, sizeof(server_responce_e));
    size_t length;
    socket->read((char*)&length, sizeof(size_t));
    s_d->message_text.resize(length);
    socket->read((char*)s_d->message_text.data(), length);
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
        c_message->message_text = ui->NameLine->text().toStdString();
        SendToServer(c_message);
        ReadFromServer(s_message);
        if(s_message->responce == s_failure){
            if(!ErrorBox){
                ErrorBox = new QMessageBox();
//                ErrorBox->setText("Your name is already used");
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
            model =  new QStandardItemModel(0, 1, this);
            ChatsTable->setModel(model);
            //need to get chats' names
            c_message->request = c_get_available_chats;
            SendToServer(c_message);
            int expected = sizeof(server_data_t);
            int row = 1;
            do{
                ReadFromServer(s_message);
                if(s_message->responce == s_success){
                    //got a name of a chat
                    QPushButton* chat = new QPushButton;
                    connect(chat, &QPushButton::clicked, this, &MainWindow::on_ChatButton_Clicked);
                    ChatsTable->setIndexWidget(model->index(row, 1), chat);
                    ++row;
                }
            }while(s_message->responce == s_success);
            //if(s_message->responce == s_failure) oh fuck
            ui->verticalLayout->insertWidget(1, ChatsTable);

        }
    }
}

void MainWindow::on_ChatButton_Clicked(){
    QPushButton* chat = qobject_cast<QPushButton*>(sender()); // get QObject that emitted the signal
    c_message->request = c_connect_chat;
    c_message->message_text = chat->text().toStdString();
    SendToServer(c_message);
    ReadFromServer(s_message);
    if(s_message->responce == s_success){
        //chat forming
        QLayoutItem *wItem;
        while ((wItem = ui->verticalLayout->layout()->takeAt(0)) != 0) {
            delete wItem->widget();
            delete wItem;
        }
        delete model;
        model =  new QStandardItemModel(0, 1, this);
        QTableView* qtv = new QTableView();
        qtv->setModel(model);
        ui->verticalLayout->insertWidget(0, qtv);
        ChatRoutine->run();
        for(ReadFromServer(s_message); s_message->responce == s_new_message; ReadFromServer(s_message)){
            auto it1 = s_message->message_text.find_first_of("\t");
            auto it2 = s_message->message_text.find_first_of("\n");
        }
    }
    else{
        QMessageBox::warning(this, "ACHTUNG!", "Could not connect to this chat!");
    }
}
