# NOTE: Feel free to change the makefile to suit your own need.

# compile and link flags
CCFLAGS = -Wall -g
LDFLAGS = -Wall -g

# make rules
TARGETS = rdt_sim 

all: $(TARGETS)

.cc.o:
	g++ $(CCFLAGS) -c -o $@ $<

rdt_sender.o: 	rdt_struct.h rdt_sender.h rdt_protocol.h

rdt_receiver.o:	rdt_struct.h rdt_receiver.h rdt_protocol.h

rdt_sim.o: 	rdt_struct.h

rdt_protocol.o:	rdt_protocol.h

rdt_sim: rdt_sim.o rdt_sender.o rdt_receiver.o rdt_protocol.o
	g++ $(LDFLAGS) -o $@ $^

clean:
	rm -f *~ *.o $(TARGETS)
