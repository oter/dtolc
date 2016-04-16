#include "eth.h"
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "drivers/pinout.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "driverlib/emac.h"
#include "utils/uartstdio.h"

// Interrupt priority definitions.  The top 3 bits of these values are
// significant with lower values indicating higher priority interrupts.
#define SYSTICK_INT_PRIORITY    0x80
#define ETHERNET_INT_PRIORITY   0xC0

#define SYSTICKHZ               100
#define SYSTICKMS               (1000 / SYSTICKHZ)

#define IPADDR_USE_DHCP         1

// System clock rate in Hz.
uint32_t g_ui32SysClock;

// The error routine that is called if the driver library encounters an error.
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif


void ConfigureUART(void)
{
    // Enable the GPIO Peripheral used by the UART.
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // Enable UART0
    ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    // Configure GPIO Pins for UART mode.
    ROM_GPIOPinConfigure(GPIO_PA0_U0RX);
    ROM_GPIOPinConfigure(GPIO_PA1_U0TX);
    ROM_GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Initialize the UART for console I/O.
    UARTStdioConfig(0, 115200, g_ui32SysClock);
}


int main(void)
{
	uint32_t ui32User0, ui32User1;
	uint8_t pui8MACArray[8];

	SysCtlMOSCConfigSet(SYSCTL_MOSC_HIGHFREQ);
    // Run from the PLL at 120 MHz.
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
            SYSCTL_OSC_MAIN |
            SYSCTL_USE_PLL |
            SYSCTL_CFG_VCO_480), 120000000);;

    // Configure the device pins.
    PinoutSet(true, false);

    // Enable the GPIO pins for the LED D1 (PN1).
    ROM_GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_1);

    // Initialize the UART.
    ConfigureUART();

    UARTprintf("Hello, world!\n");

    MAP_SysTickPeriodSet(g_ui32SysClock / SYSTICKHZ);
    MAP_SysTickEnable();
    MAP_SysTickIntEnable();

    MAP_FlashUserGet(&ui32User0, &ui32User1);
        if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
        {
            //
            // Let the user know there is no MAC address
            //
            UARTprintf("No MAC programmed!\n");

            while(1)
            {
            }
        }

        //
        // Tell the user what we are doing just now.
        //
        UARTprintf("Waiting for IP.\n");

        //
        // Convert the 24/24 split MAC address from NV ram into a 32/16 split
        // MAC address needed to program the hardware registers, then program
        // the MAC address into the Ethernet Controller registers.
        //
        pui8MACArray[0] = ((ui32User0 >>  0) & 0xff);
        pui8MACArray[1] = ((ui32User0 >>  8) & 0xff);
        pui8MACArray[2] = ((ui32User0 >> 16) & 0xff);
        pui8MACArray[3] = ((ui32User1 >>  0) & 0xff);
        pui8MACArray[4] = ((ui32User1 >>  8) & 0xff);
        pui8MACArray[5] = ((ui32User1 >> 16) & 0xff);

    // Configure Ethernet
    EthInit(g_ui32SysClock, pui8MACArray, 0, 0, 0, IPADDR_USE_DHCP);

    //IntPrioritySet(INT_EMAC0, ETHERNET_INT_PRIORITY);
    //IntPrioritySet(FAULT_SYSTICK, SYSTICK_INT_PRIORITY);

   // SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);

    //IntEnable(INT_EMAC0);
    EMACIntEnable(EMAC0_BASE, EMAC_INT_PHY);
    // We are finished.  Hang around flashing D1.
    while(1)
    {
        // Turn on D1.
        LEDWrite(CLP_D1, 1);

        // Delay for a bit.
        SysCtlDelay(g_ui32SysClock / 10 / 3);

        //
        // Turn off D1.
        //
        LEDWrite(CLP_D1, 0);

        //
        // Delay for a bit.
        //
        SysCtlDelay(g_ui32SysClock / 10 / 3);
    }
}
