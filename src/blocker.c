#include "blocker.h"
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>

static void flush_dns_cache(void) {
}

static int run_helper(const char *cmd, const char *domain) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("siteguard: fork");
        return -1;
    }

    if (pid == 0) {
        /* Child process: exec the setuid helper */
        char *argv[] = {
            (char *)HELPER_BIN,
            (char *)cmd,
            (char *)domain,
            NULL
        };
        execv(HELPER_BIN, argv);
        perror("siteguard: execv siteguard-helper");
        _exit(1);
    }

    /* Parent: wait for helper to finish */
    int status;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status)) return -1;
    return WEXITSTATUS(status);
}

int blocker_block(const char *domain) {
    int ret = run_helper("block", domain);
    flush_dns_cache();
    return ret;
}

int blocker_unblock(const char *domain) {
    int ret = run_helper("unblock", domain);
    flush_dns_cache();
    return ret;
}

void blocker_sync(AppState *state) {
    for (int i = 0; i < state->count; i++) {
        Site *s = &state->sites[i];
        if (s->used_sec >= s->budget_sec && !s->blocked) {
            fprintf(stderr, "siteguard: blocking %s (used %ds / budget %ds)\n",
                    s->domain, s->used_sec, s->budget_sec);
            if (blocker_block(s->domain) == 0)
                s->blocked = true;
        }
    }
}

void blocker_unblock_all(AppState *state) {
    for (int i = 0; i < state->count; i++) {
        Site *s = &state->sites[i];
        /* Try to unblock regardless of in-memory blocked flag —
           handles cases where state was out of sync with /etc/hosts */
        blocker_unblock(s->domain);
        s->blocked    = false;
        s->used_sec   = 0;
    }
    flush_dns_cache();
}
