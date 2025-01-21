#include <bits/time.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <panel.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "minesweeper.h"

const char *GameStateStr[] = {"Generating game...", "Creating board... ", "Placing bombs...  ",
                              "Make an action!   ", "Bomb exploded!    ", "Game exited       ",
                              "Timer expired     ", "Congratulations!  ", "Cleaning up...    "};

move_cell_func MOVE_CELL_ACTIONS[8] = {_index_up,   _index_upleft,    _index_left,  _index_downleft,
                                       _index_down, _index_downright, _index_right, _index_upright};

// Num is assumed to be a uint8_t
#define print_bits(num)                                                                                                \
  {                                                                                                                    \
    uint8_t n = num & 0xFF;                                                                                            \
    int c = 8;                                                                                                         \
    while (c > 0) {                                                                                                    \
      printw("%c", (n & 0x80) ? '1' : '0');                                                                            \
      n = n << 1;                                                                                                      \
      c--;                                                                                                             \
    }                                                                                                                  \
    printw("\n");                                                                                                      \
    refresh();                                                                                                         \
  }

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
// TODO: Replace this with a menu to increase the number of bombs
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

CellAction_T do_cell_action(GameBoard_T *board) {
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
    pending_index = _index_up(board, board->current_cell);
    break;

  /* Move down a cell */
  case KEY_DOWN:
    pending_index = _index_down(board, board->current_cell);
    break;

  /* Move right a cell */
  case KEY_RIGHT:
    pending_index = _index_right(board, board->current_cell);
    break;

  /* Move left a cell */
  case KEY_LEFT:
    pending_index = _index_left(board, board->current_cell);
    break;

  /* Do nothing */
  default:
    action = NONE;
    break;
  }

  if (BOARD_BOUND_CHECK(board, pending_index)) {
    board->current_cell = pending_index;
    action = MOVE;
  }

  /* How long did this action take in ms? */
  clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
  int diff = (stop.tv_sec - start.tv_sec) * 1000 + (stop.tv_nsec - start.tv_nsec) / 1000000;
  board->timeout -= diff;

  return action;
}

void print_headers(struct PanelData *self, void *opaque) {
  GameBoard_T *board = (GameBoard_T *)opaque;
  WINDOW *win = panel_window(self->panel);
  wmove(win, 1, 1);
  // waddstr(win, GameStateStr[board->game_state]);
  // waddstr(win, "   ");
  wprintw(win, "%03d", board->num_flags);
  wmove(win, 1, pm_panel_get_width(self) - 3);
  wprintw(win, "%03d", board->seconds_elapsed);
  wrefresh(win);
}

void print_cell_contents(WINDOW *win, GameBoard_T *board, unsigned int index) {
  if (board->current_cell == index) {
    PRINT_CELL_WITH_COLOR(win, CELL_SELECTED, waddstr(win, CELL_SELECTED_STR));

  } else if (UNCOVERED(board, index)) {
    if (HASBOMB(board, index)) {
      PRINT_CELL_WITH_COLOR(win, CELL_CONTAINS_BOMB, waddstr(win, CELL_HASBOMB_STR));
    } else {
      int bombs = NUMBOMBS(board, index);
      PRINT_CELL_WITH_COLOR(win, bombs, wprintw(win, CELL_UNCOVERED_STR, (bombs) ? bombs + '0' : ' '););
    }

  } else if (FLAGGED(board, index)) {
    PRINT_CELL_WITH_COLOR(win, CELL_FLAGGED, waddstr(win, CELL_FLAGGED_STR));

#ifdef DEBUG
  } else if (HASBOMB(board, index)) {
    PRINT_CELL_WITH_COLOR(win, CELL_CONTAINS_BOMB, waddstr(win, CELL_COVERED_STR));
#endif

  } else {
    PRINT_CELL_WITH_COLOR(win, CELL_COVERED, waddstr(win, CELL_COVERED_STR));
  }
}

void print_board(struct PanelData *self, void *opaque) {
  GameBoard_T *board = (GameBoard_T *)opaque;
  WINDOW *win = panel_window(self->panel);
  wmove(win, 1, 1);
  int curr_row = 1;
  int curr_col = 1;
  for (unsigned int index = 0; index < board->height * board->width; index++) {
    if (!PRINTED(board, index) || board->refresh_board_print) {
      wmove(win, curr_row, curr_col);
      print_cell_contents(win, board, index);
      SET_PRINTED(board, index);
    }
    curr_col += 3;

    /* Move down to the next row to print */
    if (!((index + 1) % board->width)) {
      wmove(win, ++curr_row, 1);
      curr_col = 1;
      wrefresh(win);
    }
  }
}

void print_debug_box(struct PanelData *self, void *opaque) {
  GameBoard_T *board = (GameBoard_T *)opaque;
  WINDOW *win = panel_window(self->panel);
  wmove(win, 1, 1);
  unsigned int index = board->current_cell;
  wprintw(win, "Current index: %d [%d,%d]", index, CELL_ROW(board, index), CELL_COL(board, index));
  wmove(win, 2, 1);
  wprintw(win, "\tnum_bombs=%d\n\thas_bomb=%d\n\tuncovered=%d\n\tflagged=%d\n\tprinted=%d", NUMBOMBS(board, index),
          HASBOMB(board, index) >> 4, UNCOVERED(board, index) >> 5, FLAGGED(board, index) >> 6,
          PRINTED(board, index) >> 7);
  wrefresh(win);
}

