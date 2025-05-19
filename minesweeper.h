#include <stdint.h>

#include "panel_manager.h"

/* Cell display macros */
static const unsigned int CELL_COVERED_DISPLAY = 10;
static const unsigned int CELL_SELECTED_DISPLAY = 11;
static const unsigned int CELL_FLAGGED_DISPLAY = 12;
static const unsigned int CELL_UNCOVERED_DISPLAY = 13;
static const unsigned int CELL_HASBOMB_DISPLAY = 14;
static const unsigned int CELL_BACKTRACKED_DISPLAY = 15;

static const unsigned int CELL_ONE_SURROUNDING_DISPLAY = 1;
static const unsigned int CELL_TWO_SURROUNDING_DISPLAY = 2;
static const unsigned int CELL_THREE_SURROUNDING_DISPLAY = 3;
static const unsigned int CELL_FOUR_SURROUNDING_DISPLAY = 4;
static const unsigned int CELL_FIVE_SURROUNDING_DISPLAY = 5;
static const unsigned int CELL_SIX_SURROUNDING_DISPLAY = 6;
static const unsigned int CELL_SEVEN_SURROUNDING_DISPLAY = 7;
static const unsigned int CELL_EIGHT_SURROUNDING_DISPLAY = 8;

#define CELL_FG_COLOR_ONE_SURROUNDING (10)
#define CELL_FG_COLOR_TWO_SURROUNDING (11)
#define CELL_FG_COLOR_THREE_SURROUNDING (12)
#define CELL_FG_COLOR_FOUR_SURROUNDING (13)
#define CELL_FG_COLOR_FIVE_SURROUNDING (14)
#define CELL_FG_COLOR_SIX_SURROUNDING (15)
#define CELL_FG_COLOR_SEVEN_SURROUNDING (16)
#define CELL_FG_COLOR_EIGHT_SURROUNDING (17)

static const unsigned int CELL_STR_LEN = 3;
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

static const unsigned int NUM_SCENES = 4;
static const unsigned int GAMEBOARD_SCENE_ID = 0;
static const unsigned int OPTIONS_SCENE_ID = 1;
static const unsigned int WIN_SCENE_ID = 2;
static const unsigned int LOOSE_SCENE_ID = 3;

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

/* Assume NONE = 0 */
typedef enum CellAction {
  NONE,
  MOVE,
  UNCOVER,
  FLAG,
  EXIT,
} CellAction_T;

typedef enum PrintAction { CELL_UPDATE = 1, HEADER_UPDATE, BOARD_REFRESH } PrintAction_T;

/* board (uint8_t) bitfields
  +------------+---+---+---+---+
  | 7 | 6 | 5 | 4 |    3-0     |
  +------------+---+---+---+---+
  7: Cell updates printed (0 means need print)
  6: Flagged
  5: Uncovered
  4: Has bomb
  3-0: Number of surrounding bombs (0-8)
     : When backtracking: Incoming direction
        0 = _index_up
        1 = UP_RIGHT
        2 = _index_right
        3 = DOWN_RIGHT
        4 = _index_down
        5 = DOWN_LEFT
        6 = _index_left
        7 = UP_LEFT
*/
#define CELL_PRINTED_BIT (1 << 7)
#define CELL_FLAGGED_BIT (1 << 6)
#define CELL_UNCOVERED_BIT (1 << 5)
#define CELL_HASBOMB_BIT (1 << 4)
#define CELL_NUMBOMBS_BITS (0x0f)
#define CELL_BACKTRACK_BITS (0x0f)

typedef struct GameBoard {
  /* Display data */
  PanelManager_T *pm;
  PanelScene_T *active_scene;
  PrintAction_T print_action;

  /* Board data */
  uint8_t *board;
  unsigned int height;
  unsigned int width;
  unsigned int num_bombs;
  unsigned int num_flags;
  unsigned int remaining_open_cells;

  /* Player data */
  unsigned int curr_index;

  /* State data */
  GameState_T game_state;
  unsigned int seconds_elapsed;
  int timeout;
  int is_first_turn;
  int refresh_board_print;
} GameBoard_T;

