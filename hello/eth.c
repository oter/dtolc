#include "eth.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_nvic.h"
#include "driverlib/debug.h"
#include "driverlib/emac.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"

typedef void (* tHardwareTimerHandler)(uint32_t ui32Base,
                                       uint32_t ui32IntStatus);

tHardwareTimerHandler g_pfnTimerHandler;

#define LWIP_DHCP 1
#define LWIP_AUTOIP 1

#define EMAC_PHY_CONFIG (EMAC_PHY_TYPE_INTERNAL | EMAC_PHY_INT_MDIX_EN |      \
                         EMAC_PHY_AN_100B_T_FULL_DUPLEX)

/**
 * If necessary, set the defaui32t number of transmit and receive DMA descriptors
 * used by the Ethernet MAC.
 *
 */
#ifndef NUM_RX_DESCRIPTORS
#define NUM_RX_DESCRIPTORS 4
#endif

#ifndef NUM_TX_DESCRIPTORS
#define NUM_TX_DESCRIPTORS 8
#endif

#define PBUF_POOL_BUFSIZE               512

/* Define those to better describe your network interface. */
#define IFNAME0 't'
#define IFNAME1 'i'

struct pbuf {
  /** next pbuf in singly linked pbuf chain */
  struct pbuf *next;

  /** pointer to the actual data in the buffer */
  void *payload;

  /**
   * total length of this buffer and all next buffers in chain
   * belonging to the same packet.
   *
   * For non-queue packet chains this is the invariant:
   * p->tot_len == p->len + (p->next? p->next->tot_len: 0)
   */
  uint16_t tot_len;

  /** length of this buffer */
  uint16_t len;

  /** pbuf_type as u8_t instead of enum to save space */
  uint8_t /*pbuf_type*/ type;

  /** misc flags */
  uint8_t flags;

  /**
   * the reference count always equals the number of pointers
   * that refer to this pbuf. This can be pointers from an application,
   * the stack itself, or pbuf->next pointers from a chain.
   */
  int16_t ref;

#if LWIP_PTPD
  /* the time at which the packet was received, seconds component */
  u32_t time_s;

  /* the time at which the packet was received, nanoseconds component */
  u32_t time_ns;
#endif /* #if LWIP_PTPD */
};

/**
 * A structure used to keep track of driver state and error counts.
 */
typedef struct {
    uint32_t ui32TXCount;
    uint32_t ui32TXCopyCount;
    uint32_t ui32TXCopyFailCount;
    uint32_t ui32TXNoDescCount;
    uint32_t ui32TXBufQueuedCount;
    uint32_t ui32TXBufFreedCount;
    uint32_t ui32RXBufReadCount;
    uint32_t ui32RXPacketReadCount;
    uint32_t ui32RXPacketErrCount;
    uint32_t ui32RXPacketCBErrCount;
    uint32_t ui32RXNoBufCount;
}
tDriverStats;

tDriverStats g_sDriverStats;

#ifdef DEBUG
/**
 * Note: This rather weird construction where we invoke the macro with the
 * name of the field minus its Hungarian prefix is a workaround for a problem
 * experienced with GCC which does not like concatenating tokens after an
 * operator, specifically '.' or '->', in a macro.
 */
#define DRIVER_STATS_INC(x) do{ g_sDriverStats.ui32##x++; } while(0)
#define DRIVER_STATS_DEC(x) do{ g_sDriverStats.ui32##x--; } while(0)
#define DRIVER_STATS_ADD(x, inc) do{ g_sDriverStats.ui32##x += (inc); } while(0)
#define DRIVER_STATS_SUB(x, dec) do{ g_sDriverStats.ui32##x -= (dec); } while(0)
#else
#define DRIVER_STATS_INC(x)
#define DRIVER_STATS_DEC(x)
#define DRIVER_STATS_ADD(x, inc)
#define DRIVER_STATS_SUB(x, dec)
#endif

/**
 *  Helper struct holding a DMA descriptor and the pbuf it currently refers
 *  to.
 */
typedef struct {
  tEMACDMADescriptor Desc;
  struct pbuf *pBuf;
} tDescriptor;

