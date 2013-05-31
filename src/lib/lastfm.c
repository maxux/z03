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
#include "google.h"
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "database.h"
#include "core.h"
#include "urlmanager.h"
#include "ircmisc.h"
#include "lastfm.h"

/*
 * global work:
 *  - lastfm_t contains all needed data
 *  - ask a request with lastfm_request_t, return a request_t
 *    with error or reply filled with appropriate value
 *  - when the request is completed, the request->reply is
 *    filled with asked data
 *  - lastfm->field *must* be malloc, request_new/free free
 *    all fields
 */

/* lastfm root element */
lastfm_t *lastfm_new(char *apikey, char *apisecret) {
	lastfm_t *lastfm;
	
	lastfm = (lastfm_t *) calloc(sizeof(lastfm_t), 1);
	lastfm->apikey    = strdup(apikey);
	lastfm->apisecret = strdup(apisecret);
	
	lastfm->track        = (lastfm_track_t *) calloc(sizeof(lastfm_track_t), 1);
	lastfm->track->type  = UNKNOWN;
	
	return lastfm;
}

void lastfm_free(lastfm_t *lastfm) {
	// free root elements
	free(lastfm->apikey);
	free(lastfm->apisecret);
	free(lastfm->token);
	free(lastfm->session);
	
	// free track
	free(lastfm->track->user);
	free(lastfm->track->artist);
	free(lastfm->track->title);
	free(lastfm->track->album);
	
	// free structs
	free(lastfm->track);
	free(lastfm);
}

/* lastfm request */
lastfm_request_t *lastfm_request_new() {
	lastfm_request_t *request;
		
	request = (lastfm_request_t *) calloc(sizeof(lastfm_request_t), 1);
	
	return request;
}

void lastfm_request_free(lastfm_request_t *request) {
	// free nodes
	free(request->reply);
	free(request->error);
	free(request);
}

char *lastfm_ampersand(char *original) {
	char *temp = strdup(original);
	char *work, *save;
	
	// FIXME: poor method
	original = (char *) malloc(sizeof(char) * strlen(original) * 4);
	
	save = temp;
	work = original;
	
	while(*temp) {
		if(*temp == '&') {
			strcpy(work, "%26");
			work += 3;
			
		} else *work++ = *temp;
		
		temp++;
	}
	
	*work = '\0';
	
	// freeing previous string
	free(save);
	
	return original;
}

/* lastfm error setter and cleaner */
static json_t *lastfm_json_load(char *json, size_t length, lastfm_request_t *request) {
	json_t *root;
	json_error_t error;
	
	if(!(root = json_loadb(json, length, 0, &error))) {
		fprintf(stderr, "[-] lastfm/json: parsing error: line %d: %s\n", error.line, error.text);
		
		request->error = strdup("json parsing error");
		
		return NULL;
	}
	
	if(!json_is_object(root)) {
		request->error = strdup("json: root is not an object");
		return NULL;
	}
	
	return root;
}

static void *lastfm_abort_request(json_t *root, curl_data_t *curl, lastfm_request_t *request) {
	if(curl)
		curl_data_free(curl);
		
	json_decref(root);
	return request;
}

static char *latsfm_json_checkerror(json_t *root) {
	json_t *error;
	
	/* Checking for error */
	error = json_object_get(root, "error");
	if(json_is_integer(error)) {
		fprintf(stderr, "[-] lastfm: error detected\n");
		
		if((json_string_value(json_object_get(root, "message"))))
			return strdup(json_string_value(json_object_get(root, "message")));
	}
	
	return NULL;
}

/* downloader wrapper */
static int lastfm_curl_download_text(char *url, curl_data_t *data, lastfm_request_t *request) {
	if(curl_download_text(url, data)) {
		request->error = strdup("cannot download request content");
		return 1;
	}
	
	return 0;
}

/*
 * CurrentTrack
 */
static lastfm_track_t *lastfm_build(json_t *track, lastfm_track_t *lastfm_track) {
	json_t *node;
	const char *temp;
	
	/* reading artist data */
	node = json_object_get(track, "artist");
	if(json_is_object(node)) {
		lastfm_track->artist = strdup(json_string_value(json_object_get(node, "#text")));
		printf("[+] lastfm: artist: <%s>\n", lastfm_track->artist);
		
	} else fprintf(stderr, "[-] lastfm: artist object not found\n");
	
	/* reading album data */
	node = json_object_get(track, "album");
	if(json_is_object(node)) {
		lastfm_track->album = strdup(json_string_value(json_object_get(node, "#text")));
		
		/* album is empty */
		if(!strlen(lastfm_track->album)) {
			free(lastfm_track->album);
			lastfm_track->album = strdup("album not found");
		}
		
		printf("[+] lastfm: album: <%s>\n", lastfm_track->album);
		
	} else fprintf(stderr, "[-] lastfm: album object not found\n");	
	
	/* reading (or not) date data */
	node = json_object_get(track, "date");
	if(json_is_object(node)) {
		temp = json_string_value(json_object_get(node, "uts"));
		lastfm_track->date = atoi(temp);
		printf("[+] lastfm: date: <%ld>\n", lastfm_track->date);
		
	} else fprintf(stderr, "[-] lastfm: no date found\n");	
	
	lastfm_track->title = strdup(json_string_value(json_object_get(track, "name")));
	printf("[+] lastfm: title: <%s>\n", lastfm_track->title);
	
	return lastfm_track;
}