/* Based on: https://stackoverflow.com/a/12923949 */
typedef enum {
  STR2INT_SUCCESS = 0,
  STR2INT_OVERFLOW,
  STR2INT_UNDERFLOW,
  STR2INT_INCONVERTIBLE,
  STR2INT_EMPTY,
} str2int_errno;

#define PRINT_CELL_WITH_COLOR(win, color, prints)                                                                      \
  wattron(win, COLOR_PAIR(color));                                                                                     \
  prints;                                                                                                              \
  wattroff(win, COLOR_PAIR(color))

// #define CELL_COVERED  "\x1B[47m" _COVERED "\x1B[0m"
// #define CELL_SELECTED "\x1B[42m" _SELECTED "\x1B[0m"
// #define CELL_FLAGGED  "\x1B[41m" _FLAGGED "\x1B[0m"
// #define CELL_UNCOVERED  "\x1B[0m" _UNCOVERED "\x1B[0m"

/* Gameboard cell indexing macros */
// Converts a (row, col) index into a one-dimensional offset
// #define INDEX(board, row, col)          ((row*board->width)+col)

/* Default cell
  7: Printed: 0
  6: Flagged: 0
  5: Uncovered: 0
  4: Has bomb: 0
  3-0: Number of surrounding bombs (0-8): 0
*/
static const unsigned int DEFAULT_CELL = 0b00000000;

// Invalid index: -1 (0xFFFF...) when unsigned
static const unsigned int INVALID_INDEX = -1;

// Converts an index into a row/column and vice versa
#define CELL_INDEX(board, row, col) ((row) * board->width + (col))
#define CELL_ROW(board, index) (index / board->width)
#define CELL_COL(board, index) (index % board->width)

#define CELL_ROW_CURSOR(board, index) (CELL_ROW(board, index))
#define CELL_COL_CURSOR(board, index) (CELL_COL(board, index) * CELL_STR_LEN)

// Checks if a cell index is within the bounds of the gameboard.
#define INDEX_ON_BOARD(board, index) ((unsigned int)(index < board->width * board->height))
#define ROW_ON_BOARD(board, row) ((unsigned int)(row) < board->height)
#define COL_ON_BOARD(board, col) ((unsigned int)(col) < board->width)

// Return a cell's contents if it's in the gameboard, default cell otherwise
#define CELL_KNOWN(board, index) (*(board->board + (index)))
#define CELL(board, index) (INDEX_ON_BOARD(board, index) ? CELL_KNOWN(board, index) : DEFAULT_CELL)

/* Gameboard adjacent cell indexing macros */
// Gets the surrounding indexes at the provided index if they exist, INVALID_INDEX otherwise
static const unsigned int NUM_DIRECTIONS = 8;
typedef unsigned int (*move_cell_func)(GameBoard_T *board, unsigned int index);

static inline unsigned int _index_up(GameBoard_T *board, unsigned int index) {
  unsigned int _row = CELL_ROW(board, index);
  return (ROW_ON_BOARD(board, (_row - 1))) ? (index - board->width) : INVALID_INDEX;
}

static inline unsigned int _index_upleft(GameBoard_T *board, unsigned int index) {
  unsigned int _row = CELL_ROW(board, index);
  unsigned int _col = CELL_COL(board, index);
  return (ROW_ON_BOARD(board, (_row - 1)) && COL_ON_BOARD(board, (_col - 1))) ? (index - board->width - 1)
                                                                              : INVALID_INDEX;
}

static inline unsigned int _index_left(GameBoard_T *board, unsigned int index) {
  unsigned int _col = CELL_COL(board, index);
  return (COL_ON_BOARD(board, (_col - 1))) ? (index - 1) : INVALID_INDEX;
}

