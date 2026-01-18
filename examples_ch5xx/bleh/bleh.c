/*
 * Minimal demo of iSLER with transmit and receive, on configurable PHY (1M, 2M, S2 or S8 if supported by the mcu)
 * It listens for advertisements from other BLE devices, and when one is detected it
 * changes it's own "Complete Local Name" to RX:XX where XX is the first byte of the detected BLE device's MAC.
 * The RX process happens on channel 37 AccessAddress 0x8E89BED6, which is defined in extralibs/iSLER.h.
 * When a new frame is received, the callback "incoming_frame_handler()" is called to process it.
 */
#include "ch32fun.h"


#ifndef __HIGH_CODE
#define __HIGH_CODE
#endif

#include "iSLER.h"
#include <stdio.h>


#ifdef CH570_CH572
#define LED PA9
#else
#define LED PA8
#endif
#define PHY_MODE PHY_1M
#define ACCESS_ADDRESS 0x8E89BED6 // the "BED6" address for BLE advertisements

#define logf( ... )            \
	if ( debugger )            \
	{                          \
		printf( __VA_ARGS__ ); \
	}

#include "ble.h"

#define SYSTICK_ONE_MILLISECOND ( (uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000 )
#define US_TO_TICKS( us ) ( ( us ) * ( FUNCONF_SYSTEM_CORE_CLOCK / 1000000 ) )

static void systick_init( void );

static volatile uint32_t SysTick_Ms = 0;

#if 0
__attribute__( ( aligned( 4 ) ) ) uint8_t adv[] = {
	ADV_IND,
	0, // size
	BLE_AD_MAC( 0x112233445566 ), // MAC address
	BLE_AD_FLAGS( BLE_FLAGS_CONNECTABLE ),
	BLE_AD_MANUFACTURER( CID_WCH, 'I', ' ', 'l', 'i', 'k', 'e', ' ', 'b', 'l', 'e' ),
	BLE_AD_LOCAL_NAME( 'c', 'h', '3', '2', 'f', 'u', 'n' ),
};
#elif 1
__attribute__( ( aligned( 4 ) ) ) uint8_t adv[] = {
	ADV_IND,
	0, // size
	BLE_AD_MAC( 0x112233445566 ), // MAC address
	BLE_AD_FLAGS( BLE_FLAGS_CONNECTABLE ),
	BLE_AD_SERVICE_16BIT( 0x1812 ), // HID service
};

__attribute__( ( aligned( 4 ) ) ) uint8_t scan_rsp[] = {
	SCAN_RSP,
	0, // size
	BLE_AD_MAC( 0x112233445566 ), // MAC address
	BLE_AD_MANUFACTURER( CID_WCH, 'I', ' ', 'l', 'i', 'k', 'e', ' ', 'b', 'l', 'e' ),
	BLE_AD_LOCAL_NAME( 'c', 'h', '3', '2', 'f', 'u', 'n' ),
};
#else
__attribute__( ( aligned( 4 ) ) ) uint8_t adv[] = { 0x02, 0x0d, // header for LL: PDU + frame length
	0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // MAC (reversed)
	0x06, 0x09, 'R', 'X', ':', '?', '?' }; // 0x09: "Complete Local Name"
#endif


int incoming_frame_handler( int channel )
{
	int skip = 0;
	// The chip stores the incoming frame in LLE_BUF, defined in extralibs/iSLER.h
	uint8_t *frame = (uint8_t *)LLE_BUF;

	const uint8_t pdu = bleh_get_pdu( frame );
	const uint8_t len = bleh_get_len( frame );
	// const int rssi = iSLERRSSI();
	void *data = &frame[2];

	if ( len > 37 )
	{
		// unsupported length for BLE advertisement
		return skip;
	}

	// 8c:5a:10:62:87:76
	// static const uint8_t filter[6] = { 0x76, 0x87, 0x62, 0x10, 0x5a, 0x8c };

	static const BLEH_MAC_t me = { { BLE_AD_MAC( 0x112233445566 ) } };

	switch ( pdu )
	{
		case SCAN_REQ:
		{
			BLEH_Adv_ScanReq_t *req = data;
			if ( bleh_for_me( req, me.mac ) )
			{
#if 0
            const uint32_t timestamp = SysTick->CNT;
            const uint32_t delta = timestamp - rx_timestamp;
            const uint32_t delta_us = delta / ( FUNCONF_SYSTEM_CORE_CLOCK / 1000000 );
            logf( "SCAN REQ on channel %d, delta: %dus, %d ticks\r\n", channel, (int)delta_us, (int)delta );
#endif

#if 1
				while ( (uint32_t)SysTick->CNT - rx_timestamp < US_TO_TICKS( 150 ) )
				{
					; // wait 150us
				}
#endif
				// respond with a scan response
				isler_tx( scan_rsp, sizeof( scan_rsp ), adv_channels[channel] );
				skip = 1;
			}
			// static const uint8_t filter[6] = { 0xb0, 0xec, 0xa7, 0x55, 0xd4, 0x6b};
			// if ( memcmp( req->initiator.mac, filter, sizeof( BLEH_MAC_t ) ) == 0 )
			// {
			// 	bleh_print( frame );
			// }
		}
		break;
		case CONNECT_REQ:
		{
			BLEH_Adv_ConnectReq_t *req = data;
			if ( bleh_for_me( req, me.mac ) )
			{
				bleh_print( frame );
			}
		}
		break;
	}

	return skip;
}

int main()
{
	SystemInit();

	systick_init();
	funGpioInitAll();

	iSLERInit( LL_TX_POWER_0_DBM );

	isler_config( ACCESS_ADDRESS, 0x555555, PHY_MODE );

	debugger = !WaitForDebuggerToAttach( 1000 );
	logf( ".~ ch32fun bleh ~.\n" );
	logf( "SysCLK: %d MHz\n", FUNCONF_SYSTEM_CORE_CLOCK / 1000000 );

	adv[1] = sizeof( adv ) - 2;
	scan_rsp[1] = sizeof( scan_rsp ) - 2;

	uint32_t last_ticks = SysTick->CNT;

	const uint32_t adv_interval_ticks = US_TO_TICKS( 625 * 160 ); // 100ms
	logf( "Adv interval ticks: %d, %dus, %dms\n", (int)adv_interval_ticks,
		(int)( adv_interval_ticks / ( FUNCONF_SYSTEM_CORE_CLOCK / 1000000 ) ),
		(int)( adv_interval_ticks / ( FUNCONF_SYSTEM_CORE_CLOCK / 1000 ) ) );
	int ch = 0;

	while ( 1 )
	{

		if ( (uint32_t)SysTick->CNT - last_ticks > adv_interval_ticks )
		{
			last_ticks = SysTick->CNT;
			ch = ch == 2 ? 0 : ch + 1;
			isler_tx( adv, sizeof( adv ), adv_channels[ch] );
		}

		isler_rx( adv_channels[ch] );
		while ( !rx_ready );

		const int skip = incoming_frame_handler( ch );
		(void)skip;
	}
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

static void systick_init( void )
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
	                 SYSTICK_CTLR_STCLK; // Set Clock Source to HCLK/1

	// Enable the SysTick IRQ
	NVIC_EnableIRQ( SysTick_IRQn );
}
