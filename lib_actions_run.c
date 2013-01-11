/* z03 - small bot with some network features - irc channel bot actions
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
 
#include "lib_core.h"
#include "lib_run.h"
#include "lib_actions_run.h"

void action_run_c(ircmessage_t *message, char *args) {
	lib_run_init(message, args, C);
}

void action_run_py(ircmessage_t *message, char *args) {
	lib_run_init(message, args, PYTHON);
}

void action_run_hs(ircmessage_t *message, char *args) {
	lib_run_init(message, args, HASKELL);
}

void action_run_php(ircmessage_t *message, char *args) {
	lib_run_init(message, args, PHP);
}