static inline unsigned int _index_downleft(GameBoard_T *board, unsigned int index) {
  unsigned int _row = CELL_ROW(board, index);
  unsigned int _col = CELL_COL(board, index);
  return (ROW_ON_BOARD(board, (_row + 1)) && COL_ON_BOARD(board, (_col - 1))) ? (index + board->width - 1)
                                                                              : INVALID_INDEX;
}

static inline unsigned int _index_down(GameBoard_T *board, unsigned int index) {
  unsigned int _row = CELL_ROW(board, index);
  return (ROW_ON_BOARD(board, (_row + 1))) ? (index + board->width) : INVALID_INDEX;
}

static inline unsigned int _index_downright(GameBoard_T *board, unsigned int index) {
  unsigned int _row = CELL_ROW(board, index);
  unsigned int _col = CELL_COL(board, index);
  return (ROW_ON_BOARD(board, (_row + 1)) && COL_ON_BOARD(board, (_col + 1))) ? (index + board->width + 1)
                                                                              : INVALID_INDEX;
}

static inline unsigned int _index_right(GameBoard_T *board, unsigned int index) {
  unsigned int _col = CELL_COL(board, index);
  return (COL_ON_BOARD(board, (_col + 1))) ? (index + 1) : INVALID_INDEX;
}

static inline unsigned int _index_upright(GameBoard_T *board, unsigned int index) {
  unsigned int _row = CELL_ROW(board, index);
  unsigned int _col = CELL_COL(board, index);
  return (ROW_ON_BOARD(board, (_row - 1)) && COL_ON_BOARD(board, (_col + 1))) ? (index - board->width + 1)
                                                                              : INVALID_INDEX;
}

#define CELL_IS_ADJACENT(board, src_index, index)                                                                      \
  (_index_up(board, src_index) == index || _index_upleft(board, src_index) == index ||                                 \
   _index_left(board, src_index) == index || _index_downleft(board, src_index) == index ||                             \
   _index_down(board, src_index) == index || _index_downright(board, src_index) == index ||                            \
   _index_right(board, src_index) == index || _index_upright(board, src_index) == index)

#define SURROUNDING_CELL_ACTION(board, index, ACTION)                                                                  \
  ACTION(board, _index_up(board, index));                                                                              \
  ACTION(board, _index_upleft(board, index));                                                                          \
  ACTION(board, _index_left(board, index));                                                                            \
  ACTION(board, _index_downleft(board, index));                                                                        \
  ACTION(board, _index_down(board, index));                                                                            \
  ACTION(board, _index_downright(board, index));                                                                       \
  ACTION(board, _index_right(board, index));                                                                           \
  ACTION(board, _index_upright(board, index))

// TODO: Switch if statements with (ACTION & (state & position))
#define SURROUNDING_CELL_ACTION_STATEFUL(board, index, state, ACTION)                                                  \
  if (state & 0x80)                                                                                                    \
    ACTION(board, _index_up(board, index));                                                                            \
  if (state & 0x40)                                                                                                    \
    ACTION(board, _index_upleft(board, index));                                                                        \
  if (state & 0x20)                                                                                                    \
    ACTION(board, _index_left(board, index));                                                                          \
  if (state & 0x10)                                                                                                    \
    ACTION(board, _index_downleft(board, index));                                                                      \
  if (state & 0x08)                                                                                                    \
    ACTION(board, _index_down(board, index));                                                                          \
  if (state & 0x04)                                                                                                    \
    ACTION(board, _index_downright(board, index));                                                                     \
  if (state & 0x02)                                                                                                    \
    ACTION(board, _index_right(board, index));                                                                         \
  if (state & 0x01)                                                                                                    \
  ACTION(board, _index_upright(board, index))

