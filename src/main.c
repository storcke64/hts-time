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

/* --- UPDATED CLI LOGIC --- */

static void run_cli_mode() {
    time_t now = time(NULL);
    struct tm *t = gmtime(&now); // UTC as per your universal tool design

    // Call your actual logic function to get the correct HTS notation (e.g., A40,002)
    char *hts_notation = calculate_hts_string(t->tm_year + 1900);

    char time24[10];
    char time12[20];

    strftime(time24, sizeof(time24), "%H:%M", t);
    strftime(time12, sizeof(time12), "%I:%M %p", t);

    printf("\n========================================\n");
    printf("        HUMAN TIME SYSTEM (CLI)        \n");
    printf("========================================\n");
    printf("  HTS DATE: %s\n", hts_notation);
    printf("  REF TIME: %s or %s\n", time24, time12);
    printf("========================================\n\n");

    g_free(hts_notation);
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
        /* 0: Dark */      "window { background-color: #1e1e1e; color: white; } .card { background-color: #242424; border: 1px solid #303030; } entry { background-color: #303030; color: white; }",
        /* 1: Light */     "window { background-color: #f6f5f4; color: black; } .card { background-color: white; border: 1px solid #dcdcdc; } entry { background-color: #ffffff; color: black; }",
        /* 2: Sepia */     "window { background-color: #f4ecd8; color: #5b4636; } .card { background-color: #fdf6e3; border: 1px solid #eee8d5; } entry { background-color: #fdf6e3; color: #5b4636; }",
        /* 3: Slate */     "window { background-color: #2f343f; color: #d3dae3; } .card { background-color: #383c4a; border: 1px solid #4b5162; } entry { background-color: #404552; color: white; }",
        /* 4: Neon Pink */ "window { background-color: #1a001a; color: #ff00ff; } .card { background-color: #2b002b; border: 1px solid #ff00ff; } entry { background-color: #3d003d; color: white; border-color: #ff00ff; } button.suggested-action { background-color: #ff00ff !important; }"
    };

    char *full_css = g_strdup_printf(
        "%s .card { border-radius: 12px; padding: 18px; } "
        "entry { border-radius: 6px; border: 1px solid #404040; padding: 6px; } "
        "button.suggested-action { background-color: #3584e4; color: white; font-weight: bold; border-radius: 8px; }",
        themes[app->theme_id]);

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

static void on_greg_toggle(GtkCheckButton *btn, gpointer data) {
    HtsApp *app = (HtsApp *)data;
    app->show_greg_year = gtk_check_button_get_active(btn);
    gtk_widget_set_visible(app->greg_container, app->show_greg_year);
    save_settings(app);
}

static void on_copy_clicked(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL);
    char *hts = calculate_hts_string(gmtime(&t)->tm_year + 1900);
    gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(app->label_live_clock)), hts);
    g_free(hts);
}

/* --- CALCULATORS --- */

static void on_convert_to_hts(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    char *hts = calculate_hts_string(atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_greg))));
    char *out = g_strdup_printf("<span size='large' weight='bold' foreground='#3584e4'>%s</span>", hts);
    gtk_label_set_markup(GTK_LABEL(app->label_hts_result), out);
    g_free(hts); g_free(out);
}

static void on_convert_to_greg(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    long greg = gregorian_from_hts(gtk_editable_get_text(GTK_EDITABLE(app->entry_hts)));
    char *out = g_strdup_printf("<span size='large' weight='bold' foreground='#2ec27e'>%ld Anno Domini</span>", greg);
    gtk_label_set_markup(GTK_LABEL(app->label_greg_result), out);
    g_free(out);
}

static void on_calc_future(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL);
    int target = (gmtime(&t)->tm_year + 1900) + atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_future)));
    char *hts = calculate_hts_string(target);
    gtk_label_set_markup(GTK_LABEL(app->label_future_result), hts);
    g_free(hts);
}

static void on_calc_past(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL);
    int target = (gmtime(&t)->tm_year + 1900) - atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_past)));
    char *hts = calculate_hts_string(target);
    gtk_label_set_markup(GTK_LABEL(app->label_past_result), hts);
    g_free(hts);
}

static void on_calc_duration(GtkButton *btn, gpointer data) {
    (void)btn; HtsApp *app = (HtsApp *)data;
    int diff = atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_dur_end))) - atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_dur_start)));
    char *out = g_strdup_printf("<span weight='bold' size='large'>%d Years</span>", diff);
    gtk_label_set_markup(GTK_LABEL(app->label_dur_result), out);
    g_free(out);
}

