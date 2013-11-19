/* z03 - last.fm api functions
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
#include <stdlib.h>
#include "../common/bot.h"
#include "../core/init.h"
#include "core.h"
#include "ircmisc.h"
#include "lastfm.h"
#include "settings.h"
#include "actions.h"
#include "actions_lastfm.h"

//
// registering commands
//

static request_t __action_fm = {
	.match    = ".fm",
	.callback = action_lastfm,
	.man      = "print now playing lastfm title",
	.hidden   = 0,
	.syntaxe  = ".fm, .fm <lastfm nick>, .fm $(<irc nick>), setting variable: lastfm",
};

static request_t __action_fmlove = {
	.match    = ".fmlove",
	.callback = action_lastfmlove,
	.man      = "love your current track on last.fm",
	.hidden   = 0,
	.syntaxe  = "",
};

__registrar actions_lastfm() {
	request_register(&__action_fm);
	request_register(&__action_fmlove);
}

//
// commands implementation
//

void action_lastfm(ircmessage_t *message, char *args) {
	char *user, answer[512];
	lastfm_request_t *request;
	lastfm_t *lastfm;
	char date[128], *userlist;
	char *temp = NULL, *user2 = NULL;
	struct tm *timeinfo;
	list_t *backlog;
	
	if(strlen((args = action_check_args(args)))) {
		user = args;
	
		// check if it's a settings to grab
		if(!strncmp(user, "$(", 2) && (temp = strchr(user, ')'))) {
			temp = strndup(user + 2, temp - user - 2);
			if((user2 = settings_get(temp, "lastfm", PUBLIC)))
				user = user2;
			
			free(temp);
		}
		
	} else if(!(user = settings_get(message->nick, "lastfm", PUBLIC))) {
		irc_privmsg(message->chan, "Lastfm username not set. Please set it with: .set lastfm <username>");
		return;
	}
	
	lastfm  = lastfm_new(LASTFM_APIKEY, LASTFM_APISECRET);
	request = lastfm_request_new();
	request = lastfm_getplaying(lastfm, request, user);
	if(request->reply) {
		switch(lastfm->track->type) {
			case NOW_PLAYING:
				zsnprintf(answer, "Now playing: %s - %s [%s]",
						  lastfm->track->artist, lastfm->track->title,
						  lastfm->track->album);
			break;
			
			case LAST_PLAYED:
				timeinfo = localtime(&lastfm->track->date);
				strftime(date, sizeof(date), "%d/%m/%Y %X", timeinfo);
			
				zsnprintf(answer, "Last played: %s - %s [%s] (%s)",
						  lastfm->track->artist, lastfm->track->title,
						  lastfm->track->album, date);
			break;
			
			default:
				zsnprintf(answer, "Current player state not found");
		}
		
	} else zsnprintf(answer, "Error: %s", request->error);
	
	irc_privmsg(message->chan, answer);
	
	/* check backlog */
	if((backlog = lastfm_backlog(lastfm))) {
		userlist = list_nick_implode(backlog);
		
		zsnprintf(answer, "Also listened by: %s", userlist);
		irc_privmsg(message->chan, answer);
		free(userlist);
		
		list_free(backlog);
	}
	
	lastfm_request_free(request);
	lastfm_free(lastfm);
	free(user2);
}

void action_lastfmlove(ircmessage_t *message, char *args) {
	char *key, answer[512], *user, *url;
	lastfm_request_t *request;
	lastfm_t *lastfm;
	(void) args;
	
	if(!(user = settings_get(message->nick, "lastfm", PUBLIC))) {
		irc_privmsg(message->chan, "Lastfm username not set. Please set it with: .set lastfm <username>");
		return;
	}
	
	lastfm = lastfm_new(LASTFM_APIKEY, LASTFM_APISECRET);
	
	/* check if there is a pending token */
	if((key = settings_get(message->nick, "lastfm_token", PUBLIC))) {
		printf("[+] lastfm/love: pending token found, validating...\n");
		
		// removing token, if failed a new will be created next time
		lastfm->token = strdup(key);
		settings_unset(message->nick, "lastfm_token");
		
		request = lastfm_request_new();
		request = lastfm_api_getsession(lastfm, request);
		if(request->reply) {
			// saving and copy session
			settings_set(message->nick, "lastfm_session", request->reply, PRIVATE);
			lastfm->session = strdup(request->reply);
			
			zsnprintf(answer, "Token validated: %s", key);
			irc_privmsg(message->chan, answer);
			
			settings_unset(message->nick, "lastfm_token");
			lastfm_request_free(request);
			// keep going with session
			
		} else if(request->error) {
			zsnprintf(answer, "Error: %s", request->error);
			irc_privmsg(message->chan, answer);
			
			// clearing, break
			lastfm_request_free(request);
			lastfm_free(lastfm);
			return;
		}
	}
	
	/* check if we have a session key */
	if(!(key = settings_get(message->nick, "lastfm_session", PRIVATE))) {
		printf("[+] lastfm/love: session not found, creating it...\n");
		request = lastfm_request_new();
		
		request = lastfm_api_gettoken(lastfm, request);
		
		if(request->reply) {
			// saving and copy token
			settings_set(message->nick, "lastfm_token", request->reply, PUBLIC);
			lastfm->token = strdup(request->reply);
			
			url = lastfm_api_authorize(lastfm);
			zsnprintf(answer, "Authorization required: %s", url);
			free(url);
			
		} else if(request->error) zsnprintf(answer, "Error: %s", request->error);
		
		irc_privmsg(message->chan, answer);
		
		// no valid session, break
		lastfm_request_free(request);
		lastfm_free(lastfm);
		return;
	}
	
	printf("[+] lastfm/love: using session <%s>\n", key);
	lastfm->session = strdup(key);
	
	/* grabbing current playing song */
	request = lastfm_request_new();
	request = lastfm_getplaying(lastfm, request, user);
	if(request->reply) {
		if(lastfm->track->type != NOW_PLAYING) {
			irc_privmsg(message->chan, "No current playing track found");
			lastfm_request_free(request);
			lastfm_free(lastfm);
			return;
		}
		
	} else zsnprintf(answer, "Error: %s\n", request->error);
	
	lastfm_request_free(request);
	
	request = lastfm_request_new();
	request = lastfm_api_love(lastfm, request);
	if(request->reply) {
		zsnprintf(answer, "Marked as loved track: %s - %s\n",
		                  lastfm->track->artist, lastfm->track->title);
		irc_privmsg(message->chan, answer);
		
	} else if(request->error) {
		zsnprintf(answer,  "Error: %s\n", request->error);
		irc_privmsg(message->chan, answer);
	}
	
	lastfm_request_free(request);
	lastfm_free(lastfm);
}
