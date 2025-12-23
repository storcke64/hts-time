/*
 * Human Time System (HTS) Universal Tool
 * Copyright 2023-2025 Antonio Storcke (Inventor)
 * Licensed under the Apache License, Version 2.0
 */

#include <gtk/gtk.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logic.h"

#define CONFIG_FILE "hts_settings.ini"

typedef struct {
    GtkWidget *label_live_clock, *label_greg_year, *greg_container;
    GtkWidget *entry_greg, *label_hts_result;
    GtkWidget *entry_hts, *label_greg_result;
    GtkWidget *entry_future, *label_future_result;
    GtkWidget *entry_past, *label_past_result;
    GtkWidget *entry_dur_start, *entry_dur_end, *label_dur_result;
    GtkCssProvider *css_provider;
    gboolean show_greg_year;
    int theme_id;
} HtsApp;

/* --- ADVANCED SUPPRESSION --- */
static GLogWriterOutput
black_hole_writer(GLogLevelFlags log_level, const GLogField *fields,
                  gsize n_fields, gpointer user_data) {
    if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL)) {
        return g_log_writer_default(log_level, fields, n_fields, user_data);
    }
    return G_LOG_WRITER_HANDLED;
}

/* --- SETTINGS PERSISTENCE --- */
static void save_settings(HtsApp *app) {
    GKeyFile *key_file = g_key_file_new();
    g_key_file_set_boolean(key_file, "Settings", "ShowGregorian", app->show_greg_year);
    g_key_file_set_integer(key_file, "Settings", "ThemeID", app->theme_id);
    g_key_file_save_to_file(key_file, CONFIG_FILE, NULL);
    g_key_file_free(key_file);
}

static void load_settings(HtsApp *app) {
    GKeyFile *key_file = g_key_file_new();
    if (g_key_file_load_from_file(key_file, CONFIG_FILE, G_KEY_FILE_NONE, NULL)) {
        app->show_greg_year = g_key_file_get_boolean(key_file, "Settings", "ShowGregorian", NULL);
        app->theme_id = g_key_file_get_integer(key_file, "Settings", "ThemeID", NULL);
    } else { app->show_greg_year = TRUE; app->theme_id = 0; }
    g_key_file_free(key_file);
}

/* --- THEME ENGINE --- */
static void apply_theme(HtsApp *app) {
    const char *themes[] = {
        "window { background-color: #1e1e1e; color: white; } .card { background-color: #242424; border: 1px solid #303030; }",
        "window { background-color: #f6f5f4; color: black; } .card { background-color: white; border: 1px solid #dcdcdc; }",
        "window { background-color: #f4ecd8; color: #5b4636; } .card { background-color: #fdf6e3; border: 1px solid #eee8d5; }",
        "window { background-color: #2f343f; color: #d3dae3; } .card { background-color: #383c4a; border: 1px solid #4b5162; }",
        "window { background-color: #1a001a; color: #00ff00; } .card { background-color: #2b002b; border: 1px solid #00ff00; }"
    };
    char *full_css = g_strdup_printf("%s .card { border-radius: 12px; padding: 20px; }", themes[app->theme_id]);
    gtk_css_provider_load_from_string(app->css_provider, full_css);
    g_free(full_css);
    save_settings(app);
}

/* --- HANDLERS --- */
static void on_theme_clicked(GtkButton *btn, gpointer data) {
    HtsApp *app = (HtsApp *)data;
    app->theme_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "id"));
    apply_theme(app);
}

static void on_help_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    gtk_show_about_dialog(NULL,
        "program-name", "HTS Tool",
        "version", "1.1.9",
        "copyright", "Copyright © 2023-2025 Antonio Storcke",
        "authors", (const char *[]) {"Antonio Storcke (Inventor)", NULL},
        "website", "https://storcke64.github.io/HUMAN-TIME-SYSTEM/",
        "comments", "System Lifespan: 24B Years. AUTHOR-QR-CODE.png required.",
        NULL);
}

static void on_convert_to_hts(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    char *hts = calculate_hts_string(atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_greg))));
    gtk_label_set_markup(GTK_LABEL(app->label_hts_result), hts);
    free(hts);
}

