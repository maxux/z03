#ifndef __LIB_SOMAFM_ZO3_H
	#define __LIB_SOMAFM_ZO3_H
	
	typedef struct somafm_station_t {
		char *ref;
		char *name;
	
	} somafm_station_t;
	
	extern somafm_station_t somafm_stations[];
	extern unsigned int somafm_stations_count;
	
	char * somafm_station_list();
	int somafm_handle(char *chan, somafm_station_t *station);
#endif
