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

void InitUART1Transfer(uint32_t sys_clock);

#define LASER_DATA_PACKET 99
const uint8_t packet_start[4] = {LASER_DATA_PACKET, LASER_DATA_PACKET + 2, LASER_DATA_PACKET + 50, LASER_DATA_PACKET - 80};


static uint8_t uart_dma_rx_buf[UART_DMA_RX_BLOCKS_COUNT * UART_DMA_RX_BLOCK_SIZE];

volatile uint16_t uart_dma_read_index = 0;
volatile uint16_t uart_dma_write_index = 0;

// The count of uDMA errors.  This value is incremented by the uDMA error
// handler.
static uint32_t g_ui32uDMAErrCount = 0;

// The count of times the uDMA interrupt occurred but the uDMA transfer was not
// complete.  This should remain 0.
static uint32_t g_ui32BadISR = 0;

uint16_t GetDMABlockIndex(uint16_t raw_index)
{
	return raw_index % UART_DMA_RX_BLOCK_SIZE;
}

uint16_t GetDMAIndexBlockAligned(uint16_t raw_index)
{
	return GetDMABlockIndex(raw_index) * UART_DMA_RX_BLOCK_SIZE;
}

uint16_t DmaRxDataAvailable(uint16_t raw_index)
{
	if (raw_index <= uart_dma_write_index)
	{
		return uart_dma_write_index - raw_index;
	} else
	{
		return UART_DMA_RX_BLOCK_SIZE * UART_DMA_RX_BLOCKS_COUNT - raw_index + uart_dma_write_index;
	}
}

// The interrupt handler for UART1.  This interrupt will occur when a DMA
// transfer is complete using the UART1 uDMA channel.  It will also be
// triggered if the peripheral signals an error.  This interrupt handler will
// switch between receive ping-pong buffers A and B.  It will also restart a TX
// uDMA transfer if the prior transfer is complete.  This will keep the UART
// running continuously (looping TX data back to RX).
void UART1IntHandler(void)
{
    uint32_t ui32Status;
    uint32_t ui32Mode;

    // Read the interrupt status of the UART.
    ui32Status = UARTIntStatus(UART1_BASE, 1);

    // Clear any pending status, even though there should be none since no UART
    // interrupts were enabled.  If UART error interrupts were enabled, then
    // those interrupts could occur here and should be handled.  Since uDMA is
    // used for both the RX and TX, then neither of those interrupts should be
    // enabled.
    UARTIntClear(UART1_BASE, ui32Status);

    // Check the DMA control table to see if the ping-pong "A" transfer is
    // complete.  The "A" transfer uses receive buffer "A", and the primary
    // control structure.
    ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_UART1RX | UDMA_PRI_SELECT);

    // If the primary control structure indicates stop, that means the "A"
    // receive buffer is done.
    if(ui32Mode == UDMA_MODE_STOP)
    {
        // Set up the next transfer for the "A" buffer, using the primary
        // control structure.  When the ongoing receive into the "B" buffer is
        // done, the uDMA controller will switch back to this one.  This
        // example re-uses buffer A, but a more sophisticated application could
        // use a rotating set of buffers to increase the amount of time that
        // the main thread has to process the data in the buffer before it is
        // reused.
    	uart_dma_write_index += UART_DMA_RX_BLOCK_SIZE;
    	if (uart_dma_write_index == UART_DMA_RX_BLOCKS_COUNT * UART_DMA_RX_BLOCK_SIZE)
    	{
    		uart_dma_write_index = 0;
    	}
        uDMAChannelTransferSet(UDMA_CHANNEL_UART1RX | UDMA_PRI_SELECT,
                                   UDMA_MODE_BASIC,
                                   (void *)(UART1_BASE + UART_O_DR),
                                   &uart_dma_rx_buf[uart_dma_write_index], UART_DMA_RX_BLOCK_SIZE);
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
        g_ui32uDMAErrCount++;
    }
}

// The interrupt handler for uDMA interrupts from the memory channel.  This
// interrupt will increment a counter, and then restart another memory
// transfer.
void uDMAIntHandler(void)
{
    uint32_t ui32Mode;

    // Check for the primary control structure to indicate complete.
    ui32Mode = uDMAChannelModeGet(UDMA_CHANNEL_SW);
    if(ui32Mode == UDMA_MODE_STOP)
    {
    }

    // If the channel is not stopped, then something is wrong.
    else
    {
        g_ui32BadISR++;
    }
}

void InitUartInterface(uint32_t sys_clock)
{
	uart_dma_read_index = 0;
	uart_dma_write_index = 0;

	InitUART1Transfer(sys_clock);

	// Process timer
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
	MAP_TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
	MAP_TimerLoadSet(TIMER0_BASE, TIMER_A, sys_clock / 2000);
	MAP_IntEnable(INT_TIMER0A);
	MAP_TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);
	MAP_TimerEnable(TIMER0_BASE, TIMER_A);

	MAP_IntPrioritySet(INT_TIMER0A, 0xC0);
}

