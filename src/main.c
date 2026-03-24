#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "config.h"
#include "aw_client.h"
#include "blocker.h"
#include "ui.h"

static AppState g_state;

static gboolean on_poll_tick(gpointer data) {
    (void)data;
    aw_fetch_usage(&g_state);
    blocker_sync(&g_state);
    ui_refresh();
    return G_SOURCE_CONTINUE;
}

static void on_activate(GtkApplication *app, gpointer data) {
    (void)data;
    GtkWidget *win = ui_build_window(app, &g_state);

    aw_fetch_usage(&g_state);
    blocker_sync(&g_state);
    ui_refresh();

    gtk_widget_show_all(win);
    g_timeout_add_seconds(60, on_poll_tick, NULL);
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--check") == 0) {
        config_load(&g_state);
        aw_fetch_usage(&g_state);
        blocker_sync(&g_state);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--reset") == 0) {
        config_load(&g_state);
        blocker_unblock_all(&g_state);
        fprintf(stderr, "siteguard: daily reset complete\n");
        return 0;
    }

    config_load(&g_state);

    GtkApplication *app = gtk_application_new(
        "com.siteguard.app",
        G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
