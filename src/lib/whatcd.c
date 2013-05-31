/* z03 - small bot with some network features - what.cd api
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
#include "../common/bot.h"
#include "../core/init.h"
#include "list.h"
#include "database.h"
#include "core.h"
#include "urlmanager.h"
#include "ircmisc.h"
#include "whatcd.h"
#include "entities.h"

/*
 * whatcd main structure
 */
whatcd_t *whatcd_new(char *session) {
	whatcd_t *whatcd;
	
	if(!(whatcd = malloc(sizeof(whatcd_t))))
		return NULL;
	
	whatcd->session = strdup(session);
	
	printf("[+] whatcd: new instance, session: %s\n", whatcd->session);
	
	return whatcd;
}

void whatcd_free(whatcd_t *whatcd) {
	if(!whatcd)
		return;
		
	free(whatcd->session);
	free(whatcd);
}

/*
 * whatcd release (torrent)
 */
whatcd_release_t *whatcd_release_new() {
	return calloc(1, sizeof(whatcd_release_t));
}

void whatcd_release_free(whatcd_release_t *release) {
	if(!release)
		return;
	
	free(release->artist);	
	free(release->groupname);
	free(release->format);
	free(release->media);
	free(release);
}

/*
 * general request structure
 */
whatcd_request_t *whatcd_request_new() {
	return calloc(1, sizeof(whatcd_request_t));
}

void whatcd_request_free(whatcd_request_t *request) {
	if(!request)
		return;
		
	if(request->response)
		list_free(request->response);
	
	free(request->error);
	free(request);
}

/*
 * whatcd torrent detail
 */
whatcd_torrent_t *whatcd_torrent_new() {
	return calloc(1, sizeof(whatcd_torrent_t));
}

void whatcd_torrent_free(whatcd_torrent_t *torrent) {
	if(!torrent)
		return;
	
	free(torrent->artist);
	free(torrent);
}

/*
 * json global error setter and cleaner
 */
static json_t *whatcd_json_error(json_t *root, char *error, whatcd_request_t *request) {
	request->error = strdup(error);
	json_decref(root);
	return NULL;
}

static whatcd_request_t *whatcd_json_parse_error(char *error, whatcd_request_t *request) {
	request->error = strdup(error);
	return request;
}

/*
 * read a whatcd json data and check if it's a valid json
 * parse status and return a json_t on response
 */
static json_t *whatcd_json_load(char *json, size_t length, whatcd_request_t *request) {
	json_t *root, *status;
	json_error_t error;
	
	printf("[ ] whatcd/json_load: %s\n", json);
	
	if(!(root = json_loadb(json, length, 0, &error))) {
		fprintf(stderr, "[-] lastfm/json: parsing error: line %d: %s\n", error.line, error.text);
		request->error = strdup("json parsing error");
		return NULL;
	}
	
	/* check root */
	if(!json_is_object(root))
		return whatcd_json_error(root, "json root is not an object", request);
	
	/* check status, need to be success */
	if(json_is_string((status = json_object_get(root, "status")))) {
		if(strcmp(json_string_value(status), "success"))
			return whatcd_json_error(root, "api status failure", request);
		
	} else return whatcd_json_error(root, "status not found on json response", request);
	
	/* check that response is an array */
	if(!json_is_object(json_object_get(root, "response")))
		return whatcd_json_error(root, "response is not an object", request);
	
	return root;
}


static whatcd_torrent_t *whatcd_torrent_json(json_t *item, whatcd_torrent_t *torrent) {
	json_t *data, *artist;
	
	data = json_object_get(item, "id");
	if(json_is_number(data))
		torrent->id = json_number_value(data);
	
	data = json_object_get(item, "musicInfo");
	if(!json_is_object(data)) {
		fprintf(stderr, "[-] whatcd/torrent: musicInfo not found\n");
		return torrent;
	}
	
	data = json_object_get(data, "artists");
	if(!json_is_array(data)) {
		fprintf(stderr, "[-] whatcd/torrent: artists is not a array\n");
		return torrent;
	}
	
	artist = json_array_get(data, 0);
	if(!json_is_object(artist)) {
		fprintf(stderr, "[-] whatcd/torrent: artist not found\n");
		return torrent;
	}

	data = json_object_get(artist, "name");
	if(!json_is_string(data)) {
		fprintf(stderr, "[-] whatcd/torrent: artist name is not a string\n");
		return torrent;
	}
	
	torrent->artist = strdup(json_string_value(data));
	decode_html_entities_utf8(torrent->artist, torrent->artist);
	
	printf("<%s>\n", torrent->artist);

	return torrent;
}