void uncover_cell_block(GameBoard_T *board, unsigned int index) {
  unsigned int start_index = index;
  unsigned int prev_index = index;
  unsigned int next_index = -1;

  if (UNCOVERED(board, index)) {
    return;
  }

  // Uncover only 1 cell if it has a bomb in it or adjacent to it
  if (NUMBOMBS(board, index) || HASBOMB(board, index)) {
    SET_UNCOVERED(board, index);
    CLEAR_PRINTED(board, index);
    board->remaining_open_cells--;
    return;
  }

  /* Traverse the open space and use the numbomb bits to keep track of where we
   * came from */
  do {
    if (!UNCOVERED(board, index)) {
      SET_UNCOVERED(board, index);
      CLEAR_PRINTED(board, index);
      board->remaining_open_cells--;
    }

    /* Figure out which cells have a bomb around it */
    int adjacent_bombs_bits = SURRONDING_CELL_STATE(board, index, ADJACENTBOMB);
    int surronding_uncovered_bits = SURRONDING_CELL_STATE(board, index, !UNCOVERED);

    SURRONDING_CELL_ACTION_STATEFUL(board, index, adjacent_bombs_bits, SET_UNCOVERED);
    SURRONDING_CELL_ACTION_STATEFUL(board, index, adjacent_bombs_bits, CLEAR_PRINTED);
    board->remaining_open_cells -= COUNT_BITS(adjacent_bombs_bits & surronding_uncovered_bits);

    /* Go through each direction and see if we need to uncover that cell */
    uint8_t dir = 0;
    for (dir = 0; dir < NUM_DIRECTIONS; dir++) {
      next_index = MOVE_CELL_ACTIONS[dir](board, index);
      if (next_index != INVALID_INDEX && UNCOVER_BLOCK_CONDITION(board, next_index)) {
        index = next_index;
        uint8_t bt_dir = (dir + NUM_DIRECTIONS / 2) % NUM_DIRECTIONS;
        SET_BACKTRACK_DIR(board, index, bt_dir);
        break;
      }
    }

    /* Moving cells, do not backtrack yet */
    if (dir != NUM_DIRECTIONS)
      continue;

    /* Backtrack */
    if (index != start_index) {
      prev_index = index;
      index = MOVE_CELL_ACTIONS[BACKTRACK_DIR(board, index)](board, index);
      int surrondingBombs = SURRONDING_CELL_STATE(board, prev_index, HASBOMB);
      SET_NUMBOMBS(board, prev_index, COUNT_BITS(surrondingBombs));
      CLEAR_PRINTED(board, prev_index);
    }
  } while (index != start_index);
}

GameState_T update_game_condition(GameBoard_T *board, unsigned int index) {
  if (board->game_state == QUIT) {
    return QUIT;
  } else if (UNCOVERED(board, index) && HASBOMB(board, index)) {
    return EXPLODE;
  } else if (board->remaining_open_cells == 0 && !board->is_first_turn) {
    return WIN;
  } else {
    return TURNS;
  }
}

void gameboard_scene_init(GameBoard_T *board, int rows, int columns) {
  int yalign = getmaxy(stdscr) / 2 - rows / 2;
  int xalign = getmaxx(stdscr) / 2 - (columns * CELL_STR_LEN) / 2;

  PanelScene_T *ps = pm_scene_init(3);
  pm_add_scene(board->pm, ps, GAMEBOARD_SCENE_ID);

  PanelData_T *pd;
  pd = pm_panel_init(1, 1, 3, pm_panel_get_width(ps->background), print_headers, NULL, NULL, NULL);
  pm_panel_add_border(pd, ' ', ' ', '*', '*', '*', '*', '*', '*');
  pm_scene_add_panel(ps, pd, 0);
  pd = pm_panel_init(yalign, xalign, rows + 2, columns * CELL_STR_LEN + 2, print_board, NULL, NULL, NULL);
  pm_panel_add_border(pd, '#', '#', '#', '#', '#', '#', '#', '#');
  pm_scene_add_panel(ps, pd, 1);
  pd = pm_panel_init(yalign + rows + 2, xalign, 6, columns * CELL_STR_LEN + 2, print_debug_box, NULL, NULL, NULL);
  pm_scene_add_panel(ps, pd, 2);
}

void explode_scene_init(GameBoard_T *board) {
  PanelScene_T *ps = pm_scene_init(1);
  pm_add_scene(board->pm, ps, LOOSE_SCENE_ID);

  PanelData_T *pd;
  unsigned int yalign = pm_panel_get_height(ps->background) / 2 - EXPLODE_SCENE_HEIGHT / 2;
  unsigned int xalign = pm_panel_get_width(ps->background) / 2 - EXPLODE_SCENE_WIDTH / 2;
  pd = pm_panel_init(yalign, xalign, EXPLODE_SCENE_HEIGHT + 1, EXPLODE_SCENE_WIDTH + 1, print_explode_sequence, NULL,
                     NULL, NULL);
  pm_scene_add_panel(ps, pd, 0);
}

