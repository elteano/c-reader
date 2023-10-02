CC=gcc
CFLAGS=-g -Og
LDLIBS=-lcurl -lnotcurses -lnotcurses-core

creader : source/creader.o
	${CC} ${CFLAGS} ${LDLIBS} -o $@ $^

clean:
	rm creader
	rm source/*.o
