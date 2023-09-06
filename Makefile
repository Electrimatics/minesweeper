CXX := gcc
# TODO: Remove debug flag
FLAGS := -Wall -g
LIBS := ncursesw panel

minesweeper: panel_manager.o minesweeper.c
	$(CXX) $(FLAGS) $^ -o $@ $(LIBS:%=-l%)

debug: panel_manager.o minesweeper.c
	$(CXX) $(FLAGS) -DDEBUG $^ -o minesweeper_$@ $(LIBS:%=-l%)

panel_manager.o: panel_manager.c
	$(CXX) -c $(FLAGS) $^

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./minesweeper