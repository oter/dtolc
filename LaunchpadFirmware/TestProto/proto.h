#ifndef PROTO_H
#define PROTO_H

#include <QObject>
#include <QtSerialPort/QSerialPort>

#define LASER_DATA_PACKET 99

const uint8_t packet_start[4] = {LASER_DATA_PACKET, LASER_DATA_PACKET + 2, LASER_DATA_PACKET + 50, LASER_DATA_PACKET - 80};

class Proto :  public QSerialPort
{
    Q_OBJECT
public:
    explicit Proto(QObject *parent = 0);

    bool Send(const QByteArray &data);

    void Connect();

    void Disconnect();

public slots:

    void OnRead();

signals:

public slots:
};

#endif // PROTO_H
