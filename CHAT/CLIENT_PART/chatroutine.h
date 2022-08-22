#pragma once

#include <QThread>
#include <QTcpSocket>
#include <QDebug>
#include <QObject>
#include <memory>

class ChatRoutine : public QObject {
    Q_OBJECT

    std::atomic_bool go_on;
    QTcpSocket* tcp_socket;
public:
    ChatRoutine(QTcpSocket*);

public slots:
	void process(); 	/*  создает и запускает */
	void stop();    	/*  останавливает */

signals:
	void finished(); 	/* сигнал о завершении работы */
    void new_message_come(std::string&);
};

class ChatRoutineHandler : public QObject {
	Q_OBJECT

    QTcpSocket* tcp_socket;
public:
	ChatRoutineHandler(QObject *parent, QTcpSocket*);
	~ChatRoutineHandler();
	void startThread();    
	void stopThread();

signals:
	void stop(); //остановка всех потоков
};