typedef struct {
    tDescriptor *pDescriptors;
    uint32_t ui32NumDescs;
    uint32_t ui32Read;
    uint32_t ui32Write;
} tDescriptorList;

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
typedef struct {
  struct eth_addr *ethaddr;
  /* Add whatever per-interface state that is needed here. */
  tDescriptorList *pTxDescList;
  tDescriptorList *pRxDescList;
} tStellarisIF;

/**
 * Global variable for this interface's private data.  Needed to allow
 * the interrupt handlers access to this information outside of the
 * context of the lwIP netif.
 *
 */
tDescriptor g_pTxDescriptors[NUM_TX_DESCRIPTORS];
tDescriptor g_pRxDescriptors[NUM_RX_DESCRIPTORS];
tDescriptorList g_TxDescList = {
    g_pTxDescriptors, NUM_TX_DESCRIPTORS, 0, 0
};
tDescriptorList g_RxDescList = {
    g_pRxDescriptors, NUM_RX_DESCRIPTORS, 0, 0
};
static tStellarisIF g_StellarisIFData = {
    0, &g_TxDescList, &g_RxDescList
};


void InitDMADescriptors(void)
{
    uint32_t ui32Loop;

    /* Transmit list -  mark all descriptors as not owned by the hardware */
   for(ui32Loop = 0; ui32Loop < NUM_TX_DESCRIPTORS; ui32Loop++)
   {
       g_pTxDescriptors[ui32Loop].pBuf = (struct pbuf *)0;
       g_pTxDescriptors[ui32Loop].Desc.ui32Count = 0;
       g_pTxDescriptors[ui32Loop].Desc.pvBuffer1 = 0;
       g_pTxDescriptors[ui32Loop].Desc.DES3.pLink =
               ((ui32Loop == (NUM_TX_DESCRIPTORS - 1)) ?
               &g_pTxDescriptors[0].Desc : &g_pTxDescriptors[ui32Loop + 1].Desc);
       g_pTxDescriptors[ui32Loop].Desc.ui32CtrlStatus = DES0_TX_CTRL_INTERRUPT |
               DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_IP_ALL_CKHSUMS;

   }

   g_TxDescList.ui32Read = 0;
   g_TxDescList.ui32Write = 0;


   /* Receive list -  tag each descriptor with a pbuf and set all fields to
    * allow packets to be received.
    */
  for(ui32Loop = 0; ui32Loop < NUM_RX_DESCRIPTORS; ui32Loop++)
  {
      g_pRxDescriptors[ui32Loop].pBuf = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE,
                                                 PBUF_POOL);
      g_pRxDescriptors[ui32Loop].Desc.ui32Count = DES1_RX_CTRL_CHAINED;
      if(g_pRxDescriptors[ui32Loop].pBuf)
      {
          /* Set the DMA to write directly into the pbuf payload. */
          g_pRxDescriptors[ui32Loop].Desc.pvBuffer1 =
                  g_pRxDescriptors[ui32Loop].pBuf->payload;
          g_pRxDescriptors[ui32Loop].Desc.ui32Count |=
             (g_pRxDescriptors[ui32Loop].pBuf->len << DES1_RX_CTRL_BUFF1_SIZE_S);
          g_pRxDescriptors[ui32Loop].Desc.ui32CtrlStatus = DES0_RX_CTRL_OWN;
      }
      else
      {
          //LWIP_DEBUGF(NETIF_DEBUG, ("tivaif_init: pbuf_alloc error\n"));

          /* No pbuf available so leave the buffer pointer empty. */
          g_pRxDescriptors[ui32Loop].Desc.pvBuffer1 = 0;
          g_pRxDescriptors[ui32Loop].Desc.ui32CtrlStatus = 0;
      }
      g_pRxDescriptors[ui32Loop].Desc.DES3.pLink =
              ((ui32Loop == (NUM_RX_DESCRIPTORS - 1)) ?
              &g_pRxDescriptors[0].Desc : &g_pRxDescriptors[ui32Loop + 1].Desc);
  }

  g_TxDescList.ui32Read = 0;
  g_TxDescList.ui32Write = 0;

  //
  // Set the descriptor pointers in the hardware.
  //
  EMACRxDMADescriptorListSet(EMAC0_BASE, &g_pRxDescriptors[0].Desc);
  EMACTxDMADescriptorListSet(EMAC0_BASE, &g_pTxDescriptors[0].Desc);
}


