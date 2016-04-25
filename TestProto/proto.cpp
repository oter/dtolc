#include "proto.h"

#include <QDebug>

const uint32_t BUFFER_SIZE = 16384;

uint8_t uart_rx_buffer[BUFFER_SIZE];
volatile uint16_t uart_rx_free = BUFFER_SIZE;
uint16_t uart_rx_packet_count_left = 0;
//uint8_t  uart_rx_state = STATE_RX_IDLE;
uint8_t* uart_rx_data_ptr = &uart_rx_buffer[0];
uint8_t* uart_rx_free_ptr = &uart_rx_buffer[0];

uint8_t* get_next_ptr(uint8_t* base, uint8_t* ptr)
{
    //UARTprintf("Addrds: %p %p\n", base, ptr);
    uint32_t addr1 = (uint32_t)ptr;
    uint32_t addr2 = (uint32_t)base + BUFFER_SIZE - 1;
    if (addr1 == addr2)
    {
        return base;
    }

    return ++ptr;
}

static uint8_t* FillBufferRx(uint8_t* base, uint8_t* ptr, const uint8_t* data, uint16_t len)
{
    if (BUFFER_SIZE - (ptr - base) > len)
    {
        //UARTprintf("SimpleRx\n");
        uint16_t i = 0;
        for (i = 0; i < len; ++i)
        {
            *ptr = data[i];
            ++ptr;
        }
        uart_rx_free -= len;
        //UARTprintf("Simple endRx\n");
        return ptr;
    }
    //UARTprintf("Hard1Rx\n");
    uint16_t i = 0;
    uint16_t di = 0;
    uint16_t len1 = base + BUFFER_SIZE - ptr;
    uint16_t len2 = len - len1;
    for (i = 0; i < len1; ++i)
    {
        *ptr = data[di];
        ++ptr;
        ++di;
    }
    for (i = 0; i < len2; ++i)
    {
        *base = data[di];
        ++base;
        ++di;
    }
    //UARTprintf("Hard2Rx\n");
    uart_rx_free = uart_rx_free - len1 - len2;
    return base;
}

Proto::Proto(QObject *parent) : QSerialPort(parent)
{
    connect(this, SIGNAL(readyRead()), this, SLOT(OnRead()));
}

bool Proto::Send(const QByteArray &data)
{
    QByteArray to_send;
    to_send.append(packet_start[0]);
    to_send.append(packet_start[1]);
    to_send.append(packet_start[2]);
    to_send.append(packet_start[3]);
    uint16_t size = data.size();
    to_send.append(((char*)&size)[0]);
    to_send.append(((char*)&size)[1]);

    uint8_t checksum = 0;

    for (char c : data)
    {
        checksum += c;
    }
    to_send.append(data);
    to_send.append(checksum);
    this->write(to_send);
    this->flush();
    this->waitForBytesWritten(5000);

    return true;
}

void Proto::Connect()
{
    this->setPortName("COM12");
    this->setBaudRate(921600);
    this->setDataBits(DataBits::Data8);
    this->setStopBits(StopBits::OneStop);
    this->setParity(Parity::NoParity);
    this->open(QSerialPort::OpenModeFlag::ReadWrite);
}

void Proto::Disconnect()
{
    this->close();
}

#define STATE_WAITING_START 10
#define STATE_READ_SIZE1 20
#define STATE_READ_SIZE2 25
#define STATE_READ_PACKET 30

uint8_t check_at = 0;
uint16_t packet_length;
uint16_t rec_packet_size = 0;
uint8_t rec_packet_checksum = 0;
uint8_t* start_buffer = NULL;
uint8_t rec_packet = STATE_WAITING_START;



void Proto::OnRead()
{
    static QByteArray got;
   auto data = this->readAll();

   got.append(data);

   for (uint8_t c : data)
   {
       // Try to detect packet start
       if (rec_packet == STATE_WAITING_START)
       {
           if (c == packet_start[check_at])
           {
               ++check_at;
               if (check_at == 4)
               {
                   rec_packet = STATE_READ_SIZE1;

                   check_at = 0;
                   continue;
               }
           } else
           {
               if (check_at != 0)
               {
                   qDebug() << "From UART: Invalid packet start.";
               }
               check_at = 0;
               continue;
           }
       }
       else if (rec_packet == STATE_READ_SIZE1)
       {
           rec_packet = STATE_READ_SIZE2;
           packet_length = 0;
           ((uint8_t*)&packet_length)[0] = c;
       }
       else if (rec_packet == STATE_READ_SIZE2)
       {
           ((uint8_t*)&packet_length)[1] = c;
           qDebug() << "UART:" << packet_length;
           if (packet_length >= BUFFER_SIZE)
           {
               qDebug() << "From UART: Ignoring invalid huge packet" << packet_length;
               rec_packet = STATE_WAITING_START;
               continue;
           }
           if (packet_length > uart_rx_free)
           {
               qDebug() << "From UART: Not enough memory %d\n" << packet_length;
               rec_packet = STATE_WAITING_START;
               continue;
           }
           start_buffer = uart_rx_free_ptr;
           rec_packet = STATE_READ_PACKET;
           uart_rx_free_ptr = FillBufferRx(&uart_rx_buffer[0], uart_rx_free_ptr, (uint8_t*)&packet_length, 2);
           rec_packet_size = 0;
           rec_packet_checksum = 0;
       }
       else if (rec_packet == STATE_READ_PACKET)
       {
           if (rec_packet_size == packet_length)
           {
               uint8_t checksum = c;
               if (rec_packet_checksum != checksum)
               {
                   rec_packet = STATE_WAITING_START;
                   uart_rx_free_ptr = start_buffer;
                   uart_rx_free += 2 + rec_packet_size;
                   qDebug() << "Invalid checksum. " << rec_packet_size;
                   got.clear();
                   continue;
               } else
               {
                   rec_packet = STATE_WAITING_START;
                   qDebug() << "receive " << rec_packet_size;
                   got.clear();

                   uint16_t length = 0;
                   ((uint8_t*)&length)[0] = *uart_rx_data_ptr;
                   uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
                   ((uint8_t*)&length)[1] = *uart_rx_data_ptr;
                   uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);

                   if (length >= BUFFER_SIZE)
                   {
                       qDebug() << "Invalid packet length!";
                       while(true);
                   }

                   int iter1 = 0;
                   for (iter1 = 0; iter1 < length; ++iter1)
                   {
                       uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
                   }

                   uart_rx_free += 2 + length;


                   continue;
               }
           } else
           {
               uint8_t d = c;
               rec_packet_checksum += d;
               uart_rx_free_ptr = FillBufferRx(&uart_rx_buffer[0], uart_rx_free_ptr, &d, 1);
               ++rec_packet_size;
           }
       }
   }

}
