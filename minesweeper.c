#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Cell display macros */
#define CELL_COVERED 1
#define CELL_SELECTED 2
#define CELL_FLAGGED 3
#define CELL_UNCOVERED 4
#define CELL_CONTAINS_BOMB 5
#define CELL_BACKTRACTED 6

#define CELL_COVERED_STR "   "
#define CELL_SELECTED_STR "\u2591\u2591\u2591"
#define CELL_FLAGGED_STR " \u2691 "
#define CELL_UNCOVERED_STR " %c "
#define CELL_HASBOMB_STR " \U0001F4A3 "

#define PRINT_CELL_WITH_COLOR(color, prints)                                   \
  attron(COLOR_PAIR(color));                                                   \
  prints;                                                                      \
  attroff(COLOR_PAIR(color))

// #define CELL_COVERED  "\x1B[47m" _COVERED "\x1B[0m"
// #define CELL_SELECTED "\x1B[42m" _SELECTED "\x1B[0m"
// #define CELL_FLAGGED  "\x1B[41m" _FLAGGED "\x1B[0m"
// #define CELL_UNCOVERED  "\x1B[0m" _UNCOVERED "\x1B[0m"

/* Input capture macru2592os */
#define ARROW_UP 'A'
#define ARROW_DOWN 'B'
#define ARROW_RIGHT 'C'
#define ARROW_LEFT 'D'

/* Gameboard cell indexing macros */
// Converts a (row, col) index into a one-dimensional offset
// #define INDEX(gboard, row, col)          ((row*gboard->width)+col)

// Default cell: No surronding bombs, does not have bomb, uncovered, not flagged
#define DEFAULT_CELL (0b00100000)

// Invalid index: -1 (0xFFFF...) when unsigned
#define INVALID_INDEX (-1)

// Checks if a cell index is within the bounds of the gameboard
#define CELL_BOUND_CHECk(gboard, index) (index < gboard->width * gboard->height)

// Return a cell's contents if it's in the gameboard, default cell otherwise
#define CELL(gboard, index)                                                    \
  (CELL_BOUND_CHECk(gboard, index) ? *(gboard->board + index) : DEFAULT_CELL)

// Gets the contents of a cell without checking the index. Used by other macros
#define KNOWN_CELL(gboard, index) (*(gboard->board + index))

// Returns the index if it is in the expected row, -1 otherwise
#define INDEX_IN_ROW(gboard, index, row)                                       \
  ((CELL_BOUND_CHECk(gboard, index) && ((index) / gboard->width) == (row))     \
       ? (index)                                                               \
       : INVALID_INDEX)

/* Gameboard adjacent cell indexing macros */
// Gets the surronding indexes at the provided index if they exist, -1 otherwise
#define UP(gboard, index)                                                      \
  (INDEX_IN_ROW(gboard, index - gboard->width, (index / gboard->width) - 1))
#define UPLEFT(gboard, index)                                                  \
  (INDEX_IN_ROW(gboard, index - gboard->width - 1, (index / gboard->width) - 1))
#define LEFT(gboard, index)                                                    \
  (INDEX_IN_ROW(gboard, index - 1, index / gboard->width))
#define DOWNLEFT(gboard, index)                                                \
  (INDEX_IN_ROW(gboard, index + gboard->width - 1, (index / gboard->width) + 1))
#define DOWN(gboard, index)                                                    \
  (INDEX_IN_ROW(gboard, index + gboard->width, (index / gboard->width) + 1))
#define DOWNRIGHT(gboard, index)                                               \
  (INDEX_IN_ROW(gboard, index + gboard->width + 1, (index / gboard->width) + 1))
#define RIGHT(gboard, index)                                                   \
  (INDEX_IN_ROW(gboard, index + 1, index / gboard->width))
#define UPRIGHT(gboard, index)                                                 \
  (INDEX_IN_ROW(gboard, index - gboard->width + 1, (index / gboard->width) - 1))

#define SURRONDING_CELL_ACTION(gboard, index, ACTION)                          \
  ACTION(gboard, UP(gboard, index));                                           \
  ACTION(gboard, UPLEFT(gboard, index));                                       \
  ACTION(gboard, LEFT(gboard, index));                                         \
  ACTION(gboard, DOWNLEFT(gboard, index));                                     \
  ACTION(gboard, DOWN(gboard, index));                                         \
  ACTION(gboard, DOWNRIGHT(gboard, index));                                    \
  ACTION(gboard, RIGHT(gboard, index));                                        \
  ACTION(gboard, UPRIGHT(gboard, index))

