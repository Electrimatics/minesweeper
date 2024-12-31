#include "minesweeper.h"
#include <curses.h>
#include <panel.h>
#include <time.h>

typedef void (*explode_func)(WINDOW *win);

const unsigned int NUM_EXPLODE_PHASES = 4;

void explode_three(WINDOW *win) {
  waddstr(win, "                                                      \n");
  wprintw(win, "                                 /@&            @@@@  \n");
  wprintw(win, "                           @  #@@    @@@       @  #@& \n");
  wprintw(win, "                        .,@@@@@@@#      @@      @//#  \n");
  wprintw(win, "                 @@@@#(@@@@@@@@@@@@      (@@    ,@@   \n");
  wprintw(win, "              ,@@ @@@@@@@@@@@@@@@@@         @@@@#     \n");
  wprintw(win, "             @@ @@@@@@@@@@@@@@@@@@@@@                 \n");
  wprintw(win, "            @@ @@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@/@@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@@@@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@@@@@@@@@@@@@@@@@@@@@@@@/                \n");
  wprintw(win, "             &@@@@@@@@@@@@@@@@@@@@@@                  \n");
  wprintw(win, "               @@@@@@@@@@@@@@@@@@@(                   \n");
  wprintw(win, "                  *@@@@@@@@@@@@                       \n");
  wprintw(win, "                                                      \n");
  wprintw(win, "                                                      \n");
}

void explode_two(WINDOW *win) {
  wprintw(win, "                                                      \n");
  wprintw(win, "                                 /@&      @@@@        \n");
  wprintw(win, "                           @  #@@    @@@ @  #@&       \n");
  wprintw(win, "                        .,@@@@@@@#      @ @//# @      \n");
  wprintw(win, "                 @@@@#(@@@@@@@@@@@@       ,@@         \n");
  wprintw(win, "              ,@@ @@@@@@@@@@@@@@@@@                   \n");
  wprintw(win, "             @@ @@@@@@@@@@@@@@@@@@@@@                 \n");
  wprintw(win, "            @@ @@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@/@@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@@@@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@@@@@@@@@@@@@@@@@@@@@@@@/                \n");
  wprintw(win, "             &@@@@@@@@@@@@@@@@@@@@@@                  \n");
  wprintw(win, "               @@@@@@@@@@@@@@@@@@@(                   \n");
  wprintw(win, "                  *@@@@@@@@@@@@                       \n");
  wprintw(win, "                                                      \n");
  wprintw(win, "                                                      \n");
}

void explode_one(WINDOW *win) {
  wprintw(win, "                                                      \n");
  wprintw(win, "                        **@, #/@&) **                 \n");
  wprintw(win, "                          (@  #@@// *@                \n");
  wprintw(win, "                        .,@@@@@@@#                    \n");
  wprintw(win, "                 @@@@#(@@@@@@@@@@@@                   \n");
  wprintw(win, "              ,@@ @@@@@@@@@@@@@@@@@                   \n");
  wprintw(win, "             @@ @@@@@@@@@@@@@@@@@@@@@                 \n");
  wprintw(win, "            @@ @@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@/@@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@@@@@@@@@@@@@@@@@@@@@@@@@                \n");
  wprintw(win, "            @@@@@@@@@@@@@@@@@@@@@@@@@/                \n");
  wprintw(win, "             &@@@@@@@@@@@@@@@@@@@@@@                  \n");
  wprintw(win, "               @@@@@@@@@@@@@@@@@@@(                   \n");
  wprintw(win, "                  *@@@@@@@@@@@@                       \n");
  wprintw(win, "                                                      \n");
  wprintw(win, "                                                      \n");
}

void explode_zero(WINDOW *win) {
  wprintw(win, "                 @          #                         \n");
  wprintw(win, "             .                               $$       \n");
  wprintw(win, "     .,     .,, #            #             $$$        \n");
  wprintw(win, "                ,./#     #   &           (&           \n");
  wprintw(win, "                   ,,,,,,,&  .,,.,.                   \n");
  wprintw(win, "       $$$$     ,,,,,$,,**/,,*,,,,,,,  $              \n");
  wprintw(win, "         ,$   #,.,..,$(.,***//./.&..,,                \n");
  wprintw(win, "       &&     ...$&&,,***,,***((((./(,**,             \n");
  wprintw(win, "             ,,,,,,///////**********,,$,,,,    $      \n");
  wprintw(win, "     ,      .,,,,$(&(((((((///////&&&&&&&...          \n");
  wprintw(win, "            ..$,,##(#(///********/(##$#,,,. &         \n");
  wprintw(win, "        &&&, $*. ##(((**********//((#$#....           \n");
  wprintw(win, "        .&&&&   #/,(,,***,.,&&*///(  .$....           \n");
  wprintw(win, "     .        #  (...*/..,..,*...((    $&(            \n");
  wprintw(win, "            &#,  @(.*. ..   *      (                  \n");
  wprintw(win, "   ,.        .                       (                \n");
}

void print_explode_sequence(PanelData_T *pd, void *opaque) {
  WINDOW *win = panel_window(pd->panel);
  struct timespec delay = {
      .tv_sec = 1, /* seconds */
      .tv_nsec = 0 /* nanoseconds */
  };

  explode_func efuncs[] = {explode_three, explode_two, explode_one, explode_zero};
  for (int ii = 0; ii < NUM_EXPLODE_PHASES; ii++) {
    wmove(win, 0, 0);
    efuncs[ii](win);
    wrefresh(win);
    if (nanosleep(&delay, NULL)) {
      continue;
    }
  }
}