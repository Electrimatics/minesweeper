#include "panel.h"

#define _PM_FOR_EACH(opaque, data, capacity_val, iterate_list, stmts)                                                  \
  if (iterate_list) {                                                                                                  \
    for (int _i = 0; _i < capacity_val; _i++) {                                                                        \
      data = iterate_list[_i];                                                                                         \
      if (data) {                                                                                                      \
        stmts;                                                                                                         \
      }                                                                                                                \
    }                                                                                                                  \
  }

#define PM_FOR_EACH_SCENE(pm, scene, stmts) _PM_FOR_EACH(pm, scene, pm->scene_capacity, pm->scenes, stmts)
#define PM_FOR_EACH_PANEL(ps, data, stmts) _PM_FOR_EACH(ps, data, ps->panel_capacity, ps->panels, stmts)

/**
 * 0 1 2 3 4 5 6 7
 *  0: Top / bottom#include <curses.h>
#include <panel.h>
 *  1: Left / Right
 *  2: Centered
 *  3: Outside
 * Custom: All 1
 */
typedef enum PM_PANEL_ALIGN_STRATEGY {
  PM_PANEL_ALIGN_TOP = (0),
  PM_PANEL_ALIGN_BOTTOM = (1),
  PM_PANEL_ALIGN_LEFT = (0),
  PM_PANEL_ALIGN_RIGHT = (1 << 1),
  PM_PANEL_ALIGN_CENTER = (1 << 2),
  PM_PANEL_ALIGN_OUTSIDE = (1 << 3),
} PM_PANEL_ALIGN_STRATEGY;
/* with offset */

typedef enum PM_PANEL_MOVE_STRATEGY {
  PM_PANEL_MOVE_STATIC = 0,
  PM_PANEL_MOVE_Y_AXIS = 1,
  PM_PANEL_MOVE_X_AXIS = (1 << 1),
  PM_PANEL_MOVE_Z_AXIS = (1 << 2),
} PM_PANEL_MOVE_STRATEGY;

typedef enum PM_PANEL_RESIZE_STRATEGY {
  PM_PANEL_RESIZE_NONE = 0,
  PM_PANEL_RESIZE_Y_AXIS = 1,
  PM_PANEL_RESIZE_X_AXIS = (1 << 1),
} PM_PANEL_RESIZE_STRATEGY;

typedef enum PM_PANEL_PROPERTIES { PM_PANEL_SCROLLING = (1) } PM_PANEL_PROPERTIES;

/* Forward declaration */
struct PanelData;

typedef void (*draw_handler)(struct PanelData *self, void *opaque);
typedef void (*pm_panel_init_cb)(void *opaque);
typedef void (*pm_panel_exit_cb)(void *opaque);

typedef unsigned int PanelDataID;
typedef struct PanelData {
  PANEL *panel;
  int y;
  int x;
  int height;
  int width;
  int has_border;
  int ref_count;

  PM_PANEL_ALIGN_STRATEGY align;
  PM_PANEL_MOVE_STRATEGY move;
  PM_PANEL_RESIZE_STRATEGY resize;

  draw_handler draw;

  pm_panel_init_cb init_cb;
  pm_panel_exit_cb exit_cb;
} PanelData_T;

typedef unsigned int PanelSceneID;
typedef struct PanelScene {
  /* Always on the bottom */
  PanelData_T *background;
  /* Stacking order starts at index 0 -> 1 -> 2 -> ... */
  PanelData_T **panels;
  PanelDataID panel_capacity;
  PanelDataID panel_count;
  int update_stacking_order;
} PanelScene_T;

typedef struct PanelManager {
  PanelSceneID scene_capacity;
  PanelSceneID scene_count;
  PanelScene_T **scenes;
  PanelSceneID current_scene;
} PanelManager_T;

/* Panel Manager prototypes begin */
PanelManager_T *pm_init(unsigned int scenes);

int pm_add_scene(PanelManager_T *pm, PanelScene_T *ps, PanelSceneID id);

PanelScene_T *pm_get_scene(PanelManager_T *pm, PanelSceneID id);

PanelScene_T *pm_get_current_scene(PanelManager_T *pm);

PanelScene_T *pm_switch_scene(PanelManager_T *pm, PanelSceneID id);

// PanelScene_T pm_remove_scene(PanelManager_T *pm)

void pm_exit(PanelManager_T *pm);

/* Panel Manager prototypes end */

/* Panel Scene prototypes begin */

PanelScene_T *pm_scene_init(unsigned int panels);

int pm_scene_add_panel(PanelScene_T *ps, PanelData_T *pd, PanelDataID id);

void pm_scene_update_panel_order(PanelScene_T *ps);

void pm_scene_show_all(PanelScene_T *ps);

void pm_scene_hide_all(PanelScene_T *ps);

void pm_scene_draw_all(PanelScene_T *ps, void *opaque);

void pm_scene_exit(PanelScene_T *pm);

/* Panel Scene prototypes end */

/* Panel Data prototypes begin */

PanelData_T *pm_panel_init(int y, int x, int height, int width, draw_handler draw, pm_panel_init_cb init_cb,
                           pm_panel_exit_cb exit_cb, void *user_ptr);

int pm_panel_get_height(PanelData_T *pd);

int pm_panel_get_width(PanelData_T *pd);

void pm_panel_draw(PanelData_T *pd, void *opaque);

WINDOW *pm_panel_resize(PanelData_T *pd, int new_height, int new_width, int old_height, int old_width);

void pm_panel_add_border(PanelData_T *pd, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl,
                         chtype br);

void pm_panel_add_border_wide(PanelData_T *pd, chtype *ls, chtype *rs, chtype *ts, chtype *bs, chtype *tl, chtype *tr,
                              chtype *bl, chtype *br);

void pm_panel_add_box(PanelData_T *pd, chtype v, chtype h);

void pm_panel_add_box_wide(PanelData_T *pd, chtype *v, chtype *h);

void pm_panel_exit(PanelData_T *pd);

/* Panel Data prototypes end */
