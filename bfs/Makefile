export CC_USE_MATH = 1

TARGET   = bfs
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibbfs -lbfs -Llibsqlite3 -lsqlite3 -Llibcc -lcc -ldl -lpthread -lm
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libcc libbfs libsqlite3
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libcc libbfs libsqlite3

libcc:
	$(MAKE) -C libcc

libbfs:
	$(MAKE) -C libbfs

libsqlite3:
	$(MAKE) -C libsqlite3

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libcc clean
	$(MAKE) -C libbfs clean
	$(MAKE) -C libsqlite3 clean
	rm libcc libbfs libsqlite3

$(OBJECTS): $(HFILES)
