#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// #include "fsusb.h"
#include "iSLER.h"

#ifdef CH570_CH572 // this comes from iSLER.h
#define LED PA9
#else
#define LED PA8
#endif
#define PHY_MODE PHY_1M

#define PIN_BUTTON PB8
#define BUTTON_PRESSED !funDigitalRead( PIN_BUTTON )
#define REPORT_ALL 0

#define logf(...) if(debugger){ printf(__VA_ARGS__); }

#define VA_NARGS( ... ) ( sizeof( ( int[] ){ __VA_ARGS__ } ) / sizeof( int ) )

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

__attribute__((aligned(4)))
uint8_t adv[] = {
   ADV_IND,
   0, // size
	BLE_AD_MAC( 0x112233445566 ), // MAC address
	BLE_AD_FLAGS( BLE_FLAGS_CONNECTABLE ),
	BLE_AD_MANUFACTURER( CID_WCH, 'I', ' ', 'l', 'i', 'k', 'e', ' ', 'b', 'l', 'e' ),
	BLE_AD_LOCAL_NAME( 'c', 'h', '3', '2', 'f', 'u', 'n' ),
};
uint8_t adv_channels[] = { 37, 38, 39 };
uint8_t hex_lut[] = "0123456789ABCDEF";
static bool debugger = false;

__attribute__((aligned(4)))
uint8_t scan_resp[] = {
   SCAN_RSP,
   0, // size
0x08, 0xff, 0x5a, 0x48, 0x4a, 0x49, 0x45, 0x4c, 0x49, 0x0d, 0x09, 0x53, 0x69, 0x6e, 0x69, 0x6c, 0x69, 0x6e, 0x6b, 0x2d, 0x41, 0x50, 0x50
};

const char * pdu_toString(uint8_t pdu) {
   switch(pdu) {
      case ADV_IND: return GRN"ADV_IND"RST;
      case ADV_DIRECT_IND: return CYN"ADV_DIRECT_IND"RST;
      case ADV_NONCONN_IND: return GRN"ADV_NONCONN_IND"RST;
      case SCAN_REQ: return MAG"SCAN_REQ"RST;
      case SCAN_RSP: return CYN"SCAN_RSP"RST;
      case CONNECT_REQ: return YEL"CONNECT_REQ"RST;
      case ADV_SCAN_IND: return GRN"ADV_SCAN_IND"RST;
      default: return RED"UNKNOWN"RST;
   }
}

void print_mac(uint8_t *mac) {
   logf( CYN );
   for ( int i = 5; i > 0; i-- )
   {
      logf( "%02x:", mac[i] );
   }
   logf( "%02x", mac[0] );
   logf( RST );
}

void print_scan_req(uint8_t *frame, uint8_t len) {
   logf(RED"SCAN REQ"RST", len %d, ", len);
   logf("src mac: ");
   print_mac(&frame[2]);
   logf(" dest mac: ");
   print_mac(&frame[8]);
   logf("\r\n");
}

void incoming_frame_handler()
{
	uint8_t *frame = (uint8_t *)LLE_BUF;

	const uint8_t pdu = frame[0] & 0x0f;
	const uint8_t len = frame[1];

   // if (pdu != SCAN_REQ) return;

   // 8c:5a:10:62:87:76
   static const uint8_t filter[6] = { 0x76, 0x87, 0x62, 0x10, 0x5a, 0x8c };
   // filter on MAC address
   if (memcmp(&frame[2], filter, sizeof(filter)) == 0) {
      return;
   }

   switch(pdu) {
      case SCAN_REQ:
         print_scan_req(frame, len);
         return;
      default:
         goto pass;
   }


pass:

   print_mac(&frame[2]);
	logf( " PDU:%d (%s) len:%d\r\ndata: ", pdu, pdu_toString(pdu), len );
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

int main()
{
	SystemInit();

	funGpioInitAll();
	funPinMode( LED, GPIO_CFGLR_OUT_2Mhz_PP );
	funPinMode( PIN_BUTTON, GPIO_CFGLR_IN_PUPD ); // Set PIN_BUTTON to input

   debugger = !WaitForDebuggerToAttach(1000);

	if ( BUTTON_PRESSED )
	{
		// jump_isprom();
	}

	// USBFSSetup();

	RFCoreInit( LL_TX_POWER_0_DBM );
	uint8_t frame_info[] = { 0xff, 0x10 }; // PDU, len

	logf( ".~ ch32fun iSLER ~.\r\n" );

   adv[1] = sizeof(adv) - 2;
   logf("Payload len %d\n", adv[1]);

	while ( 1 )
	{
#if SENDER || 0

      logf("Sending :%s\n", pdu_toString(adv[0]));
		for ( int c = 0; c < sizeof( adv_channels ); c++ )
		{
			Frame_TX( adv, adv_channels[c], PHY_MODE );
		}
      adv[0] += 1;
      if (adv[0] >= PD_MAX) {
         adv[0] = 0;
         Delay_Ms(5000);
      }
      Delay_Ms(500);
 #else

		Frame_RX( frame_info, 37, PHY_MODE );
		while ( !rx_ready )
			;

		incoming_frame_handler();
#endif
	}
}

