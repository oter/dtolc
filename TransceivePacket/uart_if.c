#include "uart_if.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
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
#include "driverlib/udma.h"
#include "driverlib/uart.h"
#include "utils/locator.h"
//#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"

const uint8_t packet_start[4] = {99, 101, 149, 19};

static uint8_t uart_rx_buf[UART_RX_BLOCKS_COUNT * UART_RX_BLOCK_SIZE];
volatile uint16_t uart_rx_read_index = 0;
volatile uint16_t uart_rx_write_index = 0;

enum ReceiveState
{
	STATE_WAITING_START = 10,
	STATE_READ_SIZE1 = 20,
	STATE_READ_SIZE2 = 25,
	STATE_READ_PACKET = 30
};

enum ReceiveState state = STATE_WAITING_START;
uint8_t check_at = 0;
uint16_t packet_length;
uint16_t rec_packet_size = 0;
uint8_t rec_packet_checksum = 0;
uint16_t rec_packet_start_index = 0;
uint16_t rec_packet_end_index = 0;
uint16_t current_index = 0;

static uint32_t dma_errors_count = 0;
static uint32_t dma_bad_ints = 0;

uint16_t GetDMABlockIndex(uint16_t raw_index)
{
	return raw_index % UART_RX_BLOCK_SIZE;
}

uint16_t DmaRxDataAvailable(uint16_t raw_index)
{
	if (raw_index <= uart_rx_write_index)
	{
		return uart_rx_write_index - raw_index;
	} else
	{
		return UART_RX_BLOCK_SIZE * UART_RX_BLOCKS_COUNT - raw_index + uart_rx_write_index;
	}
}

void UART1IntHandler(void)
{
	const uint32_t dma_rx_primary = UDMA_CHANNEL_UART1RX | UDMA_PRI_SELECT;
    uint32_t int_status;
    uint32_t mode;

    int_status = UARTIntStatus(UART1_BASE, 1);
    UARTIntClear(UART1_BASE, int_status);

    mode = uDMAChannelModeGet(dma_rx_primary);
    // Receive buffer is done.
    if(mode == UDMA_MODE_STOP)
    {
    	// Configure the next receive transfer
    	uart_rx_write_index += UART_RX_BLOCK_SIZE;
    	if (uart_rx_write_index == UART_RX_BLOCKS_COUNT * UART_RX_BLOCK_SIZE)
    	{
    		uart_rx_write_index = 0;
    	}
        uDMAChannelTransferSet(dma_rx_primary, UDMA_MODE_BASIC, (void *)(UART1_BASE + UART_O_DR), &uart_rx_buf[uart_rx_write_index], UART_RX_BLOCK_SIZE);
        uDMAChannelEnable(UDMA_CHANNEL_UART1RX);
    }

    // Check the DMA control table to see if the ping-pong "B" transfer is
    // complete.  The "B" transfer uses receive buffer "B", and the alternate
    // control structure.
    //ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_UART1RX | UDMA_ALT_SELECT);

    // If the UART1 DMA TX channel is disabled, that means the TX DMA transfer
    // is done.
//    if(!uDMAChannelIsEnabled(UDMA_CHANNEL_UART1TX))
//    {
//        // Start another DMA transfer to UART1 TX.
//        uDMAChannelTransferSet(UDMA_CHANNEL_UART1TX | UDMA_PRI_SELECT,
//                                   UDMA_MODE_BASIC, g_ui8TxBuf,
//                                   (void *)(UART1_BASE + UART_O_DR),
//                                   sizeof(g_ui8TxBuf));
//
//        // The uDMA TX channel must be re-enabled.
//        uDMAChannelEnable(UDMA_CHANNEL_UART1TX);
//    }
}

// The interrupt handler for uDMA errors.  This interrupt will occur if the
// uDMA encounters a bus error while trying to perform a transfer.  This
// handler just increments a counter if an error occurs.
void uDMAErrorHandler(void)
{
    uint32_t ui32Status;
    ui32Status = uDMAErrorStatusGet();
    if(ui32Status)
    {
        uDMAErrorStatusClear();
        dma_errors_count++;
    }
}

void uDMAIntHandler(void)
{
    uint32_t mode = uDMAChannelModeGet(UDMA_CHANNEL_SW);
    if(mode == UDMA_MODE_STOP)
    {
    }
    else
    {
        dma_bad_ints++;
    }
}

void InitUartInterface(uint32_t sys_clock)
{
	uart_rx_read_index = 0;
	uart_rx_write_index = 0;

	const uint32_t dma_rx_primary = UDMA_CHANNEL_UART1RX | UDMA_PRI_SELECT;
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
	SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART1);

	//921600
	//460800
	uint32_t uart_config = UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE;
	UARTConfigSetExpClk(UART1_BASE, sys_clock, 921600, uart_config);

	GPIOPinConfigure(GPIO_PB0_U1RX);
	GPIOPinConfigure(GPIO_PB1_U1TX);
	GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	UARTFIFOLevelSet(UART1_BASE, UART_FIFO_TX4_8, UART_FIFO_RX4_8);


	UARTEnable(UART1_BASE);
	//UARTDMAEnable(UART1_BASE, UART_DMA_RX | UART_DMA_TX);
	UARTDMAEnable(UART1_BASE, UART_DMA_RX);

	// Put the attributes in a known state for the uDMA UART1RX channel.  These
	// should already be disabled by default.
	uint32_t dma_config =  /*UDMA_ATTR_ALTSELECT |*/ UDMA_ATTR_USEBURST | UDMA_ATTR_HIGH_PRIORITY | UDMA_ATTR_REQMASK;
	uDMAChannelAttributeDisable(UDMA_CHANNEL_UART1RX, dma_config);
	uint32_t dma_control = UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 | UDMA_ARB_4;
	uDMAChannelControlSet(dma_rx_primary, dma_control);
	uDMAChannelTransferSet(dma_rx_primary, UDMA_MODE_BASIC, (void *)(UART1_BASE + UART_O_DR), &uart_rx_buf[uart_rx_write_index], UART_RX_BLOCK_SIZE);

	// Put the attributes in a known state for the uDMA UART1TX channel.  These
	// should already be disabled by default.
