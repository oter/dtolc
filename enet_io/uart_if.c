#include "uart_if.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/rom_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"

#define LASER_DATA_PACKET 99

#define STATE_RX_PACKET 5;
#define STATE_RX_IDLE   10;

#define STATE_TX_PACKET 6;
#define STATE_TX_IDLE   11;

#define BUFFER_SIZE 4096


uint8_t uart_rx_buffer[BUFFER_SIZE];
uint16_t uart_rx_free = BUFFER_SIZE;
uint16_t uart_rx_packet_count_left = 0;
//uint8_t  uart_rx_state = STATE_RX_IDLE;
uint8_t* uart_rx_data_ptr = &uart_rx_buffer[0];
uint8_t* uart_rx_free_ptr = &uart_rx_buffer[0];

uint8_t uart_tx_buffer[BUFFER_SIZE];
volatile uint16_t uart_tx_free = BUFFER_SIZE;
//uint16_t uart_tx_packet_left = 0;
//uint8_t  uart_tx_state = STATE_TX_IDLE;
uint8_t* uart_tx_data_ptr = &uart_tx_buffer[0];
uint8_t* uart_tx_free_ptr = &uart_tx_buffer[0];

const uint8_t packet_start[4] = {LASER_DATA_PACKET, LASER_DATA_PACKET + 2, LASER_DATA_PACKET + 50, LASER_DATA_PACKET - 80};

uint16_t GetUartTxFree()
{
	return uart_tx_free;
}

uint8_t* get_next_ptr(uint8_t* base, uint8_t* ptr)
{
	uint32_t addr1 = (uint32_t)ptr;
	uint32_t addr2 = (uint32_t)base + BUFFER_SIZE - 1;
	if (addr1 == addr2)
	{
		return base;
	}

	return ++ptr;
}

static uint8_t* FillBufferTx(uint8_t* base, uint8_t* ptr, const uint8_t* data, uint16_t len)
{
	if (BUFFER_SIZE - (ptr - base) > len)
	{
		//UARTprintf("Simple\n");
		uint16_t i = 0;
		for (i = 0; i < len; ++i)
		{
			*ptr = data[i];
			++ptr;
		}
		uart_tx_free -= len;
		//UARTprintf("Simple end\n");
		return ptr;
	}
	//UARTprintf("Hard1\n");
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
	//UARTprintf("Hard2\n");
	for (i = 0; i < len2; ++i)
	{
		*base = data[di];
		++base;
		++di;
	}
	uart_tx_free = uart_tx_free - len1 - len2;
	return base;
}

static uint8_t* FillBufferRx(uint8_t* base, uint8_t* ptr, const uint8_t* data, uint16_t len)
{
	if (ptr + BUFFER_SIZE - base >= len)
	{
		uint16_t i = 0;
		for (i = 0; i < len; ++i)
		{
			*ptr = data[i];
			++ptr;
		}
		uart_rx_free -= len;
		return ptr;
	}
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
	uart_rx_free = uart_rx_free - len1 - len2;
	return base;
}



typedef struct
{
	uint8_t type;
	uint16_t length;
	void* data;
	uint8_t check_sum;

} laser_packet_t;


