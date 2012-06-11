#include <stdio.h>
#include <string.h>
#include <math.h>

int maxof(int *values, unsigned int values_count) {
	int max = 0;
	unsigned int i;
	
	for(i = 0; i < values_count; i++)
		if(values[i] > max)
			max = values[i];
	
	return max;
}

char **ascii_chart(int *values, unsigned int values_count, unsigned int lines_count) {
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
	step = (double) max / lines_count;
	
	for(i = 0; i < values_count; i++)
		values[i] = (values[i] / max) * lines_count;
	
	/* Building chart */
	for(j = 0; j < lines_count; j++) {
		this_line = *(lines + j);
		
		/* Initializing line */
		*this_line = '\0';
		
		/* Writing line chart */
		for(i = 0; i < values_count; i++)
			strcat(this_line, (values[i] > (int) j) ? "# " : "  ");
		
		/* Line value legend */
		sprintf(temp, "| %d", (int) ceil(step * (j + 1)));
		strcat(this_line, temp);
	}
		
	return lines;
}
