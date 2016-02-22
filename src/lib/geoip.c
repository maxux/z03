/* z03 - small bot with some network features - geoip feature
* Author: Daniel Maxime (root@maxux.net)
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
* MA 02110-1301, USA.
*/

#include <stdio.h>
#include <string.h>

#ifdef ENABLE_GEOIP
#include "geoip.h"
#include "core.h"
#include "ircmisc.h"
#include "GeoIP.h"
#include "GeoIPCity.h"

char *geoip(const char *host) {
	GeoIP *gip;
	GeoIPRecord *record;
	char buffer[1024];
	const char *region;


	if(!(gip = GeoIP_open(GEOIP_DATABASE, GEOIP_INDEX_CACHE)))
		return strdup("Cannot load database :(");

	if(!(record = GeoIP_record_by_name(gip, host))) {
		GeoIP_delete(gip);
		return strdup("Unknown host");
	}
	
	region = GeoIP_region_name_by_code(record->country_code, record->region);

	zsnprintf(
		buffer,
		"%s: %s, %s (%f, %f)",
		host,
		strcheck(record->country_code),
		strcheck(region),
		record->latitude,
		record->longitude
	);

	GeoIPRecord_delete(record);
	GeoIP_delete(gip);

	return strdup(buffer);
}
#endif
