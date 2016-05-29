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

#define AUX_BUFFER_SIZE 2048

uint8_t aux_buffer[AUX_BUFFER_SIZE];
uint16_t aux_buffer_write_index = 0;
uint16_t aux_buffer_read_index = 0;
uint16_t aux_buffer_free = AUX_BUFFER_SIZE;

bool aux_data_available()
{
	return aux_buffer_free != AUX_BUFFER_SIZE;
}

uint8_t aux_get_char()
{
	if ((!aux_data_available()) || aux_buffer_free > AUX_BUFFER_SIZE)
	{
		UARTprintf("AUX data underrun\n");
		while(true);
	}
	uint8_t ret = aux_buffer[aux_buffer_read_index];
	++aux_buffer_read_index;
	if (aux_buffer_read_index == AUX_BUFFER_SIZE)
	{
		aux_buffer_read_index = 0;
	}
	++aux_buffer_free;
	return ret;
}

void aux_put_char(uint8_t c)
{
	if (aux_buffer_free == 0)
	{
		UARTprintf("AUX data overrun\n");
		while(true);
	}
	aux_buffer[aux_buffer_write_index] = c;
	++aux_buffer_write_index;
	if (aux_buffer_write_index == AUX_BUFFER_SIZE)
	{
		aux_buffer_write_index = 0;
	}

	--aux_buffer_free;
}

#define LASER_DATA_PACKET 99

const uint32_t BUFFER_SIZE = 16384;


uint8_t uart_rx_buffer[BUFFER_SIZE];
volatile uint16_t uart_rx_free = BUFFER_SIZE;
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

void UARTIntHandler3(void);

const uint8_t packet_start[4] = {LASER_DATA_PACKET, LASER_DATA_PACKET + 2, LASER_DATA_PACKET + 50, LASER_DATA_PACKET - 80};

uint16_t GetUartTxFree()
{
	return uart_tx_free;
}

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

static uint8_t* FillBufferTx(uint8_t* base, uint8_t* ptr, const uint8_t* data, uint16_t len)
{
	if (BUFFER_SIZE - (ptr - base) > len)
	{
		//UARTprintf("SimpleTx\n");
		int i = 0;
		for (i = 0; i < len; ++i)
		{
			//*ptr = 0;
			//*ptr = data[i];
			//UARTprintf("%d", i);
			*ptr = data[i];
			++ptr;
		}
		uart_tx_free -= len;
		//UARTprintf("Simple endTx\n");
		return ptr;
	}
	//UARTprintf("Hard1Tx\n");
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
	//UARTprintf("Hard2Tx\n");
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



typedef struct
{
	uint8_t type;
	uint16_t length;
	void* data;
	uint8_t check_sum;

} laser_packet_t;

int sent = 0;


#define STATE_WAITING_START 10
#define STATE_READ_SIZE1 20
#define STATE_READ_SIZE2 25
#define STATE_READ_PACKET 30

uint8_t rec_packet = STATE_WAITING_START;
uint8_t check_at = 0;
uint16_t packet_length;
uint16_t rec_packet_size = 0;
uint8_t rec_packet_checksum = 0;
uint8_t* start_buffer = NULL;

#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)

// The interrupt handler for the SysTick interrupt.
void SysTickIntHandler(void)
{
	lwIPTimer(SYSTICKMS);
}

void ProcessTimerHandler(void)
{
	MAP_IntDisable(INT_TIMER0A);
	MAP_TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

	while(aux_data_available())
	{
		uint8_t c = aux_get_char();
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
				//UARTprintf("\nFrom UART: Invalid packet start.\n");
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
			UARTprintf("UART: %d; ", packet_length);
			if (packet_length >= BUFFER_SIZE)
			{
				UARTprintf("\n\nFrom UART: Ignoring invalid huge packet %d\n", packet_length);
				rec_packet = STATE_WAITING_START;
				continue;
			}
			if (packet_length > uart_rx_free)
			{
				UARTprintf("\n\nFrom UART: Not enough memory %d\n", packet_length);
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
					//UARTprintf
					UARTprintf("invalid checksum!\n", rec_packet_size);
					continue;
				} else
				{
					rec_packet = STATE_WAITING_START;
					++uart_rx_packet_count_left;
					UARTprintf("receive;\n");
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


	if (uart_rx_packet_count_left)
	{
		uint16_t length = 0;
		((uint8_t*)&length)[0] = *uart_rx_data_ptr;
		uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
		((uint8_t*)&length)[1] = *uart_rx_data_ptr;
		uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);

		if (length >= BUFFER_SIZE)
		{
			UARTprintf("\nInvalid packet length!");
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
		}

		if (pbuf_take(packet, data, length) != ERR_OK)
		{
			UARTprintf("\nFrom UART: Cannot fill the pbuf packet.\n");
			return;
		}

		uart_rx_free += 2 + length;

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
			UARTprintf("\nuart_rx_free OVERFLOW!!!\n");
		}

		--uart_rx_packet_count_left;
	}

	MAP_IntEnable(INT_TIMER0A);

	MAP_SysTickIntEnable();

	//HWREG(NVIC_SW_TRIG) |= INT_EMAC0 - 16;
}


void UARTIfInit(uint32_t sys_clock)
{
	aux_buffer_write_index = 0;
	aux_buffer_read_index = 0;
	aux_buffer_free = AUX_BUFFER_SIZE;

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART4);
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);


	// Process timer
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, sys_clock / 40000);
	MAP_IntEnable(INT_TIMER0A);
	MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	MAP_TimerEnable(TIMER0_BASE, TIMER_A);

	MAP_IntPrioritySet(INT_TIMER0A, 0x20);

	// Enable processor interrupts.
	MAP_IntMasterEnable();

	// Set GPIO A4 and A5 as UART pins.
	GPIOPinConfigure(GPIO_PK0_U4RX);
	GPIOPinConfigure(GPIO_PK1_U4TX);
	MAP_GPIOPinTypeUART(GPIO_PORTK_BASE, GPIO_PIN_0 | GPIO_PIN_1);

	//921600
	//460800
	// Configure the UART for 115,200, 8-N-1 operation.
	uint32_t uart_config = (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_TWO | UART_CONFIG_PAR_ODD);
	MAP_UARTConfigSetExpClk(UART4_BASE, sys_clock, 921600, uart_config);

	UARTTxIntModeSet(UART4_BASE, UART_TXINT_MODE_FIFO);

	UARTEnable(UART4_BASE);
	UARTFIFOEnable(UART4_BASE); //Add
	UARTFIFOLevelSet(UART4_BASE, UART_FIFO_TX4_8, UART_FIFO_RX4_8);

	MAP_IntPrioritySet(INT_UART4, 0);

	// Enable the UART interrupt.
	MAP_IntEnable(INT_UART4);
	MAP_UARTIntEnable(UART4_BASE, (UART_INT_RX | UART_INT_RT | UART_INT_TX));
}