static void on_convert_to_greg(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    long greg = gregorian_from_hts(gtk_editable_get_text(GTK_EDITABLE(app->entry_hts)));
    char *out = g_strdup_printf("<b>%ld AD</b>", greg);
    gtk_label_set_markup(GTK_LABEL(app->label_greg_result), out);
    g_free(out);
}

static void on_calc_future(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL);
    int target = (gmtime(&t)->tm_year + 1900) + atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_future)));
    char *hts = calculate_hts_string(target);
    gtk_label_set_markup(GTK_LABEL(app->label_future_result), hts);
    free(hts);
}

static void on_calc_past(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL);
    int target = (gmtime(&t)->tm_year + 1900) - atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_past)));
    char *hts = calculate_hts_string(target);
    gtk_label_set_markup(GTK_LABEL(app->label_past_result), hts);
    free(hts);
}

static void on_calc_duration(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    int diff = atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_dur_end))) - atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_dur_start)));
    char *out = g_strdup_printf("<b>%d Years</b>", (int)abs(diff));
    gtk_label_set_markup(GTK_LABEL(app->label_dur_result), out);
    g_free(out);
}

/* --- CLOCK ENGINE --- */
static gboolean update_clock(gpointer data) {
    HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL); struct tm *tm_info = gmtime(&t);
    char date_str[64], time_str[64];
    strftime(date_str, sizeof(date_str), "%A, %B %d, %Y", tm_info);
    strftime(time_str, sizeof(time_str), "%I:%M:%S %p", tm_info);
    char *hts = calculate_hts_string(tm_info->tm_year + 1900);
    char *markup = g_strdup_printf(
        "<span size='42000' weight='bold' foreground='#3584e4'>%s</span>\n"
        "<span size='large'>%s</span>\n"
        "<span size='42000' weight='bold' foreground='#3584e4'>%s</span>",
        hts, date_str, time_str);
    gtk_label_set_markup(GTK_LABEL(app->label_live_clock), markup);
    free(hts); g_free(markup);
    return TRUE;
}

static GtkWidget* create_card(const char *title) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(box, "card");
    GtkWidget *label = gtk_label_new(NULL);
    char *markup = g_strdup_printf("<span size='small' weight='bold' alpha='60%%'>%s</span>", title);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_box_append(GTK_BOX(box), label);
    g_free(markup);
    return box;
}

