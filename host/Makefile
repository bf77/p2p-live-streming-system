CC = gcc
DD =
CFLAGS = -c
LIBS = -lm -lpthread
OBJ_WRAPPER = ../common/wrapper.c
OBJ_WEBM = ../common/webm_parser.c
OBJ_P2P = ../common/p2p.c
MAIN = main.c
OBJS = wrapper.o webm_parser.o p2p.o
PROGRAM = p2p

all: $(PROGRAM)

$(PROGRAM):
	$(CC) $(DD) $(CFLAGS) $(OBJ_WRAPPER) 
	$(CC) $(DD) $(CFLAGS) $(OBJ_WEBM)
	$(CC) $(DD) $(CFLAGS) $(OBJ_P2P)
	$(CC) $(DD) $(MAIN) $(OBJS) $(LIBS) -o $(PROGRAM)

clean:;      rm -f *.o *~ $(PROGRAM)