void HardwareInit(void)
{
	uint16_t ui16Val;

	  /* Set MAC hardware address length */
	  //psNetif->hwaddr_len = ETHARP_HWADDR_LEN;

	  /* Set MAC hardware address */
	  //EMACAddrGet(EMAC0_BASE, 0, &(psNetif->hwaddr[0]));

	  /* Maximum transfer unit */
	  //psNetif->mtu = 1500;

	  /* Device capabilities */
	  //psNetif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

	  /* Initialize the DMA descriptors. */
	  InitDMADescriptors();

	  /* Clear any stray PHY interrupts that may be set. */
	  ui16Val = EMACPHYRead(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_MISR1);
	  ui16Val = EMACPHYRead(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_MISR2);

	  /* Configure and enable the link status change interrupt in the PHY. */
	  ui16Val = EMACPHYRead(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_SCR);
	  ui16Val |= (EPHY_SCR_INTEN_EXT | EPHY_SCR_INTOE_EXT);
	  EMACPHYWrite(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_SCR, ui16Val);
	  EMACPHYWrite(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_MISR1, (EPHY_MISR1_LINKSTATEN |
	               EPHY_MISR1_SPEEDEN | EPHY_MISR1_DUPLEXMEN | EPHY_MISR1_ANCEN));

	  /* Read the PHY interrupt status to clear any stray events. */
	  ui16Val = EMACPHYRead(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_MISR1);

	  /**
	   * Set MAC filtering options.  We receive all broadcast and mui32ticast
	   * packets along with those addressed specifically for us.
	   */
	  EMACFrameFilterSet(EMAC0_BASE, (EMAC_FRMFILTER_HASH_AND_PERFECT |
	                     EMAC_FRMFILTER_PASS_MULTICAST));

	#if LWIP_PTPD
	  //
	  // Enable timestamping on all received packets.
	  //
	  // We set the fine clock adjustment mode and configure the subsecond
	  // increment to half the 25MHz PTPD clock.  This will give us maximum control
	  // over the clock rate adjustment and keep the arithmetic easy later.  It
	  // should be possible to synchronize with higher accuracy than this with
	  // appropriate juggling of the subsecond increment count and the addend
	  // register value, though.
	  //
	  EMACTimestampConfigSet(EMAC0_BASE, (EMAC_TS_ALL_RX_FRAMES |
	                         EMAC_TS_DIGITAL_ROLLOVER |
	                         EMAC_TS_PROCESS_IPV4_UDP | EMAC_TS_ALL |
	                         EMAC_TS_PTP_VERSION_1 | EMAC_TS_UPDATE_FINE),
	                         (1000000000 / (25000000 / 2)));
	  EMACTimestampAddendSet(EMAC0_BASE, 0x80000000);
	  EMACTimestampEnable(EMAC0_BASE);
	#endif

	  /* Clear any pending MAC interrupts. */
	  EMACIntClear(EMAC0_BASE, EMACIntStatus(EMAC0_BASE, false));

	  /* Enable the Ethernet MAC transmitter and receiver. */
	  EMACTxEnable(EMAC0_BASE);
	  EMACRxEnable(EMAC0_BASE);

	  /* Enable the Ethernet RX and TX interrupt source. */
	  EMACIntEnable(EMAC0_BASE, (EMAC_INT_RECEIVE | EMAC_INT_TRANSMIT |
	                EMAC_INT_TX_STOPPED | EMAC_INT_RX_NO_BUFFER |
	                EMAC_INT_RX_STOPPED | EMAC_INT_PHY));

	  /* Enable the Ethernet interrupt. */
	  IntEnable(INT_EMAC0);

	  /* Enable all processor interrupts. */
	  IntMasterEnable();

	  /* Tell the PHY to start an auto-negotiation cycle. */
	  EMACPHYWrite(EMAC0_BASE, PHY_PHYS_ADDR, EPHY_BMCR, (EPHY_BMCR_ANEN |
	               EPHY_BMCR_RESTARTAN));
}


