CC:=gcc
INCLUDES:=-I/usr/include/libxml2
CFLAGS:=-g -Og -Wall
LDLIBS:=-lcurl -lnotcurses -lnotcurses-core -lpthread -lxml2
SRC_DIR:=source
PROGNAME:=creader
OBJ_DIR:=build

HEADERS:=$(wildcard $(SRC_DIR)/*.h)
SOURCES:=$(wildcard $(SRC_DIR)/*.c)
OBJECTS:=$(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(OBJ_DIR) $(PROGNAME)

$(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(PROGNAME) : $(OBJECTS)
	$(CC) $(LDFLAGS) $(LDLIBS) $^ -o $@

clean:
	rm -f $(PROGNAME) $(OBJECTS)
	rmdir $(OBJ_DIR)
