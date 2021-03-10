all: peer.cc server.cc util.h ../setTime.h
	g++ peer.cc util.h ../setTime.h -o peer -g -lpthread
	g++ server.cc util.h ../setTime.h -o server -g -lpthread
clean:
	rm -f peer server
	rm -f tracer.txt
	rm -f receive[0-9]*.txt
	rm -f cmd.sh
	rm -f time.txt