// Initializes the UART1 peripheral and sets up the TX and RX uDMA channels.
// The UART is configured for loopback mode so that any data sent on TX will be
// received on RX.  The uDMA channels are configured so that the TX channel
// will copy data from a buffer to the UART TX output.  And the uDMA RX channel
// will receive any incoming data into a pair of buffers in ping-pong mode.
void InitUART1Transfer(uint32_t sys_clock)
{
	// Enable the GPIO Peripheral used by the UART.
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    // Enable the UART peripheral, and configure it to operate even if the CPU
    // is in sleep.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART1);

    // Configure the UART communication parameters.
    UARTConfigSetExpClk(UART1_BASE, sys_clock, 460800,
                            UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                            UART_CONFIG_PAR_NONE);

    GPIOPinConfigure(GPIO_PB0_U1RX);
	GPIOPinConfigure(GPIO_PB1_U1TX);
	GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Set both the TX and RX trigger thresholds to 4.  This will be used by
    // the uDMA controller to signal when more data should be transferred.  The
    // uDMA TX and RX channels will be configured so that it can transfer 4
    // bytes in a burst when the UART is ready to transfer more data.
    UARTFIFOLevelSet(UART1_BASE, UART_FIFO_TX4_8, UART_FIFO_RX4_8);

    // Enable the UART for operation, and enable the uDMA interface for both TX
    // and RX channels.
    UARTEnable(UART1_BASE);
    //UARTDMAEnable(UART1_BASE, UART_DMA_RX | UART_DMA_TX);
    UARTDMAEnable(UART1_BASE, UART_DMA_RX);

    // Put the attributes in a known state for the uDMA UART1RX channel.  These
    // should already be disabled by default.
    uDMAChannelAttributeDisable(UDMA_CHANNEL_UART1RX,
                                    //UDMA_ATTR_ALTSELECT |
									UDMA_ATTR_USEBURST |
                                    UDMA_ATTR_HIGH_PRIORITY |
                                    UDMA_ATTR_REQMASK);

    // Configure the control parameters for the primary control structure for
    // the UART RX channel.  The primary contol structure is used for the "A"
    // part of the ping-pong receive.  The transfer data size is 8 bits, the
    // source address does not increment since it will be reading from a
    // register.  The destination address increment is byte 8-bit bytes.  The
    // arbitration size is set to 4 to match the RX FIFO trigger threshold.
    // The uDMA controller will use a 4 byte burst transfer if possible.  This
    // will be somewhat more effecient that single byte transfers.
    uDMAChannelControlSet(UDMA_CHANNEL_UART1RX | UDMA_PRI_SELECT,
                              UDMA_SIZE_8 | UDMA_SRC_INC_NONE | UDMA_DST_INC_8 |
                              UDMA_ARB_4);

    // Set up the transfer parameters for the UART RX primary control
    // structure.  The mode is set to ping-pong, the transfer source is the
    // UART data register, and the destination is the receive "A" buffer.  The
    // transfer size is set to match the size of the buffer.
    uDMAChannelTransferSet(UDMA_CHANNEL_UART1RX | UDMA_PRI_SELECT,
                               UDMA_MODE_BASIC,
                               (void *)(UART1_BASE + UART_O_DR),
							   &uart_dma_rx_buf[uart_dma_write_index], UART_DMA_RX_BLOCK_SIZE);

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

    // Enable the UART peripheral interrupts.
    IntEnable(INT_UART1);
}

#define STATE_WAITING_START 10
#define STATE_READ_SIZE1 20
#define STATE_READ_SIZE2 25
#define STATE_READ_PACKET 30

uint8_t rec_packet = STATE_WAITING_START;
uint8_t check_at = 0;
uint16_t packet_length;
uint16_t rec_packet_size = 0;
uint8_t rec_packet_checksum = 0;
uint16_t rec_packet_start_index = 0;
uint16_t rec_packet_end_index = 0;
uint16_t current_index = 0;

void ProcessTimerHandler(void)
{
	IntDisable(INT_TIMER0A);
	uint32_t int_status = TimerIntStatus(TIMER0_BASE, false);
	TimerIntClear(TIMER0_BASE, int_status);
	uint16_t i = current_index;
	uint16_t size = DmaRxDataAvailable(current_index);
	for (i = 0; i < size; ++i)
	{
		uint8_t c = uart_dma_rx_buf[current_index];
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
				}
			} else
			{
				//UARTprintf("\nFrom UART: Invalid packet start.\n");
				check_at = 0;
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
			if (packet_length >= 16182)
			{
				UARTprintf("Ignoring invalid huge packet %d\n", packet_length);
				rec_packet = STATE_WAITING_START;
			}
			else
			{
				rec_packet_start_index = current_index;
				rec_packet = STATE_READ_PACKET;
				rec_packet_size = 0;
				rec_packet_checksum = 0;
			}
		}
		else if (rec_packet == STATE_READ_PACKET)
		{
			if (rec_packet_size == packet_length)
			{
				uint8_t checksum = c;
				if (rec_packet_checksum != checksum)
				{
					rec_packet = STATE_WAITING_START;
					UARTprintf("Invalid checksum!\n", rec_packet_size);
				} else
				{
					rec_packet = STATE_WAITING_START;

					UARTprintf("--%d--> \n", rec_packet_size);
				}
			} else
			{
				rec_packet_checksum += c;
				++rec_packet_size;
			}
		}
		++current_index;
		if (current_index == UART_DMA_RX_BLOCKS_COUNT * UART_DMA_RX_BLOCK_SIZE)
		{
			current_index = 0;
		}
		if (rec_packet == STATE_WAITING_START)
		{
			uart_dma_read_index = current_index;
		}
	}

	IntEnable(INT_TIMER0A);
}
