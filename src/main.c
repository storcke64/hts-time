/*
 * Human Time System (HTS) Universal Tool
 * Copyright 2023-2025 Antonio Storcke (Inventor)
 * Integrated GUI & CLI Universal Binary - Version 1.2.2
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

/* --- CLI LOGIC --- */
static void run_cli_output() {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char date_buffer[128];
    strftime(date_buffer, sizeof(date_buffer), "%a %b %d %I:%M:%S %p %Z %Y", tm_info);

    char *hts_notation = calculate_hts_string(tm_info->tm_year + 1900);
    if (hts_notation) {
        printf("%s  --HTS %s\n", date_buffer, hts_notation);
        free(hts_notation);
    }
}

/* --- LOG SUPPRESSION --- */
static GLogWriterOutput
black_hole_writer(GLogLevelFlags log_level, const GLogField *fields,
                  gsize n_fields, gpointer user_data) {
    if (log_level & (G_LOG_LEVEL_ERROR | G_LOG_FLAG_FATAL)) {
        return g_log_writer_default(log_level, fields, n_fields, user_data);
    }
    return G_LOG_WRITER_HANDLED;
}

/* --- SETTINGS --- */
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

/* --- HELP HUB --- */
static void on_copy_command(GtkButton *btn, gpointer data) {
    (void)data;
    GdkClipboard *clipboard = gdk_display_get_clipboard(gdk_display_get_default());
    const char *command = "touch ~/.bashrc ~/.bash_aliases && "
                          "grep -q \".bash_aliases\" ~/.bashrc || echo \"if [ -f ~/.bash_aliases ]; then . ~/.bash_aliases; fi\" >> ~/.bashrc && "
                          "echo \"alias hts-time='flatpak run com.storcke64.hts_time'\" >> ~/.bash_aliases && "
                          "source ~/.bashrc && alias hts-time";
    gdk_clipboard_set_text(clipboard, command);
    gtk_button_set_label(btn, "Command Copied!");
}

static void on_dialog_response(GtkDialog *dialog, int response_id, gpointer data) {
    (void)response_id; (void)data;
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_help_clicked(GtkButton *btn, gpointer data) {
    (void)data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("HTS System Help",
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn))),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE, NULL);
    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content), 15);
    gtk_widget_set_margin_top(content, 20);
    gtk_widget_set_margin_bottom(content, 20);
    gtk_widget_set_margin_start(content, 20);
    gtk_widget_set_margin_end(content, 20);
    GtkWidget *info_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(info_label), "<b>Terminal CLI Setup</b>\nCopy and paste the setup script to enable 'hts-time' in your terminal.");
    gtk_label_set_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_append(GTK_BOX(content), info_label);
    GtkWidget *copy_btn = gtk_button_new_with_label("Copy Setup Command");
    gtk_widget_add_css_class(copy_btn, "suggested-action");
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_command), NULL);
    gtk_box_append(GTK_BOX(content), copy_btn);
    GtkWidget *about_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(about_label), "<span size='small' alpha='70%'>\nInventor: Antonio Storcke\nSystem Lifespan: 24B Years</span>");
    gtk_box_append(GTK_BOX(content), about_label);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* --- CALC HANDLERS --- */
static void on_theme_clicked(GtkButton *btn, gpointer data) {
    HtsApp *app = (HtsApp *)data;
    app->theme_id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "id"));
    apply_theme(app);
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

