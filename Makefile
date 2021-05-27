# NOTE: Feel free to change the makefile to suit your own need.

# compile and link flags
CCFLAGS = -Wall -g
LDFLAGS = -Wall -g

# make rules
TARGETS = rdt_sim 

all: $(TARGETS)

.cc.o:
	g++ $(CCFLAGS) -c -o $@ $<

rdt_sender.o: 	rdt_struct.h rdt_sender.h rdt_util.h

rdt_receiver.o:	rdt_struct.h rdt_receiver.h rdt_util.h

rdt_util.o: rdt_util.h rdt_sender.h

rdt_sim.o: 	rdt_struct.h

# rdt_util.o: rdt_util.h

rdt_sim: rdt_sim.o rdt_sender.o rdt_receiver.o rdt_util.o
	g++ $(LDFLAGS) -o $@ $^

clean:
	rm -f *~ *.o $(TARGETS)
	rm -f *.txt


