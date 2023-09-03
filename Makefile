CXX := gcc
FLAGS := -Wall
LIBS := ncursesw

minesweeper: minesweeper.c
	$(CXX) $(FLAGS) $^ -o $@ $(LIBS:%=-l%)

debug: minesweeper.c
	$(CXX) $(FLAGS) -DDEBUG -g $^ -o minesweeper_$@ $(LIBS:%=-l%)

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./minesweeper