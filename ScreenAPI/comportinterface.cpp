#include "comportinterface.h"

#include <cstring>

#include <QDebug>

const static uint32_t kPacketStartNumber = SCREEN_PACKET_ID;
const QByteArray ComPortInterface::kPacketStart = QByteArray((const char*)&kPacketStartNumber, sizeof(kPacketStartNumber));

ComPortInterface::ComPortInterface()
{
    this->m_status = ConnectionStatus::kDisconnected;

    connect(this, SIGNAL(readyRead()), this, SLOT(OnReadyRead()));
    connect(this, SIGNAL(aboutToClose()), this, SLOT(OnDeviceClose()));
}

QVector<ScreenPacket> ComPortInterface::DispatchPacketsFromData(const QByteArray &data)
{
    m_received_data.append(data);

    QVector<ScreenPacket> packets;

    if (data.size() < sizeof(ScreenPacket))
    {
        return packets;
    }

    for (int i = 0; i < data.size() - kPacketStart.size(); ++i)
    {
        if ( *((uint32_t*)(data.data() + i)) == SCREEN_PACKET_ID )
        {
            if (i + sizeof(ScreenPacket) <= data.size())
            {
                auto index = m_received_data.indexOf(kPacketStart);
                if (index == -1)
                {
                    continue;
                }

                if (index + sizeof(ScreenPacket) <= data.size())
                {
                    ScreenPacket packet;
                    std::memcpy(&packet, data.data(), sizeof(ScreenPacket));
                    if (packet.CheckIntegrity())
                    {
                        packets.push_back(packet);
                    }
                }
                i = 0;
            }
            else
            {
                qDebug() << "Packet start not found!";
                break;
            }
        }
    }
    return packets;
}

bool ComPortInterface::Connect(const QString &port_name)
{
    if (this->m_status == ConnectionStatus::kDisconnected)
    {
        this->setPortName(port_name);
        if (!this->open(QIODevice::ReadWrite))
        {
            qDebug() << "Can't open a port with the name: " << port_name;
            return false;
        }
        if (!this->setBaudRate(QSerialPort::Baud115200))
        {
            qDebug() << "Can't set a port baud rate: " << errorString();
            return false;
        }
        if (!this->setDataBits(QSerialPort::Data8))
        {
            qDebug() << "Can't set a port data bits: " << errorString();
            return false;
        }
        if (!this->setParity(QSerialPort::NoParity))
        {
            qDebug() << "Can't set a port parity: " << errorString();
            return false;
        }

    }
}

bool ComPortInterface::Disconnect()
{
    switch (this->m_status) {
    case ConnectionStatus::kConnected:
        break;
    case ConnectionStatus::kDisconnected:
        break;
    }
}

bool ComPortInterface::SendPacket(const ScreenPacket &packet)
{

}

void ComPortInterface::OnReadyRead()
{
    auto& packets = DispatchPacketsFromData(readAll());

    for (auto& p : packets)
    {
        emit PacketReceived(p);
    }
}

void ComPortInterface::OnDeviceClose()
{
    qDebug() << "Device has been closed unexpectedly";
    DispatchPacketsFromData(QByteArray());
    this->m_status = ConnectionStatus::kDisconnected;
}
