CC=gcc
CFLAGS=-g -Og -I/usr/include/libxml2
LDLIBS=-lcurl -lnotcurses -lnotcurses-core -lpthread -lxml2

creader : source/creader.o source/parse.o source/logging.o
	${CC} ${CFLAGS} ${LDLIBS} -o $@ $^

clean:
	rm creader
	rm source/*.o
