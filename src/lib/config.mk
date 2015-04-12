EXEC  = libz03.so

# mirroring images to this path
ENABLE_MIRRORING = 0

# kickban nick longer than this limit
ENABLE_NICK_MAX_LENGTH = 1

# notificate when a user has not been seen since this time
ENABLE_NICK_LASTTIME = 1

# enable extended .bible support
ENABLE_HOLY_BIBLE = 1

# enable extended .3filles (book Trois Filles de leur Mère)
ENABLE_3FILLES_BOOK = 1

# enable geo-ip localisation support
ENABLE_GEOIP = 1


#
# 
#
BINPATH = ../../bin/

CFLAGS  = -fpic -W -Wall -O2 -pipe -g \
          -ansi -pedantic -std=gnu99 -I/usr/include/libxml2 # -D__DEBUG__

LDFLAGS = -shared -ldl -rdynamic \
          -lcurl -lcrypto -lssl -lm -lxml2 -ljansson \
          -lsqlite3 -lmagic

ifeq ($(ENABLE_GEOIP),1)
        CFLAGS  += -D ENABLE_GEOIP
        LDFLAGS += -lGeoIP
endif

ifeq ($(ENABLE_MIRRORING),1)
        CFLAGS  += -D ENABLE_MIRRORING
endif

ifeq ($(ENABLE_NICK_MAX_LENGTH),1)
        CFLAGS  += -D ENABLE_NICK_MAX_LENGTH
endif

ifeq ($(ENABLE_NICK_LASTTIME),1)
        CFLAGS  += -D ENABLE_NICK_LASTTIME
endif

ifeq ($(ENABLE_HOLY_BIBLE),1)
        CFLAGS  += -D ENABLE_HOLY_BIBLE
endif

ifeq ($(ENABLE_3FILLES_BOOK),1)
        CFLAGS  += -D ENABLE_3FILLES_BOOK
endif

CC = gcc
