#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *config_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(path, sizeof(path), "%s/.config/siteguard/config.conf", home);
    return path;
}

static void ensure_config_dir(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config/siteguard", home);
    mkdir(dir, 0755); /* silently fails if already exists */
}

int config_load(AppState *state) {
    state->count = 0;

    FILE *f = fopen(config_path(), "r");
    if (!f) return 0; /* not an error — just no config yet */

    char line[512];
    while (fgets(line, sizeof(line), f) && state->count < MAX_SITES) {
        line[strcspn(line, "\n\r")] = '\0';
        if (!line[0] || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';

        int budget = atoi(eq + 1);
        if (!line[0] || budget <= 0) continue;

        Site *s       = &state->sites[state->count++];
        strncpy(s->domain, line, MAX_DOMAIN_LEN - 1);
        s->domain[MAX_DOMAIN_LEN - 1] = '\0';
        s->budget_sec = budget;
        s->used_sec   = 0;
        s->blocked    = false;
    }

    fclose(f);
    return state->count;
}

int config_save(const AppState *state) {
    ensure_config_dir();

    FILE *f = fopen(config_path(), "w");
    if (!f) return -1;

    fprintf(f, "# SiteGuard config — format: domain=budget_seconds\n");
    for (int i = 0; i < state->count; i++)
        fprintf(f, "%s=%d\n", state->sites[i].domain, state->sites[i].budget_sec);

    fclose(f);
    return 0;
}