void UARTIntHandler4(void)
{
	//if (MAP_IntTrigger())
	if (!IntIsEnabled(INT_UART4))
	{
		UARTprintf("\n\n\n\nLogic error");
		while(true);
	}
	MAP_IntDisable(INT_UART4);
    uint32_t ui32Status;

    // Get the interrrupt status.
    ui32Status = MAP_UARTIntStatus(UART4_BASE, true);

    // Clear the asserted interrupts.
    MAP_UARTIntClear(UART4_BASE, ui32Status);

    uint32_t err = UARTRxErrorGet(UART4_BASE);

    if (err != 0)
    {
    	if (err & UART_RXERROR_OVERRUN)
    	{
    		UARTprintf("UART OVERRUN\n");
    	} else
    	{
    		static uint32_t errs_count = 0;
    		errs_count++;
    	}

    	UARTRxErrorClear(UART4_BASE);
    }

    while(MAP_UARTCharsAvail(UART4_BASE))
	{
		uint8_t c = MAP_UARTCharGetNonBlocking(UART4_BASE);
		aux_put_char(c);
		//MAP_UARTCharPutNonBlocking(UART4_BASE, c);
	}

    while (MAP_UARTSpaceAvail(UART4_BASE) && uart_tx_free < BUFFER_SIZE)
	{
		MAP_UARTCharPutNonBlocking(UART4_BASE, *uart_tx_data_ptr);
		uart_tx_data_ptr = get_next_ptr(&uart_tx_buffer[0], uart_tx_data_ptr);

		if (uart_tx_free > BUFFER_SIZE)
		{
			UARTprintf("\n\nuart_tx_free OVERFLOW!!!\n");
			while(true);
		}
		uart_tx_free++;
	}

    MAP_IntEnable(INT_UART4);

	//ROM_UARTCharPutNonBlocking(UART4_BASE, );
	// Blink the LED to show a character transfer is occuring.
	//GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
	// Delay for 1 millisecond.  Each SysCtlDelay is about 3 clocks.
	//SysCtlDelay(g_ui32SysClock / (1000 * 3));
	// Turn off the LED
   // GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);
}

err_t SendPacket(struct pbuf* packet)
{
	UARTprintf("UART send\n");
	//UARTprintf("Send packet0\n");
	//packet->tot_len + start_packet + packet_len + checksum
	if (uart_tx_free < packet->tot_len + 4 + 2 + 1)
	{
		UARTprintf("-> UART: Not enough memory. Need %d, has %d;\t \n", packet->tot_len, uart_tx_free);
		return ERR_MEM;
	}

	//UARTprintf("-> UART:Packet %d, mem %d\n", packet->tot_len, uart_tx_free);

	// Fill start of the packet
	uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, &packet_start[0], 4);

	// Fill length of the packet
	uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, (uint8_t*)&packet->tot_len, 2);

	uint8_t check_sum = 0;
	struct pbuf* current = packet;
	//UARTprintf("Send packet1\n");
	while(current != NULL)
	{
		uint16_t i = 0;
		for (i = 0; i < current->len; ++i)
		{
			check_sum += ((uint8_t*)current->payload)[i];
		}
		//UARTprintf("\nPayload length: %d\n", current->len);
		uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, current->payload, current->len);
		current = current->next;
	}
	//UARTprintf("Send packet2\n");
	// Fill checksum
	uart_tx_free_ptr = FillBufferTx(&uart_tx_buffer[0], uart_tx_free_ptr, &check_sum, 1);
	//UARTIntClear()Enable(UART4_BASE, UART_INT_TX);
//	MAP_IntEnable(INT_UART4);
//	MAP_UARTIntEnable(UART4_BASE, (UART_INT_TX | UART_INT_RX | UART_INT_RT));
	//UARTIntHandler3();

	HWREG(NVIC_SW_TRIG) |= INT_UART4 - 16;

	//UARTprintf("Send packet end\n");
	return ERR_OK;
}





