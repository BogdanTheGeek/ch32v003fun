#include "ch32fun.h"
#include <stdio.h>
#include <string.h>

#include "fsusb.h"

#define SYSTICK_ONE_MILLISECOND ( (uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000 )

/* CDC ECM Class requests Section 6.2 in CDC ECM spec */
#define SET_ETHERNET_MULTICAST_FILTERS 0x40
#define SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER 0x41
#define GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER 0x42
#define SET_ETHERNET_PACKET_FILTER 0x43
#define GET_ETHERNET_STATISTIC 0x44
/* 45h-4Fh RESERVED (future use) */

#define USB_ECM_NOTIFY_ITF 0x00
#define EP_NOTIFY 0x01
#define EP_RECV 0x02
#define EP_SEND 0x03

#define USB_ACK -1
#define USB_NAK 0

typedef struct __attribute__( ( packed ) )
{
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} usb_request_t;

static __attribute__( ( aligned( 4 ) ) ) usb_request_t notify_nc = {
	.bmRequestType = 0xA1, // 0xA1 | 0x21
	.bRequest = 0 /* NETWORK_CONNECTION */,
	.wValue = 1 /* Connected */,
	.wIndex = USB_ECM_NOTIFY_ITF,
	.wLength = 0,
};

struct
{
	int in[4];
	int out[4];
} usb_stats = { 0 };

extern volatile uint8_t usb_debug;
static volatile uint32_t SysTick_Ms = 0;
int debugger = 0;
static bool volatile send_nc = false;

static void systick_init( void );

int main()
{
	SystemInit();
	RCC->AHBPCENR = RCC_AHBPeriph_SRAM | RCC_AHBPeriph_DMA1;

	printf( "Starting %dMHz\n", FUNCONF_SYSTEM_CORE_CLOCK / 1000000 );
	systick_init();

	funGpioInitAll();
	debugger = !WaitForDebuggerToAttach( 1000 );

	usb_debug = 0;

	USBFSSetup();

	uint32_t last = 0;

	while ( 1 )
	{
		if ( SysTick_Ms - last >= 1000 )
		{
			last = SysTick_Ms;
			printf( "%lds: USB Stats - IN: EP1=%d EP2=%d EP3=%d | OUT: EP1=%d EP2=%d EP3=%d\n", SysTick_Ms,
				usb_stats.in[1], usb_stats.in[2], usb_stats.in[3], usb_stats.out[1], usb_stats.out[2],
				usb_stats.out[3] );
		}

		if ( send_nc )
		{
			(void)USBFS_SendEndpointNEW( EP_NOTIFY, (uint8_t *)&notify_nc, sizeof( notify_nc ), 0 );
			send_nc = false;
		}
	}
}

/*
 * Initialises the SysTick to trigger an IRQ with auto-reload, using HCLK/1 as
 * its clock source
 */
void systick_init( void )
{
	// Reset any pre-existing configuration
	SysTick->CTLR = 0x0000;

	// Set the SysTick Compare Register to trigger in 1 millisecond
	SysTick->CMP = SysTick->CNT + SYSTICK_ONE_MILLISECOND;

	SysTick_Ms = 0x00000000;

	// Set the SysTick Configuration
	// NOTE: By not setting SYSTICK_CTLR_STRE, we maintain compatibility with
	// busywait delay funtions used by ch32v003_fun.
	SysTick->CTLR |= SYSTICK_CTLR_STE | // Enable Counter
	                 SYSTICK_CTLR_STIE | // Enable Interrupts
#if 1
	                 SYSTICK_CTLR_STCLK; // Set Clock Source to HCLK/1
#else
	                 0; // Set Clock Source to HCLK/8
#endif

	// Enable the SysTick IRQ
	NVIC_EnableIRQ( SysTicK_IRQn );
}

/*
 * SysTick ISR - must be lightweight to prevent the CPU from bogging down.
 * Increments Compare Register and systick_millis when triggered (every 1ms)
 * NOTE: the `__attribute__((interrupt))` attribute is very important
 */
void SysTick_Handler( void ) __attribute__( ( interrupt ) );
void SysTick_Handler( void )
{
	// Set the SysTick Compare Register to trigger in 1 millisecond
	SysTick->CMP = SysTick->CNT + SYSTICK_ONE_MILLISECOND;

	// Clear the trigger state for the next IRQ
	SysTick->SR = 0x00000000;

	// Increment the milliseconds count
	SysTick_Ms++;
}

#if 1
int HandleInRequest( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	usb_stats.in[endp]++;
	return USB_NAK;
}
#else
int HandleInRequest( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	int ret = USB_NAK; // Just NAK
	switch ( endp )
	{
		case EP_NOTIFY:
			// ret = USB_ACK; // Just ACK
			break;
		case EP_SEND:
			// ret = USB_ACK; // ACK, without it RX was stuck in some cases, leaving for now as a reminder
			break;
	}
	return ret;
}
#endif

#if 1
void HandleDataOut( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	if ( endp == 0 )
	{
		ctx->USBFS_SetupReqLen = 0; // To ACK
	}
	else
	{
		usb_stats.out[endp]++;
	}
}
#else
void HandleDataOut( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	if ( endp == 0 )
	{
		ctx->USBFS_SetupReqLen = 0; // To ACK
		if ( ctx->USBFS_SetupReqCode == CDC_SET_LINE_CODING )
		{
			if ( debugger ) printf( "CDC_SET_LINE_CODING\n" );
		}
	}
	if ( endp == EP_RECV )
	{
		if ( debugger ) printf( "Out EP%d len: %d\n", endp, len );
		// USBFS->UEP2_DMA = (uint32_t)( uart_tx_buffer + write_pos );
		// printf("USBFS->UEP2_DMA = %08x\n", USBFS->UEP2_DMA);
	}
}
#endif

int HandleSetupCustom( struct _USBState *ctx, int setup_code )
{
	int ret = USB_NAK;
	if ( debugger ) printf( "HandleSetupCustom - 0x%02x, len = %d\n", setup_code, ctx->USBFS_SetupReqLen );
	if ( ctx->USBFS_SetupReqType & USB_REQ_TYP_CLASS )
	{
		switch ( setup_code )
		{
			case SET_ETHERNET_MULTICAST_FILTERS:
			case SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER:
			case GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER:
			case GET_ETHERNET_STATISTIC:
				// Optional
				ret = USB_ACK;
				break;

			case SET_ETHERNET_PACKET_FILTER:
				// This is the only mandatory request to implement
				send_nc = true;
				notify_nc.wIndex = ctx->USBFS_IndexValue;
				ret = USB_ACK;
				break;
		}
	}
	return ret;
}

