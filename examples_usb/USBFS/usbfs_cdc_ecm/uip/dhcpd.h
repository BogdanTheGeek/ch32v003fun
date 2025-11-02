#ifndef __DHCPD_H__
#define __DHCPD_H__

int dhcpd_init(void);

void dhcpd_udp_appcall(void);
#define UIP_UDP_APPCALL dhcpd_udp_appcall
		
#endif