#define SURRONDING_CELL_ACTION_STATEFUL(gboard, index, state, ACTION)          \
  if (state & 0x80)                                                            \
    ACTION(gboard, UP(gboard, index));                                         \
  if (state & 0x40)                                                            \
    ACTION(gboard, UPLEFT(gboard, index));                                     \
  if (state & 0x20)                                                            \
    ACTION(gboard, LEFT(gboard, index));                                       \
  if (state & 0x10)                                                            \
    ACTION(gboard, DOWNLEFT(gboard, index));                                   \
  if (state & 0x08)                                                            \
    ACTION(gboard, DOWN(gboard, index));                                       \
  if (state & 0x04)                                                            \
    ACTION(gboard, DOWNRIGHT(gboard, index));                                  \
  if (state & 0x02)                                                            \
    ACTION(gboard, RIGHT(gboard, index));                                      \
  if (state & 0x01)                                                            \
  ACTION(gboard, UPRIGHT(gboard, index))

// Checks the surronding cells for a provided state
#define SURRONDING_CELL_STATE(gboard, index, STATE)                            \
  (STATE(gboard, UP(gboard, index)) << 7 |                                     \
   STATE(gboard, UPLEFT(gboard, index)) << 6 |                                 \
   STATE(gboard, LEFT(gboard, index)) << 5 |                                   \
   STATE(gboard, DOWNLEFT(gboard, index)) << 4 |                               \
   STATE(gboard, DOWN(gboard, index)) << 3 |                                   \
   STATE(gboard, DOWNRIGHT(gboard, index)) << 2 |                              \
   STATE(gboard, RIGHT(gboard, index)) << 1 |                                  \
   STATE(gboard, UPRIGHT(gboard, index)))

/* This is not portable, but it is fast */
#define COUNT_BITS(result, source)                                             \
  __asm__ __volatile__("popcnt %0, %1;" : "=r"(result) : "r"(source))
// #define COUNT_BITS(result, number)  for(int i = 0; i < sizeof(number); i++) { \
//                                       (result) += (number & 0x01); \
//                                     }

/* Cell state query and modification macros */
#define SET_NUMBOMBS(gboard, index, num)                                       \
  CLEAR_NUMBOMBS(gboard, index);                                               \
  KNOWN_CELL(gboard, index) |= (num)
#define CLEAR_NUMBOMBS(gboard, index) (KNOWN_CELL(gboard, index) &= ~0x0f)
#define NUMBOMBS(gboard, index) (CELL(gboard, index) & 0X0f)
#define ADJACENTBOMB(gboard, index) ((CELL(gboard, index) & 0x0f) > 0)

#define SET_HASBOMB(gboard, index) (KNOWN_CELL(gboard, index) |= 0x10)
#define CLEAR_HASBOMB(gboard, index) (KNOWN_CELL(gboard, index) &= ~0x10)
#define HASBOMB(gboard, index) ((CELL(gboard, index) & 0x10) >> 4)

#define SET_UNCOVERED(gboard, index) (KNOWN_CELL(gboard, index) |= 0x20)
#define CLEAR_UNCOVERED(gboard, index) (KNOWN_CELL(gboard, index) &= ~0x20)
#define UNCOVERED(gboard, index) ((CELL(gboard, index) & 0x20) >> 5)

#define SET_FLAGGED(gboard, index) (KNOWN_CELL(gboard, index) |= 0x40)
#define CLEAR_FLAGGED(gboard, index) (KNOWN_CELL(gboard, index) &= ~0x40)
#define FLAGGED(gboard, index) ((CELL(gboard, index) & 0x40) >> 6)

#define SET_BACKTRACK_DIR(gboard, index, val)                                  \
  CLEAR_BACKTRACK_DIR(gboard, index);                                          \
  KNOWN_CELL(gboard, index) |= ((val & 0x03) << 6)
#define CLEAR_BACKTRACK_DIR(gboard, index) (KNOWN_CELL(gboard, index) &= ~0xc0)
#define BACKTRACK_DIR(gboard, index) ((CELL(gboard, index) & 0xc0) >> 6)
#define BT_UP 0
#define BT_DOWN 1
#define BT_RIGHT 2
#define BT_LEFT 3