// Checks the surrounding cells for a provided state
#define SURROUNDING_CELL_STATE(board, index, STATE)                                                                    \
  (STATE(board, _index_up(board, index)) << 7 | STATE(board, _index_upleft(board, index)) << 6 |                       \
   STATE(board, _index_left(board, index)) << 5 | STATE(board, _index_downleft(board, index)) << 4 |                   \
   STATE(board, _index_down(board, index)) << 3 | STATE(board, _index_downright(board, index)) << 2 |                  \
   STATE(board, _index_right(board, index)) << 1 | STATE(board, _index_upright(board, index)))

// TODO: Check how portable this is
#define COUNT_BITS(x) __builtin_popcount((unsigned int)x)

/* Cell state query and modification macros */
#define CELL_NUMBOMBS(board, index) (CELL(board, index) & CELL_NUMBOMBS_BITS)
#define CELL_CLEAR_NUMBOMBS(board, index) (CELL_KNOWN(board, index) &= ~CELL_NUMBOMBS_BITS)
#define CELL_SET_NUMBOMBS(board, index, num)                                                                           \
  CELL_CLEAR_NUMBOMBS(board, index);                                                                                   \
  CELL_KNOWN(board, index) |= (num)
#define ADJACENTBOMB(board, index) ((CELL(board, index) & CELL_NUMBOMBS_BITS) > 0)

#define CELL_HASBOMB(board, index) ((CELL(board, index) & CELL_HASBOMB_BIT))
#define CELL_CLEAR_HASBOMB(board, index) (CELL_KNOWN(board, index) &= ~CELL_HASBOMB_BIT)
#define CELL_SET_HASBOMB(board, index) (CELL_KNOWN(board, index) |= CELL_HASBOMB_BIT)

#define CELL_CLEAR_UNCOVERED(board, index) (CELL_KNOWN(board, index) &= ~CELL_UNCOVERED_BIT)
#define CELL_SET_UNCOVERED(board, index) (CELL_KNOWN(board, index) |= CELL_UNCOVERED_BIT)
#define CELL_UNCOVERED(board, index) ((CELL(board, index) & CELL_UNCOVERED_BIT))

#define CELL_CLEAR_FLAGGED(board, index) (CELL_KNOWN(board, index) &= ~CELL_FLAGGED_BIT)
#define CELL_SET_FLAGGED(board, index) (CELL_KNOWN(board, index) |= CELL_FLAGGED_BIT)
#define CELL_FLAGGED(board, index) ((CELL(board, index) & CELL_FLAGGED_BIT))

#define CELL_CLEAR_PRINTED(board, index) (CELL_KNOWN(board, index) &= ~CELL_PRINTED_BIT)
#define CELL_SET_PRINTED(board, index) (CELL_KNOWN(board, index) |= CELL_PRINTED_BIT)
#define CELL_PRINTED(board, index) (CELL(board, index) & CELL_PRINTED_BIT)

#define SET_BACKTRACK_DIR(board, index, val)                                                                           \
  CLEAR_BACKTRACK_DIR(board, index);                                                                                   \
  CELL_KNOWN(board, index) |= (val & 0x0f)
#define CLEAR_BACKTRACK_DIR(board, index) (CELL_KNOWN(board, index) &= ~0x0f)
#define BACKTRACK_DIR(board, index) ((CELL(board, index) & 0x0f))
#define BT_UP 0
#define BT_DOWN 1
#define BT_RIGHT 2
#define BT_LEFT 3

#define UNCOVER_BLOCK_CONDITION(board, index) (!CELL_NUMBOMBS(board, index) && !CELL_UNCOVERED(board, index))
#define PLACE_BOMB_CONDITION(board, index)                                                                             \
  (index != board->curr_index || CELL_HASBOMB(board, index) || CELL_IS_ADJACENT(board, board->curr_index, index))

/* Explode sequence */
#define EXPLODE_SCENE_WIDTH 54
#define EXPLODE_SCENE_HEIGHT 16
void print_explode_sequence(PanelData_T *pd, void *opaque);
