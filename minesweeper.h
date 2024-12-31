#include <stdint.h>

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

#define NUM_SCENES 4
#define GAMEBOARD_SCENE_ID 0
#define OPTIONS_SCENE_ID 1
#define WIN_SCENE_ID 2
#define LOOSE_SCENE_ID 3

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
  3-0: Number of surronding bombs (0-8)
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
  PanelManager_T *pm;
  PanelScene_T *ps;
  GameState_T game_state;
  PrintAction_T print_action;
  uint8_t *board;
  unsigned int height;
  unsigned int width;
  unsigned int current_cell;
  unsigned int num_bombs;
  unsigned int num_flags;
  unsigned int remaining_open_cells;
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

// Default cell: No surrounding bombs, does not have bomb, uncovered, not
// flagged,
#define DEFAULT_CELL (0b00100000)

// Invalid index: -1 (0xFFFF...) when unsigned
#define INVALID_INDEX (-1)

// Converts an index into a row/column and vice versa
#define CELL_INDEX(board, row, col) (row * board->width + col)
#define CELL_ROW(board, index) (index / board->width)
#define CELL_COL(board, index) (index % board->width)

#define CELL_ROW_CURSOR(board, index) (CELL_ROW(board, index))
#define CELL_COL_CURSOR(board, index) (CELL_COL(board, index) * CELL_STR_LEN)

// Checks if a cell index is within the bounds of the gameboard.
#define BOARD_BOUND_CHECK(board, index) ((unsigned int)index < board->width * board->height)

// Return a cell's contents if it's in the gameboard, default cell otherwise
#define CELL(board, index) (BOARD_BOUND_CHECK(board, index) ? *(board->board + index) : DEFAULT_CELL)

// Gets the contents of a cell without checking the index. Used when cell
// content needs to be modified
#define KNOWN_CELL(board, index) (*(board->board + index))

// Returns the index
#define INDEX_ROW_CHECK(board, index, new_index, row_offset)                                                           \
  ((BOARD_BOUND_CHECK(board, (new_index)) &&                                                                           \
    ((index) / board->width + row_offset) == (unsigned int)((new_index) / board->width))                               \
       ? new_index                                                                                                     \
       : INVALID_INDEX)

/* Gameboard adjacent cell indexing macros */
// Gets the surronding indexes at the provided index if they exist, -1 otherwise
#define NUM_DIRECTIONS 8
typedef unsigned int (*move_cell_func)(GameBoard_T *board, unsigned int index);

static inline unsigned int _index_up(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index - board->width, -1);
}

static inline unsigned int _index_upleft(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index - board->width - 1, -1);
}

static inline unsigned int _index_left(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index - 1, 0);
}

static inline unsigned int _index_downleft(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index + board->width - 1, 1);
}

static inline unsigned int _index_down(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index + board->width, 1);
}

static inline unsigned int _index_downright(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index + board->width + 1, 1);
}

static inline unsigned int _index_right(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index + 1, 0);
}

static inline unsigned int _index_upright(GameBoard_T *board, unsigned int index) {
  return INDEX_ROW_CHECK(board, index, index - board->width + 1, -1);
}

#define IS_CELL_ADJACENT(board, src_index, index)                                                                      \
  (_index_up(board, src_index) == index || _index_upleft(board, src_index) == index ||                                 \
   _index_left(board, src_index) == index || _index_downleft(board, src_index) == index ||                             \
   _index_down(board, src_index) == index || _index_downright(board, src_index) == index ||                            \
   _index_right(board, src_index) == index || _index_upright(board, src_index) == index)

#define SURRONDING_CELL_ACTION(board, index, ACTION)                                                                   \
  ACTION(board, _index_up(board, index));                                                                              \
  ACTION(board, _index_upleft(board, index));                                                                          \
  ACTION(board, _index_left(board, index));                                                                            \
  ACTION(board, _index_downleft(board, index));                                                                        \
  ACTION(board, _index_down(board, index));                                                                            \
  ACTION(board, _index_downright(board, index));                                                                       \
  ACTION(board, _index_right(board, index));                                                                           \
  ACTION(board, _index_upright(board, index))

