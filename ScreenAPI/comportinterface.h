#ifndef COMPORTINTERFACE_H
#define COMPORTINTERFACE_H

#include <atomic>
#include <queue>

#include <QSerialPort>
#include <QByteArray>
#include <QVector>

#include "screen_proto.h"


class ComPortInterface : public QSerialPort
{
    Q_OBJECT

    enum class ConnectionStatus
    {
        kConnected,
        kDisconnected
    };

public:
    ComPortInterface();
    ConnectionStatus Status() { return this->m_status; }

private:
    QVector<ScreenPacket> DispatchPacketsFromData(const QByteArray& data);


signals:
    void Connected();
    void Disconnected();

    void PacketReceived(const ScreenPacket& packet);

public slots:
    bool Connect(const QString& port_name);
    bool Disconnect();

    bool SendPacket(const ScreenPacket& packet);

private slots:
    void OnReadyRead();
    void OnDeviceClose();



private:
    ConnectionStatus m_status;
    QString m_port_name;
    QByteArray m_received_data;
    static const QByteArray kPacketStart;
};

#endif // COMPORTINTERFACE_H
