#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "app.h"

void      ui_refresh(void);
GtkWidget *ui_build_window(GtkApplication *app, AppState *state);

#endif
