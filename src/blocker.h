#ifndef BLOCKER_H
#define BLOCKER_H

#include "app.h"

#define HELPER_BIN "/usr/local/bin/siteguard-helper"
#define REDIRECT_PORT 8080

int  blocker_block(const char *domain);
int  blocker_unblock(const char *domain);
void blocker_sync(AppState *state);
void blocker_unblock_all(AppState *state);

#endif