//    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART1TX,
//                                    //UDMA_ATTR_ALTSELECT |
//                                    UDMA_ATTR_HIGH_PRIORITY |
//                                    UDMA_ATTR_REQMASK);

	// Set the USEBURST attribute for the uDMA UART TX channel.  This will
	// force the controller to always use a burst when transferring data from
	// the TX buffer to the UART.  This is somewhat more effecient bus usage
	// than the default which allows single or burst transfers.
	//uDMAChannelAttributeEnable(UDMA_CHANNEL_UART1TX, UDMA_ATTR_USEBURST);

	// Configure the control parameters for the UART TX.  The uDMA UART TX
	// channel is used to transfer a block of data from a buffer to the UART.
	// The data size is 8 bits.  The source address increment is 8-bit bytes
	// since the data is coming from a buffer.  The destination increment is
	// none since the data is to be written to the UART data register.  The
	// arbitration size is set to 4, which matches the UART TX FIFO trigger
	// threshold.
//    uDMAChannelControlSet(UDMA_CHANNEL_UART1TX | UDMA_PRI_SELECT,
//                              UDMA_SIZE_8 | UDMA_SRC_INC_8 |
//                              UDMA_DST_INC_NONE |
//                              UDMA_ARB_4);

	// Set up the transfer parameters for the uDMA UART TX channel.  This will
	// configure the transfer source and destination and the transfer size.
	// Basic mode is used because the peripheral is making the uDMA transfer
	// request.  The source is the TX buffer and the destination is the UART
	// data register.
//    uDMAChannelTransferSet(UDMA_CHANNEL_UART1TX | UDMA_PRI_SELECT,
//                               UDMA_MODE_BASIC, g_ui8TxBuf,
//                               (void *)(UART1_BASE + UART_O_DR),
//                               sizeof(g_ui8TxBuf));

	// Now both the uDMA UART TX and RX channels are primed to start a
	// transfer.  As soon as the channels are enabled, the peripheral will
	// issue a transfer request and the data transfers will begin.
	uDMAChannelEnable(UDMA_CHANNEL_UART1RX);
	//uDMAChannelEnable(UDMA_CHANNEL_UART1TX);

	// Enable the UART DMA TX/RX interrupts.
	//UARTIntEnable(UART1_BASE, UART_INT_DMATX | UART_INT_DMATX);
	UARTIntEnable(UART1_BASE, UART_INT_DMARX);
	IntEnable(INT_UART1);

	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, sys_clock / 2000);
	MAP_IntEnable(INT_TIMER0A);
	MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	MAP_TimerEnable(TIMER0_BASE, TIMER_A);

	MAP_IntPrioritySet(INT_TIMER0A, 0xC0);
}

void ProcessTimerHandler(void)
{
	IntDisable(INT_TIMER0A);
	uint32_t int_status = TimerIntStatus(TIMER0_BASE, false);
	TimerIntClear(TIMER0_BASE, int_status);
	uint16_t i = current_index;
	uint16_t size = DmaRxDataAvailable(current_index);
	for (i = 0; i < size; ++i)
	{
		uint8_t c = uart_rx_buf[current_index];
		// Try to detect packet start
		if (state == STATE_WAITING_START)
		{
			if (c == packet_start[check_at])
			{
				++check_at;
				if (check_at == 4)
				{
					state = STATE_READ_SIZE1;
					check_at = 0;
				}
			} else
			{
				//UARTprintf("\nFrom UART: Invalid packet start.\n");
				check_at = 0;
			}
		}
		else if (state == STATE_READ_SIZE1)
		{
			state = STATE_READ_SIZE2;
			packet_length = 0;
			((uint8_t*)&packet_length)[0] = c;
		}
		else if (state == STATE_READ_SIZE2)
		{
			((uint8_t*)&packet_length)[1] = c;
			UARTprintf("UART: %d; ", packet_length);
			if (packet_length >= 16182)
			{
				UARTprintf("Ignoring invalid huge packet %d\n", packet_length);
				state = STATE_WAITING_START;
			}
			else
			{
				rec_packet_start_index = current_index;
				state = STATE_READ_PACKET;
				rec_packet_size = 0;
				rec_packet_checksum = 0;
			}
		}
		else if (state == STATE_READ_PACKET)
		{
			if (rec_packet_size == packet_length)
			{
				uint8_t checksum = c;
				if (rec_packet_checksum != checksum)
				{
					state = STATE_WAITING_START;
					UARTprintf("Invalid checksum!\n", rec_packet_size);
				} else
				{
					state = STATE_WAITING_START;
					UARTprintf("--%d--> \n", rec_packet_size);
				}
			} else
			{
				rec_packet_checksum += c;
				++rec_packet_size;
			}
		}
		++current_index;
		if (current_index == UART_RX_BLOCKS_COUNT * UART_RX_BLOCK_SIZE)
		{
			current_index = 0;
		}
		if (state == STATE_WAITING_START)
		{
			uart_rx_read_index = current_index;
		}
	}

	IntEnable(INT_TIMER0A);
}
