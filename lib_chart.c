/* z03 - small bot with some network features - draw a ascii chart
 * Author: Daniel Maxime (maxux.unix@gmail.com)
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
#include <math.h>
#include "lib_chart.h"

// SQLite, 0 = Sunday
char *__chart_days[] = {"D ", "L ", "M ", "M ", "J ", "V ", "S "};

int maxof(int *values, unsigned int values_count) {
	int max = 0;
	unsigned int i;
	
	for(i = 0; i < values_count; i++)
		if(values[i] > max)
			max = values[i];
	
	return max;
}

char **ascii_chart(int *values, unsigned int values_count, unsigned int lines_count, char *days) {
	char **lines, *this_line;
	unsigned int i, j;
	double max, step;
	char temp[128];
	
	/* Allocating */
	lines = (char**) malloc(sizeof(char*) * lines_count);
	
	for(i = 0; i < lines_count; i++)
		*(lines + i) = (char*) malloc(sizeof(char) * 512);	// FIXME
	
	/* Reducing scale */
	max  = maxof(values, values_count);
	step = (double) max / (lines_count - 1);
	
	for(i = 0; i < values_count; i++)
		values[i] = (values[i] / max) * (lines_count - 1);
	
	/* Building chart */
	for(j = 1; j < lines_count; j++) {
		this_line = *(lines + j);
		
		/* Initializing line */
		*this_line = '\0';
		
		/* Writing line chart */
		for(i = 0; i < values_count; i++)
			strcat(this_line, (values[i] > (int) j - 1) ? "**" : "  ");
		
		/* Line value legend */
		sprintf(temp, "| %d", (int) ceil(step * j));
		strcat(this_line, temp);
	}
	
	/* Writing days legend */
	this_line = *(lines);
	*this_line = '\0';
	
	for(i = 0; i < values_count; i++)
		strcat(this_line, __chart_days[(int) *(days + i)]);
				
	return lines;
}
