#include "lwipopts.h"

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
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/rom_map.h"
#include "driverlib/uart.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"
#include "httpserver_raw/httpd.h"

#include "lwip/opt.h"
#include "lwip/ip.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/ip_frag.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/icmp.h"
#include "lwip/igmp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/tcp_impl.h"
#include "lwip/snmp.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"
#include "lwip/stats.h"
#include "arch/perf.h"

#include "drivers/pinout.h"
#include "io.h"

#include "netif/tivaif.h"

// Defines for setting up the system clock.
#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)

// Interrupt priority definitions.  The top 3 bits of these values are
// significant with lower values indicating higher priority interrupts.
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

//*****************************************************************************
//
// A set of flags.  The flag bits are defined as follows:
//
//     0 -> An indicator that the animation timer interrupt has occurred.
//
//*****************************************************************************
#define FLAG_TICK            0
static volatile unsigned long g_ulFlags;

// Timeout for DHCP address request (in seconds).
#ifndef DHCP_EXPIRE_TIMER_SECS
#define DHCP_EXPIRE_TIMER_SECS  45
#endif

// The current IP address.
uint32_t g_ui32IPAddress;

// The system clock frequency.  Used by the SD card driver.
uint32_t g_ui32SysClock;

// The error routine that is called if the driver library encounters an error.
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

#define STATE_RX_PACKET 5;
#define STATE_RX_IDLE   10;

#define STATE_TX_PACKET 6;
#define STATE_TX_IDLE   11;


uint8_t uart_rx_buffer[4096];
volatile uint16_t uart_rx_free = 4096;
uint16_t uart_rx_data_left = 0;
uint16_t uart_rx_packet_count_left = 0;
//uint8_t  uart_rx_state = STATE_RX_IDLE;
uint8_t* uart_rx_data_ptr = &uart_rx_buffer[0];
uint8_t* uart_rx_free_ptr = &uart_rx_buffer[0];

uint8_t uart_tx_buffer[4096];
volatile uint16_t uart_tx_free = 4096;
uint16_t uart_tx_data_left = 0;
//uint16_t uart_tx_packet_left = 0;
//uint8_t  uart_tx_state = STATE_TX_IDLE;
uint8_t* uart_tx_data_ptr = &uart_tx_buffer[0];
uint8_t* uart_tx_free_ptr = &uart_tx_buffer[0];


uint8_t* get_next_ptr(uint8_t* base, uint8_t* ptr)
{
	if (ptr >= base + 4095)
	{
		return base;
	}

	return ptr++;
}

uint8_t* get_prev_ptr(uint8_t* base, uint8_t* ptr)
{
	if (ptr >= base + 4095)
	{
		return base;
	}

	return ptr++;
}



// The interrupt handler for the SysTick interrupt.
void SysTickIntHandler(void)
{
	if (uart_tx_data_left)
	{

		--uart_tx_data_left;
		uart_tx_free++;
		ROM_UARTCharPut(UART3_BASE, *uart_tx_data_ptr);
		uart_tx_data_ptr = get_next_ptr(&uart_tx_buffer[0], uart_tx_data_ptr);
		if (uart_tx_free > 4096)
		{
			uart_tx_free = 4096;
			UARTprintf("\n\nuart_tx_free OVERFLOW!!!");
		}
	}

	if (uart_rx_packet_count_left)
	{
		uint16_t length = *((uint16_t*)uart_rx_data_ptr);
		uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);
		uart_rx_data_ptr = get_next_ptr(&uart_rx_buffer[0], uart_rx_data_ptr);

		if (length >= 4096)
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
		data = mem_malloc(length);
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

		UARTprintf("To Ethernet: Sending packet. Length: %d\n", length);

		struct netif* interface = lwIPGetNetworkInterface();
		interface->ip_addr.addr = 0xC0A80022;
		err_t err = tivaif_transmit(interface, packet);
		if (err != ERR_OK)
		{
			UARTprintf("\nTo Ethernet: Cannot send packet. : %d\n", err);
		}
		if (data != NULL)
		{
			mem_free(data);
			data = NULL;
		}

		pbuf_free(packet);

		if (uart_rx_free > 4096)
		{
			uart_rx_free = 4096;
			UARTprintf("\n\nuart_rx_free OVERFLOW!!!");
		}

		--uart_rx_packet_count_left;
	}

    // Call the lwIP timer handler.
    //lwIPTimer(SYSTICKMS);
}

#define LASER_DATA_PACKET 99

typedef struct
{
	uint8_t type;
	uint32_t length;
	void* data;

} laser_packet_t;


// The interrupt handler for the timer used to pace the animation.
void AnimTimerIntHandler(void)
{
    // Clear the timer interrupt.
    MAP_TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    // Indicate that a timer interrupt has occurred.
    HWREGBITW(&g_ulFlags, FLAG_TICK) = 1;
}

// Display an lwIP type IP Address.
void DisplayIPAddress(uint32_t ui32Addr)
{
    char pcBuf[16];

    // Convert the IP Address into a string.
    usprintf(pcBuf, "%d.%d.%d.%d", ui32Addr & 0xff, (ui32Addr >> 8) & 0xff,
            (ui32Addr >> 16) & 0xff, (ui32Addr >> 24) & 0xff);

    UARTprintf(pcBuf);
}

