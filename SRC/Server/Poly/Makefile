CXX	= g++15
BIN = ../lib/libpoly.a

GCC_VERSION = $(shell $(CC) --version 2>&1 | grep "(GCC)" | cut -d' ' -f3  | cut -d'.' -f1)

CFLAGS  = -m64 -Wall -O2 -pipe -fno-exceptions -fno-rtti
CFLAGS += -std=c++23
INCDIR = -I../
LIBS =

OBJFILES = Base.o Poly.o SymTable.o Symbol.o

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

include Depend

