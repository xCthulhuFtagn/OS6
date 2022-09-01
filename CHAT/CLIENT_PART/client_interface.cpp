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

int BlockedRead(char* object, size_t length, QTcpSocket* socket) {
    ssize_t err;
    size_t off = 0;
    socket->waitForReadyRead(10);
    while ((err = socket->read(object + off, length - off)) >= 0) {
        if (off == length) return 0;
        socket->waitForReadyRead(10);
        off += err;
    }
    return err;
}

bool ReadFromServer(QTcpSocket* socket, server_data_t* s_d){
    int received = BlockedRead((char*)&s_d->responce, sizeof(server_responce_e), socket);
    if (received < 0) return false;
    size_t length;
    received = BlockedRead((char*)&length, sizeof(size_t), socket);
    if (received < 0) return false;
    s_d->message_text.resize(length);
    received = BlockedRead((char*)s_d->message_text.data(), length, socket);
    if (received < 0) return false;
    return true;
}

void SafeCleaning(QLayout* layout) {
    QLayoutItem *wItem;
    while ((wItem = layout->takeAt(0)) != 0) {
        if(wItem->widget()) wItem->widget()->deleteLater();
        delete wItem;
    }
}
