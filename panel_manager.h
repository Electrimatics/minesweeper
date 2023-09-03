#include "panel.h"

#define _PM_PANELS_EXIST(pm, op) (pm->panels) ? op : NULL

typedef void (*draw_handler)(WINDOW *win, void *opaque);
typedef void (*pm_panel_init_cb)(void *opaque);
typedef void (*pm_panel_exit_cb)(void *opaque);
typedef struct PanelData {
  PANEL *panel;
  int y;
  int x;
  int height;
  int width;
  char name[16];
  draw_handler draw_handler;

  pm_panel_init_cb init_cb;
  pm_panel_exit_cb exit_cb;
} PanelData_T;

typedef struct PanelManager {
  unsigned int panel_capacity;
  unsigned int panel_count;
  PanelData_T **panels;
} PanelManager_T;

PanelManager_T *pm_init(unsigned int panels);

void pm_exit(PanelManager_T *pm);

PanelData_T *pm_panel_init(PanelManager_T *pm, int y, int x, int height,
                           int width, char *name, draw_handler draw_handler,
                           pm_panel_init_cb init_fn, pm_panel_exit_cb exit_fn);

void pm_show_panels(PanelManager_T *pm);

void pm_hide_panels(PanelManager_T *pm);

PanelData_T *pm_get_panel(PanelManager_T *pm, int index);

PanelData_T *pm_get_panel_top(PanelManager_T *pm);

PanelData_T *pm_get_panel_bottom(PanelManager_T *pm);

WINDOW *pm_panel_resize(PanelData_T *pd, int new_height, int new_width,
                        int old_height, int old_width);

void pm_panel_draw(PanelManager_T *pm, int index, void *opaque);

void pm_panel_draw_all(PanelManager_T *pm, void *opaque);

void pm_panel_move(PanelData_T *pd, int new_x, int new_y, int old_x, int old_y);

void pm_panel_exit(PanelData_T *pd);
