CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g
SRC     = src/server.c src/logger.c
OBJ     = $(SRC:.c=.o)

server: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) server

.PHONY: clean
