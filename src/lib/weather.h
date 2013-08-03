#ifndef __LIB_WEATHER_ZO3_H
	#define __LIB_WEATHER_ZO3_H

	typedef struct weather_data_t {
		char location[128];         /* location name            */
		char date[32];              /* date of the last writing */
		double temp;                /* temperature (°C)         */
		double temp_min;            /* temperature min (°C)     */
		char temp_min_time[8];      /* hour of min temp         */
		double temp_max;            /* temperature max (°C)     */
		char temp_max_time[8];      /* hour of max temp         */
		double wind_speed;          /* wind speed (km/h)        */
		float humidity;             /* humidity (%)             */
		
	} weather_data_t;
	
	void weather_error(char *chan, char *message);
	void weather_print(char *chan, weather_data_t *weather);
#endif