/* --- CLOCK ENGINE (Integrated Local/UTC) --- */
static gboolean update_clock(gpointer data) {
    HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL);

    struct tm *tm_utc = gmtime(&t);
    char hts_year[32], utc_time[64], utc_date[64];
    strftime(utc_date, sizeof(utc_date), "%A, %B %d, %Y", tm_utc);
    strftime(utc_time, sizeof(utc_time), "%H:%M:%S", tm_utc);
    char *hts = calculate_hts_string(tm_utc->tm_year + 1900);

    struct tm *tm_local = localtime(&t);
    char local_time[64], tz_name[32];
    strftime(local_time, sizeof(local_time), "%I:%M:%S %p", tm_local);
    strftime(tz_name, sizeof(tz_name), "%Z", tm_local);

    char *markup = g_strdup_printf(
        "<span size='42000' weight='bold' foreground='#3584e4'>%s</span>\n"
        "<span size='large' alpha='70%%'>%s</span>\n"
        "<span size='42000' weight='bold'>%s <span alpha='50%%' size='small'>UTC</span></span>    "
        "<span size='42000' weight='bold' foreground='#2ec27e'>%s <span alpha='50%%' size='small'>%s</span></span>",
        hts, utc_date, utc_time, local_time, tz_name);

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
    gtk_window_set_icon_name(GTK_WINDOW(window), "com.storcke64.hts_time");
    app->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(app->css_provider), 800);
    apply_theme(app);
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 25);
    gtk_widget_set_margin_top(main_box, 30);
    gtk_widget_set_margin_bottom(main_box, 30);
    gtk_widget_set_margin_start(main_box, 30);
    gtk_widget_set_margin_end(main_box, 30);
    gtk_window_set_child(GTK_WINDOW(window), main_box);
    GtkWidget *header_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(header_grid), 25);
    gtk_box_append(GTK_BOX(main_box), header_grid);
    GtkWidget *clock_card = create_card("CURRENT HUMAN TIME NOTATION / SYSTEM CLOCKS");
    app->label_live_clock = gtk_label_new(NULL);
    gtk_label_set_justify(GTK_LABEL(app->label_live_clock), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(clock_card), app->label_live_clock);
    gtk_grid_attach(GTK_GRID(header_grid), clock_card, 0, 0, 1, 1);
    gtk_widget_set_hexpand(clock_card, TRUE);
    GtkWidget *logo_image = gtk_image_new_from_resource("/com/storcke64/hts/logo.png");
    gtk_image_set_pixel_size(GTK_IMAGE(logo_image), 150);
    gtk_grid_attach(GTK_GRID(header_grid), logo_image, 1, 0, 1, 1);
    GtkWidget *tool_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(tool_grid), 30);
    gtk_grid_set_row_spacing(GTK_GRID(tool_grid), 25);
    gtk_grid_set_column_homogeneous(GTK_GRID(tool_grid), TRUE);
    gtk_box_append(GTK_BOX(main_box), tool_grid);
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
    app->entry_future = gtk_entry_new(); gtk_box_append(GTK_BOX(c3), app->entry_future);
    GtkWidget *bf = gtk_button_new_with_label("Get Future");
    g_signal_connect(bf, "clicked", G_CALLBACK(on_calc_future), app);
    gtk_box_append(GTK_BOX(c3), bf);
    app->label_future_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c3), app->label_future_result);
    app->entry_past = gtk_entry_new(); gtk_box_append(GTK_BOX(c3), app->entry_past);
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
    GtkWidget *help_btn = gtk_button_new_with_label("Help & Utilities");
    g_signal_connect(help_btn, "clicked", G_CALLBACK(on_help_clicked), app);
    gtk_box_append(GTK_BOX(footer), help_btn);
    gtk_box_append(GTK_BOX(main_box), footer);
    g_timeout_add_seconds(1, update_clock, app);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    gboolean force_gui = FALSE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gui") == 0) {
            force_gui = TRUE;
            for (int j = i; j < argc - 1; j++) argv[j] = argv[j+1];
            argc--;
            break;
        }
    }

    if (isatty(STDOUT_FILENO) && !force_gui) {
        run_cli_output();
        return 0;
    } else {
        g_log_set_writer_func(black_hole_writer, NULL, NULL);
        g_setenv("NO_AT_BRIDGE", "1", TRUE);
        HtsApp *app_data = g_new0(HtsApp, 1);
        GtkApplication *app = gtk_application_new("com.storcke64.hts_time", G_APPLICATION_FLAGS_NONE);
        g_signal_connect(app, "activate", G_CALLBACK(activate), app_data);
        int status = g_application_run(G_APPLICATION(app), argc, argv);
        g_object_unref(app);
        g_free(app_data);
        return status;
    }
}
