CORE_EXEC = z03-bot
LIB_EXEC  = libz03.so

# flags
EXTRA_CFLAGS = # -D__DEBUG__

CORE_CFLAGS  = -W -Wall -O2 -pipe -std=gnu99 -g
CORE_LDFLAGS = -lsqlite3 -rdynamic

LIB_CFLAGS   = -fpic -W -Wall -O2 -pipe -g -ansi -pedantic -std=gnu99 -I/usr/include/libxml2
LIB_LDFLAGS  = -ldl -lcurl -lcrypto -lssl -lm -shared -lxml2 -ljansson

CC = i686-pc-linux-gnu-gcc