/* --- UI HELPERS --- */

static gboolean update_clock(gpointer data) {
    HtsApp *app = (HtsApp *)data;
    time_t t = time(NULL); struct tm *tm_info = gmtime(&t);
    char date_str[64]; strftime(date_str, sizeof(date_str), "%A, %B %d, %Y", tm_info);
    char *hts = calculate_hts_string(tm_info->tm_year + 1900);
    char *markup = g_strdup_printf("<span size='x-large' font_family='monospace' weight='bold' foreground='#3584e4'>%s</span>\n<span size='small' alpha='70%%'>%s</span>", hts, date_str);
    char *greg_markup = g_strdup_printf("<span font_family='monospace' size='small'>Gregorian Year: %d</span>", tm_info->tm_year + 1900);
    gtk_label_set_markup(GTK_LABEL(app->label_live_clock), markup);
    gtk_label_set_markup(GTK_LABEL(app->label_greg_year), greg_markup);
    g_free(hts); g_free(markup); g_free(greg_markup);
    return TRUE;
}

static GtkWidget* create_card(const char *title) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(box, "card");
    GtkWidget *label = gtk_label_new(NULL);
    char *markup = g_strdup_printf("<span size='small' weight='bold' alpha='60%%'>%s</span>", title);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(box), label);
    g_free(markup);
    return box;
}

/* --- MAIN ACTIVATION --- */

