CC=gcc
CFLAGS=-g -Og
LDLIBS=-lcurl -lnotcurses

creader : source/creader.o
	${CC} ${CFLAGS} ${LDLIBS} -o $@ $^

clean:
	rm creader
	rm source/*.o
