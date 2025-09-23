all: client server
	mkdir machine.3000 \
	machine.3001 \
	machine.3002 \
	machine.3003 \
	machine.3004 \
	machine.3005 \
	machine.3006 \
	machine.3007

client: dgrep.c
	gcc -o dgrep dgrep.c

server: node.c
	gcc -o node node.c

clean:
	rm dgrep \
	rm node \
	rm medium_lorem_ipsum.txt \
	rm large_lorem_ipsum.txt \
	rm -rf machine.3000 \
	rm -rf machine.3001 \
	rm -rf machine.3002 \
	rm -rf machine.3003 \
	rm -rf machine.3004 \
	rm -rf machine.3005 \
	rm -rf machine.3006 \
	rm -rf machine.3007
