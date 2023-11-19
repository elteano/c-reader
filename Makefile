CC:=gcc
INCLUDES:=-I/usr/include/libxml2
CFLAGS:=-g -Og -Wall
LDLIBS:=-lcurl -lnotcurses -lnotcurses-core -lpthread -lxml2
SRC_DIR:=source
PROGNAME:=creader
OBJ_DIR:=build

SOURCES:=$(wildcard $(SRC_DIR)/*.c)
OBJECTS:=$(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(PROGNAME)

$(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(PROGNAME) : $(OBJECTS)
	echo $(OBJECTS)
	$(CC) $(LDFLAGS) $(LDLIBS) $^ -o $@

clean:
	rm -f $(PROGNAME) $(OBJECTS)
	rmdir $(OBJ_DIR)
