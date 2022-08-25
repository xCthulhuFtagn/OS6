#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <string.h>
#include <QTableView>
#include <QListWidget>
#include <QPushButton>
#include <QDebug>
#include <QHeaderView>
#include <QStandardItem>
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
    socket->setReadBufferSize(10000);
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
    SafeCleaning(ui->verticalLayout->layout());
    //creating mid ui
    QLineEdit* NewChatLine = new QLineEdit();
    connect(NewChatLine, &QLineEdit::returnPressed, this, &MainWindow::on_createChat);
    ui->verticalLayout->insertWidget(0, NewChatLine);
    QTableView* ChatsTable = new QTableView;
    QStringList chats = QString::fromStdString(s_message->message_text).split("\n");
    auto model = new QStandardItemModel(chats.length(), 1);
    ChatsTable->setModel(model);
//    ChatsTable->setRootIndex(model->index(0, 0));
    std::string tmp;
    for(size_t row = 0; row < chats.length(); ++row){
        auto chat = new QPushButton();
        chat->setText(chats.at(row));
        ChatsTable->setIndexWidget(model->index(row, 0), chat);
        connect(chat, &QPushButton::clicked, this, &MainWindow::on_ChatButton_Clicked);
    }
//    ChatsTable->setModel(model);
    ChatsTable->horizontalHeader()->hide();
    ChatsTable->verticalHeader()->hide();
    ui->verticalLayout->insertWidget(1, ChatsTable);
    ChatsTable->show();
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
            ReadFromServer(socket, s_message);
            if(s_message->responce == s_failure){
                QMessageBox::warning(this, "ACHTUNG!", "Could not create this chat");
            }
            else{
                GetAvailableChats();
            }
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
        SafeCleaning(ui->verticalLayout->layout());
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
