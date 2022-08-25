#include "chatroutine.h"
#include "client_interface.h"
#include <QDebug>

ChatRoutine::ChatRoutine(QTcpSocket* socket) {
    tcp_socket = socket;
    go_on = true;
}

//ChatRoutine::~ChatRoutine() {}

void ChatRoutine::process() {
    server_data_t* s_message = new server_data_t;
    while (go_on) {
        ReadFromServer(tcp_socket, s_message);
        switch(s_message->responce){
            case s_new_message:
                emit new_message_come(s_message->message_text);
                break;
            default:
                qDebug() << "Chat routine received wrong data";
                //wrong message
        }
    }
    delete s_message;
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
    connect(chat_proc, SIGNAL(new_message_come()), parent(), SLOT(on_newMessage()));
    connect(thread, SIGNAL(started()), chat_proc, SLOT(process()));
    connect(chat_proc, SIGNAL(finished()), thread, SLOT(quit()));
    connect(this, SIGNAL(stop()), chat_proc, SLOT(stop()));
    connect(chat_proc, SIGNAL(finished()), chat_proc, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    thread->start();
}

void ChatRoutineHandler::stopThread() {
	emit stop(); 
}

ChatRoutineHandler::~ChatRoutineHandler(){
    stop();
}
