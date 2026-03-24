#include "aw_client.h"
#include <curl/curl.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* ---- libcurl response buffer ---- */

typedef struct {
    char  *data;
    size_t len;
} CurlBuf;

static size_t write_cb(void *ptr, size_t sz, size_t nmemb, void *userdata) {
    CurlBuf *b = userdata;
    size_t n   = sz * nmemb;
    b->data    = realloc(b->data, b->len + n + 1);
    if (!b->data) return 0;
    memcpy(b->data + b->len, ptr, n);
    b->len += n;
    b->data[b->len] = '\0';
    return n;
}

/* Returns heap-allocated response body, or NULL on error. Caller frees. */
static char *http_get(const char *url) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    CurlBuf buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL,           url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL,      1L); /* safe in multi-threaded GTK */

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        free(buf.data);
        return NULL;
    }
    return buf.data;
}

/* ---- URL → domain extraction ---- */

/*
 * "https://www.youtube.com/watch?v=xxx" → "youtube.com"
 * "http://reddit.com/r/linux"           → "reddit.com"
 */
static void extract_domain(const char *url, char *out, size_t len) {
    const char *p = strstr(url, "://");
    p = p ? p + 3 : url;

    if (strncmp(p, "www.", 4) == 0) p += 4;

    size_t i = 0;
    while (i < len - 1 && p[i] && p[i] != '/' && p[i] != '?' && p[i] != '#' && p[i] != ':') {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
}

/* ---- ISO 8601 UTC timestamps ---- */

static void utc_iso(time_t t, char *out, size_t len) {
    struct tm *g = gmtime(&t);
    strftime(out, len, "%Y-%m-%dT%H:%M:%SZ", g);
}

/* Midnight of today in local time, returned as UTC epoch seconds */
static time_t today_midnight(void) {
    time_t now  = time(NULL);
    struct tm t = *localtime(&now);
    t.tm_hour = 0;
    t.tm_min  = 0;
    t.tm_sec  = 0;
    return mktime(&t);
}

/* ---- Bucket discovery ---- */

#define MAX_WEB_BUCKETS 16

/*
 * Lists all AW buckets and returns those matching "aw-watcher-web-*".
 * Handles both Firefox and Chromium watchers simultaneously.
 */
static int find_web_buckets(char ids[MAX_WEB_BUCKETS][MAX_DOMAIN_LEN]) {
    char *resp = http_get(AW_BASE_URL "/buckets/");
    if (!resp) return 0;

    json_object *root = json_tokener_parse(resp);
    free(resp);
    if (!root) return 0;

    int n = 0;
    json_object_object_foreach(root, key, val) {
        (void)val;
        if (strncmp(key, "aw-watcher-web-", 15) == 0 && n < MAX_WEB_BUCKETS)
            strncpy(ids[n++], key, MAX_DOMAIN_LEN - 1);
    }

    json_object_put(root);
    return n;
}

/* ---- Event fetching and aggregation ---- */

static void fetch_bucket_events(const char *bucket_id, AppState *state,
                                 const char *start_iso, const char *end_iso) {
    char url[1024];
    snprintf(url, sizeof(url),
        AW_BASE_URL "/buckets/%s/events?start=%s&end=%s&limit=100000",
        bucket_id, start_iso, end_iso);

    char *resp = http_get(url);
    if (!resp) return;

    json_object *arr = json_tokener_parse(resp);
    free(resp);

    if (!arr || !json_object_is_type(arr, json_type_array)) {
        if (arr) json_object_put(arr);
        return;
    }

    int num_events = json_object_array_length(arr);
    for (int i = 0; i < num_events; i++) {
        json_object *ev   = json_object_array_get_idx(arr, i);
        json_object *jdur = json_object_object_get(ev, "duration");
        json_object *data = json_object_object_get(ev, "data");
        if (!jdur || !data) continue;

        json_object *jurl = json_object_object_get(data, "url");
        if (!jurl) continue;

        char domain[MAX_DOMAIN_LEN];
        extract_domain(json_object_get_string(jurl), domain, sizeof(domain));
        if (!domain[0]) continue;

        int dur_sec = (int)(json_object_get_double(jdur) + 0.5); /* round, don't truncate */

        for (int j = 0; j < state->count; j++) {
            if (strcasecmp(state->sites[j].domain, domain) == 0) {
                state->sites[j].used_sec += dur_sec;
                break;
            }
        }
    }

    json_object_put(arr);
}

/* ---- Public API ---- */

void aw_fetch_usage(AppState *state) {
    /* Reset all usage counters first */
    for (int i = 0; i < state->count; i++)
        state->sites[i].used_sec = 0;

    char start[32], end_[32];
    utc_iso(today_midnight(), start, sizeof(start));
    utc_iso(time(NULL),       end_,  sizeof(end_));

    char ids[MAX_WEB_BUCKETS][MAX_DOMAIN_LEN];
    int n = find_web_buckets(ids);
    if (n == 0) return; /* AW not running or aw-watcher-web not installed */

    for (int i = 0; i < n; i++)
        fetch_bucket_events(ids[i], state, start, end_);
}
