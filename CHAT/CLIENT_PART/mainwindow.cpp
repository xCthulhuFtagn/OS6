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
    socket = new QTcpSocket(this);
    bool start_ok = StartConnection();
    auto a = ui->NameLine;
    connect(ui->NameLine, &QLineEdit::returnPressed, this, &MainWindow::on_nameLine_returnPressed1st);
    CRH = new ReadRoutineHandler(this, socket);
    if(start_ok){
        CRH->startThread();
    }
    else QMessageBox::warning(this, "ACHTUNG!", "Server error: could not connect.\nTry reopening app once we reanimate the server!");
}

bool MainWindow::StartConnection(){
    socket->connectToHost("127.0.0.1", 5000);
    socket->setReadBufferSize(10000);
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    if(socket->waitForConnected(3000)){
        qDebug() << "Connected!";
        return true;
    }
    else{
        qDebug() << "Not connected!";
        return false;
    }
}

void MainWindow::show_chats(const std::string& chat_list){
    //deleting start ui
    SafeCleaning(ui->verticalLayout->layout());
    //creating mid ui
    QLineEdit* NewChatLine = new QLineEdit();
    NewChatLine->setPlaceholderText("Enter new chat's name:");
    connect(NewChatLine, &QLineEdit::returnPressed, this, &MainWindow::on_createChat1st);
    ui->verticalLayout->insertWidget(0, NewChatLine);
    QTableView* ChatsTable = new QTableView;
    QStringList chats = QString::fromStdString(chat_list).split('\n');
    model = new QStandardItemModel(chats.length() - 1, 1);
    ChatsTable->setModel(model);
    for(size_t row = 0; row < chats.length() - 1; ++row){
        auto chat = new QPushButton();
        chat->setText(chats.at(row));
        ChatsTable->setIndexWidget(model->index(row, 0), chat);
        connect(chat, &QPushButton::clicked, this, &MainWindow::on_chatButton_Clicked1st);
        connect(ChatsTable, &QTableView::destroyed, chat, &QPushButton::deleteLater);
    }
    ChatsTable->horizontalHeader()->hide();
    ChatsTable->verticalHeader()->hide();
    ChatsTable->horizontalHeader()->setStretchLastSection(true); //try to fill the whole widget
    ui->verticalLayout->insertWidget(1, ChatsTable);
    ChatsTable->show();
}
MainWindow::~MainWindow()
{
    delete ui;
    CRH->stopThread();
    delete CRH;
}

void MainWindow::on_nameLine_returnPressed1st()
{
    if(ui->NameLine->text().size() > 32){
        QMessageBox::warning(this, "ACHTUNG!","This name is too big!");
        ui->NameLine->clear();
    }
    else{
        c_message.request = c_set_name;
        c_message.message_text = ui->NameLine->text().toStdString();
        SendToServer(socket, &c_message);
    }
}

void MainWindow::on_nameLine_returnPressed2nd(server_data_t s_message){
    if(s_message.responce == s_failure){
        QMessageBox::warning(this, "ACHTUNG!","This username is already used!");
    }
//    else CRH->chat_proc->client_state = no_chat;
}

void MainWindow::list_of_chats(server_data_t s_message){
    if(s_message.responce == s_failure){
        QMessageBox::warning(this, "ACHTUNG!", "Could not get list of chats");
    }
    else{
//        CRH->chat_proc->client_state = no_chat;
        show_chats(s_message.message_text);
    }
}

void MainWindow::on_chatButton_Clicked1st(){
    QPushButton* chat = qobject_cast<QPushButton*>(sender()); // get QObject that emitted the signal
    c_message.request = c_connect_chat;
    c_message.message_text = chat->text().toStdString();
    SendToServer(socket, &c_message);
}

void MainWindow::on_chatButton_Clicked2nd(server_data_t s_message){
        if(s_message.responce == s_success){
//            CRH->chat_proc->client_state = in_chat;
            SafeCleaning(ui->verticalLayout->layout());
            QPushButton* LeaveButton = new QPushButton("Leave chat");
            connect(LeaveButton, &QPushButton::clicked, this, &MainWindow::on_leaveChat1st);
            QListWidget* QLW = new QListWidget();
            QLineEdit* MessageLine = new QLineEdit();
            MessageLine->setPlaceholderText("Enter your message:");
            connect(MessageLine, &QLineEdit::returnPressed, this, &MainWindow::on_sendMessage);
            ui->verticalLayout->insertWidget(0, LeaveButton);
            ui->verticalLayout->insertWidget(1, QLW);
            ui->verticalLayout->insertWidget(2, MessageLine);
        }
        else{
            QMessageBox::warning(this, "ACHTUNG!", "Could not connect to this chat!");
        }
}

void MainWindow::on_newMessage(std::string& message_text){
    static size_t row = 0;
    auto QLW = qobject_cast<QListWidget*>(ui->verticalLayout->itemAt(1)->widget());
    QLW->addItem(QString::fromStdString(message_text));
}

void MainWindow::on_createChat1st(){
    auto line = qobject_cast<QLineEdit*>(sender());
    if(line->text().contains(' ')){
        QMessageBox::warning(this, "ACHTUNG!", "Chat name cannot contain with a space");
        line->clear();
    }
    else{
        c_message.request = c_create_chat;
        c_message.message_text = line->text().toStdString();
        SendToServer(socket, &c_message);
    }
    line->clear();
}

void MainWindow::on_createChat2nd(server_data_t s_message){
    if(s_message.responce == s_failure){
        QMessageBox::warning(this, "ACHTUNG!", "Could not create this chat");
    }
}

void MainWindow::on_leaveChat1st(){
    client_data_t c_message;
    c_message.request = c_leave_chat;
    c_message.message_text = "";
    SendToServer(socket, &c_message);
//    CRH->chat_proc->client_state = no_chat;
}

void MainWindow::on_sendMessage(){
    client_data_t c_message;
    c_message.message_text = qobject_cast<QLineEdit*>(sender())->text().toStdString();
    c_message.request = c_send_message;
    SendToServer(socket, &c_message);
    qobject_cast<QLineEdit*>(sender())->clear();
}
