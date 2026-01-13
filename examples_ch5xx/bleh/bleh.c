/*
 * Minimal demo of iSLER with transmit and receive, on configurable PHY (1M, 2M, S2 or S8 if supported by the mcu)
 * It listens for advertisements from other BLE devices, and when one is detected it
 * changes it's own "Complete Local Name" to RX:XX where XX is the first byte of the detected BLE device's MAC.
 * The RX process happens on channel 37 AccessAddress 0x8E89BED6, which is defined in extralibs/iSLER.h.
 * When a new frame is received, the callback "incoming_frame_handler()" is called to process it.
 */
#include "ch32fun.h"


#define __HIGH_CODE

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

#define VA_NARGS( ... ) ( sizeof( (int[]){ __VA_ARGS__ } ) / sizeof( int ) )

// MAC address (6 bytes, reversed)
#define BLE_AD_MAC( mac )                                                                        \
	( mac & 0xFF ), ( ( mac >> 8 ) & 0xFF ), ( ( mac >> 16 ) & 0xFF ), ( ( mac >> 24 ) & 0xFF ), \
		( ( mac >> 32 ) & 0xFF ), ( ( mac >> 40 ) & 0xFF )

// Flags (1-2 bytes)
#define BLE_AD_FLAGS( flags ) 0x02, 0x01, flags

// Tx Power Level (2-3 bytes)
#define BLE_AD_TX_POWER( power ) 0x02, 0x0A, power

// Complete 16-bit Service UUIDs (variable length)
#define BLE_AD_SERVICES_16BIT( ... ) VA_NARGS( __VA_ARGS__ ) * 2 + 1, 0x03, __VA_ARGS__

// Complete Local Name (variable length, max 248 bytes)
#define BLE_AD_LOCAL_NAME( ... ) VA_NARGS( __VA_ARGS__ ) + 1, 0x09, __VA_ARGS__

// Manufacturer Specific Data (variable length)
#define BLE_AD_MANUFACTURER( company_id, ... ) \
	VA_NARGS( __VA_ARGS__ ) + 3, 0xFF, ( company_id & 0xFF ), ( ( company_id >> 8 ) & 0xFF ), __VA_ARGS__

#define CID_WCH 0x07D7

typedef enum
{
	ADV_IND = 0x00,
	ADV_DIRECT_IND = 0x01,
	ADV_NONCONN_IND = 0x02,
	SCAN_REQ = 0x03,
	SCAN_RSP = 0x04,
	CONNECT_REQ = 0x05,
	ADV_SCAN_IND = 0x06,
	AUX_EXT_IND = 0x07,
	AUX_CONNNECT_RSP = 0x08,
	PD_MAX
} PD_t;

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define RST "\x1B[0m"


typedef enum
{
	BLE_FLAGS_LE_LIMITED_DISC = 0x01,
	BLE_FLAGS_LE_GENERAL_DISC = 0x02,
	BLE_FLAGS_BR_EDR_NOT_SUPPORTED = 0x04,
	BLE_FLAGS_SIMULTANEOUS_LE_AND_BR_EDR = 0x08,
	BLE_FLAGS_CONNECTABLE = 0x06,
} AD_Flags_t;

#if 1
__attribute__( ( aligned( 4 ) ) ) uint8_t adv[] = {
	ADV_NONCONN_IND,
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
	BLE_AD_SERVICES_16BIT( 0x1812 ), // HID service
};
#else
__attribute__( ( aligned( 4 ) ) ) uint8_t adv[] = { 0x02, 0x0d, // header for LL: PDU + frame length
	0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // MAC (reversed)
	0x06, 0x09, 'R', 'X', ':', '?', '?' }; // 0x09: "Complete Local Name"
#endif
uint8_t adv_channels[] = { 37, 38, 39 };
uint8_t hex_lut[] = "0123456789ABCDEF";
static bool debugger = false;

__attribute__( ( aligned( 4 ) ) ) uint8_t scan_resp[] = { SCAN_RSP,
	0, // size
	0x08, 0xff, 0x5a, 0x48, 0x4a, 0x49, 0x45, 0x4c, 0x49, 0x0d, 0x09, 0x53, 0x69, 0x6e, 0x69, 0x6c, 0x69, 0x6e, 0x6b,
	0x2d, 0x41, 0x50, 0x50 };

void hexdump( const void *data, int size )
{
	const uint8_t *p = (const uint8_t *)data;
	for ( int i = 0; i < size; i++ )
	{
		if ( ( i & 0x0f ) == 0 ) logf( "\r\n%04x: ", i );
		logf( "%02x ", p[i] );
	}
	logf( "\r\n" );
}

const char *pdu_toString( uint8_t pdu )
{
	switch ( pdu )
	{
		case ADV_IND: return GRN "ADV_IND" RST;
		case ADV_DIRECT_IND: return CYN "ADV_DIRECT_IND" RST;
		case ADV_NONCONN_IND: return GRN "ADV_NONCONN_IND" RST;
		case SCAN_REQ: return MAG "SCAN_REQ" RST;
		case SCAN_RSP: return CYN "SCAN_RSP" RST;
		case CONNECT_REQ: return YEL "CONNECT_REQ" RST;
		case ADV_SCAN_IND: return GRN "ADV_SCAN_IND" RST;
		case AUX_EXT_IND: return BLU "AUX_EXT_IND" RST;
		case AUX_CONNNECT_RSP: return YEL "AUX_CONNNECT_RSP" RST;
		default: return RED "UNKNOWN" RST;
	}
}

void print_mac( uint8_t *mac )
{
	logf( CYN );
	for ( int i = 5; i > 0; i-- )
	{
		logf( "%02x:", mac[i] );
	}
	logf( "%02x", mac[0] );
	logf( RST );
}

