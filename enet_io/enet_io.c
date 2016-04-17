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
#include "cgifuncs.h"

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


uint8_t uart_rx_buffer[4096];
uint16_t uart_rx_free = 4096;
uint16_t uart_rx_alloc = 0;
uint8_t* uart_rx_data_ptr = &uart_rx_buffer[0];
uint8_t* uart_rx_free_ptr = &uart_rx_buffer[0];

uint8_t uart_tx_buffer[4096];
uint16_t uart_tx_free = 4096;
uint16_t uart_tx_alloc = 0;
uint8_t* uart_tx_data_ptr = &uart_rx_buffer[0];
uint8_t* uart_tx_free_ptr = &uart_rx_buffer[0];



// The interrupt handler for the SysTick interrupt.
void SysTickIntHandler(void)
{
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


void UARTIntHandler3(void)
{
    uint32_t ui32Status;

    // Get the interrrupt status.
    ui32Status = ROM_UARTIntStatus(UART3_BASE, true);

    // Clear the asserted interrupts.
    ROM_UARTIntClear(UART3_BASE, ui32Status);

    if (!ROM_UARTCharsAvail(UART3_BASE))
    {
    	return;
    }
	uint8_t type = ROM_UARTCharGet(UART3_BASE);
	if (type == LASER_DATA_PACKET)
	{
		int iter = 0;
		for (iter = 0; iter < 3; ++iter)
		{
			if (ROM_UARTCharGet(UART3_BASE) != LASER_DATA_PACKET + iter + 1)
			{
				UARTprintf("\nFrom UART: Invalid packet start.\n");
				return;
			}
		}
	} else
	{
		return;
	}

	uint16_t length;
	((uint8_t*)&length)[0] = ROM_UARTCharGet(UART3_BASE);
	((uint8_t*)&length)[1] = ROM_UARTCharGet(UART3_BASE);

	UARTprintf("From UART: Packet: %d;\t", length);


	struct pbuf* packet;
	packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
	if (packet == NULL)
	{
		UARTprintf("\nFrom UART: Cannot allocate pbuf.\n");
		while(true);
	}

	if (length > 8128)
	{
		UARTprintf("\n From UART: Receiving HUGE number of the packets.\n");
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
		data[iter1] = ROM_UARTCharGet(UART3_BASE);
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

	while(to_send != NULL)
	{
		//UARTprintf("To UART: Sending sub-packet.\n");
		uint16_t i;
		for (i = 0; i < to_send->len; ++i)
		{
			ROM_UARTCharPut(UART3_BASE, ((uint8_t*)to_send->payload)[i]);
		}
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

void InitUART3(void)
{

	    // Enable UART3
	    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART3);

	    // Enable processor interrupts.
	    ROM_IntMasterEnable();


	    // Set GPIO A0 and A1 as UART pins.
	    GPIOPinConfigure(GPIO_PA4_U3RX);
	    GPIOPinConfigure(GPIO_PA5_U3TX);
	    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_4 | GPIO_PIN_5);

	    // Configure the UART for 115,200, 8-N-1 operation.
	    ROM_UARTConfigSetExpClk(UART3_BASE, g_ui32SysClock, 115200,
	                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
	                             UART_CONFIG_PAR_NONE));

	    // Enable the UART interrupt.
	    ROM_IntEnable(INT_UART3);
	    ROM_UARTIntEnable(UART3_BASE, UART_INT_RX | UART_INT_RT);
}


int main(void)
{
    uint32_t ui32User0, ui32User1;
    uint8_t pui8MACArray[8];

    set_eth_input_callback(&callback);
    //
    // Make sure the main oscillator is enabled because this is required by
    // the PHY.  The system must have a 25MHz crystal attached to the OSC
    // pins.  The SYSCTL_MOSC_HIGHFREQ parameter is used when the crystal
    // frequency is 10MHz or higher.
    //
    SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);

    // Run from the PLL at 120 MHz.
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);

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

        while(1)
        {
        }
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


    //
    // Setup the device locator service.
    //
    //LocatorInit();
    //LocatorMACAddrSet(pui8MACArray);
    //LocatorAppTitleSet("EK-TM4C1294XL enet_io");

    //
    // Initialize a sample httpd server.
    //
    //httpd_init();

    // Set the interrupt priorities.  We set the SysTick interrupt to a higher
    // priority than the Ethernet interrupt to ensure that the file system
    // tick is processed if SysTick occurs while the Ethernet handler is being
    // processed.  This is very likely since all the TCP/IP and HTTP work is
    // done in the context of the Ethernet interrupt.
    MAP_IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    MAP_IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);
    MAP_IntPrioritySet(INT_UART3, SYSTICK_INT_PRIORITY);

    //
    // Pass our tag information to the HTTP server.
    //
//    http_set_ssi_handler((tSSIHandler)SSIHandler, g_pcConfigSSITags,
//            NUM_CONFIG_SSI_TAGS);

    //
    // Pass our CGI handlers to the HTTP server.
    //
    //http_set_cgi_handlers(g_psConfigCGIURIs, NUM_CONFIG_CGI_URIS);

    //
    // Initialize IO controls
    //
    io_init();

    while(1)
    {
        // Wait for a new tick to occur.
        while(!g_ulFlags)
        {
            // Do nothing.
        }

        // Clear the flag now that we have seen it.
        HWREGBITW(&g_ulFlags, FLAG_TICK) = 0;

        // Toggle the GPIO
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1,
                (MAP_GPIOPinRead(GPIO_PORTN_BASE, GPIO_PIN_1) ^
                 GPIO_PIN_1));
    }
}
