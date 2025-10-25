#include "ch32fun.h"
#include <stdio.h>
#include <string.h>

#include "fsusb.h"

extern volatile uint8_t usb_debug;
static volatile uint32_t SysTick_Ms = 0;
int debugger = 0;

int main()
{
	SystemInit();
	RCC->AHBPCENR = RCC_AHBPeriph_SRAM | RCC_AHBPeriph_DMA1;

	funGpioInitAll();
	debugger = !WaitForDebuggerToAttach( 1000 );

	printf( "Starting\n" );
	systick_init();

#if defined( CH32V203F8 )
#warning This package has USB IO on the same pins as SWD. SWD will be disabled after 3s delay
	printf( "disabling SWD in 3s\n" );
	Delay_Ms( 3000 );
	AFIO->PCFR1 |= AFIO_PCFR1_SWJ_CFG_DISABLE;
#endif

	usb_debug = 0;


	USBFSSetup();

	int count = 0;

	while ( 1 )
	{
		printf( "%u\t:loop %d, debugger=%d\n", SysTick_Ms, count++, debugger );
		Delay_Ms( 1000 );
	}
}

void systick_init( void )
{
	// Disable default SysTick behavior
	SysTick->CTLR = 0;

	// Enable the SysTick IRQ
	NVIC_EnableIRQ( SysTicK_IRQn );

	// Set the tick interval to 1ms for normal op
	SysTick->CMP = SysTick->CNT + ( FUNCONF_SYSTEM_CORE_CLOCK / 1000 ) - 1;

	// Start at zero
	SysTick_Ms = 0;

	// Enable SysTick counter, IRQ, HCLK/1
	SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE | SYSTICK_CTLR_STCLK;
}

void SysTick_Handler( void ) __attribute__( ( interrupt ) );
void SysTick_Handler( void )
{
	SysTick->CMP += ( FUNCONF_SYSTEM_CORE_CLOCK / 1000 ) - 1;
	SysTick->SR = 0;
	++SysTick_Ms;
}

int HandleInRequest( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	int ret = 0; // Just NAK
	if ( debugger ) printf( "In EP%d len: %d\n", endp, len );
	switch ( endp )
	{
		case 1:
			// ret = -1; // Just ACK
			break;
		case 3:
			// ret = -1; // ACK, without it RX was stuck in some cases, leaving for now as a reminder
			break;
	}
	return ret;
}

void HandleDataOut( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	if ( debugger ) printf( "Out EP%d len: %d\n", endp, len );
	if ( endp == 0 )
	{
		ctx->USBFS_SetupReqLen = 0; // To ACK
		if ( ctx->USBFS_SetupReqCode == CDC_SET_LINE_CODING )
		{
			if ( debugger ) printf( "CDC_SET_LINE_CODING\n" );
		}
	}
	if ( endp == 2 )
	{
		// USBFS->UEP2_DMA = (uint32_t)( uart_tx_buffer + write_pos );
		// printf("USBFS->UEP2_DMA = %08x\n", USBFS->UEP2_DMA);
	}
}

int HandleSetupCustom( struct _USBState *ctx, int setup_code )
{
	int ret = -1;
	if ( debugger ) printf( "HandleSetupCustom - 0x%02x, len = %d\n", setup_code, ctx->USBFS_SetupReqLen );
	if ( ctx->USBFS_SetupReqType & USB_REQ_TYP_CLASS )
	{
		switch ( setup_code )
		{
			case CDC_SET_LINE_CODING:
			case CDC_SET_LINE_CTLSTE:
			case CDC_SEND_BREAK: ret = ( ctx->USBFS_SetupReqLen ) ? ctx->USBFS_SetupReqLen : -1; break;
			case CDC_GET_LINE_CODING: break;

			default: ret = 0; break;
		}
	}
	else if ( ctx->USBFS_SetupReqType & USB_REQ_TYP_VENDOR )
	{
		/* Manufacturer request */
	}
	else
	{
		ret = 0; // Go to STALL
	}
	return ret;
}