void EthInit(uint32_t ui32SysClkHz, const uint8_t *pui8MAC, uint32_t ui32IPAddr,
         uint32_t ui32NetMask, uint32_t ui32GWAddr, uint32_t ui32IPMode)
{
    //
    // Check the parameters.
    //
#if LWIP_DHCP && LWIP_AUTOIP
    ASSERT((ui32IPMode == IPADDR_USE_STATIC) ||
           (ui32IPMode == IPADDR_USE_DHCP) ||
           (ui32IPMode == IPADDR_USE_AUTOIP));
#elif LWIP_DHCP
    ASSERT((ui32IPMode == IPADDR_USE_STATIC) ||
           (ui32IPMode == IPADDR_USE_DHCP));
#elif LWIP_AUTOIP
    ASSERT((ui32IPMode == IPADDR_USE_STATIC) ||
           (ui32IPMode == IPADDR_USE_AUTOIP));
#else
    ASSERT(ui32IPMode == IPADDR_USE_STATIC);
#endif

    // Enable the ethernet peripheral.
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_EMAC0);
    MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_EPHY0);

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
    MAP_EMACInit(EMAC0_BASE, ui32SysClkHz,
                 EMAC_BCONFIG_MIXED_BURST | EMAC_BCONFIG_PRIORITY_FIXED,
                 4, 4, 0);

    // Set MAC configuration options.
    MAP_EMACConfigSet(EMAC0_BASE, (EMAC_CONFIG_FULL_DUPLEX |
                                   EMAC_CONFIG_CHECKSUM_OFFLOAD |
                                   EMAC_CONFIG_7BYTE_PREAMBLE |
                                   EMAC_CONFIG_IF_GAP_96BITS |
                                   EMAC_CONFIG_USE_MACADDR0 |
                                   EMAC_CONFIG_SA_FROM_DESCRIPTOR |
                                   EMAC_CONFIG_BO_LIMIT_1024),
                      (EMAC_MODE_RX_STORE_FORWARD |
                       EMAC_MODE_TX_STORE_FORWARD |
                       EMAC_MODE_TX_THRESHOLD_64_BYTES |
                       EMAC_MODE_RX_THRESHOLD_64_BYTES), 0);

    // Program the hardware with its MAC address (for filtering).
   MAP_EMACAddrSet(EMAC0_BASE, 0, (uint8_t *)pui8MAC);

    // Save the network configuration for later use by the private
    // initialization.
//    g_ui32IPMode = ui32IPMode;
//    g_ui32IPAddr = ui32IPAddr;
//    g_ui32NetMask = ui32NetMask;
//    g_ui32GWAddr = ui32GWAddr;

    // Perform initialization?
   HardwareInit();
}

void SysTickIntHandler(void)
{
    // Call the lwIP timer handler.
    //lwIPTimer(SYSTICKMS);


	HWREG(NVIC_SW_TRIG) |= INT_EMAC0 - 16;
}


void EthTimerCallbackRegister(tHardwareTimerHandler pfnTimerFunc)
{
    // Remember the callback function address passed.
    g_pfnTimerHandler = pfnTimerFunc;
}

void Receive(void)
{
	int a = 5;
	a *= 2;
}

void EthIntHandler(void)
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
        if(g_pfnTimerHandler)
        {
            g_pfnTimerHandler(EMAC0_BASE, ui32TimerStatus);
        }
    }

    // The handling of the interrupt is different based on the use of a RTOS.

    // No RTOS is being used.  If a transmit/receive interrupt was active,
    // run the low-level interrupt handler.
    if(ui32Status)
    {
    	Receive();
        //tivaif_interrupt(&g_sNetIF, ui32Status);
    }

    // Service the lwIP timers.
    //lwIPServiceTimers();
}