#define UNCOVER_BLOCK_CONDITION(gboard, index, DIR)                            \
  (DIR(board, index) != -1 && !NUMBOMBS(board, DIR(board, index)) &&           \
   !UNCOVERED(board, DIR(board, index)))

// Num is assumed to be a uint8_t
#define print_bits(num)                                                        \
  {                                                                            \
    uint8_t n = num & 0xFF;                                                    \
    int c = 8;                                                                 \
    while (c > 0) {                                                            \
      printw("%c", (n & 0x80) ? '1' : '0');                                    \
      n = n << 1;                                                              \
      c--;                                                                     \
    }                                                                          \
    printw("\n");                                                              \
    refresh();                                                                 \
  }

typedef enum GameState {
  GAME_INIT,
  BOARD_GENERATION,
  BOMB_GENERATION,
  TURNS,
  EXPLODE,
  TIMEOUT,
  WIN,
  CLEANUP
} GameState_T;

typedef enum CellAction {
  NONE,
  UNCOVER,
  FLAG,
} CellAction_T;

/* board (uint8_t) bitfields:game_state
  +------------+---+---+---+---+
  |    0-3     | 4 | 5 | 6 | 7 |
  +------------+---+---+---+---+
  0-3: Number of surronding bombs (0-8)
  4: Has bomb
  5: Uncovered
  6: Flagged
  6-7: Backtrack direction
*/
typedef struct GameBoard {
  uint8_t *board;
  unsigned int height;
  unsigned int width;
  unsigned int numBombs;
  unsigned int reamining;
  GameState_T game_state;
} GameBoard_T;

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
str2int_errno str2int(unsigned int *out, char *s, int base) {
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

void select_cell(GameBoard_T *board, int *index) {
  int pending_index = INVALID_INDEX;
  switch (getch()) {
  case ARROW_UP:
    pending_index = UP(board, *index);
    break;

  case ARROW_DOWN:
    pending_index = DOWN(board, *index);
    break;

  case ARROW_RIGHT:
    pending_index = RIGHT(board, *index);
    break;

  case ARROW_LEFT:
    pending_index = LEFT(board, *index);
    break;

  default:
    break;
  }
  if (CELL_BOUND_CHECk(board, pending_index)) {
    *index = pending_index;
  }
}

CellAction_T do_cell_action(GameBoard_T *board, int *index) {
  while (1) {
    switch (getch()) {
    /* Flag cell */
    case 'f':
    case 'F':
      return FLAG;

    /* Uncover cell */
    case 'u':
    case 'U':
      return UNCOVER;

    /* Move to different cell */
    case '\x1B':
      // Skip the '['
      getch();
      select_cell(board, index);
      return NONE;

    default:
      break;
    }
  }
}

void print_board(GameBoard_T *board, int select) {
  // TODO: Center board + some other terminal validation
  move(0, 0);
  for (int index = 0; index < board->height * board->width; index++) {
    if (select == index) {
      attron(COLOR_PAIR(CELL_SELECTED));
      addstr(CELL_SELECTED_STR);
      attroff(COLOR_PAIR(CELL_SELECTED));

    } else if (UNCOVERED(board, index)) {
      if (HASBOMB(board, index)) {
        PRINT_CELL_WITH_COLOR(CELL_CONTAINS_BOMB, addstr(CELL_HASBOMB_STR););
      } else {
        int bombs = NUMBOMBS(board, index);
        printw(CELL_UNCOVERED_STR, (bombs) ? bombs + '0' : ' ');
      }

    } else if (FLAGGED(board, index)) {
      PRINT_CELL_WITH_COLOR(CELL_FLAGGED, addstr(CELL_FLAGGED_STR));

    } else if (HASBOMB(board, index)) {
      PRINT_CELL_WITH_COLOR(CELL_CONTAINS_BOMB, addstr(CELL_COVERED_STR));

    } else {
      PRINT_CELL_WITH_COLOR(CELL_COVERED, addstr(CELL_COVERED_STR));
    }

    if (!((index + 1) % board->width)) {
      addch('\n');
      refresh();
    }
  }
  printw("Current cell: ");
  print_bits(CELL(board, select));
  printw("\tNUMBOMBS: %d\tHASBOMB: %d\tUNCOVERED: %d\tFLAGGED: %d\n",
         NUMBOMBS(board, select), HASBOMB(board, select),
         UNCOVERED(board, select), FLAGGED(board, select));
  printw("Remaining: %d\n", board->reamining);
}

void uncover_cell_block(GameBoard_T *board, int index) {
  int start_index = index;
  int prev_index = index;

  if (UNCOVERED(board, index)) {
    return;
  }

  // Uncover only 1 cell if it has a bomb in it
  if (NUMBOMBS(board, index)) {
    SET_UNCOVERED(board, index);
    board->reamining--;
    return;
  }

  // Traverse open space
  do {
    if (index == start_index) {
      unsigned int dir_counter = BACKTRACK_DIR(board, start_index);
      SET_BACKTRACK_DIR(board, start_index, dir_counter + 1);
    }

    if (!UNCOVERED(board, index)) {
      SET_UNCOVERED(board, index);
      board->reamining--;
    }

    int adjacent_bombs_bits = SURRONDING_CELL_STATE(board, index, ADJACENTBOMB);
    int surronding_uncovered_bits =
        SURRONDING_CELL_STATE(board, index, !UNCOVERED);
    int newly_uncovered;
    COUNT_BITS(newly_uncovered,
               adjacent_bombs_bits & surronding_uncovered_bits);

    SURRONDING_CELL_ACTION_STATEFUL(board, index, adjacent_bombs_bits,
                                    SET_UNCOVERED);
    board->reamining -= newly_uncovered;
    print_board(board, index);
    print_bits(adjacent_bombs_bits);
    print_bits(surronding_uncovered_bits);
    // printw("%d\n", newly_uncovered);
    // getch();

    if (UNCOVER_BLOCK_CONDITION(board, index, UP)) {
      index = UP(board, index);
      SET_BACKTRACK_DIR(board, index, BT_DOWN);
      continue;
    }
    if (UNCOVER_BLOCK_CONDITION(board, index, RIGHT)) {
      index = RIGHT(board, index);
      SET_BACKTRACK_DIR(board, index, BT_LEFT);
      continue;
    }
    if (UNCOVER_BLOCK_CONDITION(board, index, DOWN)) {
      index = DOWN(board, index);
      SET_BACKTRACK_DIR(board, index, BT_UP);
      continue;
    }
    if (UNCOVER_BLOCK_CONDITION(board, index, LEFT)) {
      index = LEFT(board, index);
      SET_BACKTRACK_DIR(board, index, BT_RIGHT);
      continue;
    }
    if (index != start_index) {
      prev_index = index;
      switch (BACKTRACK_DIR(board, index)) {
      case BT_UP:
        if (UNCOVER_BLOCK_CONDITION(board, index, DOWNRIGHT)) {
          uncover_cell_block(board, DOWNRIGHT(board, index));
        }
        if (UNCOVER_BLOCK_CONDITION(board, index, DOWNLEFT)) {
          uncover_cell_block(board, DOWNLEFT(board, index));
        }
        index = UP(board, index);
        break;

      case BT_RIGHT:
        if (UNCOVER_BLOCK_CONDITION(board, index, UPLEFT)) {
          uncover_cell_block(board, UPLEFT(board, index));
        }
        if (UNCOVER_BLOCK_CONDITION(board, index, DOWNLEFT)) {
          uncover_cell_block(board, DOWNLEFT(board, index));
        }
        index = RIGHT(board, index);
        break;

      case BT_DOWN:
        if (UNCOVER_BLOCK_CONDITION(board, index, UPLEFT)) {
          uncover_cell_block(board, UPLEFT(board, index));
        }
        if (UNCOVER_BLOCK_CONDITION(board, index, UPRIGHT)) {
          uncover_cell_block(board, UPRIGHT(board, index));
        }
        index = DOWN(board, index);
        break;

      case BT_LEFT:
        if (UNCOVER_BLOCK_CONDITION(board, index, UPRIGHT)) {
          uncover_cell_block(board, UPRIGHT(board, index));
        }
        if (UNCOVER_BLOCK_CONDITION(board, index, DOWNRIGHT)) {
          uncover_cell_block(board, DOWNRIGHT(board, index));
        }
        index = LEFT(board, index);
        break;

      default:
        break;
      }
      CLEAR_BACKTRACK_DIR(board, prev_index);
    }
  } while (BACKTRACK_DIR(board, start_index) != 3);

  /* Do a final check on diagonals */
  if (UNCOVER_BLOCK_CONDITION(board, index, UPRIGHT)) {
    uncover_cell_block(board, UPRIGHT(board, index));
  }
  if (UNCOVER_BLOCK_CONDITION(board, index, DOWNRIGHT)) {
    uncover_cell_block(board, DOWNRIGHT(board, index));
  }
  if (UNCOVER_BLOCK_CONDITION(board, index, DOWNLEFT)) {
    uncover_cell_block(board, DOWNLEFT(board, index));
  }
  if (UNCOVER_BLOCK_CONDITION(board, index, UPLEFT)) {
    uncover_cell_block(board, UPLEFT(board, index));
  }
  CLEAR_BACKTRACK_DIR(board, start_index);
}

GameState_T check_game_condition(GameBoard_T *board, int index) {
  if (UNCOVERED(board, index) && HASBOMB(board, index)) {
    return EXPLODE;
  } else if (board->reamining == 0) {
    return WIN;
  } else {
    return TURNS;
  }
}

void generate_board(GameBoard_T *board, int rows, int columns) {
  board->height = rows;
  board->width = columns;
  board->board =
      (uint8_t *)calloc(board->width * board->height, sizeof(uint8_t));
  board->numBombs = 0;
  board->reamining = 0;
  board->game_state = BOARD_GENERATION;
}

int generate_bombs(GameBoard_T *state, int bombs) {
  // TODO: Instead of choosing all random spots, choose random points for
  // clusters of bombs
  // TODO: Generate bombs that create a radius around the initial click
  state->game_state = BOMB_GENERATION;
  for (int b = 0; b < bombs; b++) {
    int placement;
    do {
      placement = rand() % (state->width * state->height);
    } while (HASBOMB(state, placement));
    SET_HASBOMB(state, placement);
  }

  // Update the number of bombs around each cell
  for (int i = 0; i < state->height * state->width; i++) {
    int surrondingBombs = SURRONDING_CELL_STATE(state, i, HASBOMB);
    int numBombs;
    COUNT_BITS(numBombs, surrondingBombs);
    SET_NUMBOMBS(state, i, numBombs);
  }

  state->reamining = (state->width * state->height) - bombs;

  return 0;
}

int main(int argc, char **argv, char **envp) {
  GameBoard_T *game_board = (GameBoard_T *)malloc(sizeof(GameBoard_T));
  game_board->game_state = GAME_INIT;

  unsigned int rows, cols, bombs;
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <rows> <cols> <bombs>\n", argv[0]);
    exit(1);
  }

  srand(11);
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();

  start_color();
  init_pair(CELL_COVERED, COLOR_BLACK, COLOR_WHITE);
  init_pair(CELL_SELECTED, COLOR_GREEN, COLOR_GREEN);
  init_pair(CELL_FLAGGED, COLOR_YELLOW, COLOR_YELLOW);
  init_pair(CELL_CONTAINS_BOMB, COLOR_RED, COLOR_RED);
  init_pair(CELL_BACKTRACTED, COLOR_CYAN, COLOR_CYAN);

  if (str2int(&rows, argv[1], 10)) {
    fprintf(stderr, "Specified rows %s cannot be converted into an integer\n",
            argv[1]);
  }

  if (str2int(&cols, argv[2], 10)) {
    fprintf(stderr,
            "Specified columns %s cannot be converted into an integer\n",
            argv[2]);
  }

  if (str2int(&bombs, argv[3], 10)) {
    fprintf(
        stderr,
        "Specified number of bombs %s cannot be converted into an integer\n",
        argv[3]);
  }

  generate_board(game_board, rows, cols);

  generate_bombs(game_board, bombs);

  game_board->game_state = TURNS;
  CellAction_T next_action;
  int selected_index = 0;
  while (game_board->game_state == TURNS) {
    print_board(game_board, selected_index);

    next_action = do_cell_action(game_board, &selected_index);
    switch (next_action) {
    case UNCOVER:
      // We have to check for a explode condition before win condition due to
      // this logic
      uncover_cell_block(game_board, selected_index);
      break;

    case FLAG:
      if (!FLAGGED(game_board, selected_index)) {
        SET_FLAGGED(game_board, selected_index);
      } else {
        CLEAR_FLAGGED(game_board, selected_index);
      }
      break;

    case NONE:
    default:
      break;
    }

    game_board->game_state = check_game_condition(game_board, selected_index);
  }

  print_board(game_board, -1);
  if (game_board->game_state == WIN) {
    printw("WINNER\n");
    refresh();
  }
  getch();

  game_board->game_state = CLEANUP;
  endwin();
  if (game_board->board) {
    free(game_board->board);
  }
  free(game_board);
}