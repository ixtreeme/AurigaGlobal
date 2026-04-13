CC	= g++15
BIN_DIR = ../lib
BIN     = $(BIN_DIR)/libthecore.a


GCC_VERSION = $(shell $(CC) --version 2>&1 | grep "(GCC)" | cut -d' ' -f3  | cut -d'.' -f1)

CFLAGS  = -m64 -Wall -O2 -pipe -g
CFLAGS += -std=c++23

LIBS    = 
INCDIR = -I../

OBJFILES = socket.o fdwatch.o buffer.o signal.o log.o utils.o \
	kstbl.o hangul.o heart.o main.o tea.o des.o gost.o

default:
	$(MAKE) $(BIN)

$(BIN): $(OBJFILES)
	if [ ! -d $(BIN_DIR) ]; then mkdir $(BIN_DIR); fi
	ar cru $(BIN) $(OBJFILES) $(LIBS)
	ranlib $(BIN)
	chmod 700 $(BIN)

clean:
	rm -f *.o
	rm -f $(BIN)

dep:
	$(CC) $(CFLAGS) $(INCDIR) -MM *.cpp > Depend

$(OBJFILES):
	$(CC) $(CFLAGS) $(INCDIR) -c $<


include Depend
