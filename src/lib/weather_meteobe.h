#ifndef __LIB_WEATHERBE_ZO3_H
	#define __LIB_WEATHERBE_ZO3_H
	
	typedef enum weather_station_type_t {
		STATION_METAR,
		STATION_STATION,
		STATION_MAXUX
		
	} weather_station_type_t;
	
	typedef struct weather_station_t {
		int id;
		char *ref;
		char *name;
		enum weather_station_type_t type;
	
	} weather_station_t;
	
	extern weather_station_t weather_stations[];
	extern unsigned int weather_stations_count;
	extern int weather_default_station;
	
	char *weather_station_list();
	void weather_parse(const char *data, weather_data_t *weather);
	int weather_handle(char *chan, weather_station_t *station);
	int weather_get_station(char *name);
#endif
