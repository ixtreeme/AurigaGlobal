CXX	= g++15

PLATFORM = $(shell file /bin/ls | cut -d' ' -f3 | cut -d'-' -f1)

GCC_VERSION = $(shell $(CC) --version 2>&1 | grep "(GCC)" | cut -d' ' -f3  | cut -d'.' -f1)
BSD_VERSION = $(shell uname -v 2>&1 | cut -d' ' -f2 | cut -d'.' -f1)
SVN_VERSION = $(shell svnversion -n -c . | cut -d':' -f2)

BIN     = ../lib/libsql.a

IFLAGS = -I/usr/local/include/
CFLAGS  = $(IFLAGS) -m64 -Wall -O2 -pipe -D_THREAD_SAFE -fno-exceptions
CFLAGS += -std=c++23

INCDIR = -I../

LIBS =

OBJFILES = AsyncSQL.o Semaphore.o Tellwait.o Statement.o

default:
	$(MAKE) $(BIN)

$(BIN): $(OBJFILES)
	ar cru $(BIN) $(OBJFILES) $(LIBS)
	ranlib $(BIN)
	chmod 700 $(BIN)

clean:
	rm -f *.o
	rm -f $(BIN)

dep:
	touch Depend
	$(CXX) $(CFLAGS) $(INCDIR) -MM *.cpp > Depend

$(OBJFILES):
	$(CXX) $(CFLAGS) $(INCDIR) -c $<

sinclude Depend
