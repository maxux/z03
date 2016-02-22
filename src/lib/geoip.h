#ifndef __LIB_GEOIP_ZO3_H
	#define __LIB_GEOIPI_ZO3_H
	
	#ifdef ENABLE_GEOIP
	
	#define GEOIP_DATABASE    "/usr/share/GeoIP/GeoLiteCity.dat"
	char *geoip(const char *host);
	
	#endif
#endif