// The interrupt handler for the SysTick interrupt.
void SysTickIntHandler(void)
{
	uint8_t c = 0;
	for (c = 0; c < 24; ++c)
	{
		if (uart_tx_free < BUFFER_SIZE)
		{
			uart_tx_free++;
			ROM_UARTCharPut(UART3_BASE, *uart_tx_data_ptr);
			uart_tx_data_ptr = get_next_ptr(&uart_tx_buffer[0], uart_tx_data_ptr);
			if (uart_tx_free > BUFFER_SIZE)
			{
				uart_tx_free = BUFFER_SIZE;
				UARTprintf("\n\nuart_tx_free OVERFLOW!!!");
			}
		} else
		{
			break;
		}
	}

	if (uart_rx_packet_count_left)
	{
		uint16_t length = *((uint16_t*)uart_rx_data_ptr);
		uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
		uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
		uart_rx_free += 2;

		if (length >= BUFFER_SIZE)
		{
			UARTprintf("\n\\Invalid packet length!");
			while(true);
		}
		struct pbuf* packet;
		packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
		if (packet == NULL)
		{
			UARTprintf("\nFrom UART: Cannot allocate pbuf.\n");
			while(true);
		}

		uint8_t* data = NULL;
		data = (uint8_t*)mem_malloc(length);
		if (data == NULL)
		{
			UARTprintf("\nFrom UART: Cannot allocate buffer.\n");
			while(true);
		}

		int iter1 = 0;
		for (iter1 = 0; iter1 < length; ++iter1)
		{
			data[iter1] = *uart_rx_data_ptr;
			uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
			uart_rx_free++;
		}

		if (pbuf_take(packet, data, length) != ERR_OK)
		{
			UARTprintf("\nFrom UART: Cannot fill the pbuf packet.\n");
			return;
		}

		OnReceivePacket(packet);

		if (data != NULL)
		{
			mem_free(data);
			data = NULL;
		}

		pbuf_free(packet);

		if (uart_rx_free > BUFFER_SIZE)
		{
			uart_rx_free = BUFFER_SIZE;
			UARTprintf("\n\nuart_rx_free OVERFLOW!!!");
		}

		--uart_rx_packet_count_left;
	}

    // Call the lwIP timer handler.
    //lwIPTimer(SYSTICKMS);
}

// Enable UART3
void UARTIfInit(uint32_t sys_clock)
{
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART3);

	// Enable processor interrupts.
	ROM_IntMasterEnable();

	// Set GPIO A4 and A5 as UART pins.
	GPIOPinConfigure(GPIO_PA4_U3RX);
	GPIOPinConfigure(GPIO_PA5_U3TX);
	ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_4 | GPIO_PIN_5);

	// Configure the UART for 115,200, 8-N-1 operation.
	uint32_t uart_config = UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE;
	ROM_UARTConfigSetExpClk(UART3_BASE, sys_clock, 115200, uart_config);

	// Enable the UART interrupt.
	ROM_IntEnable(INT_UART3);
	ROM_UARTIntEnable(UART3_BASE, UART_INT_RX | UART_INT_RT);
}


#define STATE_WAITING_START 10
#define STATE_READ_SIZE 20
#define STATE_READ_PACKET 30

uint8_t rec_packet = STATE_WAITING_START;
uint8_t check_at = 0;
uint16_t packet_length;
uint16_t rec_packet_size = 0;
uint8_t rec_packet_checksum = 0;
uint8_t* start_buffer = NULL;

