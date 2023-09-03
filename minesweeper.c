#include <bits/time.h>
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
#include <time.h>

#include "panel_manager.h"

/* Cell display macros */
#define CELL_COVERED 10
#define CELL_SELECTED 11
#define CELL_FLAGGED 12
#define CELL_UNCOVERED 13
#define CELL_CONTAINS_BOMB 14
#define CELL_BACKTRACTED 15

#define CELL_ONE_SURRONDING 1
#define CELL_TWO_SURRONDING 2
#define CELL_THREE_SURRONDING 3
#define CELL_FOUR_SURRONDING 4
#define CELL_FIVE_SURRONDING 5
#define CELL_SIX_SURRONDING 6
#define CELL_SEVEN_SURRONDING 7
#define CELL_EIGHT_SURRONDING 8

#define CELL_STR_LEN 3
#define CELL_UNCOVERED_STR " %c "
#ifndef DEBUG
#define CELL_COVERED_STR "   "
#define CELL_SELECTED_STR "   "
#define CELL_FLAGGED_STR " \u2691 "
#define CELL_HASBOMB_STR " \U0001F4A3 "
#else
#define CELL_COVERED_STR "[C]"
#define CELL_SELECTED_STR "[S]"
#define CELL_FLAGGED_STR "[F]"
#define CELL_HASBOMB_STR "[B]"
#endif

#define PRINT_CELL_WITH_COLOR(color, prints)                                   \
  attron(COLOR_PAIR(color));                                                   \
  prints;                                                                      \
  attroff(COLOR_PAIR(color))

// #define CELL_COVERED  "\x1B[47m" _COVERED "\x1B[0m"
// #define CELL_SELECTED "\x1B[42m" _SELECTED "\x1B[0m"
// #define CELL_FLAGGED  "\x1B[41m" _FLAGGED "\x1B[0m"
// #define CELL_UNCOVERED  "\x1B[0m" _UNCOVERED "\x1B[0m"

/* Gameboard cell indexing macros */
// Converts a (row, col) index into a one-dimensional offset
// #define INDEX(board, row, col)          ((row*board->width)+col)

// Default cell: No surronding bombs, does not have bomb, uncovered, not flagged,
#define DEFAULT_CELL (0b00100000)

// Invalid index: -1 (0xFFFF...) when unsigned
#define INVALID_INDEX (-1)

// Converts an index into a row/column and vice versa
#define CELL_INDEX(board, row, col) (row*board->width+col)
#define CELL_ROW(board, index) (index/board->width)
#define CELL_COL(board, index) (index%board->width)

#define CELL_ROW_CURSOR(board, index) (CELL_ROW(board, index))
#define CELL_COL_CURSOR(board, index) (CELL_COL(board, index)*CELL_STR_LEN)

// Checks if a cell index is within the bounds of the gameboard.
#define CELL_BOUND_CHECK(board, index) ((unsigned int)index < board->width * board->height)

// Return a cell's contents if it's in the gameboard, default cell otherwise
#define CELL(board, index)                                                    \
  (CELL_BOUND_CHECK(board, index) ? *(board->board + index) : DEFAULT_CELL)

// Gets the contents of a cell without checking the index. Used when cell content needs to be modified
#define KNOWN_CELL(board, index) (*(board->board + index))

// Returns the index if it is in the expected row, -1 otherwise
#define INDEX_IN_ROW(board, index, row)                                       \
  ((CELL_BOUND_CHECK(board, index) && ((index) / board->width) == (row))     \
       ? (index)                                                               \
       : INVALID_INDEX)

/* Gameboard adjacent cell indexing macros */
// Gets the surronding indexes at the provided index if they exist, -1 otherwise
#define UP(board, index)                                                      \
  (INDEX_IN_ROW(board, index - board->width, (index / board->width) - 1))
#define UPLEFT(board, index)                                                  \
  (INDEX_IN_ROW(board, index - board->width - 1, (index / board->width) - 1))
#define LEFT(board, index)                                                    \
  (INDEX_IN_ROW(board, index - 1, index / board->width))
#define DOWNLEFT(board, index)                                                \
  (INDEX_IN_ROW(board, index + board->width - 1, (index / board->width) + 1))
#define DOWN(board, index)                                                    \
  (INDEX_IN_ROW(board, index + board->width, (index / board->width) + 1))
#define DOWNRIGHT(board, index)                                               \
  (INDEX_IN_ROW(board, index + board->width + 1, (index / board->width) + 1))
#define RIGHT(board, index)                                                   \
  (INDEX_IN_ROW(board, index + 1, index / board->width))
