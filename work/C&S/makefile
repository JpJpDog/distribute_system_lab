all: client.cc server.cc ../setTime.h
	g++ client.cc ../setTime.h -o client -g -lpthread
	g++ server.cc ../setTime.h -o server -g -lpthread
clean:
	rm -f client server
	rm -f time.txt
	find . -name "*receive.txt" | xargs rm -rf
	rm -f cmd.sh