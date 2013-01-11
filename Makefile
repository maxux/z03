include config.mk

# Core files
CORE_SRC = $(wildcard core_*.c)
CORE_OBJ = $(CORE_SRC:.c=.o)

# Lib files
LIB_SRC = $(wildcard lib_*.c)
LIB_OBJ = $(LIB_SRC:.c=.o)

all: options $(CORE_EXEC) $(LIB_EXEC)

options: config.mk
	@echo "CC           = $(CC)"
	@echo "EXTRA_CFLAGS = $(EXTRA_CFLAGS)"
	@echo "CORE_CFLAGS  = $(CORE_CFLAGS)"
	@echo "CORE_LDFLAGS = $(CORE_LDFLAGS)"
	@echo "LIB_CFLAGS   = $(LIB_CFLAGS)"
	@echo "LIB_LDFLAGS  = $(LIB_LDFLAGS)"
	@echo "CORE_OBJ     = $(CORE_OBJ)"
	@echo "LIB_OBJ      = $(LIB_OBJ)"


$(CORE_EXEC): $(CORE_OBJ)
	$(CC) -o $@ $^ $(CORE_LDFLAGS)

$(CORE_OBJ):
	$(CC) -c $(CORE_CFLAGS) $(EXTRA_CFLAGS) $*.c


$(LIB_EXEC): $(LIB_OBJ)
	$(CC) -o $@ $^ $(LIB_LDFLAGS)
	
$(LIB_OBJ): $(LIB_SRC)
	$(CC) -c $(LIB_CFLAGS) $(EXTRA_CFLAGS) $*.c

clean:
	rm -fv *.o

mrproper: clean
	rm -fv $(CORE_EXEC) $(LIB_EXEC)

