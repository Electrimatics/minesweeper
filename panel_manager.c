#include "panel_manager.h"
#include <curses.h>
#include <panel.h>
#include <stdlib.h>

#define PM_FOR_EACH_PANEL(pm, stmts)                                           \
  for (int p = 0; p < pm->panel_count; p++) {                                  \
    stmts                                                                      \
  }

PanelManager_T *pm_init(unsigned int panels) {
  PanelManager_T *pm = (PanelManager_T *)calloc(1, sizeof(PanelManager_T));
  pm->panel_count = 0;
  pm->panel_capacity = panels;
  if (panels) {
    pm->panels = (PanelData_T **)calloc(panels, sizeof(PanelData_T *));
  } else {
    pm->panels = NULL;
  }

  return pm;
}

void pm_exit(PanelManager_T *pm) {
  /* Free all panels */
  if (pm->panels) {
    PM_FOR_EACH_PANEL(pm, del_panel(pm->panels[p]->panel);
                      pm->panels[p]->panel = NULL;)
  }
  free(pm->panels);
  pm->panels = NULL;
  free(pm);
}

PanelData_T *pm_panel_init(PanelManager_T *pm, int y, int x, int height,
                           int width, char *name, draw_handler draw_handler,
                           pm_panel_init_cb init_cb, pm_panel_exit_cb exit_cb) {
  PanelData_T *pd = (PanelData_T *)calloc(1, sizeof(PanelData_T));

  /* Check for available space */
  if (pm->panel_count >= pm->panel_capacity) {
    // TODO: What is the best increment? Develop different memory allocation
    // profiles?
    pm->panel_capacity += 5;
    // TODO: Clear the new memory?
    pm->panels = (PanelData_T **)realloc(pm->panels, pm->panel_capacity *
                                                         sizeof(PanelData_T *));
  }

  /* Create window */
  WINDOW *win = newwin(height, width, y, x);

  /* Create panel. Window is accessable under pd->panel->win */
  pd->panel = new_panel(win);
  pd->y = y;
  pd->x = x;
  pd->height = height;
  pd->width = width;
  // pd->name = name;
  pd->draw_handler = draw_handler;
  pd->init_cb = init_cb;
  pd->exit_cb = exit_cb;

  pm->panels[pm->panel_count] = pd;
  pm->panel_count++;

  box(pd->panel->win, 0, 0);

  return pd;
}

void pm_show_panels(PanelManager_T *pm) {
  PM_FOR_EACH_PANEL(pm, show_panel(pm->panels[p]->panel);)
}

void pm_hide_panels(PanelManager_T *pm) {
  PM_FOR_EACH_PANEL(pm, hide_panel(pm->panels[p]->panel);)
}

PanelData_T *pm_get_panel(PanelManager_T *pm, int index) {
  index %= pm->panel_count;
  return (pm->panels) ? pm->panels[index] : NULL;
}

PanelData_T *pm_get_panel_top(PanelManager_T *pm) {
  return pm_get_panel(pm, (int)pm->panel_count - 1);
}

PanelData_T *pm_get_panel_bottom(PanelManager_T *pm) {
  return pm_get_panel(pm, 0);
}

WINDOW *pm_panel_resize(PanelData_T *pd, int new_height, int new_width,
                        int old_height, int old_width);

void pm_panel_draw(PanelManager_T *pm, int index, void *opaque) {
  PanelData_T *pd = pm_get_panel(pm, index);
  pd->draw_handler(pd->panel->win, opaque);
  doupdate();
}

void pm_panel_draw_all(PanelManager_T *pm, void *opaque) {
  for (int p = 0; p < pm->panel_count; p++) {
    if (pm->panels[p]->draw_handler) {
      pm->panels[p]->draw_handler(pm->panels[p]->panel->win, opaque);
    }
  }
  doupdate();
}

void pm_panel_move(PanelData_T *pd, int new_x, int new_y, int old_x, int old_y);

void pm_panel_exit(PanelData_T *pd);
