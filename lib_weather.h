#ifndef __LIB_WEATHER_ZO3_H
	#define __LIB_WEATHER_ZO3_H
	
	typedef enum weather_station_type_t {
		STATION_METAR,
		STATION_STATION
		
	} weather_station_type_t;
	
	typedef struct weather_station_t {
		int id;
		char *ref;
		char *name;
		enum weather_station_type_t type;
	
	} weather_station_t;

	typedef struct weather_data_t {
		char date[32];              /* date of the last writing */
		double temp;                /* temperature (°C)         */
		double temp_min;            /* temperature min (°C)     */
		char temp_min_time[8];      /* hour of min temp         */
		double temp_max;            /* temperature max (°C)     */
		char temp_max_time[8];      /* hour of max temp         */
		double wind_speed;          /* wind speed (km/h)        */
		int humidity;               /* humidity (%)             */
		
	} weather_data_t;
	
	extern weather_station_t weather_stations[];
	extern unsigned int weather_stations_count;
	
	char * weather_station_list();
	void weather_parse(const char *data, weather_data_t *weather);
	int weather_handle(char *chan, weather_station_t *station);
#endif
