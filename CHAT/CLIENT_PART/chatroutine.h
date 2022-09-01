#pragma once

#include <QThread>
#include <QTcpSocket>
#include <QDebug>
#include <QObject>
#include <memory>
#include "client_interface.h"

class ChatRoutine : public QObject {
    Q_OBJECT

    enum unblocked_read_state {
        TYPE,
        LENGTH,
        MESSAGE
    };

    server_data_t s_d;
    QTcpSocket* tcp_socket;

    size_t off = 0;
    unblocked_read_state state = TYPE;
    size_t length;
    bool readed = false;

public:
    std::atomic_bool go_on;
    ChatRoutine(QTcpSocket*);

public slots:
    bool UnblockedReadFromServer();
//	void process(); 	/*  создает и запускает */
    void stop();    	/*  останавливает */

signals:
	void finished(); 	/* сигнал о завершении работы */
    void new_message_come(std::string&);
    void get_chats(server_data_t);
};

class ChatRoutineHandler : public QObject {
	Q_OBJECT

    QTcpSocket* tcp_socket;
public:
	ChatRoutineHandler(QObject *parent, QTcpSocket*);
	~ChatRoutineHandler();
    void startThread();

public slots:
	void stopThread();

signals:
    void stop(); //остановка потока
};
