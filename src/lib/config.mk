EXEC  = libz03.so

# flags
EXTRA_CFLAGS = # -D__DEBUG__

CFLAGS  = -fpic -W -Wall -O2 -pipe -g -ansi -pedantic -std=gnu99 -I/usr/include/libxml2
LDFLAGS = -ldl -rdynamic -lcurl -lcrypto -lssl -lm -shared -lxml2 -ljansson -lsqlite3
BINPATH = ../../bin/

CC = gcc
