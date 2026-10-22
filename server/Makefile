CC = gcc

CFLAGS = -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -pthread -g
CPPFLAGS = -Isrc
LDLIBS = -lm

SRC = src/server.c src/logger.c src/net.c \
      src/game.c src/players.c src/lobby.c \
      src/engine.c src/match_loop.c src/protocol.c

OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

TEST_NAMES = game players lobby engine match_loop protocol
TEST_BINS = $(addprefix build/test_,$(TEST_NAMES))

.DEFAULT_GOAL := server
.RECIPEPREFIX := >

server: $(OBJ)
>$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c
>$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
>mkdir -p build

build/test_game: tests/test_game.c src/game.c src/game.h | build
>$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_game.c src/game.c -o $@ $(LDLIBS)

build/test_players: tests/test_players.c src/players.c src/players.h | build
>$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_players.c src/players.c -o $@ $(LDLIBS)

LOGIC_SRC = src/game.c src/players.c src/lobby.c
LOGIC_HEADERS = src/game.h src/players.h src/lobby.h

build/test_lobby: tests/test_lobby.c $(LOGIC_SRC) $(LOGIC_HEADERS) | build
>$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_lobby.c $(LOGIC_SRC) -o $@ $(LDLIBS)

build/test_engine: tests/test_engine.c $(LOGIC_SRC) $(LOGIC_HEADERS) src/engine.c src/engine.h | build
>$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_engine.c $(LOGIC_SRC) src/engine.c -o $@ $(LDLIBS)

build/test_match_loop: tests/test_match_loop.c $(LOGIC_SRC) $(LOGIC_HEADERS) src/engine.c src/engine.h src/match_loop.c src/match_loop.h | build
>$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_match_loop.c $(LOGIC_SRC) src/engine.c src/match_loop.c -o $@ $(LDLIBS)

build/test_protocol: tests/test_protocol.c src/protocol.c src/protocol.h src/game.c src/game.h src/players.h src/net.h | build
>$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_protocol.c src/protocol.c src/game.c -o $@ $(LDLIBS)

test: $(TEST_BINS)
>./build/test_game
>./build/test_players
>./build/test_lobby
>./build/test_engine
>./build/test_match_loop
>./build/test_protocol

clean:
>rm -f $(OBJ) $(DEP) server
>rm -rf build

-include $(DEP)

.PHONY: test clean