// Required by lwIP library to support any host-related timer functions.
void lwIPHostTimerHandler(void)
{
    uint32_t ui32NewIPAddress;

    // Get the current IP address.
    ui32NewIPAddress = lwIPLocalIPAddrGet();

    // See if the IP address has changed.
    if(ui32NewIPAddress != g_ui32IPAddress)
    {
        // See if there is an IP address assigned.
        if(ui32NewIPAddress == 0xffffffff)
        {
            // Indicate that there is no link.
            UARTprintf("Waiting for link.\n");
        }
        else if(ui32NewIPAddress == 0)
        {
            // There is no IP address, so indicate that the DHCP process is
            // running.
            UARTprintf("Waiting for IP address.\n");
        }
        else
        {
            // Display the new IP address.
            UARTprintf("IP Address: ");
            DisplayIPAddress(ui32NewIPAddress);
            UARTprintf("\n");
            UARTprintf("Open a browser and enter the IP address.\n");
        }

        // Save the new IP address.
        g_ui32IPAddress = ui32NewIPAddress;
    }

    // If there is not an IP address.
    if((ui32NewIPAddress == 0) || (ui32NewIPAddress == 0xffffffff))
    {
       // Do nothing and keep waiting.
    }
}

uint8_t rec_packet = 0;


uint8_t start_seq[4] = {LASER_DATA_PACKET, LASER_DATA_PACKET + 1, LASER_DATA_PACKET + 2, LASER_DATA_PACKET + 3};
uint8_t check_at = 0;
uint16_t packet_length;
uint8_t* start_buffer = NULL;

void UARTIntHandler3(void)
{
    uint32_t ui32Status;

    // Get the interrrupt status.
    ui32Status = ROM_UARTIntStatus(UART3_BASE, true);

    // Clear the asserted interrupts.
    ROM_UARTIntClear(UART3_BASE, ui32Status);

    while(ROM_UARTCharsAvail(UART3_BASE))
    {
		// Try to detect packet start
		if (rec_packet == 0)
		{

			if (ROM_UARTCharGet(UART3_BASE) == start_seq[check_at])
			{
				++check_at;
				if (check_at == 4)
				{
					rec_packet = 1;
					check_at = 0;
					continue;
				}
			} else
			{
				//UARTprintf("\nFrom UART: Invalid packet start.\n");
				check_at = 0;
				continue;
			}

		} else if (rec_packet == 1)
		{
			packet_length = 0;
			((uint8_t*)&packet_length)[0] = ROM_UARTCharGet(UART3_BASE);
			((uint8_t*)&packet_length)[1] = ROM_UARTCharGet(UART3_BASE);
			UARTprintf("From UART: Packet: %d;\t", packet_length);
			if (packet_length >= 4096)
			{
				UARTprintf("\n\nFrom UART: Ignoring invalid huge packet %d\n", packet_length);
				rec_packet = 0;
			}
			//if ()
			rec_packet = 2;
		}
    }





	//ROM_UARTCharPutNonBlocking(UART3_BASE, );

	//
	// Blink the LED to show a character transfer is occuring.
	//
	//GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);

	//
	// Delay for 1 millisecond.  Each SysCtlDelay is about 3 clocks.
	//
	//SysCtlDelay(g_ui32SysClock / (1000 * 3));

	//
	// Turn off the LED
	//
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

err_t callback(struct pbuf *p, struct netif *netif)
{

	UARTprintf("From Ethernet: Packet: %d;\t ", p->tot_len);
	UARTSend3(p);

	pbuf_free(p);

	return ERR_OK;
}

// Enable UART3
void InitUART3(void)
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
	    ROM_UARTConfigSetExpClk(UART3_BASE, g_ui32SysClock, 115200, uart_config);

	    // Enable the UART interrupt.
	    ROM_IntEnable(INT_UART3);
	    ROM_UARTIntEnable(UART3_BASE, UART_INT_RX | UART_INT_RT);
}


int main(void)
{
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MACArray[8];

    set_eth_input_callback(&callback);

    // Make sure the main oscillator is enabled because this is required by the PHY.
    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    // Run from the PLL at 120 MHz.
    uint32_t pll_config = SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480;
    g_ui32SysClock = MAP_SysCtlClockFreqSet(pll_config, 120000000);

    // Configure the device pins.
    PinoutSet(true, false);

    // Configure debug port for internal use.
    UARTStdioConfig(0, 115200, g_ui32SysClock);

    InitUART3();

    // Configure SysTick for a periodic interrupt.
    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKHZ);
    MAP_SysTickEnable();
    MAP_SysTickIntEnable();

    // Configure the hardware MAC address for Ethernet Controller filtering of
    // incoming packets.  The MAC address will be stored in the non-volatile
    // USER0 and USER1 registers.
    MAP_FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
        // Let the user know there is no MAC address
        UARTprintf("No MAC programmed!\n");
        while(true);
    }

    // Convert the 24/24 split MAC address from NV ram into a 32/16 split
    // MAC address needed to program the hardware registers, then program
    // the MAC address into the Ethernet Controller registers.
    pui8MACArray[0] = 0x08;
	pui8MACArray[1] = 0x00;
	pui8MACArray[2] = 0x28;
	pui8MACArray[3] = 0x5A;
	pui8MACArray[4] = 0x8C;
	pui8MACArray[5] = 0x4A;

    // Initialze the lwIP library, using DHCP.
	// 192.168.0.34
    lwIPInit(g_ui32SysClock, pui8MACArray, 0xC0A80022, 0, 0, IPADDR_USE_DHCP);

    // Set the interrupt priorities.
    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    MAP_IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);
    MAP_IntPrioritySet(INT_UART3, SYSTICK_INT_PRIORITY);

    // Initialize IO controls
    io_init();

    while(1)
    {
        // Wait for a new tick to occur.
        while(!g_ulFlags);

        // Clear the flag now that we have seen it.
        HWREGBITW(&g_ulFlags, FLAG_TICK) = 0;

        // Toggle the GPIO
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, (MAP_GPIOPinRead(GPIO_PORTN_BASE, GPIO_PIN_1) ^ GPIO_PIN_1));
    }
}