// TODO: Switch if statements with (ACTION & (state & position))
#define SURRONDING_CELL_ACTION_STATEFUL(board, index, state, ACTION)                                                   \
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

// Checks the surronding cells for a provided state
#define SURRONDING_CELL_STATE(board, index, STATE)                                                                     \
  (STATE(board, _index_up(board, index)) << 7 | STATE(board, _index_upleft(board, index)) << 6 |                       \
   STATE(board, _index_left(board, index)) << 5 | STATE(board, _index_downleft(board, index)) << 4 |                   \
   STATE(board, _index_down(board, index)) << 3 | STATE(board, _index_downright(board, index)) << 2 |                  \
   STATE(board, _index_right(board, index)) << 1 | STATE(board, _index_upright(board, index)))

// TODO: Check how portable this is
#define COUNT_BITS(x) __builtin_popcount((unsigned int)x)

/* Cell state query and modification macros */
#define SET_NUMBOMBS(board, index, num)                                                                                \
  CLEAR_NUMBOMBS(board, index);                                                                                        \
  KNOWN_CELL(board, index) |= (num)
#define CLEAR_NUMBOMBS(board, index) (KNOWN_CELL(board, index) &= ~CELL_NUMBOMBS_BITS)
#define NUMBOMBS(board, index) (CELL(board, index) & CELL_NUMBOMBS_BITS)
#define ADJACENTBOMB(board, index) ((CELL(board, index) & CELL_NUMBOMBS_BITS) > 0)

#define SET_HASBOMB(board, index) (KNOWN_CELL(board, index) |= CELL_HASBOMB_BIT)
#define CLEAR_HASBOMB(board, index) (KNOWN_CELL(board, index) &= ~CELL_HASBOMB_BIT)
// #define TOGGLE_HASBOMB(board, index) (KNOWN_CELL(board, index) ^=
// ~KNOWN_CELL(board, index) & (HASBOMB(board, index))
#define HASBOMB(board, index) ((CELL(board, index) & CELL_HASBOMB_BIT))

#define SET_UNCOVERED(board, index) (KNOWN_CELL(board, index) |= CELL_UNCOVERED_BIT)
#define CLEAR_UNCOVERED(board, index) (KNOWN_CELL(board, index) &= ~CELL_UNCOVERED_BIT)
#define UNCOVERED(board, index) ((CELL(board, index) & CELL_UNCOVERED_BIT))

#define SET_FLAGGED(board, index) (KNOWN_CELL(board, index) |= CELL_FLAGGED_BIT)
#define CLEAR_FLAGGED(board, index) (KNOWN_CELL(board, index) &= ~CELL_FLAGGED_BIT)
#define FLAGGED(board, index) ((CELL(board, index) & CELL_FLAGGED_BIT))

#define SET_PRINTED(board, index) (KNOWN_CELL(board, index) |= CELL_PRINTED_BIT)
#define CLEAR_PRINTED(board, index) (KNOWN_CELL(board, index) &= ~CELL_PRINTED_BIT)
#define PRINTED(board, index) (CELL(board, index) & CELL_PRINTED_BIT)

#define SET_BACKTRACK_DIR(board, index, val)                                                                           \
  CLEAR_BACKTRACK_DIR(board, index);                                                                                   \
  KNOWN_CELL(board, index) |= (val & 0x0f)
#define CLEAR_BACKTRACK_DIR(board, index) (KNOWN_CELL(board, index) &= ~0x0f)
#define BACKTRACK_DIR(board, index) ((CELL(board, index) & 0x0f))
#define BT_UP 0
#define BT_DOWN 1
#define BT_RIGHT 2
#define BT_LEFT 3

#define UNCOVER_BLOCK_CONDITION(board, index) (!NUMBOMBS(board, index) && !UNCOVERED(board, index))

/* Explode sequence */
#define EXPLODE_SCENE_WIDTH 54
#define EXPLODE_SCENE_HEIGHT 16
void print_explode_sequence(PanelData_T *pd, void *opaque);
