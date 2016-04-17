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
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/rom_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "utils/locator.h"
#include "utils/lwiplib.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"


#include "arch/perf.h"

#include "drivers/pinout.h"
#include "io.h"

#include "netif/tivaif.h"

#include "uart_if.h"

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



err_t OnReceivePacket(struct pbuf* packet)
{
	//UARTprintf("To Ethernet: Sending packet. Length: %d\n", packet->tot_len);

	struct netif* interface = lwIPGetNetworkInterface();
	interface->ip_addr.addr = 0xC0A80022;
	//err_t err = tivaif_transmit(interface, packet);
//	if (err != ERR_OK)
//	{
//		UARTprintf("\nTo Ethernet: Cannot send packet. : %d\n", err);
//	}

	return ERR_OK;
}


err_t callback(struct pbuf *p, struct netif *netif)
{

	//UARTprintf("From Ethernet: Packet: %d;\t ", p->tot_len);
	//UARTSend3(p);

	pbuf_free(p);

	return ERR_OK;
}

#define TEST_SIZE 512
uint8_t test_data[TEST_SIZE];

struct pbuf* test_packet = NULL;

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

    UARTIfInit(g_ui32SysClock);

    // Configure SysTick for a periodic interrupt.
    MAP_SysTickPeriodSet(g_ui32SysClock / 100);
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

    test_packet = pbuf_alloc(PBUF_RAW, TEST_SIZE, PBUF_POOL);
    if (test_packet == NULL)
    {
    	UARTprintf("Cannot allocate test data buffer");
    	while(true);
    }

    uint16_t i = 0;
    for (i = 0; i < TEST_SIZE; ++i)
    {
    	test_data[i] = i;
    }

   err_t err = pbuf_take(test_packet, test_data, TEST_SIZE);
   if (err != ERR_OK)
   {
	   UARTprintf("Cannot fill data p_buffer");
	   while(true);
   }


    while(1)
    {
        // Wait for a new tick to occur.
        while(!g_ulFlags);

        SendPacket(test_packet);

        // Clear the flag now that we have seen it.
        HWREGBITW(&g_ulFlags, FLAG_TICK) = 0;

        // Toggle the GPIO
        MAP_GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_1, (MAP_GPIOPinRead(GPIO_PORTN_BASE, GPIO_PIN_1) ^ GPIO_PIN_1));
    }

   pbuf_free(test_packet);
}