void print_scan_req( uint8_t *frame, uint8_t len )
{
	logf( RED "SCAN REQ" RST ", len %d, ", len );
	logf( "src mac: " );
	print_mac( &frame[2] );
	logf( " dest mac: " );
	print_mac( &frame[8] );
	logf( "\r\n" );
}

void print_connect_req( uint8_t *frame, uint8_t len )
{
	logf( YEL "CONNECT REQ" RST ", len %d, ", len );
	logf( "init mac: " );
	print_mac( &frame[2] );
	logf( " adv mac: " );
	print_mac( &frame[8] );
	logf( "\r\n" );
	logf( "AA: 0x%08x\r\n", frame[14] | ( frame[15] << 8 ) | ( frame[16] << 16 ) | ( frame[17] << 24 ) );
	logf( "crcinit: 0x%02x%02x%02x\r\n", frame[20], frame[19], frame[18] );
	logf( "win size: %d, win offset: %d\r\n", frame[21], frame[22] | ( frame[23] << 8 ) );
	logf( "interval: %d, latency: %d, timeout: %d\r\n", frame[24] | ( frame[25] << 8 ), frame[26] | ( frame[27] << 8 ),
		frame[28] | ( frame[29] << 8 ) );
	logf( "chan map: 0x%02x%02x%02x%02x\r\n", frame[30], frame[31], frame[32], frame[33] );
	logf( "hop: %d, sca: %d\r\n", frame[34] & 0x1f, ( frame[34] >> 5 ) & 0x07 );
	logf( "\r\n" );
	hexdump( &frame[0], len + 2 );
}


uint8_t flip( uint8_t val )
{
	uint8_t res = 0;
	for ( int i = 0; i < 8; i++ )
	{
		res |= ( val & 1 );
		res <<= 1;
		val >>= 1;
	}
	return res;
}

void blink( int n )
{
	for ( int i = n - 1; i >= 0; i-- )
	{
		funDigitalWrite( LED, FUN_LOW ); // Turn on LED
		Delay_Ms( 33 );
		funDigitalWrite( LED, FUN_HIGH ); // Turn off LED
		if ( i ) Delay_Ms( 33 );
	}
}

void incoming_frame_handler()
{
	// The chip stores the incoming frame in LLE_BUF, defined in extralibs/iSLER.h
	uint8_t *frame = (uint8_t *)LLE_BUF;
	int rssi = iSLERRSSI();

	const uint8_t pdu = frame[0] & 0xF;
	const uint8_t len = frame[1] & 0b111111;

	const uint8_t raw = flip( frame[0] );
	const uint8_t pduf = raw & 0x0F;
	const uint8_t lenf = flip( frame[1] );

	// 8c:5a:10:62:87:76
	static const uint8_t filter[6] = { 0x76, 0x87, 0x62, 0x10, 0x5a, 0x8c };

	if ( len > 37 ) return;

	switch ( pdu )
	{
		case CONNECT_REQ: print_connect_req( frame, len ); break;
		case SCAN_REQ:
                        break;
			// filter on MAC address
			if ( memcmp( &frame[8], filter, sizeof( filter ) ) == 0 )
			{
				print_scan_req( frame, len );
			}
			break;
		case SCAN_RSP:
			if ( memcmp( &frame[2], filter, sizeof( filter ) ) == 0 )
			{
				logf( MAG "SCAN RSP" RST ", len %d, ", len );
				logf( "src mac: " );
				print_mac( &frame[2] );
				logf( "\r\n" );
				hexdump( &frame[8], len - 6 );
			}
			break;
	}

	return;


pass:

	print_mac( &frame[2] );
	logf( " PDU:%d (%s) len:%d\r\ndata: ", pdu, pdu_toString( pdu ), len );
	for ( int i = 8; i < len + 2; i++ )
	{
		logf( "0x%02x, ", frame[i] );
	}
	logf( "\r\n\r\n" );

#if 0
	// advertise reception of a FindMy frame
	if(REPORT_ALL || (frame[8] == 0x1e && frame[10] == 0x4c)) {
		adv[sizeof(adv) -2] = hex_lut[(frame[7] >> 4)];
		adv[sizeof(adv) -1] = hex_lut[(frame[7] & 0xf)];
		for(int c = 0; c < sizeof(adv_channels); c++) {
			Frame_TX(adv, sizeof(adv), adv_channels[c], PHY_MODE);
		}
	}
#endif
}

#define SYSTICK_ONE_MILLISECOND ( (uint32_t)FUNCONF_SYSTEM_CORE_CLOCK / 1000 )
static volatile uint32_t SysTick_Ms = 0;

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

int main()
{
	SystemInit();

	systick_init();
	funGpioInitAll();
	funPinMode( LED, GPIO_CFGLR_OUT_2Mhz_PP );

	iSLERInit( LL_TX_POWER_0_DBM );

	debugger = !WaitForDebuggerToAttach( 1000 );
	logf( ".~ ch32fun iSLER ~.\n" );

	adv[1] = sizeof( adv ) - 2; // set length

	size_t last_ms = SysTick_Ms;

	while ( 1 )
	{
		if ( SysTick_Ms - last_ms > 1000 )
		{
			last_ms = SysTick_Ms;
			iSLERTX( ACCESS_ADDRESS, adv, sizeof( adv ), adv_channels[0], PHY_MODE );
		}

		// now listen for frames on channel 37 on bed6. When the RF subsystem
		// detects and finalizes one, "rx_ready" in iSLER.h is set true
		iSLERRX( ACCESS_ADDRESS, 37, PHY_MODE );
		while ( !rx_ready );

		incoming_frame_handler();
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