/* --- ACTIVATION --- */
static void activate(GtkApplication *gtk_app, gpointer user_data) {
    HtsApp *app = (HtsApp *)user_data;
    load_settings(app);
    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_default_size(GTK_WINDOW(window), 1250, 950);
    gtk_window_set_icon_name(GTK_WINDOW(window), "com.storcke64.hts-time");

    app->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(app->css_provider), 800);
    apply_theme(app);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 25);
    gtk_widget_set_margin_top(main_box, 30); gtk_widget_set_margin_bottom(main_box, 30);
    gtk_widget_set_margin_start(main_box, 30); gtk_widget_set_margin_end(main_box, 30);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *header_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(header_grid), 25);
    gtk_box_append(GTK_BOX(main_box), header_grid);

    GtkWidget *clock_card = create_card("CURRENT HUMAN TIME NOTATION");
    app->label_live_clock = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(app->label_live_clock), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(clock_card), app->label_live_clock);
    gtk_grid_attach(GTK_GRID(header_grid), clock_card, 0, 0, 1, 1);
    gtk_widget_set_hexpand(clock_card, TRUE);

    GtkWidget *prov_card = create_card("PROVENANCE");
    GtkWidget *prov_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(prov_label), "<span size='small' font_family='monospace'>Inventor: Antonio Storcke\nSystem: HTS\nLifespan: 24B Years</span>");
    gtk_box_append(GTK_BOX(prov_card), prov_label);
    gtk_grid_attach(GTK_GRID(header_grid), prov_card, 1, 0, 1, 1);

    GtkWidget *logo_image = gtk_image_new_from_resource("/com/storcke64/hts/logo.png");
    gtk_image_set_pixel_size(GTK_IMAGE(logo_image), 150);
    gtk_grid_attach(GTK_GRID(header_grid), logo_image, 2, 0, 1, 1);

    GtkWidget *tool_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(tool_grid), 30);
    gtk_grid_set_row_spacing(GTK_GRID(tool_grid), 25);
    gtk_grid_set_column_homogeneous(GTK_GRID(tool_grid), TRUE);
    gtk_box_append(GTK_BOX(main_box), tool_grid);

    /* Calculator Suite */
    GtkWidget *c1 = create_card("GREGORIAN TO HTS");
    app->entry_greg = gtk_entry_new(); gtk_box_append(GTK_BOX(c1), app->entry_greg);
    GtkWidget *b1 = gtk_button_new_with_label("Calculate");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_convert_to_hts), app);
    gtk_box_append(GTK_BOX(c1), b1);
    app->label_hts_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c1), app->label_hts_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c1, 0, 0, 1, 1);

    GtkWidget *c2 = create_card("HTS TO GREGORIAN");
    app->entry_hts = gtk_entry_new(); gtk_box_append(GTK_BOX(c2), app->entry_hts);
    GtkWidget *b2 = gtk_button_new_with_label("Lookup");
    g_signal_connect(b2, "clicked", G_CALLBACK(on_convert_to_greg), app);
    gtk_box_append(GTK_BOX(c2), b2);
    app->label_greg_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c2), app->label_greg_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c2, 1, 0, 1, 1);

    GtkWidget *c3 = create_card("RELATIVE LOOKUP");
    app->entry_future = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_future), "Future Years");
    gtk_box_append(GTK_BOX(c3), app->entry_future);
    GtkWidget *bf = gtk_button_new_with_label("Get Future");
    g_signal_connect(bf, "clicked", G_CALLBACK(on_calc_future), app);
    gtk_box_append(GTK_BOX(c3), bf);
    app->label_future_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c3), app->label_future_result);
    app->entry_past = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_past), "Past Years");
    gtk_box_append(GTK_BOX(c3), app->entry_past);
    GtkWidget *bp = gtk_button_new_with_label("Get Past");
    g_signal_connect(bp, "clicked", G_CALLBACK(on_calc_past), app);
    gtk_box_append(GTK_BOX(c3), bp);
    app->label_past_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c3), app->label_past_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c3, 0, 1, 1, 1);

    GtkWidget *c4 = create_card("DURATION");
    app->entry_dur_start = gtk_entry_new(); app->entry_dur_end = gtk_entry_new();
    gtk_box_append(GTK_BOX(c4), app->entry_dur_start); gtk_box_append(GTK_BOX(c4), app->entry_dur_end);
    GtkWidget *b4 = gtk_button_new_with_label("Span");
    g_signal_connect(b4, "clicked", G_CALLBACK(on_calc_duration), app);
    gtk_box_append(GTK_BOX(c4), b4);
    app->label_dur_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c4), app->label_dur_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c4, 1, 1, 1, 1);

    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(footer, GTK_ALIGN_CENTER);
    const char *names[] = {"Dark", "Light", "Sepia", "Slate", "Neon"};
    for (int i=0; i<5; i++) {
        GtkWidget *b = gtk_button_new_with_label(names[i]);
        g_object_set_data(G_OBJECT(b), "id", GINT_TO_POINTER(i));
        g_signal_connect(b, "clicked", G_CALLBACK(on_theme_clicked), app);
        gtk_box_append(GTK_BOX(footer), b);
    }
    GtkWidget *help_btn = gtk_button_new_with_label("Help & About");
    g_signal_connect(help_btn, "clicked", G_CALLBACK(on_help_clicked), app);
    gtk_box_append(GTK_BOX(footer), help_btn);
    gtk_box_append(GTK_BOX(main_box), footer);

    g_timeout_add_seconds(1, update_clock, app);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    // 1. Silent Logger: Trap all GTK/GLib noise
    g_log_set_writer_func(black_hole_writer, NULL, NULL);
    g_setenv("NO_AT_BRIDGE", "1", TRUE);

    HtsApp *app_data = g_new0(HtsApp, 1);
    GtkApplication *app = gtk_application_new("com.storcke64.hts-time", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), app_data);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    g_free(app_data);
    return status;
}
