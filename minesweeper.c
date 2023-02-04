#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <ncurses.h>

#define ROWWIDTH    5
#define ROWHEADER   "\b+---+"
#define ROWDIVIDER  "\b| %d |"

// Converts a (row, col) index into a one-dimensional offset
#define INDEX(row, col, width)          ((row*width)+col)

// Checks if a cell index is within the bounds of the gameboard
#define CELL_BOUND_CHECk(gboard, index)  ((index >= 0 && index < gboard->width*gboard->height))

// Return a cell's contents if it's in the gameboard, null otherwise 
#define CELL(gboard, index)              (CELL_BOUND_CHECk(gboard, index)? *(gboard->board+index) : 0)

// Gets the contents of a cell without checking the index. Used by other macros
#define KNOWN_CELL(gboard, index)        (*(gboard->board+index))

// Returns the index if it is in the expected row, -1 otherwise
#define INDEX_IN_ROW(gboard, index, row) ((CELL_BOUND_CHECk(gboard, index) && ((index) / gboard->width) == (row))? (index) : -1)

// Gets the surronding indexes at the provided index if they exist, -1 otherwise
#define UP(gboard, index)                (INDEX_IN_ROW(gboard, index-gboard->width, (index/gboard->width)-1))
#define UPLEFT(gboard, index)            (INDEX_IN_ROW(gboard, index-gboard->width-1, (index/gboard->width)-1))
#define LEFT(gboard, index)              (INDEX_IN_ROW(gboard, index-1, index/gboard->width))
#define DOWNLEFT(gboard, index)          (INDEX_IN_ROW(gboard, index+gboard->width-1, (index/gboard->width)+1))
#define DOWN(gboard, index)              (INDEX_IN_ROW(gboard, index+gboard->width, (index/gboard->width)+1))
#define DOWNRIGHT(gboard, index)         (INDEX_IN_ROW(gboard, index+gboard->width+1, (index/gboard->width)+1))
#define RIGHT(gboard, index)             (INDEX_IN_ROW(gboard, index+1, index/gboard->width))
#define UPRIGHT(gboard, index)           (INDEX_IN_ROW(gboard, index-gboard->width+1, (index/gboard->width)-1))

#define SURRONDING_CELL_STATE(gboard, index, CHECK) (CHECK(CELL(gboard, UP(gboard, index))) << 7 | \
                                                    CHECK(CELL(gboard, UPLEFT(gboard, index))) << 6 | \
                                                    CHECK(CELL(gboard, LEFT(gboard, index))) << 5 | \
                                                    CHECK(CELL(gboard, DOWNLEFT(gboard, index))) << 4 | \
                                                    CHECK(CELL(gboard, DOWN(gboard, index))) << 3 | \
                                                    CHECK(CELL(gboard, DOWNRIGHT(gboard, index))) << 2 | \
                                                    CHECK(CELL(gboard, RIGHT(gboard, index))) << 1 | \
                                                    CHECK(CELL(gboard, UPRIGHT(gboard, index))))
/* This is not portable, but it is fast */
#define COUNT_BITS(result, source)       __asm__ __volatile__ \
                                        ("popcnt %0, %1;" \
                                         : "=r" (result) \
                                         : "r" (source))

#define SET_NUMBOMBS(gboard, index, num) (KNOWN_CELL(gboard, index) |= num)
#define NUMBOMBS(cell)                  (cell & 0X0F)

#define SET_HASBOMB(gboard, index)       (KNOWN_CELL(gboard, index) |= 0x10)
#define HASBOMB(cell)                   ((cell & 0x10) >> 4)

#define SET_UNCOVERED(gboard, index)     (KNOWN_CELL(gboard, index) |= 0x20)
#define UNCOVERED(cell)                 ((cell & 0x20) >> 5)

#define SET_FLAGGED(gboard, index)       (gKNOWN_CELL(board, index) | 0x40)
#define FLAGGED(cell)                   ((cell & 0x40) >> 6)