#define UPRIGHT(board, index)                                                 \
  (INDEX_IN_ROW(board, index - board->width + 1, (index / board->width) - 1))

#define SURRONDING_CELL_ACTION(board, index, ACTION)                          \
  ACTION(board, UP(board, index));                                           \
  ACTION(board, UPLEFT(board, index));                                       \
  ACTION(board, LEFT(board, index));                                         \
  ACTION(board, DOWNLEFT(board, index));                                     \
  ACTION(board, DOWN(board, index));                                         \
  ACTION(board, DOWNRIGHT(board, index));                                    \
  ACTION(board, RIGHT(board, index));                                        \
  ACTION(board, UPRIGHT(board, index))

//TODO: Switch if statements with (ACTION & (state & position))
#define SURRONDING_CELL_ACTION_STATEFUL(board, index, state, ACTION)          \
  if (state & 0x80)                                                            \
    ACTION(board, UP(board, index));                                         \
  if (state & 0x40)                                                            \
    ACTION(board, UPLEFT(board, index));                                     \
  if (state & 0x20)                                                            \
    ACTION(board, LEFT(board, index));                                       \
  if (state & 0x10)                                                            \
    ACTION(board, DOWNLEFT(board, index));                                   \
  if (state & 0x08)                                                            \
    ACTION(board, DOWN(board, index));                                       \
  if (state & 0x04)                                                            \
    ACTION(board, DOWNRIGHT(board, index));                                  \
  if (state & 0x02)                                                            \
    ACTION(board, RIGHT(board, index));                                      \
  if (state & 0x01)                                                            \
  ACTION(board, UPRIGHT(board, index))

// Checks the surronding cells for a provided state
#define SURRONDING_CELL_STATE(board, index, STATE)                            \
  (STATE(board, UP(board, index)) << 7 |                                     \
   STATE(board, UPLEFT(board, index)) << 6 |                                 \
   STATE(board, LEFT(board, index)) << 5 |                                   \
   STATE(board, DOWNLEFT(board, index)) << 4 |                               \
   STATE(board, DOWN(board, index)) << 3 |                                   \
   STATE(board, DOWNRIGHT(board, index)) << 2 |                              \
   STATE(board, RIGHT(board, index)) << 1 |                                  \
   STATE(board, UPRIGHT(board, index)))

//TODO: Write portable routine
/* This is not portable, but it is fast */
#define COUNT_BITS(result, source)                                             \
  __asm__ __volatile__("popcnt %0, %1;" : "=r"(result) : "r"(source))
// #define COUNT_BITS(result, number)  for(int i = 0; i < sizeof(number); i++) { \
//                                       (result) += (number & 0x01); \
//                                     }

/* Cell state query and modification macros */
#define SET_NUMBOMBS(board, index, num)                                       \
  CLEAR_NUMBOMBS(board, index);                                               \
  KNOWN_CELL(board, index) |= (num)
#define CLEAR_NUMBOMBS(board, index) (KNOWN_CELL(board, index) &= ~CELL_NUMBOMBS_BITS)
#define NUMBOMBS(board, index) (CELL(board, index) & CELL_NUMBOMBS_BITS)
#define ADJACENTBOMB(board, index) ((CELL(board, index) & CELL_NUMBOMBS_BITS) > 0)

#define SET_HASBOMB(board, index) (KNOWN_CELL(board, index) |= CELL_HASBOMB_BIT)
#define CLEAR_HASBOMB(board, index) (KNOWN_CELL(board, index) &= ~CELL_HASBOMB_BIT)
#define HASBOMB(board, index) ((CELL(board, index) & CELL_HASBOMB_BIT))

#define SET_UNCOVERED(board, index) (KNOWN_CELL(board, index) |= CELL_UNCOVERED_BIT)
#define CLEAR_UNCOVERED(board, index) (KNOWN_CELL(board, index) &= ~CELL_UNCOVERED_BIT)
#define UNCOVERED(board, index) ((CELL(board, index) & CELL_UNCOVERED_BIT))

#define SET_FLAGGED(board, index) (KNOWN_CELL(board, index) |= CELL_FLAGGED_BIT)
#define CLEAR_FLAGGED(board, index) (KNOWN_CELL(board, index) &= ~CELL_FLAGGED_BIT)
#define FLAGGED(board, index) ((CELL(board, index) & CELL_FLAGGED_BIT))

#define SET_UPDATED(board, index) (KNOWN_CELL(board, index) |= CELL_UPDATED_BIT)
#define CLEAR_UPDATED(board, index) (KNOWN_CELL(board, index) &= ~CELL_UPDATED_BIT)
#define UPDATED(board, index) (CELL(board, index) & CELL_UPDATED_BIT)

