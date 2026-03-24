#ifndef APP_H
#define APP_H

#include <stdbool.h>

#define MAX_SITES       64
#define MAX_DOMAIN_LEN  256

typedef struct {
    char   domain[MAX_DOMAIN_LEN];
    int    budget_sec;
    int    used_sec;
    bool   blocked;
} Site;

typedef struct {
    Site  sites[MAX_SITES];
    int   count;
} AppState;

#endif