#define DEBUG_PRINT(gboard, INFO) { \
  char* rowDividerLine = (char*)calloc((ROWWIDTH+2) * gboard->width, sizeof(char)); \
  for(int i = 0; i < gboard->width; i++) { \
    strncat(rowDividerLine, ROWHEADER, ROWWIDTH+2); \
  } \
  for (int row = 0; row < gboard->height; row++) { \
    printf("\n%s\n", rowDividerLine); \
    for (int col = 0; col < gboard->width; col++) { \
      printf(ROWDIVIDER, INFO(CELL(board, INDEX(row, col, gboard->width)))); \
    } \
  } \
  printf("\n%s\n", rowDividerLine); \
  free(rowDividerLine); \
}

// Num is assumed to be a uint8_t
#define print_bits(num) { \
  uint8_t n = num & 0xFF; \
  int c = 8; \
  while(c > 0) { \
    printf("%c", (n&0x80)? '1' : '0'); \
    n = n << 1; \
    c--; \
  } \
  printf("\n"); \
}

/* Bitfields:
    0-3: Number of surronding bombs
    4: Has bomb
    5: Flagged
    6: Uncovered
    7: Reserved
  */
typedef struct GameBoard {
  uint8_t* board;
  int height;
  int width;
  int numBombs;
} GameBoard_T;

typedef enum GameState {BOARD_GENERATION, BOMB_GENERATION, TURNS, EXPLODE, WIN, CLEANUP} GameState_T;

/* Based on: https://stackoverflow.com/a/12923949 */
typedef enum {
    STR2INT_SUCCESS = 0,
    STR2INT_OVERFLOW,
    STR2INT_UNDERFLOW,
    STR2INT_INCONVERTIBLE,
    STR2INT_EMPTY,
} str2int_errno;

/**
 * @brief Convert string s to int out.
 * 
 * @param out The converted int. Cannot be NULL.
 * @param s Input string to be converted.
 *     The format is the same as strtol,
 *     except that the following are inconvertible:
 *     - empty string
 *     - leading whitespace
 *     - any trailing characters that are not part of the number
 * @param base Base to interpret string in. Same range as strtol (2 to 36).
 * @return str2int_errno 
 */
str2int_errno str2int(int *out, char *s, int base) {
    char *end;
    if (s[0] == '\0' || isspace(s[0]))
        return STR2INT_EMPTY;
    errno = 0;
    long l = strtol(s, &end, base);
    /* Both checks are needed because INT_MAX == LONG_MAX is possible. */
    if (l > INT_MAX || (errno == ERANGE && l == LONG_MAX))
        return STR2INT_OVERFLOW;
    if (l < INT_MIN || (errno == ERANGE && l == LONG_MIN))
        return STR2INT_UNDERFLOW;
    if (*end != '\0')
        return STR2INT_INCONVERTIBLE;
    *out = l;
    return STR2INT_SUCCESS;
}

unsigned int string_strip(char** prompt, size_t max_len) {
  unsigned int len = 0;
  char* itr = *prompt;
  while(*itr && *itr != '\n' && *itr != '\r' && len < max_len-1) {
      len++;
      itr++;
    }
  *itr = '\0';
  return len;
}

// void getstrn(char** buff, unsigned int* len) {
//   char next;
//   unsigned int curr_size = 0;
//   if(*buff) {
//     free(*buff);
//     *buff = 0;
//   }
//   *len = 0;
//   while ((next = getch())) {
//     if(*len >= curr_size) {
//       *buff = (char*)realloc(buff, *len*2);
//     }
//     *buff[*len] = next;
//   }
//   *buff[*len] = '\0';

//   while(getch());
// }