#define SET_BACKTRACK_DIR(board, index, val)                                  \
  CLEAR_BACKTRACK_DIR(board, index);                                          \
  KNOWN_CELL(board, index) |= ((val & 0x03) << 6)
#define CLEAR_BACKTRACK_DIR(board, index) (KNOWN_CELL(board, index) &= ~0xc0)
#define BACKTRACK_DIR(board, index) ((CELL(board, index) & 0xc0) >> 6)
#define BT_UP 0
#define BT_DOWN 1
#define BT_RIGHT 2
#define BT_LEFT 3

#define UNCOVER_BLOCK_CONDITION(board, index, DIR)                            \
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
  QUIT,
  TIMEOUT,
  WIN,
  CLEANUP,
} GameState_T;

char* GameStateStr[] = {
  "Generating game...",
  "Creating board... ",
  "Placing bombs...  ",
  "Make an action!   ",
  "Bomb exploded!    ",
  "Game exited       ",
  "Timer expired     ",
  "Congratulations!  ",
  "Cleaning up...    "
};

/* Assume NONE = 0 */
#define NONE 0
typedef enum CellAction {
  MOVE = 1,
  UNCOVER,
  FLAG,
  EXIT,
} CellAction_T;

typedef enum PrintAction {
  CELL_UPDATE = 1,
  HEADER_UPDATE,
  BOARD_REFRESH
} PrintAction_T;

/* board (uint8_t) bitfields
  +------------+---+---+---+---+
  | 7 | 6 | 5 | 4 |    3-0     |
  +------------+---+---+---+---+
  7: Modified state
  6: Flagged
  5: Uncovered
  4: Has bomb
  3-0: Number of surronding bombs (0-8)
     : When backtracking: Incoming direction
*/
#define CELL_UPDATED_BIT (1 << 7)
#define CELL_FLAGGED_BIT (1 << 6)
#define CELL_UNCOVERED_BIT (1 << 5)
#define CELL_HASBOMB_BIT (1 << 4)
#define CELL_NUMBOMBS_BITS (0x0f)

