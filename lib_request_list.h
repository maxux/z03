request_t __request[] = {
	{
		.match    = ".weather",
		.callback = action_weather,
		.man      = "print weather information: .weather list, .weather [station]",
		.hidden   = 0,
	},
	{
		.match    = ".ping",
		.callback = action_ping,
		.man      = "ping request/reply",
		.hidden   = 0,
	},
	{		
		.match    = ".time",
		.callback = action_time,
		.man      = "print the time",
		.hidden   = 0,
	},
	{
		.match    = ".rand",
		.callback = action_random,
		.man      = "random generator: .rand, .rand max, .rand min max",
		.hidden   = 0,
	},
	{
		.match    = ".stats",
		.callback = action_stats,
		.man      = "print url statistics",
		.hidden   = 0,
	},
	{
		.match    = ".chart",
		.callback = action_chart,
		.man      = "print a chart about url usage",
		.hidden   = 0,
	},
	{
		.match    = ".chartlog",
		.callback = action_chartlog,
		.man      = "chart of lines log",
		.hidden   = 0,
	},
	{
		.match    = ".uptime",
		.callback = action_uptime,
		.man      = "print the bot's uptime",
		.hidden   = 0,
	},
	{
		.match    = ".somafm",
		.callback = action_somafm,
		.man      = "print the current track on SomaFM radio: .somafm list, .somafm station",
		.hidden   = 0,
	},
	{
		.match    = ".dns",
		.callback = action_dns,
		.man      = "resolve a dns name address: .dns domain-name",
		.hidden   = 0,
	},
	{
		.match    = ".rdns",
		.callback = action_rdns,
		.man      = "resolve a reverse dns address: .dns ip-address",
		.hidden   = 0,
	},
	{
		.match    = ".count",
		.callback = action_count,
		.man      = "print the number of line posted by a given nick: .count nick",
		.hidden   = 0,
	},
	{
		.match    = ".known",
		.callback = action_known,
		.man      = "check if a given nick is already known, by hostname: .known nick",
		.hidden   = 0,
	},
	{
		.match    = ".url",
		.callback = action_url,
		.man      = "search on url database, by url or title. Use % as wildcard. ie: .url gang%youtube",
		.hidden   = 0,
	},
	{
		.match    = ".goo",
		.callback = action_google,
		.man      = "search on Google, print the first result: .goo keywords",
		.hidden   = 0,
	},
	{
		.match    = ".google",
		.callback = action_google,
		.man      = "search on Google, print the 3 firsts result: .google keywords",
		.hidden   = 0,
	},
	{
		.match    = ".help",
		.callback = action_help,
		.man      = "print the list of all the commands available",
		.hidden   = 0,
	},
	{
		.match    = ".man",
		.callback = action_man,
		.man      = "print 'man page' of a given bot command: .man command",
		.hidden   = 0,
	},
	{
		.match    = ".note",
		.callback = action_notes,
		.man      = "leave a message to someone, will be sent when connecting.",
		.hidden   = 0,
	},
	{
		.match    = ".c",
		.callback = action_run_c,
		.man      = "compile and run c code, from arguments: .run printf(\"Hello world\\n\");",
		.hidden   = 0,
	},
	{
		.match    = ".py",
		.callback = action_run_py,
		.man      = "compile and run inline python code, from arguments: .py print('Hello world')",
		.hidden   = 0,
	},
	{
		.match    = ".hs",
		.callback = action_run_hs,
		.man      = "compile and run inline haskell code, from arguments: .hs print \"Hello\"",
		.hidden   = 0,
	},
	{
		.match    = ".php",
		.callback = action_run_php,
		.man      = "compile and run inline php code, from arguments: .php echo \"Hello\";",
		.hidden   = 0,
	},
	{
		.match    = ".backlog",
		.callback = action_backlog,
		.man      = "print last lines: .backlog [nick]",
		.hidden   = 0,
	},
	{
		.match    = ".wi",
		.callback = action_wiki,
		.man      = "summary an english wikipedia's article: .wiki keywords",
		.hidden   = 0,
	},
	{
		.match    = ".wiki",
		.callback = action_wiki,
		.man      = "summary a wiki's international article: .wiki lang keywords",
		.hidden   = 0,
	},
	{
		.match    = ".set",
		.callback = action_set,
		.man      = "set a variable value: .set var1 val1",
		.hidden   = 0,
	},
	{
		.match    = ".unset",
		.callback = action_unset,
		.man      = "unset a variable value: .unset var1",
		.hidden   = 0,
	},
	{
		.match    = ".fm",
		.callback = action_lastfm,
		.man      = "print now playing lastfm title: .fm [username]",
		.hidden   = 0,
	},
	{
		.match    = ".fmlove",
		.callback = action_lastfmlove,
		.man      = "love your current track on last.fm",
		.hidden   = 0,
	},
	{
		.match    = "sudo",
		.callback = action_sudo,
		.man      = "",
		.hidden   = 1,
	},
	{
		.match    = ".blowjob",
		.callback = action_blowjob,
		.man      = "",
		.hidden   = 1,
	},
};