int get_input_int(char* prompt, int min, int max) {
  char* buff = 0;
  char* itr;
  size_t len = 0;
  int num = 0;

  do {
    printf("> %s (%d-%d): ", prompt, min, max);
    getline(&buff, &len, stdin);
    string_strip(&buff, len);
  } while(str2int(&num, buff, 10) && num >= min && num <= max);

  return num;
}

void print_board(GameBoard_T* board) {
  for(int r = 0; r < board->height; r++) {
    char row[board->width+1];
    row[board->width] = '\0';
    for(int c = 0; c < board->width; c++) {
      int index = INDEX(r, c, board->width);
      if(UNCOVERED(CELL(board, index)) && !NUMBOMBS(CELL(board, index))) {
        row[c] = ' ';
      } else if(UNCOVERED(CELL(board, index)) && NUMBOMBS(CELL(board, index))) {
        row[c] = NUMBOMBS(CELL(board, index))+'0';
      } else if(FLAGGED(CELL(board, index))) {
        row[c] = 'F';
      } else if(HASBOMB(CELL(board, index))) {
        row[c] = 'X';
      } else {
        row[c] = '?';
      }
    }
    printw("%s\n", row);
    refresh();
  }
}

void generate_board(GameBoard_T* state, int rows, int columns) {
  state->height = rows;
  state->width = columns;
  state->board = (uint8_t*)calloc(state->width * state->height, sizeof(uint8_t));
  state->numBombs = 0;
}

int generate_bombs(GameBoard_T* state, int bombs) {
  // TODO: Instead of choosing all random spots, choose random points for clusters of bombs
  // TODO: Generate bombs that create a radius around the initial click
  for(int b = 0; b < bombs; b++) {
    int placement;
    do {
      placement = rand() % (state->width*state->height);
    } while(HASBOMB(KNOWN_CELL(state, placement)));
    SET_HASBOMB(state, placement);
    // printf("Placed bomb at index %d\n", placement);
  }

  // Update the number of bombs around each cell
  for(int i = 0; i < state->height*state->width; i++) {
    int surrondingBombs = SURRONDING_CELL_STATE(state, i, HASBOMB);
    int numBombs;
    COUNT_BITS(numBombs, surrondingBombs);
    SET_NUMBOMBS(state, i, numBombs);
  }

  return 0;
}

int main(int argc, char** argv, char** envp) {
  GameState_T gameState = BOARD_GENERATION;
  GameBoard_T* gameBoard = (GameBoard_T*)malloc(sizeof(GameBoard_T));
  int rows, cols, bombs;

  srand(11);
  if(argc != 4) {
    fprintf(stderr, "Usage: %s <rows> <cols> <bombs>\n", argv[0]);
    exit(1);
  }

  if(str2int(&rows, argv[1], 10)) {
    fprintf(stderr, "Specified rows %s cannot be converted into an integer\n", argv[1]);
  }

  if(str2int(&cols, argv[2], 10)) {
    fprintf(stderr, "Specified columns %s cannot be converted into an integer\n", argv[2]);
  }

  if(str2int(&bombs, argv[3], 10)) {
    fprintf(stderr, "Specified number of bombs %s cannot be converted into an integer\n", argv[3]);
  }

  // rows = get_input_int("Enter number of rows", 5, 100);
  // cols = get_input_int("Enter number of columns", 5, 100);

  generate_board(gameBoard, rows, cols);
  generate_bombs(gameBoard, bombs);
  // DEBUG_PRINT(gameBoard, HASBOMB);
  // DEBUG_PRINT(gameBoard, NUMBOMBS);

  initscr();

  int row;
  int col;
  int count = 0;
  while(count < 10) {
    print_board(gameBoard);
    // row = get_input_int("Enter row to uncover", 0, gameBoard->height-1);
    // col = get_input_int("Enter column to uncover", 0, gameBoard->width-1);
    SET_UNCOVERED(gameBoard, INDEX(row, col, gameBoard->width));
  }

  endwin();
  if (gameBoard->board) {
    free(gameBoard->board);
  }
  free(gameBoard);
}