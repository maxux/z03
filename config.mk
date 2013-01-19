CORE_EXEC = z03-bot
LIB_EXEC  = libz03.so

# flags
EXTRA_CFLAGS = # -D__DEBUG__

CORE_CFLAGS  = -W -Wall -O2 -pipe -std=gnu99 -g
CORE_LDFLAGS = -rdynamic -ldl -lssl -lpthread

LIB_CFLAGS   = -fpic -W -Wall -O2 -pipe -g -ansi -pedantic -std=gnu99 -I/usr/include/libxml2
LIB_LDFLAGS  = -ldl -rdynamic -lcurl -lcrypto -lssl -lm -shared -lxml2 -ljansson -lsqlite3

CC = gcc
