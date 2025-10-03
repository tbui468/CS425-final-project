all: client server
	mkdir \
    ./test/temp \
    ./test/temp/machine.3000 \
	./test/temp/machine.3001 \
	./test/temp/machine.3002 \
	./test/temp/machine.3003 \
	./test/temp/machine.3004 \
	./test/temp/machine.3005 \
	./test/temp/machine.3006 \
	./test/temp/machine.3007

client: dgrep.c msg.h net.h
	gcc -o dgrep dgrep.c

server: node.c msg.h net.h
	gcc -o node node.c

clean:
	rm dgrep \
	rm node \
	rm -rf ./test/temp/
