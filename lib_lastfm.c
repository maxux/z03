/* z03 - small bot with some network features - irc miscallinious functions (anti highlights, ...)
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
#include <jansson.h>
#include "lib_google.h"
#include "bot.h"
#include "core_init.h"
#include "lib_list.h"
#include "lib_core.h"
#include "lib_urlmanager.h"
#include "lib_ircmisc.h"
#include "lib_lastfm.h"

#define LASTFM_URL "http://ws.audioscrobbler.com/2.0/?method=user.getrecenttracks&api_key=%s&format=json&user=%s"

lastfm_t * lastfm_new() {
	lastfm_t *lastfm;
	
	lastfm = (lastfm_t*) malloc(sizeof(lastfm_t));
	lastfm->type    = UNKNOWN;
	lastfm->date    = NULL;
	lastfm->message = NULL;
	lastfm->total   = 0;
	
	lastfm->artist  = NULL;
	lastfm->title   = NULL;
	lastfm->album   = NULL;
	
	return lastfm;
}

void lastfm_free(lastfm_t *lastfm) {
	free(lastfm->date);
	free(lastfm->message);
	free(lastfm->artist);
	free(lastfm->title);
	free(lastfm->album);
	free(lastfm);
}

lastfm_t * lastfm_error(json_t *node, lastfm_t *lastfm) {
	const char *errstr;
	
	lastfm->type = ERROR;
	
	if((errstr = json_string_value(json_object_get(node, "message"))))
		lastfm->message = strdup(errstr);
	
	return lastfm;
}

lastfm_t * lastfm_build(json_t *track, lastfm_t *lastfm) {
	json_t *node;
	
	/* reading artist data */
	node = json_object_get(track, "artist");
	if(json_is_object(node)) {
		lastfm->artist = strdup(json_string_value(json_object_get(node, "#text")));
		printf("[+] lastfm: artist: <%s>\n", lastfm->artist);
		
	} else fprintf(stderr, "[-] lastfm: artist object not found\n");
	
	/* reading album data */
	node = json_object_get(track, "album");
	if(json_is_object(node)) {
		lastfm->album = strdup(json_string_value(json_object_get(node, "#text")));
		
		/* album is empty */
		if(!strlen(lastfm->album)) {
			free(lastfm->album);
			lastfm->album = strdup("album not found");
		}
		
		printf("[+] lastfm: album: <%s>\n", lastfm->album);
		
	} else fprintf(stderr, "[-] lastfm: album object not found\n");	
	
	/* reading (or not) date data */
	node = json_object_get(track, "date");
	if(json_is_object(node)) {
		lastfm->date = strdup(json_string_value(json_object_get(node, "#text")));
		printf("[+] lastfm: date: <%s>\n", lastfm->date);
		
	} else fprintf(stderr, "[-] lastfm: no date found\n");	
	
	lastfm->title = strdup(json_string_value(json_object_get(track, "name")));
	printf("[+] lastfm: title: <%s>\n", lastfm->title);
	
	return lastfm;
}

lastfm_t * lastfm_tracks(json_t *node, lastfm_t *lastfm) {
	json_t *tracks, *firsttrack, *attr;
	const char *nowplaying;
	
	/* loading tracks */
	tracks  = json_object_get(node, "track");
	if(!json_is_array(tracks)) {
		fprintf(stderr, "[-] lastfm: tracks is not an array\n");
		lastfm->type    = ERROR;
		lastfm->message = strdup("no recents tracks found");
		return lastfm;
	}
	
	/* counting tracks */
	if(json_array_size(tracks) < 1) {
		fprintf(stderr, "[-] lastfm/json: nothing on track array\n");
		return lastfm;
	}
	
	firsttrack = json_array_get(tracks, 0);	
	
	attr = json_object_get(firsttrack, "@attr");
	if(json_is_object(attr)) {
		nowplaying = json_string_value(json_object_get(attr, "nowplaying"));
		
		if(!strcmp(nowplaying, "true")) {
			printf("[+] lastfm: track is a nowplaying track\n");
			lastfm->type = NOW_PLAYING;	
			
		} else printf("[+] lastfm: track seems to be a last played track\n");
		
	} else lastfm->type = LAST_PLAYED;
	
	lastfm_build(firsttrack, lastfm);
	
	return lastfm;
}

lastfm_t * lastfm_getplaying(char *user) {
	curl_data_t curl;
	lastfm_t *lastfm;
	char url[512];
	json_t *root, *recents, *err;
	json_error_t error;
	
	lastfm = lastfm_new();
	
	snprintf(url, sizeof(url), LASTFM_URL, LASTFM_APIKEY, user);	
	if(curl_download(url, &curl, 1) || !curl.length)
		return lastfm;
	
	printf("[ ] lastfm: json dump: <%s>\n", curl.data);
	
	/* loading json */
	if(!(root = json_loadb(curl.data, curl.length, 0, &error))) {
		fprintf(stderr, "[-] lastfm/json: parsing error\n");
		return lastfm;
	}
	
	if(!json_is_object(root)) {
		fprintf(stderr, "[-] lastfm/json: root not an object\n");
		goto clear;
	}
	
	/* Checking for error */
	err = json_object_get(root, "error");
	if(json_is_integer(err)) {
		fprintf(stderr, "[-] lastfm: error %.2f, parsing it\n", json_real_value(err));
		lastfm_error(root, lastfm);
		goto clear;
	}
	
	/* Loading tracks list */
	recents = json_object_get(root, "recenttracks");
	if(!json_is_object(recents)) {
		fprintf(stderr, "[-] lastfm: recenttracks not found\n");
		goto clear;
	}
	
	lastfm_tracks(recents, lastfm);
	
	/* cleaning */
	clear:
		json_decref(root);	
		free(curl.data);
	
	return lastfm;
}