static lastfm_request_t *lastfm_recenttracks(json_t *node, lastfm_request_t *request, lastfm_track_t *track) {
	json_t *tracks, *firsttrack, *attr;
	const char *nowplaying;
	
	/* loading tracks */
	tracks  = json_object_get(node, "track");
	if(!json_is_array(tracks)) {
		request->error = strdup("no recents tracks found (array not found)");
		return NULL;
	}
	
	/* counting tracks */
	if(json_array_size(tracks) < 1) {
		request->error = strdup("no recents tracks found (array empty)");
		return NULL;
	}
	
	firsttrack = json_array_get(tracks, 0);	
	
	attr = json_object_get(firsttrack, "@attr");
	if(json_is_object(attr)) {
		nowplaying = json_string_value(json_object_get(attr, "nowplaying"));
		
		if(!strcmp(nowplaying, "true")) {
			printf("[+] lastfm: track is a nowplaying track\n");
			track->type = NOW_PLAYING;	
			
		} else printf("[+] lastfm: track seems to be a last played track\n");
		
	} else track->type = LAST_PLAYED;
	
	lastfm_build(firsttrack, track);
	
	return request;
}

lastfm_request_t *lastfm_getplaying(lastfm_t *lastfm, lastfm_request_t *request, char *user) {
	curl_data_t *curl;
	char url[512];
	json_t *root, *recents;
	
	curl = curl_data_new();
	
	lastfm->track->user = strdup(user);
	lastfm_url(url, "&method=user.getrecenttracks&api_key=%s&user=%s", lastfm->apikey, user);
	
	if(lastfm_curl_download_text(url, curl, request))
		return request;
	
	printf("[ ] lastfm: json dump: <%s>\n", curl->data);
	
	/* loading json */
	if(!(root = lastfm_json_load(curl->data, curl->length, request)))
		return lastfm_abort_request(root, curl, request);
	
	if((request->error = latsfm_json_checkerror(root)))
		return lastfm_abort_request(root, curl, request);
	
	/* Loading tracks list */
	recents = json_object_get(root, "recenttracks");
	if(!json_is_object(recents)) {
		request->error = strdup("json: recenttracks not found");
		return lastfm_abort_request(root, curl, request);
	}
	
	if(lastfm_recenttracks(recents, request, lastfm->track))
		request->reply = strdup("Success");
	
	curl_data_free(curl);
	
	return request;
}


/*
 * Love Track
 */

/*
 * api note:
 *  1) get Token
 *  2) authorize client/application (http)
 *  3) get Session
 *  4) have Fun
*/
/* token */
// sign the auth.getToken
static char *lastfm_sig_gettoken(lastfm_t *lastfm) {
	char *str = (char *) malloc(sizeof(char) * MALLOC_DEFAULT);
	
	snprintf(str, MALLOC_DEFAULT, "api_key%smethodauth.getToken%s", lastfm->apikey, lastfm->apisecret);
	return str;
}

// get a token
lastfm_request_t *lastfm_api_gettoken(lastfm_t *lastfm, lastfm_request_t *request) {
	char url[512], *sig;
	curl_data_t *curl;
	json_t *root;
	
	sig = lastfm_sig_gettoken(lastfm);
	lastfm_url(url, "&method=auth.getToken&api_key=%s&api_sig=%s", lastfm->apikey, sig);
	free(sig);
	
	curl = curl_data_new();
	
	if(curl_download_text(url, curl))
		return request;
	
	printf("[ ] lastfm: json dump: <%s>\n", curl->data);
	
	if(!(root = lastfm_json_load(curl->data, curl->length, request)))
		return lastfm_abort_request(root, curl, request);
	
	if((request->error = latsfm_json_checkerror(root)))
		return lastfm_abort_request(root, curl, request);
	
	if(json_string_value(json_object_get(root, "token")))
		request->reply = strdup(json_string_value(json_object_get(root, "token")));
	
	curl_data_free(curl);
	
	return request;
}

// return url for client/application grant
char *lastfm_api_authorize(lastfm_t *lastfm) {
	char *str = (char *) malloc(sizeof(char) * MALLOC_DEFAULT);
	
	snprintf(str, MALLOC_DEFAULT, "http://www.last.fm/api/auth/?api_key=%s&token=%s", lastfm->apikey, lastfm->token);
	return str;
}

/* session */
// sign the auth.getSession
static char *lastfm_sig_getsession(lastfm_t *lastfm) {
	char str[MALLOC_DEFAULT], *md5;
	
	snprintf(str, MALLOC_DEFAULT, "api_key%smethodauth.getSessiontoken%s%s", lastfm->apikey, lastfm->token, lastfm->apisecret);
	md5 = md5ascii(str);
	
	return md5;
}

