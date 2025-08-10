
CXXFLAGS=$(CXXOPTFLAGS) -O0 -Wall -std=c++14

LDFLAGS+= -lwinmm

PROGNAME=timetest

OBJS=timetest.o

default: all

all: $(PROGNAME)

clean:
	rm -f *.o *.exe $(PROGNAME)

$(PROGNAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(PROGNAME) $(OBJS) $(LDFLAGS)

install: $(PROGNAME)
	install -d $(DESTDIR)/bin/
	install -m 755 $(PROGNAME) $(DESTDIR)/bin/

