#include "client_interface.h"

bool SendToServer(QTcpSocket* socket, client_data_t* c_d){
    int written = socket->write(QByteArray::fromRawData((char*)(&c_d->request), sizeof(client_request_e))); //writes by using this bullshit interface
    if (written < 0) return false;
    size_t tmp = c_d->message_text.size();
    written = socket->write(QByteArray::fromRawData((char*)(&tmp), sizeof(size_t))); //writes by using this bullshit interface
    if (written < 0) return false;
    written = socket->write(QByteArray::fromRawData((char*)(c_d->message_text.c_str()), c_d->message_text.size())); //writes by using this bullshit interface
    if (written < 0) return false;
    qDebug() << "Written a message to server";
    return true;
}

bool ReadFromServer(QTcpSocket* socket, server_data_t* s_d){
    socket->waitForReadyRead();
    int received = socket->read((char*)&s_d->responce, sizeof(server_responce_e));
    if (received < 0) return false;
    size_t length;
    socket->waitForReadyRead();
    received = socket->read((char*)&length, sizeof(size_t));
    if (received < 0) return false;
    s_d->message_text.resize(length);
    if(length > 0) socket->waitForReadyRead();
    received = socket->read((char*)s_d->message_text.data(), length);
    if (received < 0) return false;
    return true;
}