static whatcd_request_t *whatcd_torrent_parse(json_t *response, whatcd_t *whatcd, whatcd_request_t *request) {
	json_t *group;
	whatcd_torrent_t *torrent;
	(void) whatcd;
	
	/* building list */
	if(!(request->response = list_init((void (*)(void *)) whatcd_torrent_free)))
		return NULL;
	
	if(!json_is_object((group = json_object_get(response, "group"))))
		return whatcd_json_parse_error("group is not an object", request);
		
	torrent = whatcd_torrent_new();
	torrent = whatcd_torrent_json(group, torrent);
	
	list_append(request->response, "group", torrent);
	
	return request;
}

whatcd_request_t *whatcd_torrent(whatcd_t *whatcd, int tid) {
	whatcd_request_t *request;
	curl_data_t *curl;
	char cookie[2048], url[256];
	json_t *root, *response;
	
	if(!(request = whatcd_request_new()))
		return NULL;
	
	/* downloading json */
	if(!(curl = curl_data_new()))
		return NULL;
	
	zsnprintf(url, WHATCD_API_BASE "torrent&id=%d", tid);
	
	zsnprintf(cookie, "session=%s", whatcd->session);
	curl->cookie = cookie;
	
	if(curl_download_text(url, curl)) {
		fprintf(stderr, "[-] whatcd/torrent: curl data seems empty\n");
		return NULL;
	}
	
	if((root = whatcd_json_load(curl->data, curl->length, request))) {
		response = json_object_get(root, "response");
		request = whatcd_torrent_parse(response, whatcd, request);
		json_decref(root);
		
	} else fprintf(stderr, "[-] whatcd/torrent: error: %s\n", request->error);
	
	curl_data_free(curl);
	
	return request;
}

static whatcd_release_t *whatcd_release_json(json_t *item, whatcd_release_t *release) {
	json_t *data;
	
	data = json_object_get(item, "torrentId");
	if(json_is_number(data))
		release->torrentid = json_number_value(data);
	
	data = json_object_get(item, "groupId");
	if(json_is_number(data))
		release->groupid = json_number_value(data);
	
	data = json_object_get(item, "groupName");
	if(json_is_string(data)) {
		release->groupname = strdup(json_string_value(data));
		decode_html_entities_utf8(release->groupname, release->groupname);
	}
	
	data = json_object_get(item, "format");
	if(json_is_string(data))
		release->format = strdup(json_string_value(data));
	
	data = json_object_get(item, "media");
	if(json_is_string(data))
		release->media = strdup(json_string_value(data));
	
	data = json_object_get(item, "size");
	if(json_is_number(data))
		release->size = json_number_value(data);
	
	data = json_object_get(item, "unread");
	if(json_is_boolean(data))
		release->unread = json_is_true(data);
		
	return release;
}

static whatcd_request_t *whatcd_notification_parse(json_t *response, whatcd_t *whatcd, whatcd_request_t *request) {
	json_t *check, *notif;
	whatcd_release_t *release;
	whatcd_torrent_t *torrent;
	whatcd_request_t *torrentreq;
	unsigned int remain;
	char id[32];
	
	/* building list */
	if(!(request->response = list_init((void (*)(void *)) whatcd_release_free)))
		return NULL;
	
	if(!json_is_array((check = json_object_get(response, "results"))))
		return whatcd_json_parse_error("results is not an array", request);
	
	if(!(remain = json_array_size(check)))
		return whatcd_json_parse_error("notifications list is empty", request);
	
	for(; remain > 0; remain--) {
		notif = json_array_get(check, remain - 1);
		
		release = whatcd_release_new();
		release = whatcd_release_json(notif, release);
		
		/* request artist name if new notification */
		if(release->unread) {
			torrentreq = whatcd_torrent(whatcd, release->torrentid);
			torrent = (whatcd_torrent_t *) list_search(torrentreq->response, "group");
			release->artist = strdup(torrent->artist);
			whatcd_request_free(torrentreq);
		}
		
		sprintf(id, "%d", remain);		
		list_append(request->response, id, release);
		
		printf("[+] whatcd/notification: [%d] %s - %s\n", release->unread, release->artist, release->groupname);
	}
	
	return request;
}

whatcd_request_t *whatcd_notification(whatcd_t *whatcd) {
	curl_data_t *curl;
	whatcd_request_t *request;
	char cookie[2048], *url;
	json_t *root, *response;
	
	if(!(request = whatcd_request_new()))
		return NULL;
	
	/* downloading json */
	if(!(curl = curl_data_new()))
		return NULL;
	
	url = WHATCD_API_BASE "notifications";
	
	zsnprintf(cookie, "session=%s", whatcd->session);
	curl->cookie = cookie;
	
	if(curl_download_text(url, curl)) {
		fprintf(stderr, "[-] whatcd/notifications: curl data seems empty\n");
		return NULL;
	}
	
	if((root = whatcd_json_load(curl->data, curl->length, request))) {
		response = json_object_get(root, "response");
		request = whatcd_notification_parse(response, whatcd, request);
		json_decref(root);
		
	} else fprintf(stderr, "[-] whatcd/notifications: error: %s\n", request->error);
	
	curl_data_free(curl);
	
	return request;	
}
