#include "panel_manager.h"
#include <curses.h>
#include <ncurses.h>
#include <panel.h>
#include <stdlib.h>

PanelManager_T *pm_init(unsigned int scenes) {
  PanelManager_T *pm = (PanelManager_T *)calloc(1, sizeof(PanelManager_T));
  pm->scene_count = 0;
  pm->scene_capacity = scenes;
  pm->current_scene = 0;
  if (scenes) {
    pm->scenes = (PanelScene_T **)calloc(scenes, sizeof(PanelScene_T *));
  } else {
    pm->scenes = NULL;
  }

  return pm;
}

int pm_add_scene(PanelManager_T *pm, PanelScene_T *ps, PanelSceneID id) {
  if (id >= pm->scene_capacity || pm->scenes[id]) {
    return 1;
  }

  pm->scenes[id] = ps;
  pm->scene_count++;
  return 0;
}

PanelScene_T *pm_get_scene(PanelManager_T *pm, PanelSceneID id) {
  if (id >= pm->scene_capacity) {
    return NULL;
  }

  return pm->scenes[id];
}

PanelScene_T *pm_get_current_scene(PanelManager_T *pm) { return pm_get_scene(pm, pm->current_scene); }

PanelScene_T *pm_switch_scene(PanelManager_T *pm, PanelSceneID id) {
  // TODO: Should the caller or this function be responsible to hiding/showing other scenes?
  PanelScene_T *ps_current = pm_get_current_scene(pm);
  if (ps_current) {
    pm_scene_hide_all(ps_current);
  }

  PanelScene_T *ps = pm_get_scene(pm, id);
  if (ps) {
    ps->update_stacking_order = 1;

    /* Show panels before updating stacking order */
    pm_scene_show_all(ps);
    pm_scene_update_panel_order(ps);
    pm->current_scene = id;
  }
  return ps;
}

void pm_exit(PanelManager_T *pm) {
  /* Free all panels */
  PanelScene_T *scene;
  PM_FOR_EACH_SCENE(pm, scene, pm_scene_exit(scene))
  free(pm->scenes);
  pm->scenes = NULL;
  free(pm);
  pm = NULL;
}

PanelScene_T *pm_scene_init(unsigned int panels) {
  PanelScene_T *ps = (PanelScene_T *)calloc(1, sizeof(PanelScene_T));
  ps->panel_count = 0;
  ps->panel_capacity = panels;
  if (panels) {
    ps->panels = (PanelData_T **)calloc(panels, sizeof(PanelData_T *));
  } else {
    ps->panels = NULL;
  }

  /**
   * Initialize background with defaults.
   * This panel is meant to be the "base" for the other panels in this scene.
   * All other panels will react according to how the background changes (ie: moving, re-sizing, etc)
   */
  ps->background = pm_panel_init(0, 0, getmaxy(stdscr), getmaxx(stdscr), NULL, NULL, NULL, NULL);
  pm_panel_add_box(ps->background, '|', '-');
  ps->update_stacking_order = 1;
  return ps;
}

int pm_scene_add_panel(PanelScene_T *ps, PanelData_T *pd, PanelDataID id) {
  if (id >= ps->panel_capacity || ps->panels[id]) {
    return 1;
  }

  ps->panels[id] = pd;
  ps->panel_count++;
  ps->update_stacking_order = 1;

  pd->ref_count++;

  // TODO: Align window to background?
  return 0;
}

void pm_scene_update_panel_order(PanelScene_T *ps) {
  PanelData_T *data;
  if (ps->update_stacking_order) {
    top_panel(ps->background->panel);
    PM_FOR_EACH_PANEL(ps, data, top_panel(data->panel));
    ps->update_stacking_order = 0;
  }
  update_panels();
  doupdate();
}

void pm_scene_show_all(PanelScene_T *ps) {
  PanelData_T *data;
  PM_FOR_EACH_PANEL(ps, data, show_panel(data->panel));
}

void pm_scene_hide_all(PanelScene_T *ps) {
  PanelData_T *data;
  PM_FOR_EACH_PANEL(ps, data, hide_panel(data->panel));
}

void pm_scene_draw_all(PanelScene_T *ps, void *opaque) {
  PanelData_T *data;
  pm_scene_update_panel_order(ps);
  PM_FOR_EACH_PANEL(ps, data, pm_panel_draw(data, opaque));
  doupdate();
}

void pm_scene_exit(PanelScene_T *ps) {
  PanelData_T *data;
  PM_FOR_EACH_PANEL(ps, data, pm_panel_exit(data));
  free(ps->panels);
  ps->panels = NULL;
  free(ps);
  ps = NULL;
}

PanelData_T *pm_panel_init(int y, int x, int height, int width, draw_handler draw, pm_panel_init_cb init_cb,
                           pm_panel_exit_cb exit_cb, void *user_ptr) {
  PanelData_T *pd = (PanelData_T *)calloc(1, sizeof(PanelData_T));

  /* Set the panel's strategies */
  // pd->align = align;
  // pd->move = move;
  // pd->resize = resize;

  /* Create window */
  WINDOW *win = newwin(height, width, y, x);

  /* Create panel. Window is accessible under pd->panel->win */
  pd->panel = new_panel(win);
  pd->y = y;
  pd->x = x;
  pd->height = height;
  pd->width = width;
  pd->ref_count = 0;

  pd->draw = draw;
  pd->init_cb = init_cb;
  pd->exit_cb = exit_cb;
  set_panel_userptr(pd->panel, user_ptr);

  /* Hide by default */
  hide_panel(pd->panel);

  return pd;
}

int pm_panel_get_height(PanelData_T *pd) { return (!pd->has_border) ? pd->height : pd->height - 2; }

int pm_panel_get_width(PanelData_T *pd) { return (!pd->has_border) ? pd->width : pd->width - 2; }

WINDOW *pm_panel_resize(PanelData_T *pd, int new_height, int new_width, int old_height, int old_width) {
  // TODO: Implement me
  return NULL;
}

void pm_panel_draw(PanelData_T *pd, void *opaque) {
  if (pd->draw) {
    pd->draw(pd, opaque);
  }
  doupdate();
}

void pm_panel_add_border(PanelData_T *pd, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl,
                         chtype br) {
  pd->has_border = 1;
  wborder(panel_window(pd->panel), ls, rs, ts, bs, tl, tr, bl, br);
}

// void pm_panel_add_border_wide(PanelData_T *pd, chtype *ls, chtype *rs, chtype *ts, chtype *bs, chtype *tl, chtype
// *tr,
//                               chtype *bl, chtype *br) {
//   pd->has_border = 1;
//   wborder_set(panel_window(pd->panel), ls, rs, ts, bs, tl, tr, bl, br);
// }

void pm_panel_add_box(PanelData_T *pd, chtype v, chtype h) {
  pd->has_border = 1;
  box(panel_window(pd->panel), v, h);
}

// void pm_panel_add_box_wide(PanelData_T *pd, chtype *v, chtype *h) {
//   pd->has_border = 1;
//   box_set(panel_window(pd->panel), v, h);
// }

void pm_panel_exit(PanelData_T *pd) {}