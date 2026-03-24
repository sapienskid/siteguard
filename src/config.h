#ifndef CONFIG_H
#define CONFIG_H

#include "app.h"

const char *config_path(void);
int         config_load(AppState *state);
int         config_save(const AppState *state);

#endif