typedef struct GameBoard {
  PanelManager_T *pm;
  GameState_T game_state;
  PrintAction_T print_action;
  uint8_t *board;
  unsigned int height;
  unsigned int width;
  unsigned int current_cell;
  unsigned int numBombs;
  unsigned int reamining;
  unsigned seconds_remaining;
  int timeout;
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

CellAction_T do_cell_action(GameBoard_T* board) {
  CellAction_T action;
  unsigned int pending_index = INVALID_INDEX;
  struct timespec start, stop;

  /* Get the start time and set the timeout for this action */
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  timeout(board->timeout);
  switch (getch()) {
  /* Flag cell */
  case 'f':
  case 'F':
    action = FLAG;
    break;

  /* Uncover cell */
  case 'u':
  case 'U':
    action = UNCOVER;
    break;

  /* Exit game (ESC)*/
  case 27:
    action = EXIT;
    break;

  /* Move up a cell */
  case KEY_UP:
    pending_index = UP(board, board->current_cell);
    break;

  /* Move down a cell */
  case KEY_DOWN:
    pending_index = DOWN(board, board->current_cell);
    break;

  /* Move right a cell */
  case KEY_RIGHT:
    pending_index = RIGHT(board, board->current_cell);
    break;

  /* Move left a cell */
  case KEY_LEFT:
    pending_index = LEFT(board, board->current_cell);
    break;

  /* Do nothing */
  default:
    action = NONE;
    break;
  }

  if (CELL_BOUND_CHECK(board, pending_index)) {
    board->current_cell = pending_index;
    action = MOVE;
  }

  /* How long did this action take in ms? */
  clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
  int diff = (stop.tv_sec - start.tv_sec)*1000 + (stop.tv_nsec - start.tv_nsec)/1000000;
  board->timeout -= diff;

  return action;
}

void print_cell_contents(GameBoard_T *board, unsigned int index) {
  if (board->current_cell == index) {
      PRINT_CELL_WITH_COLOR(CELL_SELECTED, addstr(CELL_SELECTED_STR));

  } else if (UNCOVERED(board, index)) {
    if (HASBOMB(board, index)) {
      PRINT_CELL_WITH_COLOR(CELL_CONTAINS_BOMB, addstr(CELL_HASBOMB_STR));
    } else {
      int bombs = NUMBOMBS(board, index);
      PRINT_CELL_WITH_COLOR(bombs,
        printw(CELL_UNCOVERED_STR, (bombs) ? bombs + '0' : ' ');
      );
    }

  } else if (FLAGGED(board, index)) {
    PRINT_CELL_WITH_COLOR(CELL_FLAGGED, addstr(CELL_FLAGGED_STR));

#ifdef DEBUG
  } else if (HASBOMB(board, index)) {
    PRINT_CELL_WITH_COLOR(CELL_CONTAINS_BOMB, addstr(CELL_COVERED_STR));
#endif

  } else {
    PRINT_CELL_WITH_COLOR(CELL_COVERED, addstr(CELL_COVERED_STR));
  }
}

void print_headers(WINDOW* win, void *opaque) {
  GameBoard_T *board = (GameBoard_T*)opaque;
  wmove(win, 0, 0);
  waddstr(win, GameStateStr[board->game_state]);
  move(0, getmaxx(win)-3);
  wprintw(win, "%03d", board->seconds_remaining);
  wrefresh(win);
}

// void print_board_cell(WINDOW* win, GameBoard_T *board, unsigned int index, unsigned int select) {
//   wmove(win, 0, 0);
//   print_cell_contents(board, index, select);
//   /* Move the cursor out of the gameboard */
//   //FIXME: This is now broken
// #ifdef DEBUG
//   move(board->y_win_position+board->height, board->x_win_position);
//   printw("Current cell: ");
//   print_bits(CELL(board, select));
//   printw("\tNUMBOMBS: %d\tHASBOMB: %d\tUNCOVERED: %d\tFLAGGED: %d\n",
//          NUMBOMBS(board, select), HASBOMB(board, select),
//          UNCOVERED(board, select), FLAGGED(board, select));
//   printw("Remaining: %d\n", board->reamining);
// #endif
//   move(0,0);
//   refresh();
// }

void print_board(WINDOW* win, GameBoard_T *board) {
  wmove(win, 0, 0);
  int curr_row = 0;
  for (unsigned int index = 0; index < board->height * board->width; index++) {
    /* If the cell has been updated, print it again */
    if (UPDATED(board, index)) {
      print_cell_contents(board, index);
    }

    /* Move down to the next row to print */
    if (!((index + 1) % board->width)) {
      move(++curr_row, 0);
      wrefresh(win);
    }
  }
}

void uncover_cell_block(GameBoard_T *board, unsigned int index) {
  unsigned int start_index = index;
  unsigned int prev_index = index;

  if (UNCOVERED(board, index)) {
    return;
  }

  // Uncover only 1 cell if it has a bomb in it
  if (NUMBOMBS(board, index) || HASBOMB(board, index)) {
    SET_UNCOVERED(board, index);
    board->reamining--;
    return;
  }

  // Traverse open space
  /* TODO: Refactor this to use bits 0-3 to keep track of backtrack index */
  do {
    if (index == start_index) {
      unsigned int dir_counter = BACKTRACK_DIR(board, start_index);
      SET_BACKTRACK_DIR(board, start_index, (dir_counter + 1));
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
    // print_board_refresh(board, index);
    // print_bits(adjacent_bombs_bits);
    // print_bits(surronding_uncovered_bits);
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

GameState_T check_game_condition(GameBoard_T *board, unsigned int index) {
  if (board->game_state == QUIT) {
    return QUIT;
  } else if (UNCOVERED(board, index) && HASBOMB(board, index)) {
    return EXPLODE;
  } else if (board->reamining == 0) {
    return WIN;
  } else {
    return TURNS;
  }
}

int terminal_setup(GameBoard_T* board, unsigned int rows, unsigned int columns) {
  int max_x = getmaxx(stdscr);
  int max_y = getmaxy(stdscr);
  if (max_y < 5 + board->height || max_x < 2 + board->width) {
    return 0;
  }
  board->pm = pm_init(3);
  // TODO: Support scroling
  int y_align = getmaxy(stdscr)/2 - rows/2;
  int x_align = getmaxx(stdscr)/2 - (columns*CELL_STR_LEN)/2;
  /* (bottom) stdstr -> background -> headers -> game board (top)*/
  pm_panel_init(board->pm, 0, 0, max_y, max_x, "background", NULL, NULL, NULL);
  pm_panel_init(board->pm, y_align-1, x_align, 3, 3, "headers", print_headers, NULL, NULL);
  pm_panel_init(board->pm, y_align, x_align, board->height, board->width, "gameboard", NULL, NULL, NULL);
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  start_color();
  init_pair(CELL_COVERED, COLOR_BLACK, COLOR_WHITE);
  init_pair(CELL_SELECTED, COLOR_WHITE, COLOR_GREEN);
  init_pair(CELL_FLAGGED, COLOR_WHITE, COLOR_YELLOW);
  init_pair(CELL_CONTAINS_BOMB, COLOR_WHITE, COLOR_RED);
  init_pair(CELL_BACKTRACTED, COLOR_WHITE, COLOR_CYAN);

  init_pair(CELL_ONE_SURRONDING, COLOR_BLUE, COLOR_BLACK);
  init_pair(CELL_TWO_SURRONDING, COLOR_GREEN, COLOR_BLACK);
  init_pair(CELL_THREE_SURRONDING, COLOR_RED, COLOR_BLACK);
  init_pair(CELL_FOUR_SURRONDING, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(CELL_FIVE_SURRONDING, COLOR_BLACK, COLOR_BLACK);
  init_pair(CELL_SIX_SURRONDING, COLOR_CYAN, COLOR_BLACK);
  init_pair(CELL_SEVEN_SURRONDING, COLOR_BLACK, COLOR_BLACK);
  init_pair(CELL_EIGHT_SURRONDING, COLOR_BLACK, COLOR_BLACK);

  return 1;
}

void generate_board(GameBoard_T *board, unsigned int rows,
                    unsigned int columns) {
  board->game_state = BOARD_GENERATION;
  board->height = rows;
  board->width = columns;
  board->board =
      (uint8_t *)calloc(board->width * board->height, sizeof(uint8_t));
  board->current_cell = 0;
  board->numBombs = 0;
  board->reamining = 0;
  board->current_cell = 0;
  board->seconds_remaining = 300;
  board->timeout = 1000;    /* 1000 ms */
}

void welcome_screen(void) {
  /* https://patorjk.com/software/taag/#p=display&v=0&f=Sub-Zero&t=Minesweeper:
   * Subzero, default width/height*/
}

int generate_bombs(GameBoard_T *board, int bombs) {
  // TODO: Instead of choosing all random spots, choose random points for
  // clusters of bombs
  // TODO: Generate bombs that create a radius around the initial click
  board->game_state = BOMB_GENERATION;
  for (int b = 0; b < bombs; b++) {
    int placement;
    do {
      placement = rand() % (board->width * board->height);
    } while (HASBOMB(board, placement));
    SET_HASBOMB(board, placement);
  }

  // Update the number of bombs around each cell
  for (int i = 0; i < board->height * board->width; i++) {
    int surrondingBombs = SURRONDING_CELL_STATE(board, i, HASBOMB);
    int numBombs;
    COUNT_BITS(numBombs, surrondingBombs);
    SET_NUMBOMBS(board, i, numBombs);
  }

  board->reamining = (board->width * board->height) - bombs;
  return 0;
}

int main(int argc, char **argv, char **envp) {
  GameBoard_T *board = (GameBoard_T *)malloc(sizeof(GameBoard_T));
  board->game_state = GAME_INIT;

  unsigned int rows, cols, bombs;
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <rows> <cols> <bombs>\n", argv[0]);
    exit(1);
  }

  srand(11);

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

  box(stdscr, 0, 0);
  if (!terminal_setup(board, rows, cols)) {
    printw("Terminal initialization failed. Exiting...\n");
  } else {
    generate_bombs(board, bombs);

    board->game_state = TURNS;
    CellAction_T next_action;
    unsigned int prev_index;

    print_board(board, board->current_cell);
    while (board->game_state == TURNS) {
      next_action = *(CellAction_T*)pm_do_active_keypress_handler((void*)board);
      if (board->timeout <= 0) {
        board->timeout = 1000;
        board->seconds_remaining--;
      }
      switch (next_action) {
      case UNCOVER:
        // We have to check for a explode condition before win condition due to
        // this logic
        uncover_cell_block(board, selected_index);
        print_board_refresh(board, selected_index);
        break;

      case FLAG:
        if (!FLAGGED(board, selected_index)) {
          SET_FLAGGED(board, selected_index);
        } else {
          CLEAR_FLAGGED(board, selected_index);
        }
        break;

      case EXIT:
        board->game_state = QUIT;
        break;

      case MOVE:
        print_board_cell(board, prev_index, selected_index);
        break;

      case NONE:
      default:
        break;
      }

      print_headers(board);
      print_board_cell(board, selected_index, selected_index);
      board->game_state = check_game_condition(board, selected_index);
    }

    print_board_refresh(board, -1);
  }
  printw("Press any key to continue...")
  getch();

  board->game_state = CLEANUP;
  endwin();
  if (board->board) {
    free(board->board);
  }
  free(board);

  return 0;
}