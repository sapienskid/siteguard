#include "ui.h"
#include "app.h"
#include "config.h"
#include "aw_client.h"
#include "blocker.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>

/* Module-level state — single window, single app */
static GtkWidget *g_window   = NULL;
static GtkWidget *g_list_box = NULL;
static GtkWidget *g_status   = NULL; /* statusbar label */
static AppState  *g_state    = NULL;

/* ---- CSS for progress bar error state ---- */

static void apply_css(void) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        /* Red progress bar when site is blocked or at limit */
        "progressbar.over-limit trough  { background-color: #3d0000; }"
        "progressbar.over-limit progress { background-color: #cc3333; }"
        /* Dimmed text for blocked status indicator */
        ".status-blocked { color: #cc3333; font-weight: bold; }"
        ".status-ok      { color: #44bb44; }"
        ".status-warning { color: #ddaa22; }"
        /* Column header row */
        ".col-header { font-size: 0.8em; color: alpha(currentColor, 0.5); }",
        -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css);
}

/* ---- List row construction ---- */

static GtkWidget *make_site_row(const Site *s) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    gtk_widget_set_margin_start(box,   14);
    gtk_widget_set_margin_end(box,     14);
    gtk_widget_set_margin_top(box,      7);
    gtk_widget_set_margin_bottom(box,   7);

    /* Domain label — fixed width, left-aligned, ellipsized */
    GtkWidget *lbl_domain = gtk_label_new(s->domain);
    gtk_widget_set_size_request(lbl_domain, 190, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl_domain), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(lbl_domain), PANGO_ELLIPSIZE_END);
    gtk_widget_set_tooltip_text(lbl_domain, s->domain);

    /* Progress bar */
    GtkWidget *progress = gtk_progress_bar_new();
    gtk_widget_set_hexpand(progress, TRUE);
    gtk_widget_set_valign(progress, GTK_ALIGN_CENTER);

    double frac = (s->budget_sec > 0)
        ? (double)s->used_sec / (double)s->budget_sec
        : 0.0;
    if (frac > 1.0) frac = 1.0;
    gtk_progress_bar_set_fraction(progress, frac);

    if (s->blocked || s->used_sec >= s->budget_sec) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(progress);
        gtk_style_context_add_class(ctx, "over-limit");
    }

    /* Time label: "45m / 60m" */
    char time_str[48];
    snprintf(time_str, sizeof(time_str), "%dm / %dm",
             s->used_sec / 60, s->budget_sec / 60);
    GtkWidget *lbl_time = gtk_label_new(time_str);
    gtk_widget_set_size_request(lbl_time, 96, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl_time), 1.0);

    /* Status indicator */
    const char *status_text;
    const char *status_css;
    if (s->blocked) {
        status_text = "BLOCKED";
        status_css  = "status-blocked";
    } else if (s->used_sec >= s->budget_sec) {
        status_text = "LIMIT";
        status_css  = "status-warning";
    } else {
        status_text = "OK";
        status_css  = "status-ok";
    }

    GtkWidget *lbl_status = gtk_label_new(status_text);
    gtk_widget_set_size_request(lbl_status, 60, -1);
    gtk_label_set_xalign(GTK_LABEL(lbl_status), 1.0);
    GtkStyleContext *sctx = gtk_widget_get_style_context(lbl_status);
    gtk_style_context_add_class(sctx, status_css);

    gtk_box_pack_start(GTK_BOX(box), lbl_domain, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), progress,   TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(box), lbl_time,   FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), lbl_status, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), box);
    gtk_widget_show_all(row);
    return row;
}

