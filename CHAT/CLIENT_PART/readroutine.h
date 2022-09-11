#pragma once

#include <QThread>
#include <QTcpSocket>
#include <QDebug>
#include <QObject>
#include <memory>
#include "client_interface.h"

class ReadRoutine : public QObject {
    Q_OBJECT

    enum unblocked_read_state {
        REQ_TYPE,
        RESP_TYPE,
        LENGTH,
        MESSAGE
    };

    server_data_t s_d;
    QTcpSocket* tcp_socket;

    size_t off = 0;
    unblocked_read_state state = REQ_TYPE;
    size_t length;
    bool readed = false;

public:
    std::atomic_bool go_on;
    ReadRoutine(QTcpSocket*);

public slots:
    bool UnblockedReadFromServer(); /*  создает и запускает */
    void stop();    	/*  останавливает */

signals:
	void finished(); 	/* сигнал о завершении работы */
    void new_message_come(std::string&);
    void get_chats(server_data_t);
    void connect_chat(server_data_t);
    void create_chat(server_data_t);
    void send_name(server_data_t);
    void leave_chat(server_data_t);
};

class ReadRoutineHandler : public QObject {
	Q_OBJECT

    QTcpSocket* tcp_socket;
public:
    ReadRoutineHandler(QObject *parent, QTcpSocket*);
    ~ReadRoutineHandler();
    void startThread();

public slots:
	void stopThread();

signals:
    void stop(); //остановка потока
};
