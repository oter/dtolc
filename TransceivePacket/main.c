#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_uart.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/udma.h"
#include "utils/cpu_usage.h"
#include "utils/uartstdio.h"
#include "utils/ustdlib.h"

#include "uart_if.h"




// System clock rate in Hz.
uint32_t g_ui32SysClock;

// The number of SysTick ticks per second used for the SysTick interrupt.
#define SYSTICKS_PER_SECOND     100

// The size of the UART transmit and receive buffers.  They do not need to be
// the same size.
#define UART_TXBUF_SIZE         256

// The transmit and receive buffers used for the UART transfers.  There is one
// transmit buffer and a pair of recieve ping-pong buffers.
static uint8_t g_ui8TxBuf[UART_TXBUF_SIZE];

// The number of seconds elapsed since the start of the program.  This value is
// maintained by the SysTick interrupt handler.
static uint32_t g_ui32Seconds = 0;

// The CPU usage in percent, in 16.16 fixed point format.
static uint32_t g_ui32CPUUsage;

// The control table used by the uDMA controller.  This table must be aligned
// to a 1024 byte boundary.
#if defined(ewarm)
#pragma data_alignment=1024
uint8_t pui8ControlTable[1024];
#elif defined(ccs)
#pragma DATA_ALIGN(pui8ControlTable, 1024)
uint8_t pui8ControlTable[1024];
#else
uint8_t pui8ControlTable[1024] __attribute__ ((aligned(1024)));
#endif

// The error routine that is called if the driver library encounters an error.
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

// The interrupt handler for the SysTick timer.  This handler will increment a
// seconds counter whenever the appropriate number of ticks has occurred.  It
// will also call the CPU usage tick function to find the CPU usage percent.
void SysTickHandler(void)
{
    static uint32_t ui32TickCount = 0;

    ui32TickCount++;

    if(!(ui32TickCount % SYSTICKS_PER_SECOND))
    {
        g_ui32Seconds++;
    }

    g_ui32CPUUsage = CPUUsageTick();
}

// Configure the UART and its pins.  This must be called before UARTprintf().
void ConfigureUART(void)
{
    // Enable the GPIO Peripheral used by the UART.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    // Enable UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);

    // Configure GPIO Pins for UART mode.
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    // Initialize the UART for console I/O.
    UARTStdioConfig(0, 115200, g_ui32SysClock);
}

int main(void)
{
    // Set the clocking to run directly from the crystal at 120MHz.
    g_ui32SysClock = MAP_SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                             SYSCTL_OSC_MAIN |
                                             SYSCTL_USE_PLL |
                                             SYSCTL_CFG_VCO_480), 120000000);

    // Enable peripherals to operate when CPU is in sleep.
    SysCtlPeripheralClockGating(true);

    // Enable the GPIO port that is used for the on-board LED.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPION);

    // Enable the GPIO pins for the LED (PN0).
    GPIOPinTypeGPIOOutput(GPIO_PORTN_BASE, GPIO_PIN_0);

    // Initialize the UART.
    ConfigureUART();

    // Show statistics headings.
    UARTprintf("-----START-----\n");

    // Configure SysTick to occur 100 times per second, to use as a time
    // reference.  Enable SysTick to generate interrupts.
    SysTickPeriodSet(g_ui32SysClock / SYSTICKS_PER_SECOND);
    SysTickIntEnable();
    SysTickEnable();

    // Initialize the CPU usage measurement routine.
    CPUUsageInit(g_ui32SysClock, SYSTICKS_PER_SECOND, 2);

    // Enable the uDMA controller at the system level.  Enable it to continue
    // to run while the processor is in sleep.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UDMA);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UDMA);

    // Enable the uDMA controller error interrupt.  This interrupt will occur
    // if there is a bus error during a transfer.
    IntEnable(INT_UDMAERR);

    // Enable the uDMA controller.
    uDMAEnable();

    // Point at the control table to use for channel control structures.
    uDMAControlBaseSet(pui8ControlTable);

    // Initialize the uDMA UART transfers.
    InitUartInterface(g_ui32SysClock);

    // Loop forever with the CPU not sleeping, so the debugger can connect.
    while(1)
    {
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, GPIO_PIN_0);
        SysCtlDelay(g_ui32SysClock / 20 / 3);
        GPIOPinWrite(GPIO_PORTN_BASE, GPIO_PIN_0, 0);
        SysCtlDelay(g_ui32SysClock / 20 / 3);
    }
}
