#include "chatroutine.h"
#include "client_interface.h"
#include "mainwindow.h"
#include <QDebug>

ChatRoutine::ChatRoutine(QTcpSocket* socket) {
    tcp_socket = socket;
    go_on = true;
//    length = sizeof(server_responce_e);
}

//ChatRoutine::~ChatRoutine() {}

//bool ChatRoutine::UnblockedReadFromServer(server_data_t* s_d){
//    tcp_socket->waitForReadyRead(10);
//    int received;
//    static size_t help_length;

//    switch (state) {
//        case TYPE:
//            received = tcp_socket->read((char*)&s_d->responce, length);
//            if (received < 0) return false;
//            off += received;
//            if (off == length) {
//                state = LENGTH;
//                length = sizeof(size_t);
//                off = 0;
//            }
//            return true;
//        case LENGTH:
//            received = tcp_socket->read((char*)&help_length, length);
//            if (received < 0) return false;
//            off += received;
//            if (off == length) {
//                length = help_length;
//                state = MESSAGE;
//                s_d->message_text.resize(length);
//                off = 0;
//            }
//            return true;
//        case MESSAGE:
//            received = tcp_socket->read((char*)s_d->message_text.data(), length);
//            if (received < 0) return false;
//            off += received;
//            if (off == length) {
//                state = TYPE;
//                length = sizeof(server_responce_e);
//                off = 0;
//                readed = true;
//            }
//            return true;
//    }
//}

void ChatRoutine::process() {
    server_data_t s_message;
    while (go_on) {
        ReadFromServer(tcp_socket, &s_message);
        switch(s_message.responce){
            case s_new_message:
                emit new_message_come(s_message.message_text);
                break;
            case s_success:
                emit get_chats(s_message);
                this->disconnect();
                emit finished();
                return;
            default:
                qDebug() << "Chat routine received wrong data";
                //wrong message
        }
    }
	emit finished();
	return;
}

void ChatRoutine::stop() {
    go_on = false;
}

ChatRoutineHandler::ChatRoutineHandler(QObject *parent, QTcpSocket* tcp_socket) {
    setParent(parent);
    this->tcp_socket = tcp_socket;
}

void ChatRoutineHandler::startThread(){
    ChatRoutine* chat_proc = new ChatRoutine(tcp_socket);
    QThread* thread = new QThread;
    chat_proc->moveToThread(thread);
    connect(chat_proc, &ChatRoutine::new_message_come, qobject_cast<MainWindow*>(parent()), &MainWindow::on_newMessage);
    connect(chat_proc, &ChatRoutine::get_chats, qobject_cast<MainWindow*>(parent()), &MainWindow::on_leaveChat2nd);
    connect(thread, &QThread::started, chat_proc, &ChatRoutine::process);
    connect(chat_proc, &ChatRoutine::finished, thread, &QThread::quit);
    connect(this, &ChatRoutineHandler::stop, chat_proc, &ChatRoutine::stop);
    connect(chat_proc, &ChatRoutine::finished, chat_proc, &ChatRoutine::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void ChatRoutineHandler::stopThread() {
    emit stop();
}

ChatRoutineHandler::~ChatRoutineHandler(){
    stop();
}
