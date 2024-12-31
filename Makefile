CXX := gcc
FLAGS := -Wall
LIBS := ncursesw panel

minesweeper: panel_manager.o minesweeper.c explode.c
	$(CXX) $(FLAGS) $^ -o $@ $(LIBS:%=-l%)

panel_manager.o: panel_manager.c
	$(CXX) -c $(FLAGS) $^

debug: panel_manager_debug.o minesweeper.c explode.c
	$(CXX) $(FLAGS) -g -DDEBUG $^ -o minesweeper-debug $(LIBS:%=-l%)

panel_manager_debug.o: panel_manager.c
	$(CXX) -c $(FLAGS) -g -DDEBUG $^ -o $@

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./minesweeper

ROWS ?= 10
COLS ?= 10
BOMBS ?= 10
run-debug:
	gdbserver --once localhost:9999 ./minesweeper-debug $(ROWS) $(COLS) $(BOMBS)

