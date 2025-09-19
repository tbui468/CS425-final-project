all: client server
	mkdir machine.3000 \
	machine.3001 \
	machine.3002 \
	machine.3003 \
	machine.3004 \
	machine.3005 \
	machine.3006 \
	machine.3007

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c

clean:
	rm *.log \
	rm client \
	rm server \
	rm -rf machine.3000 \
	rm -rf machine.3001 \
	rm -rf machine.3002 \
	rm -rf machine.3003 \
	rm -rf machine.3004 \
	rm -rf machine.3005 \
	rm -rf machine.3006 \
	rm -rf machine.3007
