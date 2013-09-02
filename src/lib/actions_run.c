/* z03 - run commands (c, python, ...). see code-daemon/client
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
 
#include "core.h"
#include "run.h"
#include "actions_run.h"

//
// registering commands
//

static request_t __action_c = {
	.match    = ".c",
	.callback = action_run_c,
	.man      = "compile and run c code, from arguments",
	.hidden   = 0,
	.syntaxe  = ".c <c code> eg: .c printf(\"Hello world\\n\");",
};

static request_t __action_python = {
	.match    = ".python",
	.callback = action_run_py,
	.man      = "compile and run inline python code, from arguments",
	.hidden   = 0,
	.syntaxe  = ".python <python code> eg: .py print('Hello world')",
};

static request_t __action_haskell = {
	.match    = ".haskell",
	.callback = action_run_hs,
	.man      = "compile and run inline haskell code, from arguments",
	.hidden   = 0,
	.syntaxe  = ".haskell <haskell code> eg: .hs print \"Hello\"",
};

static request_t __action_php = {
	.match    = ".php",
	.callback = action_run_php,
	.man      = "compile and run inline php code, from arguments",
	.hidden   = 0,
	.syntaxe  = ".php <php code> eg: .php echo \"Hello\";",
};

__registrar actions_run() {
	request_register(&__action_c);
	request_register(&__action_python);
	request_register(&__action_haskell);
	request_register(&__action_php);
}

//
// commands implementation
//

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