static GtkWidget *make_header_row(void) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_widget_set_sensitive(row, FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(box,  14);
    gtk_widget_set_margin_end(box,    14);
    gtk_widget_set_margin_top(box,     4);
    gtk_widget_set_margin_bottom(box,  2);

    GtkWidget *c1 = gtk_label_new("Domain");
    GtkWidget *c2 = gtk_label_new("Daily Usage");
    GtkWidget *c3 = gtk_label_new("Time");
    GtkWidget *c4 = gtk_label_new("Status");

    gtk_widget_set_size_request(c1,  190, -1);
    gtk_widget_set_hexpand(c2, TRUE);
    gtk_widget_set_size_request(c3,   96, -1);
    gtk_widget_set_size_request(c4,   60, -1);

    gtk_label_set_xalign(GTK_LABEL(c1), 0.0);
    gtk_label_set_xalign(GTK_LABEL(c2), 0.0);
    gtk_label_set_xalign(GTK_LABEL(c3), 1.0);
    gtk_label_set_xalign(GTK_LABEL(c4), 1.0);

    GtkWidget *headers[] = { c1, c2, c3, c4, NULL };
    for (int h = 0; headers[h]; h++) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(headers[h]);
        gtk_style_context_add_class(ctx, "col-header");
    }

    gtk_box_pack_start(GTK_BOX(box), c1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), c2, TRUE,  TRUE,  0);
    gtk_box_pack_start(GTK_BOX(box), c3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), c4, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(row), box);
    gtk_widget_show_all(row);
    return row;
}

/* ---- Public refresh ---- */

void ui_refresh(void) {
    if (!g_list_box || !g_state) return;

    /* Destroy all existing rows */
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_list_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    /* Column headers */
    gtk_list_box_insert(GTK_LIST_BOX(g_list_box), make_header_row(), -1);

    /* Site rows */
    if (g_state->count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        GtkWidget *lbl = gtk_label_new("No sites yet. Press + to add one.");
        gtk_widget_set_margin_top(lbl, 28);
        gtk_widget_set_margin_bottom(lbl, 28);
        gtk_container_add(GTK_CONTAINER(row), lbl);
        gtk_widget_show_all(row);
        gtk_list_box_insert(GTK_LIST_BOX(g_list_box), row, -1);
    } else {
        for (int i = 0; i < g_state->count; i++)
            gtk_list_box_insert(GTK_LIST_BOX(g_list_box), make_site_row(&g_state->sites[i]), -1);
    }
}

/* ---- Signal handlers ---- */

static void on_refresh_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;

    if (g_status)
        gtk_label_set_text(GTK_LABEL(g_status), "Fetching usage…");

    aw_fetch_usage(g_state);
    blocker_sync(g_state);
    ui_refresh();

    if (g_status)
        gtk_label_set_text(GTK_LABEL(g_status), "Updated.");
}

static void on_add_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (!g_window || !g_state) return;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Add Site",
        GTK_WINDOW(g_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Add",    GTK_RESPONSE_ACCEPT,
        NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid),    10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 14);
    gtk_widget_set_margin_start(grid,  18);
    gtk_widget_set_margin_end(grid,    18);
    gtk_widget_set_margin_top(grid,    14);
    gtk_widget_set_margin_bottom(grid, 10);

    GtkWidget *lbl_d   = gtk_label_new("Domain:");
    GtkWidget *entry_d = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_d), "youtube.com");
    gtk_widget_set_size_request(entry_d, 240, -1);

    GtkWidget *lbl_b   = gtk_label_new("Budget (minutes):");
    GtkWidget *spin_b  = gtk_spin_button_new_with_range(1, 1440, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_b), 30);

    gtk_label_set_xalign(GTK_LABEL(lbl_d), 0.0);
    gtk_label_set_xalign(GTK_LABEL(lbl_b), 0.0);

    gtk_grid_attach(GTK_GRID(grid), lbl_d,   0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_d, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), lbl_b,   0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), spin_b,  1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);

    /* Make entry activate the Accept button */
    gtk_entry_set_activates_default(GTK_ENTRY(entry_d), TRUE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    gint resp = gtk_dialog_run(GTK_DIALOG(dialog));

    if (resp == GTK_RESPONSE_ACCEPT && g_state->count < MAX_SITES) {
        const char *domain  = gtk_entry_get_text(GTK_ENTRY(entry_d));
        int         minutes = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_b));

        if (domain && domain[0]) {
            Site *s       = &g_state->sites[g_state->count++];
            strncpy(s->domain, domain, MAX_DOMAIN_LEN - 1);
            s->domain[MAX_DOMAIN_LEN - 1] = '\0';
            s->budget_sec = minutes * 60;
            s->used_sec   = 0;
            s->blocked    = false;

            config_save(g_state);
            ui_refresh();
        }
    }

    gtk_widget_destroy(dialog);
}

