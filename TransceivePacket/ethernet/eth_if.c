#include "eth_if.h"

#include <stdbool.h>
#include <driverlib/sysctl.h>
#include <driverlib/rom_map.h>
#include <driverlib/emac.h>
#include <inc/hw_memmap.h>

#include "tiva_eth_if.h"

#define EMAC_PHY_CONFIG (EMAC_PHY_TYPE_INTERNAL | EMAC_PHY_INT_MDIX_EN |      \
                         EMAC_PHY_AN_100B_T_FULL_DUPLEX)

void InitEthInterface(uint32_t system_clock)
{
	uint8_t mac_address[8];
	mac_address[0] = 0x08;
	mac_address[1] = 0x00;
	mac_address[2] = 0x28;
	mac_address[3] = 0x5A;
	mac_address[4] = 0x8C;
	mac_address[5] = 0x4A;


	// Enable the ethernet peripheral.
	MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EMAC0);
	MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EMAC0);

	// Enable the internal PHY if it's present and we're being
	// asked to use it.
	if((EMAC_PHY_CONFIG & EMAC_PHY_TYPE_MASK) == EMAC_PHY_TYPE_INTERNAL)
	{
		// We've been asked to configure for use with the internal
		// PHY.  Is it present?
		if(MAP_SysCtlPeripheralPresent(SYSCTL_PERIPH_EPHY0))
		{
			// Yes - enable and reset it.
			MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EPHY0);
			MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EPHY0);
		}
		else
		{
			// Internal PHY is not present on this part so hang here.
			while(1)
			{
			}
		}
	}

	// Wait for the MAC to come out of reset.
	while(!MAP_SysCtlPeripheralReady(SYSCTL_PERIPH_EMAC0))
	{
	}

	// Configure for use with whichever PHY the user requires.
	MAP_EMACPHYConfigSet(EMAC0_BASE, EMAC_PHY_CONFIG);

	// Initialize the MAC and set the DMA mode.
	MAP_EMACInit(EMAC0_BASE, system_clock,
				 EMAC_BCONFIG_MIXED_BURST | EMAC_BCONFIG_PRIORITY_FIXED,
				 4, 4, 0);

	// Set MAC configuration options.
	MAP_EMACConfigSet(EMAC0_BASE, (EMAC_CONFIG_FULL_DUPLEX |
								   //EMAC_CONFIG_CHECKSUM_OFFLOAD |
								   EMAC_CONFIG_7BYTE_PREAMBLE |
								   EMAC_CONFIG_IF_GAP_96BITS |
								   //EMAC_CONFIG_USE_MACADDR0 |
								   //EMAC_CONFIG_SA_FROM_DESCRIPTOR |
								   EMAC_CONFIG_JUMBO_ENABLE |
								   EMAC_CONFIG_BO_LIMIT_1024),
					  (EMAC_MODE_RX_STORE_FORWARD |
					   EMAC_MODE_TX_STORE_FORWARD |
					   EMAC_MODE_TX_THRESHOLD_64_BYTES |
					   EMAC_MODE_RX_THRESHOLD_64_BYTES), 0);

	// Program the hardware with its MAC address (for filtering).
	MAP_EMACAddrSet(EMAC0_BASE, 0, (uint8_t *)mac_address);

	// Save the network configuration for later use by the private
	// initialization.
//	g_ui32IPMode = ui32IPMode;
//	g_ui32IPAddr = ui32IPAddr;
//	g_ui32NetMask = ui32NetMask;
//	g_ui32GWAddr = ui32GWAddr;

	// Initialize lwIP.  The remainder of initialization is done immediately if
	// not using a RTOS and it is deferred to the TCP/IP thread's context if
	// using a RTOS.
	//lwIPPrivateInit(0);
	tivaif_init();
}

void EthernetIntHandler(void)
{
	uint32_t ui32Status;
	uint32_t ui32TimerStatus;

	// Read and Clear the interrupt.
	ui32Status = MAP_EMACIntStatus(EMAC0_BASE, true);

	// If the interrupt really came from the Ethernet and not our
	// timer, clear it.
	if(ui32Status)
	{
		MAP_EMACIntClear(EMAC0_BASE, ui32Status);
	}

	// Check to see whether a hardware timer interrupt has been reported.
	if(ui32Status & EMAC_INT_TIMESTAMP)
	{
		// Yes - read and clear the timestamp interrupt status.
		ui32TimerStatus = EMACTimestampIntStatus(EMAC0_BASE);

		// If a timer interrupt handler has been registered, call it.
//		if(g_pfnTimerHandler)
//		{
//			g_pfnTimerHandler(EMAC0_BASE, ui32TimerStatus);
//		}
	}

	if(ui32Status)
	{
		tivaif_interrupt(ui32Status);
	}

	// Service the lwIP timers.
	//lwIPServiceTimers();
}


