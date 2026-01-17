#pragma once

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
#define BLE_AD_SERVICE_16BIT( uuid ) 3, 0x03, ( uuid & 0xFF ), ( ( uuid >> 8 ) & 0xFF )

// Complete Local Name (variable length, max 248 bytes)
#define BLE_AD_LOCAL_NAME( ... ) VA_NARGS( __VA_ARGS__ ) + 1, 0x09, __VA_ARGS__

// Manufacturer Specific Data (variable length)
#define BLE_AD_MANUFACTURER( company_id, ... ) \
	VA_NARGS( __VA_ARGS__ ) + 3, 0xFF, ( company_id & 0xFF ), ( ( company_id >> 8 ) & 0xFF ), __VA_ARGS__

#define CID_WCH 0x07D7

#ifndef PACKED
#define PACKED __attribute__( ( packed ) )
#endif

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define RST "\x1B[0m"

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
} BLEH_PDU_e;

typedef enum
{
	BLE_FLAGS_LE_LIMITED_DISC = 0x01,
	BLE_FLAGS_LE_GENERAL_DISC = 0x02,
	BLE_FLAGS_BR_EDR_NOT_SUPPORTED = 0x04,
	BLE_FLAGS_SIMULTANEOUS_LE_AND_BR_EDR = 0x08,
	BLE_FLAGS_CONNECTABLE = 0x06,
} BLEH_AD_Flags_e;

typedef struct
{
	uint8_t mac[6]; // Reversed MAC address
} BLEH_MAC_t;

typedef struct
{
	uint8_t pdu_type;
	uint8_t len;
} BLEH_Adv_Header_t;

typedef struct
{
	BLEH_MAC_t initiator;
	BLEH_MAC_t advertiser;
} BLEH_Adv_ScanReq_t;

typedef struct PACKED
{
	BLEH_MAC_t initiator;
	BLEH_MAC_t advertiser;
	uint8_t aa[4];
	uint8_t crcinit[3];
	uint8_t win_size;
	uint16_t win_offset;
	uint16_t interval;
	uint16_t latency;
	uint16_t timeout;
	uint8_t chan_map[5];
	uint8_t hop_sca;
} BLEH_Adv_ConnectReq_t;


uint8_t adv_channels[] = { 37, 38, 39 };
static bool debugger = false;

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

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARGS( mac ) mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]

#define GENERIC_PDU_FMT GRN "PDU" RST ":%d (%s), len %d\r\n"
#define GENERIC_PDU_ARGS( pdu, len ) pdu, pdu_toString( pdu ), len

#define SCAN_REQ_FMT RED "SCAN REQ" RST ", len %d, src mac: " MAC_FMT ", dest mac: " MAC_FMT "\r\n"
#define SCAN_REQ_ARGS( req ) len, MAC_ARGS( req->initiator.mac ), MAC_ARGS( req->advertiser.mac )

#define CONNECT_REQ_FMT                                                               \
	YEL "CONNECT REQ" RST ", len %d, init mac: " MAC_FMT ", adv mac: " MAC_FMT "\r\n" \
		"AA: 0x%08x\r\n"                                                              \
		"crcinit: 0x%02x%02x%02x\r\n"                                                 \
		"win size: %d, win offset: %d\r\n"                                            \
		"interval: %d, latency: %d, timeout: %d\r\n"                                  \
		"chan map: 0x%02x%02x%02x%02x%02x\r\n"                                        \
		"hop: %d, sca: %d\r\n"

#define CONNECT_REQ_ARGS( req )                                                                                      \
	len, MAC_ARGS( req->initiator.mac ), MAC_ARGS( req->advertiser.mac ),                                            \
		req->aa[0] | ( req->aa[1] << 8 ) | ( req->aa[2] << 16 ) | ( req->aa[3] << 24 ), req->crcinit[2],             \
		req->crcinit[1], req->crcinit[0], req->win_size, req->win_offset, req->interval, req->latency, req->timeout, \
		req->chan_map[0], req->chan_map[1], req->chan_map[2], req->chan_map[3], req->chan_map[4],                    \
		bleh_get_hop( req->hop_sca ), bleh_get_sca( req->hop_sca )

#define TO_MAC( data ) ( (BLEH_MAC_t *)( data ) )

#define bleh_get_pdu( frame ) ( frame[0] & 0b00001111 )
#define bleh_get_len( frame ) ( frame[1] & 0b111111 )

#define bleh_get_hop( hop_sca ) ( ( hop_sca >> 5 ) & 0x03 )
#define bleh_get_sca( hop_sca ) ( hop_sca & 0x07 )

#define bleh_for_me( req, my_mac ) ( memcmp( req->advertiser.mac, my_mac, sizeof( BLEH_MAC_t ) ) == 0 )
#define bleh_from( req, my_mac ) ( memcmp( req->initiator.mac, my_mac, sizeof( BLEH_MAC_t ) ) == 0 )


void bleh_print( uint8_t *frame )
{
	const uint8_t pdu = bleh_get_pdu( frame );
	const uint8_t len = bleh_get_len( frame );

	void *data = &frame[2];

	switch ( pdu )
	{
		case SCAN_REQ:
		{
			BLEH_Adv_ScanReq_t *scan_req = data;
			logf( SCAN_REQ_FMT, SCAN_REQ_ARGS( scan_req ) );
		}
		break;
		case CONNECT_REQ:
		{
			BLEH_Adv_ConnectReq_t *connect_req = data;
			logf( CONNECT_REQ_FMT, CONNECT_REQ_ARGS( connect_req ) );
			hexdump( &frame[0], len + 2 );
		}
		break;
		default:
			logf( GENERIC_PDU_FMT, GENERIC_PDU_ARGS( pdu, len ) );
			hexdump( &frame[0], len + 2 );
			break;
	}
}
