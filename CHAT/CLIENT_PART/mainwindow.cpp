#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <string.h>
#include <QTableView>
#include <QListWidget>
#include <QPushButton>
#include <QDebug>
#include <sstream>
#include "client_interface.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    c_message = new client_data_t;
    s_message = new server_data_t;
    socket = new QTcpSocket(this);
    CRH = new ChatRoutineHandler(this, socket);
    StartConnection();
}

void MainWindow::StartConnection(){
    socket->connectToHost("127.0.0.1", 5000);
    if(socket->waitForConnected(3000)){
        qDebug() << "Connected!";
    }
    else{
        qDebug() << "Not connected!";
    }
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
}

void MainWindow::GetAvailableChats(){
    //deleting start ui
    QLayoutItem *wItem;
    while ((wItem = ui->verticalLayout->layout()->takeAt(0)) != 0) {
        delete wItem->widget();
        delete wItem;
    }
    //creating mid ui
    QLineEdit* NewChatLine = new QLineEdit();
    connect(NewChatLine, &QLineEdit::returnPressed, this, &MainWindow::on_createChat);
    ui->verticalLayout->insertWidget(0, NewChatLine);
    QTableView* ChatsTable = new QTableView;
    ChatsTable->setModel(new QStandardItemModel(0, 1, this));
    //need to get chats' names
    ReadFromServer(socket, s_message);
    if(s_message->responce == s_success){
        std::stringstream sstream(s_message->message_text);
        std::string tmp;
        for(size_t row = 1; !sstream.eof(); ++row){
            QPushButton* chat = new QPushButton;
            std::getline(sstream, tmp);
            connect(chat, &QPushButton::clicked, this, &MainWindow::on_ChatButton_Clicked);
            ChatsTable->setIndexWidget(ChatsTable->model()->index(row, 1), chat);
        }
    }
    //if(s_message->responce == s_failure) oh fuck
    ui->verticalLayout->insertWidget(1, ChatsTable);

}

MainWindow::~MainWindow()
{
    delete s_message;
    delete c_message;
    delete ui;
    delete CRH;
}

void MainWindow::on_NameLine_returnPressed()
{
    if(ui->NameLine->text().size() > 32){
        QMessageBox::warning(this, "ACHTUNG!","This name is too big!");
    }
    else{
        c_message->message_text = ui->NameLine->text().toStdString();
        SendToServer(socket, c_message);
        ReadFromServer(socket, s_message);
        if(s_message->responce == s_failure){
            QMessageBox::warning(this, "ACHTUNG!","This name is already used!");
        }
        else{
            GetAvailableChats();
        }
    }
}

void MainWindow::on_ChatButton_Clicked(){
    QPushButton* chat = qobject_cast<QPushButton*>(sender()); // get QObject that emitted the signal
    c_message->request = c_connect_chat;
    c_message->message_text = chat->text().toStdString();
    SendToServer(socket, c_message);
    ReadFromServer(socket, s_message);
    if(s_message->responce == s_success){
        //chat forming
       QLayoutItem *wItem;
        while ((wItem = ui->verticalLayout->layout()->takeAt(0)) != 0) {
            delete wItem->widget();
            delete wItem;
        }
        QListWidget* QLW = new QListWidget();
        ui->verticalLayout->insertWidget(0, QLW);
        CRH->startThread();
    }
    else{
        QMessageBox::warning(this, "ACHTUNG!", "Could not connect to this chat!");
    }
}

void MainWindow::on_newMessage(std::string& message_text){
    static size_t row = 0;
    auto it1 = message_text.find_first_of("\t");
    auto it2 = message_text.find_first_of("\n");
    auto QLW = qobject_cast<QListWidget*>(ui->verticalLayout->itemAt(0)->widget());
    QLW->insertItem(row++, QString(message_text.c_str()));
}

void MainWindow::on_createChat(){
    auto line = qobject_cast<QLineEdit*>(sender());
    c_message->request = c_create_chat;
    c_message->message_text = line->text().toStdString();
    SendToServer(socket, c_message);
    ReadFromServer(socket, s_message);
    if(s_message->responce == s_failure){
        QMessageBox::warning(this, "ACHTUNG!", "Could not create this chat");
    }
    else{
        GetAvailableChats();
    }
}
