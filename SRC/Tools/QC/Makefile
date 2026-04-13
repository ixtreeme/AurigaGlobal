CPP = g++15
CPPFLAGS=-g

INCLUDEDIR =
LIBDIR =

## LIST OF CONSTANTS BEGIN
ENABLE_LUA_5_VERSION = 0
ENABLE_STATIC = 1
## LIST OF CONSTANTS END

ifeq ($(ENABLE_STATIC), 1)
CPP += -static
endif

INCLUDEDIR += -I../../Server/liblua/5.0/include
INCLUDEDIR += -I../../Server/liblua/5.0/src
LIBDIR += -L../../Server/liblua/5.0/lib
LIBS += ../../Server/liblua/5.0/lib/liblua.a ../../Server/liblua/5.0/lib/liblualib.a

all: qc

qc: qc.cc qc.o crc32.o
	$(CPP) -g -o ./qc $(INCLUDEDIR) $(LIBDIR) qc.o crc32.o $(LIBS)

crc32.o: crc32.cc
	$(CPP) $(CPPFLAGS) -c $< $(INCLUDEDIR) $(LIBDIR)

qc.o: qc.cc
	$(CPP) $(CPPFLAGS) -c $< $(INCLUDEDIR) $(LIBDIR)

create_conversion.o: create_conversion.cc
	$(CPP) $(CPPFLAGS) -c $< $(INCLUDEDIR) $(LIBDIR)

create_conversion: crc32.o create_conversion.o
	$(CPP) -g -o ./create_conversion $(INCLUDEDIR) $(LIBDIR) create_conversion.o crc32.o $(LIBS)

clean:
	rm -f qc qc.o crc32.o cc.o create_conversion.o
