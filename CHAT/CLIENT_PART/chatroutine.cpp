#include "chatroutine.h"
#include "client_interface.h"
#include "mainwindow.h"
#include <QDebug>

ChatRoutine::ChatRoutine(QTcpSocket* socket) {
    tcp_socket = socket;
    go_on = true;
    length = sizeof(server_responce_e);
}

bool ChatRoutine::UnblockedReadFromServer(){
//    if (!tcp_socket->waitForReadyRead(1))
//        return true;
    int received = 1;
    static size_t help_length;

    while (received > 0) {
        switch (state) {
            case TYPE:
                received = tcp_socket->read((char*)&s_d.responce, length);
                if (received < 0) return false;
                off += received;
                if (off == length) {
                    state = LENGTH;
                    length = sizeof(size_t);
                    off = 0;
                }
                break;
            case LENGTH:
                received = tcp_socket->read((char*)&help_length, length);
                if (received < 0) return false;
                off += received;
                if (off == length) {
                    length = help_length;
                    state = MESSAGE;
                    s_d.message_text.resize(length);
                    off = 0;
                }
                break;
            case MESSAGE:
                received = tcp_socket->read((char*)s_d.message_text.data(), length);
                if (received < 0) return false;
                off += received;
                if (off == length) {
                    state = TYPE;
                    length = sizeof(server_responce_e);
                    off = 0;
                    readed = true;
                    switch(s_d.responce){
                        case s_new_message:
                            emit new_message_come(s_d.message_text);
                            break;
                        case s_success:
                            emit get_chats(s_d);
                            emit finished();
                            return true;
                        default:
                            qDebug() << "Chat routine received wrong data";
                            //wrong message
                    }
                }
                break;
        }
    }
    return true;
}

//void ChatRoutine::process() {
//    server_data_t s_message;
//    while (go_on) {
//        UnblockedReadFromServer(&s_message);
//        if(readed){
//            switch(s_message.responce){
//                case s_new_message:
//                    emit new_message_come(s_message.message_text);
//                    break;
//                case s_success:
//                    emit get_chats(s_message);
//                    emit finished();
//                    return;
//                default:
//                    qDebug() << "Chat routine received wrong data";
//                    //wrong message
//            }
//            readed = false;
//        }
//    }
//    emit finished();
//	return;
//}

void ChatRoutine::stop() {
    emit finished();
}

ChatRoutineHandler::ChatRoutineHandler(QObject *parent, QTcpSocket* tcp_socket) {
    setParent(parent);
    this->tcp_socket = tcp_socket;
}

void ChatRoutineHandler::startThread(){
    ChatRoutine* chat_proc = new ChatRoutine(tcp_socket);
    QThread* thread = new QThread;
    chat_proc->moveToThread(thread);
//    connect(qobject_cast<MainWindow*>(parent()), &MainWindow::destroyed, chat_proc, &ChatRoutine::stop);
    connect(chat_proc, &ChatRoutine::new_message_come, qobject_cast<MainWindow*>(parent()), &MainWindow::on_newMessage);
    connect(chat_proc, &ChatRoutine::get_chats, qobject_cast<MainWindow*>(parent()), &MainWindow::on_leaveChat2nd);
//    connect(thread, &QThread::started, chat_proc, &ChatRoutine::process);
    connect(chat_proc, &ChatRoutine::finished, thread, &QThread::quit);
    connect(qobject_cast<QIODevice*>(tcp_socket), &QIODevice::readyRead, chat_proc, &ChatRoutine::UnblockedReadFromServer);
    connect(this, &ChatRoutineHandler::stopThread, chat_proc, &ChatRoutine::stop);
    connect(chat_proc, &ChatRoutine::finished, chat_proc, &ChatRoutine::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void ChatRoutineHandler::stopThread() {
    emit stop();
}

ChatRoutineHandler::~ChatRoutineHandler(){
//    stopThread();
}
