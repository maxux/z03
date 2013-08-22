CREATE TABLE delay (id integer primary key, timestamp integer, nick varchar(32), chan varchar(64), message text, finished tinyint);
CREATE TABLE hosts (nick varchar(32), username varchar(32), host varchar(64), chan varchar(32), PRIMARY KEY(nick, host));
CREATE TABLE lastfm_logs (lastfm_nick varchar(32), artist varchar(128), title varchar(256), primary key (lastfm_nick, artist, title));
CREATE TABLE logs (id integer primary key, chan varchar(32), timestamp integer, nick varchar(32), message text);
CREATE TABLE notes (id integer primary key autoincrement, fnick varchar(32), tnick varchar(32), chan varchar(32), message text, seen bool, ts integer, host varchar(32));
CREATE TABLE settings (nick varchar(32), key varchar(32), value varchar(512), private bool, primary key (nick, key));
CREATE TABLE stats (nick varchar(32), chan varchar(32), words integer, lines integer, primary key (nick, chan));
CREATE TABLE stats_fast (nick varchar(32), chan varchar(32), day varchar(12), lines integer, primary key (nick, chan, day));
CREATE TABLE url(id integer, nick varchar(25), url text, hit integer, time integer, title text, sha1 varchar(40), chan varchar(32), primary key (id));
CREATE INDEX chan on url (chan);
CREATE INDEX delay_finished on delay (finished);
CREATE INDEX delay_timestamp on delay (timestamp);
CREATE INDEX hosts_chan on hosts (chan);
CREATE INDEX linked ON logs (nick, chan);
CREATE INDEX logs_chan on logs (chan);
CREATE INDEX logs_chan_nick_timestamp on logs (chan, nick, timestamp);
CREATE INDEX logs_chan_timestamp on logs (chan, timestamp);
CREATE INDEX logs_nick on logs (nick);
CREATE INDEX logs_timestamp on logs (timestamp);
CREATE INDEX notes_tnick on notes (tnick);
CREATE INDEX url_nick on url (nick);
CREATE INDEX url_time on url (time);
CREATE INDEX url_url on url (url);