// get a session key
lastfm_request_t *lastfm_api_getsession(lastfm_t *lastfm, lastfm_request_t *request) {
	char url[512], *sig;
	curl_data_t *curl;
	json_t *root, *session;
	
	sig = lastfm_sig_getsession(lastfm);
	lastfm_url(url, "&method=auth.getSession&api_key=%s&token=%s&api_sig=%s", lastfm->apikey, lastfm->token, sig);
	free(sig);
	
	curl = curl_data_new();
	
	if(curl_download_text(url, curl))
		return request;
	
	printf("[ ] lastfm: json dump: <%s>\n", curl->data);
	
	if(!(root = lastfm_json_load(curl->data, curl->length, request)))
		return lastfm_abort_request(root, curl, request);
	
	if((request->error = latsfm_json_checkerror(root)))
		return lastfm_abort_request(root, curl, request);
	
	session = json_object_get(root, "session");
	if(!json_is_object(session)) {
		request->error = strdup("json: session type mismatch");
		return lastfm_abort_request(root, curl, request);
	}
	
	if(json_string_value(json_object_get(session, "key")))
		request->reply = strdup(json_string_value(json_object_get(session, "key")));
	
	curl_data_free(curl);
	
	return request;
}

/* love track */
// sign the track.love
static char *lastfm_sig_love(lastfm_t *lastfm) {
	char str[MALLOC_DEFAULT];
	
	snprintf(str, MALLOC_DEFAULT, "api_key%sartist%smethodtrack.lovesk%strack%s%s",
	                              lastfm->apikey, lastfm->track->artist, lastfm->session,
	                              lastfm->track->title, lastfm->apisecret);
	
	printf("[ ] lasfm/sig: <%s>\n", str);
	
	return md5ascii(str);
}

// request love track
lastfm_request_t *lastfm_api_love(lastfm_t *lastfm, lastfm_request_t *request) {
	char post[512], *sig;
	curl_data_t *curl;
	json_t *root;
	char *artist, *title;
	
	curl = curl_data_new();
	
	// signature with original names
	sig = lastfm_sig_love(lastfm);
	
	// fix artist and title name (ampersand on request)
	artist = lastfm_ampersand(lastfm->track->artist);
	title  = lastfm_ampersand(lastfm->track->title);
	
	// building request	
	snprintf(post, sizeof(post), "format=json&method=track.love&api_key=%s&sk=%s&api_sig=%s&artist=%s&track=%s",
                                     lastfm->apikey, lastfm->session, sig, artist, title);
	free(sig);
	free(artist);
	free(title);
	
	if(curl_download_text_post(LASTFM_API_BASE, curl, space_encode(post)))
		return request;
	
	printf("[ ] lastfm: json dump: <%s>\n", curl->data);
	
	if(!(root = lastfm_json_load(curl->data, curl->length, request)))
		return lastfm_abort_request(root, curl, request);
	
	if((request->error = latsfm_json_checkerror(root)))
		return lastfm_abort_request(root, curl, request);
	
	if(json_string_value(json_object_get(root, "status")))
		request->reply = strdup(json_string_value(json_object_get(root, "status")));
	
	curl_data_free(curl);
	
	return request;
}

/*
 * Tracks backlog
 */
list_t *lastfm_backlog(lastfm_t *lastfm) {
	list_t *list;
	sqlite3_stmt *stmt;
	char *sqlquery;
	
	if(!lastfm->track->user || !lastfm->track->artist || !lastfm->track->title) {
		fprintf(stderr, "[-] lastfm/backlog: user, artist or title not set\n");
		return NULL;
	}
	
	list = list_init(NULL);

	/* insert current track */
	sqlquery = sqlite3_mprintf(
		"INSERT INTO lastfm_logs (lastfm_nick, artist, title) VALUES "
		"('%q', '%q', '%q')",
		lastfm->track->user, lastfm->track->artist, lastfm->track->title
	);
	
	if(!db_sqlite_simple_query(sqlite_db, sqlquery))
		fprintf(stderr, "[-] lastfm/backlog: insertion error\n");
	
	sqlite3_free(sqlquery);
	
	/* select backlog */
	sqlquery = sqlite3_mprintf(
		"SELECT s.nick FROM lastfm_logs l, settings s "
		"WHERE s.key = 'lastfm' "
		"  AND s.value = l.lastfm_nick"
		"  AND l.artist = '%q' "
		"  AND l.title = '%q' "
		"  AND l.lastfm_nick != '%q'",
		lastfm->track->artist, lastfm->track->title, lastfm->track->user		
	);
	                           
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery)) == NULL)
		fprintf(stderr, "[-] lastfm/backlog: sql select error\n");
	
	while(sqlite3_step(stmt) == SQLITE_ROW)
		list_append(list, (char *) sqlite3_column_text(stmt, 0), (char *) sqlite3_column_text(stmt, 0));
	
	sqlite3_finalize(stmt);
	sqlite3_free(sqlquery);
	
	if(!list->length) {
		list_free(list);
		return NULL;
	}
	
	return list;
}
