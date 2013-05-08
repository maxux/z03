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
#include "bot.h"
#include "core_init.h"
#include "lib_list.h"
#include "lib_database.h"
#include "lib_core.h"
#include "lib_urlmanager.h"
#include "lib_ircmisc.h"
#include "lib_whatcd.h"
#include "lib_entities.h"

whatcd_t *whatcd_new(char *session) {
	whatcd_t *whatcd;
	
	if(!(whatcd = malloc(sizeof(whatcd_t))))
		return NULL;
	
	whatcd->session = strdup(session);
	
	return whatcd;
}

void whatcd_free(whatcd_t *whatcd) {
	if(!whatcd)
		return;
		
	free(whatcd->session);
	free(whatcd);
}

whatcd_release_t *whatcd_release_new() {
	return calloc(1, sizeof(whatcd_release_t));
}

void whatcd_release_free(whatcd_release_t *release) {
	if(!release)
		return;
		
	free(release->groupname);
	free(release->format);
	free(release->media);
	free(release);
}

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

static json_t *whatcd_json_error(json_t *root, char *error, whatcd_request_t *request) {
	request->error = strdup(error);
	json_decref(root);
	return NULL;
}

static whatcd_request_t *whatcd_json_parse_error(char *error, whatcd_request_t *request) {
	request->error = strdup(error);
	return request;
}

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

static whatcd_request_t *whatcd_notification_parse(json_t *response, whatcd_request_t *request) {
	json_t *check, *notif;
	whatcd_release_t *release;
	unsigned int remain;
	char id[32];
	
	/* building list */
	if(!(request->response = list_init((void (*)(void *)) whatcd_release_free)))
		return NULL;
	
	/* checking is there is new notifications */
	/* check = json_object_get(response, "numNew");
	if(json_is_integer(check)) {
		if(!(newnotif = json_integer_value(check))) {
			printf("[+] whatcd/notification: no new notifications\n");
			// return request;
		}
	} */
	
	if(!json_is_array((check = json_object_get(response, "results"))))
		return whatcd_json_parse_error("results is not an array", request);
	
	if(!(remain = json_array_size(check)))
		return whatcd_json_parse_error("notifications list is empty", request);
	
	for(; remain > 0; remain--) {
		notif = json_array_get(check, remain - 1);
		
		release = whatcd_release_new();
		release = whatcd_release_json(notif, release);
		
		sprintf(id, "%d", remain);		
		list_append(request->response, id, release);
		
		printf("[+] whatcd/notification: [%d] %s\n", release->unread, release->groupname);
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
	
	if(curl_download_text(url, curl))
		return NULL;
	
	if((root = whatcd_json_load(curl->data, curl->length, request))) {
		response = json_object_get(root, "response");
		request = whatcd_notification_parse(response, request);
		json_decref(root);
		
	} else fprintf(stderr, "[-] whatcd/notifications: error: %s\n", request->error);
	
	curl_data_free(curl);
	
	return request;	
}
