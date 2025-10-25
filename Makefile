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

client: dgrep.c msg.h net.h arena.h common.h
	clang -o dgrep dgrep.c common.c

server: node.c msg.h net.h membership.h arena.h common.h common.c membership.c
	clang -o node -fsanitize=address -g node.c membership.c common.c

clean:
	rm dgrep \
	rm node \
	rm -rf ./test/temp/