int terminal_setup(GameBoard_T *board, unsigned int rows, unsigned int columns) {
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);

  /* Use the stdscr as a base and validate we can fit the gameboard */
  int maxy = getmaxy(stdscr);
  int maxx = getmaxx(stdscr);
  if (!maxx || !maxy || maxy < 5 + rows || maxx < 2 + columns) {
    return 1;
  }

  /* Setup colors */
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

  /* Create panel manager and scenes */
  board->pm = pm_init(NUM_SCENES);
  gameboard_scene_init(board, rows, columns);
  explode_scene_init(board);

  /* +2 for boarder */

  return 0;
}

void generate_board(GameBoard_T *board, unsigned int rows, unsigned int columns) {
  board->game_state = BOARD_GENERATION;
  board->height = rows;
  board->width = columns;
  board->board = (uint8_t *)calloc(board->width * board->height, sizeof(uint8_t));
  board->current_cell = 0;
  board->num_bombs = 0;
  board->num_flags = 0;
  board->remaining_open_cells = 0;
  board->seconds_elapsed = 0;
  board->timeout = 1000; /* 1000 ms */
  board->is_first_turn = 1;
  board->refresh_board_print = 0;
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
  board->num_bombs = bombs;
  board->num_flags = bombs;
  for (int b = 0; b < board->num_bombs; b++) {
    int placement;
    do {
      placement = rand() % (board->width * board->height);
    } while (HASBOMB(board, placement) || IS_CELL_ADJACENT(board, board->current_cell, placement));
    SET_HASBOMB(board, placement);
  }

  // Update the number of bombs around each cell
  for (int i = 0; i < board->height * board->width; i++) {
    int surrondingBombs = SURRONDING_CELL_STATE(board, i, HASBOMB);
    SET_NUMBOMBS(board, i, COUNT_BITS(surrondingBombs));
  }

  board->remaining_open_cells = (board->width * board->height) - bombs;
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
    fprintf(stderr, "Specified rows %s cannot be converted into an integer\n", argv[1]);
  }

  if (str2int(&cols, argv[2], 10)) {
    fprintf(stderr, "Specified columns %s cannot be converted into an integer\n", argv[2]);
  }

  if (str2int(&bombs, argv[3], 10)) {
    fprintf(stderr, "Specified number of bombs %s cannot be converted into an integer\n", argv[3]);
  }

  if (terminal_setup(board, rows, cols)) {
    printw("Terminal initialization failed. Exiting.\n");
  } else {
    generate_board(board, rows, cols);
    // generate_bombs(board, bombs);

    board->game_state = TURNS;
    CellAction_T next_action;

    while (board->game_state == TURNS) {
      pm_scene_draw_all(pm_get_scene(board->pm, GAMEBOARD_SCENE_ID), (void *)board);
      CLEAR_PRINTED(board, board->current_cell);
      next_action = do_cell_action(board);
      if (board->timeout <= 0) {
        board->timeout = 1000;
        board->seconds_elapsed++;
      }
      switch (next_action) {
      case UNCOVER:
        // We have to check for a explode condition before win condition due to
        // this logic
        if (board->is_first_turn) {
          generate_bombs(board, bombs);
          board->is_first_turn = 0;
        }
        uncover_cell_block(board, board->current_cell);
        break;

      case FLAG:
        if (board->num_flags == 0 || UNCOVERED(board, board->current_cell)) {
          break;
        }
        if (!FLAGGED(board, board->current_cell)) {
          SET_FLAGGED(board, board->current_cell);
          board->num_flags--;
        } else {
          CLEAR_FLAGGED(board, board->current_cell);
          board->num_flags++;
        }
        CLEAR_PRINTED(board, board->current_cell);
        break;

      case EXIT:
        board->game_state = QUIT;
        break;

      case MOVE:
        CLEAR_PRINTED(board, board->current_cell);
        break;

      case NONE:
      default:
        break;
      }

      board->game_state = update_game_condition(board, board->current_cell);
    }
  }

  // switch (board->game_state) {
  // case EXPLODE:
  //   pm_switch_scene(board->pm, LOOSE_SCENE_ID);
  //   pm_scene_draw_all(pm_get_current_scene(board->pm), NULL);
  //   break;
  // }

  board->refresh_board_print = 1;
  board->current_cell = INVALID_INDEX;
  pm_switch_scene(board->pm, GAMEBOARD_SCENE_ID);
  pm_scene_draw_all(pm_get_current_scene(board->pm), board);
  board->refresh_board_print = 0;

  // printw("Press any key to continue...");
  timeout(-1);
  getch();

  board->game_state = CLEANUP;
  endwin();
  if (board->board) {
    free(board->board);
  }
  free(board);

  return 0;
}