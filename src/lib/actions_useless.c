/* z03 - useless/easter eggs commands
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
#include "core.h"
#include "run.h"
#include "actions_useless.h"
#include "settings.h"
#include "downloader.h"
#include "actions_backlog.h"

//
// registering commands
//

static request_t __action_blowjob = {
	.match    = ".blowjob",
	.callback = action_useless_blowjob,
	.man      = "",
	.hidden   = 1,
	.syntaxe  = "",
};

__registrar actions_useless() {
	request_register(&__action_blowjob);
}

//
// commands implementation
//

void action_useless_blowjob(ircmessage_t *message, char *args) {
	(void) args;
	irc_kick(message->chan, message->nick, "Tu vois, ça marche, connard !");
}

/* void action_sudo(ircmessage_t *message, char *args) {
	(void) args;
	irc_privmsg(message->chan, "sudo: you are not sudoers");
} */