static void on_remove_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (!g_list_box || !g_state) return;

    GtkListBoxRow *row = gtk_list_box_get_selected_row(GTK_LIST_BOX(g_list_box));
    if (!row) return;

    /* Row 0 is the header; site rows start at index 1 */
    int row_idx  = gtk_list_box_row_get_index(row);
    int site_idx = row_idx - 1;

    if (site_idx < 0 || site_idx >= g_state->count) return;

    /* Unblock before removing */
    if (g_state->sites[site_idx].blocked)
        blocker_unblock(g_state->sites[site_idx].domain);

    /* Shift array down */
    for (int i = site_idx; i < g_state->count - 1; i++)
        g_state->sites[i] = g_state->sites[i + 1];
    g_state->count--;

    config_save(g_state);
    ui_refresh();
}

/* ---- Window builder ---- */

GtkWidget *ui_build_window(GtkApplication *app, AppState *state) {
    g_state = state;

    apply_css();

    g_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(g_window), "SiteGuard");
    gtk_window_set_default_size(GTK_WINDOW(g_window), 600, 380);
    gtk_window_set_resizable(GTK_WINDOW(g_window), TRUE);

    /* Header bar */
    GtkWidget *headerbar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(headerbar), "SiteGuard");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(headerbar), "Daily site budgets");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(headerbar), TRUE);

    GtkWidget *btn_refresh = gtk_button_new_from_icon_name(
        "view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(btn_refresh, "Refresh usage from ActivityWatch");
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(headerbar), btn_refresh);

    gtk_window_set_titlebar(GTK_WINDOW(g_window), headerbar);

    /* Main vertical layout */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Scrolled list */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);

    g_list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(g_list_box), GTK_SELECTION_SINGLE);
    gtk_container_add(GTK_CONTAINER(scroll), g_list_box);
    gtk_container_add(GTK_CONTAINER(vbox), scroll);

    /* Separator */
    gtk_container_add(GTK_CONTAINER(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Action bar */
    GtkWidget *action_bar = gtk_action_bar_new();

    GtkWidget *btn_add = gtk_button_new_with_label("+ Add");
    GtkWidget *btn_rem = gtk_button_new_with_label("− Remove");
    gtk_widget_set_tooltip_text(btn_add, "Add a new site to monitor");
    gtk_widget_set_tooltip_text(btn_rem, "Remove selected site");

    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_clicked),    NULL);
    g_signal_connect(btn_rem, "clicked", G_CALLBACK(on_remove_clicked), NULL);

    gtk_action_bar_pack_start(GTK_ACTION_BAR(action_bar), btn_add);
    gtk_action_bar_pack_start(GTK_ACTION_BAR(action_bar), btn_rem);

    /* Status label on the right */
    g_status = gtk_label_new("Ready");
    GtkStyleContext *sctx = gtk_widget_get_style_context(g_status);
    gtk_style_context_add_class(sctx, "col-header");
    gtk_action_bar_pack_end(GTK_ACTION_BAR(action_bar), g_status);

    gtk_container_add(GTK_CONTAINER(vbox), action_bar);

    gtk_container_add(GTK_CONTAINER(g_window), vbox);

    return g_window;
}