void UARTIntHandler3(void)
{
    uint32_t ui32Status;

    // Get the interrrupt status.
    ui32Status = ROM_UARTIntStatus(UART3_BASE, true);

    // Clear the asserted interrupts.
    ROM_UARTIntClear(UART3_BASE, ui32Status);

    if (ui32Status & UART_INT_RX)
    {
    	while(ROM_UARTCharsAvail(UART3_BASE))
		{
			// Try to detect packet start
			if (rec_packet == STATE_WAITING_START)
			{
				if (ROM_UARTCharGet(UART3_BASE) == packet_start[check_at])
				{
					++check_at;
					if (check_at == 4)
					{
						rec_packet = STATE_READ_SIZE;
						check_at = 0;
						continue;
					}
				} else
				{
					//UARTprintf("\nFrom UART: Invalid packet start.\n");
					check_at = 0;
					continue;
				}
			}
			else if (rec_packet == STATE_READ_SIZE)
			{
				packet_length = 0;
				((uint8_t*)&packet_length)[0] = ROM_UARTCharGet(UART3_BASE);
				((uint8_t*)&packet_length)[1] = ROM_UARTCharGet(UART3_BASE);
				UARTprintf("From UART: Packet: %d;\t", packet_length);
				if (packet_length >= BUFFER_SIZE)
				{
					UARTprintf("\n\nFrom UART: Ignoring invalid huge packet %d\n", packet_length);
					rec_packet = STATE_WAITING_START;
					continue;
				}
				if (packet_length > uart_rx_free)
				{
					UARTprintf("\n\nFrom UART: Ignoring invalid huge packet %d\n", packet_length);
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
					uint8_t checksum = ROM_UARTCharGet(UART3_BASE);
					if (rec_packet_checksum != checksum)
					{
						rec_packet = STATE_WAITING_START;
						uart_rx_free_ptr = start_buffer;
						uart_rx_free += 2 + rec_packet_size;
						UARTprintf("\n\nFrom UART: Invalid checksum!\n", rec_packet_size);
						continue;
					} else
					{
						++uart_rx_packet_count_left;
						UARTprintf("From UART: receive packet%d\n", rec_packet_size);
						continue;
					}
				}
				uint8_t d = ROM_UARTCharGet(UART3_BASE);
				uart_rx_free_ptr = FillBufferRx(&uart_rx_buffer[0], uart_rx_free_ptr, &d, 1);
				++rec_packet_size;
			}
		}
    }

	//ROM_UARTCharPutNonBlocking(UART3_BASE, );
	// Blink the LED to show a character transfer is occuring.
	//GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
	// Delay for 1 millisecond.  Each SysCtlDelay is about 3 clocks.
	//SysCtlDelay(g_ui32SysClock / (1000 * 3));
	// Turn off the LED
   // GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);
}

void UARTSend3(struct pbuf *p)
{
	ROM_UARTCharPut(UART3_BASE, LASER_DATA_PACKET);
	ROM_UARTCharPut(UART3_BASE, LASER_DATA_PACKET + 1);
	ROM_UARTCharPut(UART3_BASE, LASER_DATA_PACKET + 2);
	ROM_UARTCharPut(UART3_BASE, LASER_DATA_PACKET + 3);

	ROM_UARTCharPut(UART3_BASE, ((uint8_t*)&p->tot_len)[0]);
	ROM_UARTCharPut(UART3_BASE, ((uint8_t*)&p->tot_len)[1]);

	UARTprintf("-> UART: Sending packet.\n");

	struct pbuf* to_send = p;

	uint8_t ui32NumChained = pbuf_clen(to_send);
	while(ui32NumChained)
	{
		//UARTprintf("To UART: Sending sub-packet.\n");
		uint16_t i;
		for (i = 0; i < to_send->len; ++i)
		{
			ROM_UARTCharPut(UART3_BASE, ((uint8_t*)to_send->payload)[i]);
		}
		--ui32NumChained;
		to_send = to_send->next;
	}
}

err_t SendPacket(struct pbuf* packet)
{
	UARTprintf("Send packet0\n");
	//packet->tot_len + start_packet + packet_len + checksum
	if (uart_tx_free < packet->tot_len + 4 + 2 + 1)
	{
		UARTprintf("-> UART: Not enough memory. Need %d, has %d;\t ", packet->tot_len, uart_tx_free);
		return ERR_MEM;
	}

	//UARTprintf("-> UART:Packet %d, mem %d\n", packet->tot_len, uart_tx_free);

	// Fill start of the packet
	uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, &packet_start[0], 4);
	// Fill length of the packet
	uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, (uint8_t*)&packet->tot_len, 2);

	uint8_t check_sum = 0;
	struct pbuf* current = packet;
	UARTprintf("Send packet1\n");
	while(current != NULL)
	{
		uint16_t i = 0;
		for (i = 0; i < packet->len; ++i)
		{
			check_sum += ((uint8_t*)packet->payload)[i];
		}
		uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, packet->payload, packet->len);
		current = current->next;
	}
	UARTprintf("Send packet2\n");
	// Fill checksum
	uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, &check_sum, 1);

	UARTprintf("Send packet end\n");

	return ERR_OK;
}