static void activate(GtkApplication *gtk_app, gpointer user_data) {
    HtsApp *app = (HtsApp *)user_data;
    load_settings(app);

    GtkWidget *window = gtk_application_window_new(gtk_app);
    gtk_window_set_title(GTK_WINDOW(window), "Human Time System Universal Tool");
    gtk_window_set_default_size(GTK_WINDOW(window), 1250, 850);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    gtk_window_set_icon_name(GTK_WINDOW(window), "com.storcke64.hts_time");

    app->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(app->css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    apply_theme(app);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(main_box, 30);
    gtk_widget_set_margin_bottom(main_box, 30);
    gtk_widget_set_margin_start(main_box, 30);
    gtk_widget_set_margin_end(main_box, 30);
    gtk_window_set_child(GTK_WINDOW(window), main_box);

    GtkWidget *header_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(header_grid), 25);
    gtk_box_append(GTK_BOX(main_box), header_grid);

    GtkWidget *clock_card = create_card("CURRENT HUMAN TIME NOTATION (UTC)");
    GtkWidget *clock_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    app->label_live_clock = gtk_label_new(NULL);
    gtk_box_append(GTK_BOX(clock_hbox), app->label_live_clock);
    GtkWidget *copy_btn = gtk_button_new_from_icon_name("edit-copy-symbolic");
    gtk_widget_set_valign(copy_btn, GTK_ALIGN_START);
    g_signal_connect(copy_btn, "clicked", G_CALLBACK(on_copy_clicked), app);
    gtk_box_append(GTK_BOX(clock_hbox), copy_btn);
    gtk_box_append(GTK_BOX(clock_card), clock_hbox);

    app->greg_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    app->label_greg_year = gtk_label_new(NULL);
    gtk_box_append(GTK_BOX(app->greg_container), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(app->greg_container), app->label_greg_year);
    gtk_box_append(GTK_BOX(clock_card), app->greg_container);
    gtk_widget_set_visible(app->greg_container, app->show_greg_year);

    gtk_grid_attach(GTK_GRID(header_grid), clock_card, 0, 0, 1, 1);
    gtk_widget_set_hexpand(clock_card, TRUE);
    g_timeout_add_seconds(1, update_clock, app);

    GtkWidget *prov_card = create_card("PROVENANCE");
    GtkWidget *prov_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(prov_label), "<span size='small' font_family='monospace'>Inventor: Antonio Storcke\nDate: Jan 15, 2023\nLifespan: 24B Years (∞)</span>");
    gtk_box_append(GTK_BOX(prov_card), prov_label);
    gtk_grid_attach(GTK_GRID(header_grid), prov_card, 1, 0, 1, 1);

    GtkWidget *logo_image = gtk_image_new_from_resource("/com/storcke64/hts/logo.png");
    gtk_image_set_pixel_size(GTK_IMAGE(logo_image), 200);
    gtk_grid_attach(GTK_GRID(header_grid), logo_image, 2, 0, 1, 1);

    GtkWidget *greg_switch = gtk_check_button_new_with_label("Show Gregorian Year in Clock Display");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(greg_switch), app->show_greg_year);
    g_signal_connect(greg_switch, "toggled", G_CALLBACK(on_greg_toggle), app);
    gtk_box_append(GTK_BOX(main_box), greg_switch);

    GtkWidget *tool_grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(tool_grid), 30);
    gtk_grid_set_row_spacing(GTK_GRID(tool_grid), 25);
    gtk_grid_set_column_homogeneous(GTK_GRID(tool_grid), TRUE);
    gtk_box_append(GTK_BOX(main_box), tool_grid);

    GtkWidget *c1 = create_card("GREGORIAN TO HTS");
    app->entry_greg = gtk_entry_new(); gtk_box_append(GTK_BOX(c1), app->entry_greg);
    GtkWidget *b1 = gtk_button_new_with_label("Calculate Notation");
    gtk_widget_add_css_class(b1, "suggested-action");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_convert_to_hts), app);
    gtk_box_append(GTK_BOX(c1), b1);
    app->label_hts_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c1), app->label_hts_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c1, 0, 0, 1, 1);

    GtkWidget *c2 = create_card("HTS TO GREGORIAN");
    app->entry_hts = gtk_entry_new(); gtk_box_append(GTK_BOX(c2), app->entry_hts);
    GtkWidget *b2 = gtk_button_new_with_label("Calculate Anno Domini");
    g_signal_connect(b2, "clicked", G_CALLBACK(on_convert_to_greg), app);
    gtk_box_append(GTK_BOX(c2), b2);
    app->label_greg_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c2), app->label_greg_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c2, 1, 0, 1, 1);

    GtkWidget *c3 = create_card("RELATIVE TIME LOOKUP");
    app->entry_future = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_future), "Years in Future");
    gtk_box_append(GTK_BOX(c3), app->entry_future);
    GtkWidget *bf = gtk_button_new_with_label("Get Future Notation");
    g_signal_connect(bf, "clicked", G_CALLBACK(on_calc_future), app);
    gtk_box_append(GTK_BOX(c3), bf);
    app->label_future_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c3), app->label_future_result);

    app->entry_past = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_past), "Years in Past");
    gtk_box_append(GTK_BOX(c3), app->entry_past);
    GtkWidget *bp = gtk_button_new_with_label("Get Past Notation");
    g_signal_connect(bp, "clicked", G_CALLBACK(on_calc_past), app);
    gtk_box_append(GTK_BOX(c3), app->entry_past);
    app->label_past_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c3), app->label_past_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c3, 0, 1, 1, 1);

    GtkWidget *c4 = create_card("DURATION CALCULATOR");
    app->entry_dur_start = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_dur_start), "Start Year");
    app->entry_dur_end = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_dur_end), "End Year");
    gtk_box_append(GTK_BOX(c4), app->entry_dur_start); gtk_box_append(GTK_BOX(c4), app->entry_dur_end);
    GtkWidget *b4 = gtk_button_new_with_label("Calculate Difference");
    g_signal_connect(b4, "clicked", G_CALLBACK(on_calc_duration), app);
    gtk_box_append(GTK_BOX(c4), b4);
    app->label_dur_result = gtk_label_new("---"); gtk_box_append(GTK_BOX(c4), app->label_dur_result);
    gtk_grid_attach(GTK_GRID(tool_grid), c4, 1, 1, 1, 1);

    GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(bottom_bar, GTK_ALIGN_CENTER);
    const char *names[] = {"Dark", "Light", "Sepia", "Slate", "Neon Pink"};
    for (int i=0; i<5; i++) {
        GtkWidget *b = gtk_button_new_with_label(names[i]);
        g_object_set_data(G_OBJECT(b), "id", GINT_TO_POINTER(i));
        g_signal_connect(b, "clicked", G_CALLBACK(on_theme_clicked), app);
        gtk_box_append(GTK_BOX(bottom_bar), b);
    }
    gtk_box_append(GTK_BOX(main_box), bottom_bar);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    if (isatty(STDOUT_FILENO)) {
        run_cli_mode();
        return 0;
    }

    g_setenv("GTK_A11Y", "none", TRUE);
    HtsApp *app_data = g_new0(HtsApp, 1);

    GtkApplication *app = gtk_application_new("com.storcke64.hts_time", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), app_data);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app); g_free(app_data); return status;
}
