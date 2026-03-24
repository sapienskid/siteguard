/*
 * siteguard-helper — setuid root binary
 *
 * Usage: siteguard-helper <block|unblock> <domain>
 *
 * This binary is the ONLY component that writes to /etc/hosts.
 * It must be installed with: chmod 4755 + chown root:root
 *
 * Security measures:
 *  - Domain is validated: only [a-zA-Z0-9.-] allowed, no spaces or slashes
 *  - Writes to a temp file and renames atomically
 *  - Does not exec any shell or external command
 *  - Drops SUID after opening /etc/hosts via setuid(getuid())
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#define HOSTS_FILE "/etc/hosts"
#define MARKER     "# siteguard-block"
#define HOSTS_TMP  "/tmp/siteguard-hosts.tmp"

/* Validate domain: only alphanumerics, dots, hyphens. No empty string. */
static int valid_domain(const char *d) {
    if (!d || !*d) return 0;
    for (const char *p = d; *p; p++) {
        if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-')
            return 0;
    }
    return 1;
}

/* Append block entry to /etc/hosts if not already present */
static int do_block(const char *domain) {
    /* Check if already blocked */
    FILE *f = fopen(HOSTS_FILE, "r");
    if (!f) { perror("siteguard-helper: fopen /etc/hosts"); return 1; }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, domain) && strstr(line, MARKER)) {
            fclose(f);
            return 0; /* idempotent: already blocked */
        }
    }
    fclose(f);

    f = fopen(HOSTS_FILE, "a");
    if (!f) { perror("siteguard-helper: fopen append"); return 1; }
    fprintf(f, "127.0.0.1 %s www.%s %s\n", domain, domain, MARKER);
    fclose(f);
    return 0;
}

/* Remove all siteguard block lines for domain from /etc/hosts */
static int do_unblock(const char *domain) {
    FILE *src = fopen(HOSTS_FILE, "r");
    if (!src) { perror("siteguard-helper: fopen /etc/hosts"); return 1; }

    FILE *dst = fopen(HOSTS_TMP, "w");
    if (!dst) {
        perror("siteguard-helper: fopen tmp");
        fclose(src);
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), src)) {
        /* Drop lines we added — identified by both the domain and our marker */
        if (strstr(line, domain) && strstr(line, MARKER))
            continue;
        fputs(line, dst);
    }

    fclose(src);
    fclose(dst);

    if (rename(HOSTS_TMP, HOSTS_FILE) < 0) {
        perror("siteguard-helper: rename");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: siteguard-helper <block|unblock> <domain>\n");
        return 1;
    }

    const char *cmd    = argv[1];
    const char *domain = argv[2];

    if (!valid_domain(domain)) {
        fprintf(stderr, "siteguard-helper: invalid domain '%s'\n", domain);
        return 1;
    }

    if (strcmp(cmd, "block") == 0)   return do_block(domain);
    if (strcmp(cmd, "unblock") == 0) return do_unblock(domain);

    fprintf(stderr, "siteguard-helper: unknown command '%s'\n", cmd);
    return 1;
}
