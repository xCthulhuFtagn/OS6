#include "readroutine.h"
#include "client_interface.h"
#include "mainwindow.h"
#include <QDebug>

ReadRoutine::ReadRoutine(QTcpSocket* socket) {
    tcp_socket = socket;
    go_on = true;
    length = sizeof(server_responce_e);
}

bool ReadRoutine::UnblockedReadFromServer(){
//    if (!tcp_socket->waitForReadyRead(1))
//        return true;
    int received = 1;
    static size_t help_length;

    while (received > 0) {
        switch (state) {
            case REQ_TYPE:
                received = tcp_socket->read((char*)&s_d.request, length);
                if (received < 0) return false;
                off += received;
                if (off == length) {
                    state = RESP_TYPE;
                    length = sizeof(server_responce_e);
                    off = 0;
                }
                break;
            case RESP_TYPE:
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
                    state = REQ_TYPE;
                    length = sizeof(server_responce_e);
                    off = 0;
                    readed = true;
                    qDebug() << "Client state: " << client_state;
                    switch(client_state){
                        case no_name:
                            switch(s_d.request){
                                case c_set_name:
                                    client_state = no_chat;
                                    emit send_name(s_d);
                                    break;
                                default:
                                    qDebug() << "Chat routine received wrong data";
                            }
                            break;
                        case no_chat:
                            switch(s_d.request){
                                case c_get_chats:
                                    emit get_chats(s_d);
                                    break;
                                case c_create_chat:
                                    emit create_chat(s_d);
                                    break;
                                case c_connect_chat:
                                    client_state = in_chat;
                                    emit connect_chat(s_d);
                                    break;
                                default:
                                    qDebug() << "Chat routine received wrong data";
                            }
                            break;
                        case in_chat:
                            switch(s_d.request){
                                case c_receive_message:
                                    emit new_message_come(s_d.message_text);
                                    break;
                                case c_leave_chat:
                                    client_state = no_chat;
                                    emit get_chats(s_d);
                                    break;
                                default:
                                    qDebug() << "Chat routine received wrong data";
                            }
                    }
                }
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

void ReadRoutine::stop() {
    emit finished();
}

ReadRoutineHandler::ReadRoutineHandler(QObject *parent, QTcpSocket* tcp_socket) {
    setParent(parent);
    this->tcp_socket = tcp_socket;
    client_state = no_name;
}

void ReadRoutineHandler::startThread(){
    ReadRoutine* chat_proc = new ReadRoutine(tcp_socket);
    QThread* thread = new QThread;
    chat_proc->moveToThread(thread);

    connect(chat_proc, &ReadRoutine::new_message_come, qobject_cast<MainWindow*>(parent()), &MainWindow::on_newMessage);
    connect(chat_proc, &ReadRoutine::get_chats, qobject_cast<MainWindow*>(parent()), &MainWindow::list_of_chats);
    connect(chat_proc, &ReadRoutine::create_chat, qobject_cast<MainWindow*>(parent()), &MainWindow::on_createChat2nd);
    connect(chat_proc, &ReadRoutine::connect_chat, qobject_cast<MainWindow*>(parent()), &MainWindow::on_chatButton_Clicked2nd);
    connect(chat_proc, &ReadRoutine::send_name, qobject_cast<MainWindow*>(parent()), &MainWindow::on_nameLine_returnPressed2nd);

    connect(chat_proc, &ReadRoutine::finished, thread, &QThread::quit);
    connect(qobject_cast<QIODevice*>(tcp_socket), &QIODevice::readyRead, chat_proc, &ReadRoutine::UnblockedReadFromServer);
    connect(this, &ReadRoutineHandler::stop, chat_proc, &ReadRoutine::stop);
    connect(chat_proc, &ReadRoutine::finished, chat_proc, &ReadRoutine::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void ReadRoutineHandler::stopThread() {
    emit stop();
}

ReadRoutineHandler::~ReadRoutineHandler(){
//    stopThread();
}
