/* HTS_VERSION: main.c v2.00015 — 2026-02-22
 * v2.00015: Clock forced cyan #00d4ff with !important-equivalent GTK override
 * v2.00014: libmicrohttpd replaces GSocketService (Gemini fix); video playback solved
 * v2.00014: Vault files served as file:// not http:// — HTTP streaming was always broken
 * v2.00013: Honor large seeks (>8MB) fully; cap only small sequential chunks at 4MB
 * v2.00011: Removed cap on Range requests — caused GStreamer seeking panic, reverted
 * v2.00010: 32MB chunk (too large, caused constant SHORT disconnects)
 * v2.00009: 4MB cap all requests (too small, caused rebuffer cycling)
 * v2.00009: Cap ALL video responses at 4MB incl plain GET (no Range header)
 * v2.00008: Import progress polls actual disk file size every 2s (honest progress)
 * v2.00007: Import progress callback (unreliable, replaced)
 * v2.00006: 4MB cap on all video range requests; GThreadedSocketService; no hash
 * v2.00001: log_mutex protects FILE* debug_log from concurrent thread writes
 * v2.00000: Threaded video streaming via GTask + raw POSIX fd (crash fix)
 * v1.8: Per-request HTTP video logging (REQ/DONE/DISC/SHORT/416)
 * v1.7: overlay_hide/show hides logo_image AND hud_box
 * v1.6: log_warn + log_error functions added
 * v1.5: %c format cast fix, misleading-indentation warnings cleared
 * v1.4: Range handler honours both start AND end bytes
 */
/*
 * Storcke Human Time System - HTS Universal Tool
 * STATUS: DIAMOND MASTER (Strict HUD Routing + Smart Chooser)
 */

#include <gtk/gtk.h>
#include <webkitgtk-6.0/webkit/webkit.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <gio/gnetworking.h>
#include <microhttpd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef DATADIR
#define DATADIR "/app/bin"
#endif

/* ══════════════════════════════════════════════════════════════════
 * HTS LOG CONTROL CENTER
 * Set HTS_DEBUG to 1 for full terminal output, 0 for silent mode.
 * The file log (hts_debug.log) is ALWAYS written regardless of this
 * flag — only terminal/stdout output is silenced.
 *
 * SILENCED when HTS_DEBUG 0:
 * - log_debug()  — all BRIDGE, MHD, PAGE, CHOOSER, THEME, etc. msgs
 * - log_warn()   — ⚠ WARN lines
 * - log_error()  — ✖ ERROR lines
 *
 * ALWAYS printed (never silenced):
 * - print_hts()  — the HTS timestamp banner on startup
 * ══════════════════════════════════════════════════════════════════ */
#define HTS_DEBUG 1
/* ══════════════════════════════════════════════════════════════════ */

// --- GLOBALS ---
static GtkWidget *clock_label = NULL, *hud_box = NULL, *exit_btn = NULL, *logo_image = NULL;
static gboolean is_screensaver = FALSE;
static gboolean is_doctor_mode = FALSE;
static int hts_server_port = 0; 

typedef enum { CHOOSER_MODE_VIDEO, CHOOSER_MODE_SCREENSAVER, CHOOSER_MODE_DOCTOR, CHOOSER_MODE_THEME, CHOOSER_MODE_FACEPLATE } ChooserMode;

typedef struct {
    GtkWidget *window, *flowbox, *path_label, *select_btn, *up_btn;
    char *root_path, *current_path, *selected_path;   
    WebKitWebView *webview_target;
    ChooserMode mode;            
} HTSFileChooser;

typedef struct {
    char *filename, *dest_path, *src_path, *source_hash;
    WebKitWebView *webview; int port, mode; goffset file_size; time_t start_time;
} VideoCopyContext;

static HTSFileChooser *active_chooser = NULL;
static FILE *debug_log = NULL;
static time_t session_start_time = 0;
static gboolean copy_in_progress = FALSE;
static char *current_copy_destination = NULL;
static GMutex copy_mutex;
static GMutex log_mutex;   /* protects debug_log FILE* from concurrent thread writes */
/* Import progress poller globals */
static WebKitWebView *copy_progress_webview = NULL;
static goffset        copy_progress_total   = 0;
static char          *copy_progress_name    = NULL;
static guint          copy_progress_timer   = 0;

// --- FORWARD DECLARATIONS ---
static void hts_chooser_scan_directory(HTSFileChooser *c, const char *path);
static void open_hts_file_chooser(GtkWidget *parent_widget, WebKitWebView *wv);
static void open_hts_screensaver_chooser(GtkWidget *parent_widget, WebKitWebView *wv);
void sync_and_scan_themes(WebKitWebView *web_view);
void scan_faceplates(WebKitWebView *web_view);

static gboolean is_video_file(const char *name) {
    if (!name) return FALSE;
    char *l = g_ascii_strdown(name, -1);
    gboolean v = g_str_has_suffix(l,".webm")||g_str_has_suffix(l,".mp4")||g_str_has_suffix(l,".mkv");
    g_free(l); return v;
}
static gboolean is_html_file(const char *name) {
    if (!name) return FALSE;
    char *l = g_ascii_strdown(name, -1);
    gboolean v = g_str_has_suffix(l,".html")||g_str_has_suffix(l,".htm");
    g_free(l); return v;
}
static gboolean is_image_file(const char *name) {
    if (!name) return FALSE;
    char *l = g_ascii_strdown(name, -1);
    gboolean v = g_str_has_suffix(l,".png")||g_str_has_suffix(l,".jpg")||g_str_has_suffix(l,".jpeg")||g_str_has_suffix(l,".webp");
    g_free(l); return v;
}

static gboolean is_screensaver_folder(const char *path) {
    char *index_file = g_build_filename(path, "index.html", NULL);
    gboolean valid = g_file_test(index_file, G_FILE_TEST_IS_REGULAR);
    g_free(index_file); return valid;
}

static char* js_escape_string(const char *str) {
    if (!str) return g_strdup("");
    GString *result = g_string_new("");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\'': g_string_append(result, "\\'"); break;
            case '\\': g_string_append(result, "\\\\"); break;
            default: g_string_append_c(result, *p); break;
        }
    }
    return g_string_free(result, FALSE);
}

static char* calculate_file_hash(const char *filepath) {
    GFile *file = g_file_new_for_path(filepath); GFileInputStream *stream = g_file_read(file, NULL, NULL);
    if (!stream) { g_object_unref(file); return g_strdup("ERROR"); }
    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256); guchar buffer[8192]; gssize bytes_read;
    while ((bytes_read = g_input_stream_read(G_INPUT_STREAM(stream), buffer, sizeof(buffer), NULL, NULL)) > 0) g_checksum_update(checksum, buffer, bytes_read);
    const char *hash_str = g_checksum_get_string(checksum); char *result = g_strdup(hash_str);
    g_checksum_free(checksum); g_object_unref(stream); g_object_unref(file); return result;
}

void log_debug(const char *format, ...) {
    g_mutex_lock(&log_mutex);
    if (debug_log) {
        time_t now = time(NULL); struct tm *local = localtime(&now); char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%I:%M:%S %p", local);
        if (session_start_time > 0) {
            long elapsed = (long)(now - session_start_time);
            fprintf(debug_log, "[%s | +%02ld:%02ld:%02ld] ", timestamp, elapsed/3600, (elapsed%3600)/60, elapsed%60);
        } else { fprintf(debug_log, "[%s] ", timestamp); }
        va_list args; va_start(args, format); vfprintf(debug_log, format, args); va_end(args);
        fprintf(debug_log, "\n"); fflush(debug_log);
    }
    if (HTS_DEBUG) { va_list args2; va_start(args2, format); vprintf(format, args2); va_end(args2); printf("\n"); fflush(stdout); }
    g_mutex_unlock(&log_mutex);
}

void log_warn(const char *format, ...) {
    g_mutex_lock(&log_mutex);
    if (debug_log) {
        time_t now = time(NULL); struct tm *local = localtime(&now); char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%I:%M:%S %p", local);
        if (session_start_time > 0) {
            long elapsed = (long)(now - session_start_time);
            fprintf(debug_log, "[%s | +%02ld:%02ld:%02ld] \xe2\x9a\xa0 WARN  | ", timestamp, elapsed/3600, (elapsed%3600)/60, elapsed%60);
        } else { fprintf(debug_log, "[%s] \xe2\x9a\xa0 WARN  | ", timestamp); }
        va_list args; va_start(args, format); vfprintf(debug_log, format, args); va_end(args);
        fprintf(debug_log, "\n"); fflush(debug_log);
    }
    if (HTS_DEBUG) { printf("\xe2\x9a\xa0 WARN  | "); va_list args2; va_start(args2, format); vprintf(format, args2); va_end(args2); printf("\n"); fflush(stdout); }
    g_mutex_unlock(&log_mutex);
}

void log_error(const char *format, ...) {
    g_mutex_lock(&log_mutex);
    if (debug_log) {
        time_t now = time(NULL); struct tm *local = localtime(&now); char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%I:%M:%S %p", local);
        if (session_start_time > 0) {
            long elapsed = (long)(now - session_start_time);
            fprintf(debug_log, "[%s | +%02ld:%02ld:%02ld] \xe2\x9c\x96 ERROR | ", timestamp, elapsed/3600, (elapsed%3600)/60, elapsed%60);
        } else { fprintf(debug_log, "[%s] \xe2\x9c\x96 ERROR | ", timestamp); }
        va_list args; va_start(args, format); vfprintf(debug_log, format, args); va_end(args);
        fprintf(debug_log, "\n"); fflush(debug_log);
    }
    if (HTS_DEBUG) { printf("\xe2\x9c\x96 ERROR | "); va_list args2; va_start(args2, format); vprintf(format, args2); va_end(args2); printf("\n"); fflush(stdout); }
    g_mutex_unlock(&log_mutex);
}

// --- 1. THE PASSOVER PROTOCOL (HTS Logic) ---
void print_hts() {
    time_t now; time(&now);
    struct tm *local = localtime(&now);
    long absolute = (long)local->tm_year + 1900 + 37977;
    long display_year = absolute % 50000;
    char epoch_char = 'A' + ((absolute / 50000) % 26);
    char date_buf[100];
    strftime(date_buf, sizeof(date_buf), "%a %b %e %H:%M:%S %Z %Y", local);
    printf("%s  HTS %c%05ld\n", date_buf, epoch_char, display_year);
    fflush(stdout);
}

// --- MICRO HTTP SERVER (Bypasses the sandbox for videos) ---
struct MHD_Daemon *hts_http_daemon = NULL;

#if MHD_VERSION >= 0x00097002
#define HTS_MHD_RESULT enum MHD_Result
#else
#define HTS_MHD_RESULT int
#endif

static HTS_MHD_RESULT answer_to_connection(void *cls, struct MHD_Connection *connection,
                                           const char *url, const char *method,
                                           const char *version, const char *upload_data,
                                           size_t *upload_data_size, void **con_cls) {
    if (strcmp(method, "GET") != 0) return MHD_NO;

    /* Queue response on the 2nd pass. */
    static int aptr;
    if (&aptr != *con_cls) {
        *con_cls = &aptr;
        return MHD_YES; 
    }
    *con_cls = NULL;

    const char *filename = url;
    if (filename[0] == '/') filename++;

    gchar *unescaped_filename = g_uri_unescape_string(filename, NULL);
    if (!unescaped_filename) return MHD_NO;

    char *file_path = NULL;
    if (g_str_has_prefix(unescaped_filename, "screensavers/")) {
        const char *rel = unescaped_filename + strlen("screensavers/");
        file_path = g_build_filename(DATADIR, "data", "screensavers", rel, NULL);
        if (!g_file_test(file_path, G_FILE_TEST_IS_REGULAR)) {
            g_free(file_path);
            file_path = g_build_filename(g_get_user_config_dir(), "HTS_Screensavers", rel, NULL);
        }
    } else {
        file_path = g_build_filename(g_get_user_config_dir(), "HTS_Video_Vault", unescaped_filename, NULL);
        if (!g_file_test(file_path, G_FILE_TEST_IS_REGULAR)) {
            g_free(file_path);
            file_path = g_build_filename(DATADIR, "data", "videos", unescaped_filename, NULL);
        }
    }

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        g_free(unescaped_filename);
        g_free(file_path);
        struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
        HTS_MHD_RESULT ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
        return ret;
    }

    struct stat st;
    fstat(fd, &st);
    long filesize = (long)st.st_size;

    long range_start = 0, range_end = filesize - 1;
    int is_partial = 0;
    const char *range_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Range");

    if (range_header && strncmp(range_header, "bytes=", 6) == 0) {
        is_partial = 1;
        char raw_range[64] = {0};
        strncpy(raw_range, range_header + 6, sizeof(raw_range) - 1);
        char *dash = strchr(raw_range, '-');
        if (dash) {
            *dash = '\0';
            range_start = atol(raw_range);
            if (*(dash + 1) != '\0') {
                range_end = atol(dash + 1);
            }
        }
    }

    if (range_end >= filesize) range_end = filesize - 1;

    if (range_start > range_end) {
        close(fd);
        g_free(unescaped_filename);
        g_free(file_path);
        struct MHD_Response *response = MHD_create_response_from_buffer(0, "", MHD_RESPMEM_PERSISTENT);
        char r416[128];
        snprintf(r416, sizeof(r416), "bytes */%ld", filesize);
        MHD_add_response_header(response, "Content-Range", r416);
        HTS_MHD_RESULT ret = MHD_queue_response(connection, MHD_HTTP_REQUESTED_RANGE_NOT_SATISFIABLE, response);
        MHD_destroy_response(response);
        return ret;
    }

    long content_length = range_end - range_start + 1;

    struct MHD_Response *response = MHD_create_response_from_fd_at_offset64((uint64_t)content_length, fd, (uint64_t)range_start);

    const char *mime = "application/octet-stream";
    if      (g_str_has_suffix(unescaped_filename, ".webm")) mime = "video/webm";
    else if (g_str_has_suffix(unescaped_filename, ".mp4"))  mime = "video/mp4";
    else if (g_str_has_suffix(unescaped_filename, ".mkv"))  mime = "video/x-matroska";
    else if (g_str_has_suffix(unescaped_filename, ".html")) mime = "text/html";
    else if (g_str_has_suffix(unescaped_filename, ".css"))  mime = "text/css";
    else if (g_str_has_suffix(unescaped_filename, ".js"))   mime = "application/javascript";
    else if (g_str_has_suffix(unescaped_filename, ".png"))  mime = "image/png";
    else if (g_str_has_suffix(unescaped_filename, ".jpg") || g_str_has_suffix(unescaped_filename, ".jpeg")) mime = "image/jpeg";
    else if (g_str_has_suffix(unescaped_filename, ".svg"))  mime = "image/svg+xml";
    else if (g_str_has_suffix(unescaped_filename, ".woff2")) mime = "font/woff2";
    else if (g_str_has_suffix(unescaped_filename, ".json")) mime = "application/json";

    MHD_add_response_header(response, "Content-Type", mime);
    MHD_add_response_header(response, "Accept-Ranges", "bytes");

    HTS_MHD_RESULT ret;
    if (is_partial) {
        char range_val[128];
        snprintf(range_val, sizeof(range_val), "bytes %ld-%ld/%ld", range_start, range_end, filesize);
        MHD_add_response_header(response, "Content-Range", range_val);
        ret = MHD_queue_response(connection, MHD_HTTP_PARTIAL_CONTENT, response);
        log_debug("MHD REQ   : %s  range=%ld-%ld  filesize=%ld", unescaped_filename, range_start, range_end, filesize);
    } else {
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        log_debug("MHD REQ   : %s  FULL  filesize=%ld", unescaped_filename, filesize);
    }

    MHD_destroy_response(response);
    g_free(unescaped_filename);
    g_free(file_path);
    return ret;
}

static void start_micro_server() {
    if (hts_server_port > 0) return; 

    hts_http_daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
                                       0, NULL, NULL,
                                       &answer_to_connection, NULL, MHD_OPTION_END);

    if (hts_http_daemon) {
        const union MHD_DaemonInfo *info = MHD_get_daemon_info(hts_http_daemon, MHD_DAEMON_INFO_LISTEN_FD);
        if (info && info->listen_fd != -1) {
            struct sockaddr_in sin;
            socklen_t len = sizeof(sin);
            if (getsockname(info->listen_fd, (struct sockaddr *)&sin, &len) == 0) {
                hts_server_port = ntohs(sin.sin_port);
                log_debug("MHD SERVER LIVE ON PORT: %d", hts_server_port);
            }
        }
    } else {
        log_error("Failed to start libmicrohttpd server!");
    }
}

static void cleanup_partial_copy() {
    g_mutex_lock(&copy_mutex);
    if (current_copy_destination && g_file_test(current_copy_destination, G_FILE_TEST_EXISTS)) g_remove(current_copy_destination);
    if (current_copy_destination) { g_free(current_copy_destination); current_copy_destination = NULL; }
    copy_in_progress = FALSE; g_mutex_unlock(&copy_mutex);
}

void save_active_theme(const char *path) {
    char *config_file = g_build_filename(g_get_user_config_dir(), "active_theme.conf", NULL);
    if (!path || path[0] == '\0') {
        /* Empty path = F5 reset — delete the conf so next boot uses default */
        g_remove(config_file);
        log_debug("SAVE_THEME: empty path received — deleted active_theme.conf (F5 reset)");
    } else {
        g_file_set_contents(config_file, path, -1, NULL);
        log_debug("SAVE_THEME: saved [%s]", path);
    }
    g_free(config_file);
}

void inject_saved_theme(WebKitWebView *web_view) {
    char *config_file = g_build_filename(g_get_user_config_dir(), "active_theme.conf", NULL);
    if (g_file_test(config_file, G_FILE_TEST_EXISTS)) {
        char *content = NULL;
        if (g_file_get_contents(config_file, &content, NULL, NULL)) {
            g_strstrip(content);
            gboolean valid = g_strstr_len(content, -1, "control-panel-themes") != NULL
                          || g_str_has_prefix(content, "FACEPLATE|");
            log_debug("INJECT_THEME: path=[%s] valid=%s", content, valid ? "YES" : "NO (BLOCKED+DELETED)");
            if (valid) {
                char *js = g_strdup_printf("window.postMessage({type: 'RESTORE_THEME', path: '%s'}, '*');", content);
                webkit_web_view_evaluate_javascript(web_view, js, -1, NULL, NULL, NULL, NULL, NULL);
                g_free(js);
            } else {
                g_file_delete(g_file_new_for_path(config_file), NULL, NULL);
            }
            g_free(content);
        }
    } else {
        log_debug("INJECT_THEME: no active_theme.conf found");
    }
    g_free(config_file);
}

static gboolean update_clock_hud(gpointer user_data) {
    time_t now; time(&now); struct tm *local = localtime(&now);
    long absolute = (long)local->tm_year + 1900 + 37977; long display_year = absolute % 50000;
    char epoch_char = 'A' + ((absolute / 50000) % 26); char date_buf[100]; strftime(date_buf, sizeof(date_buf), "%a %b %e %H:%M:%S", local); 
    char final_buf[150]; snprintf(final_buf, sizeof(final_buf), "%s  HTS %c%05ld", date_buf, epoch_char, display_year);
    if (clock_label && GTK_IS_LABEL(clock_label)) gtk_label_set_text(GTK_LABEL(clock_label), final_buf);
    return G_SOURCE_CONTINUE;
}

static void on_video_copy_complete(GObject *source, GAsyncResult *res, gpointer user_data) {
    VideoCopyContext *ctx = (VideoCopyContext *)user_data; GError *error = NULL;
    /* Stop the progress poller */
    if (copy_progress_timer) { g_source_remove(copy_progress_timer); copy_progress_timer = 0; }
    if (copy_progress_name)  { g_free(copy_progress_name); copy_progress_name = NULL; }
    copy_progress_webview = NULL;

    if (g_file_copy_finish(G_FILE(source), res, &error)) {
        char *vault_dir = g_build_filename(g_get_user_config_dir(), "HTS_Video_Vault", NULL);
        char *full_path = g_build_filename(vault_dir, ctx->filename, NULL);
        GFile *vault_file = g_file_new_for_path(full_path);
        
        char *esc_name = g_uri_escape_string(ctx->filename, NULL, TRUE);
        
        /* RESTORE HTTP ROUTING: Bypasses the WebKit file:// security block */
        char *final_uri = g_strdup_printf("http://127.0.0.1:%d/%s", ctx->port, esc_name);
        char *esc_uri = js_escape_string(final_uri);
        
        g_object_unref(vault_file); g_free(full_path); g_free(vault_dir);
        char *display_name = ctx->filename;
        if (ctx->mode == CHOOSER_MODE_DOCTOR && g_str_has_prefix(ctx->filename, "_EXAM_")) display_name = ctx->filename + 6;
        char *raw_name_esc = js_escape_string(display_name);
        const char *action = (ctx->mode == CHOOSER_MODE_DOCTOR) ? "doctor_video_selected" : "import_absolute";
        char *js;
        if (ctx->mode == CHOOSER_MODE_DOCTOR) {
            js = g_strdup_printf("window.postMessage({type:'HTS_CMD',action:'%s',data:'%s',name:'%s'},'*');", action, esc_uri, raw_name_esc);
        } else {
            js = g_strdup_printf("window.postMessage({type:'HTS_CMD',action:'%s',data:'%s',name:'%s',size:%ld,hash:'',verified:true},'*');", action, esc_uri, raw_name_esc, (long)ctx->file_size);
        }
        webkit_web_view_evaluate_javascript(ctx->webview,
            "window.postMessage({type:'HTS_CMD',action:'import_progress',data:{pct:100,done:true}},'*');",
            -1, NULL, NULL, NULL, NULL, NULL);
        log_debug("IMPORT DONE: %s", ctx->filename);
        webkit_web_view_evaluate_javascript(ctx->webview, js, -1, NULL, NULL, NULL, NULL, NULL);
        g_free(js); g_free(esc_uri); g_free(raw_name_esc); g_free(final_uri); g_free(esc_name);
    } else {
        log_warn("IMPORT FAILED: %s — %s", ctx->filename, error ? error->message : "unknown");
        webkit_web_view_evaluate_javascript(ctx->webview,
            "window.postMessage({type:'HTS_CMD',action:'import_progress',data:{pct:-1,done:true}},'*');",
            -1, NULL, NULL, NULL, NULL, NULL);
    }
    
    g_mutex_lock(&copy_mutex); copy_in_progress = FALSE; if (current_copy_destination) { g_free(current_copy_destination); current_copy_destination = NULL; } g_mutex_unlock(&copy_mutex);
    g_free(ctx->filename); g_free(ctx->dest_path); g_free(ctx->src_path); g_free(ctx->source_hash); g_free(ctx);
}

static gboolean import_progress_poll(gpointer user_data) {
    /* Runs on GTK main loop every 2 seconds. Checks actual bytes written to disk. */
    if (!copy_progress_webview || !copy_progress_name) return G_SOURCE_REMOVE;
    const char *dest_path = NULL;
    g_mutex_lock(&copy_mutex);
    dest_path = current_copy_destination ? g_strdup(current_copy_destination) : NULL;
    g_mutex_unlock(&copy_mutex);
    if (!dest_path) return G_SOURCE_REMOVE;
    GStatBuf st;
    goffset on_disk = (g_stat(dest_path, &st) == 0) ? (goffset)st.st_size : 0;
    g_free((gpointer)dest_path);
    int pct = (copy_progress_total > 0)
        ? (int)((on_disk * 100) / copy_progress_total) : 0;
    if (pct > 99) pct = 99;  /* 100% only sent by on_video_copy_complete */
    char *safe_name = g_uri_escape_string(copy_progress_name, NULL, TRUE);
    char *js = g_strdup_printf(
        "window.postMessage({type:'HTS_CMD',action:'import_progress',"
        "data:{pct:%d,current:%ld,total:%ld,name:'%s'}},'*');",
        pct, (long)on_disk, (long)copy_progress_total, safe_name);
    webkit_web_view_evaluate_javascript(copy_progress_webview, js, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(js); g_free(safe_name);
    return G_SOURCE_CONTINUE;
}

static void video_copy_progress_cb(goffset current_num_bytes, goffset total_num_bytes, gpointer user_data) {
    /* Intentionally empty — we use import_progress_poll() for honest disk-based progress */
    (void)current_num_bytes; (void)total_num_bytes; (void)user_data;
}

static void chooser_commit(HTSFileChooser *c) {
    if (!c->selected_path || !c->webview_target) return;
    if (c->mode == CHOOSER_MODE_VIDEO || c->mode == CHOOSER_MODE_DOCTOR) {
        char *filename = g_path_get_basename(c->selected_path);
        char *vault_dir = g_build_filename(g_get_user_config_dir(), "HTS_Video_Vault", NULL);
        g_mkdir_with_parents(vault_dir, 0755);
        
        /* Sanitize filename — strip newlines/control chars that break HTTP routing */
        char *clean_filename = g_strdup(filename);
        for (char *p = clean_filename; *p; p++) {
            if (*p == '\n' || *p == '\r' || *p == '\t' || (unsigned char)*p < 0x20)
                *p = '_';
        }
        char *dest_filename = clean_filename;
        if(c->mode == CHOOSER_MODE_DOCTOR) dest_filename = g_strdup_printf("_EXAM_%s", clean_filename);
        
        char *dest_path = g_build_filename(vault_dir, dest_filename, NULL);
        GFile *src_file = g_file_new_for_path(c->selected_path);
        GFile *dest_file = g_file_new_for_path(dest_path);
        
        GFileInfo *file_info = g_file_query_info(src_file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
        goffset file_size = file_info ? g_file_info_get_size(file_info) : 0;
        if(file_info) g_object_unref(file_info);
        
        g_mutex_lock(&copy_mutex); copy_in_progress = TRUE; current_copy_destination = g_strdup(dest_path); g_mutex_unlock(&copy_mutex);
        
        VideoCopyContext *ctx = g_new0(VideoCopyContext, 1);
        ctx->filename = g_strdup(dest_filename); ctx->dest_path = g_strdup(dest_path); ctx->src_path = g_strdup(c->selected_path);
        ctx->webview = c->webview_target; ctx->port = hts_server_port; ctx->file_size = file_size;
        ctx->source_hash = g_strdup(""); ctx->mode = c->mode;
        
        g_file_copy_async(src_file, dest_file, G_FILE_COPY_OVERWRITE, G_PRIORITY_DEFAULT, NULL, (GFileProgressCallback)video_copy_progress_cb, ctx, (GAsyncReadyCallback)on_video_copy_complete, ctx);
        /* Start disk-polling progress timer */
        copy_progress_webview = c->webview_target;
        copy_progress_total   = file_size;
        copy_progress_name    = g_strdup(dest_filename);
        if (copy_progress_timer) { g_source_remove(copy_progress_timer); copy_progress_timer = 0; }
        copy_progress_timer = g_timeout_add(2000, import_progress_poll, NULL);
        
        g_object_unref(src_file); g_object_unref(dest_file); g_free(dest_path); g_free(vault_dir);
        if(c->mode == CHOOSER_MODE_DOCTOR) g_free(dest_filename);
        g_free(clean_filename); g_free(filename);
        
    } else if (c->mode == CHOOSER_MODE_SCREENSAVER) {
        char *folder_name = g_path_get_basename(c->selected_path);
        char *hts_uri = g_strdup_printf("hts://screensavers/hts/%s/index.html", folder_name);
        char *esc_name = g_strescape(folder_name, NULL); char *esc_uri = g_strescape(hts_uri, NULL);
        char *js = g_strdup_printf("if (typeof receiveImportedScreensaver === 'function') { receiveImportedScreensaver('%s', '%s'); }", esc_name, esc_uri);
        webkit_web_view_evaluate_javascript(c->webview_target, js, -1, NULL, NULL, NULL, NULL, NULL);
        g_free(js); g_free(esc_uri); g_free(esc_name); g_free(hts_uri); g_free(folder_name);
    } else if (c->mode == CHOOSER_MODE_THEME) {
        /* Send file:// URI of the selected .html theme file back to theme-browser.html */
        char *file_uri = g_strdup_printf("file://%s", c->selected_path);
        char *name = g_path_get_basename(c->selected_path);
        char *theme_name = g_strdup(name);
        /* Strip .html suffix for display */
        if (g_str_has_suffix(theme_name, ".html")) theme_name[strlen(theme_name)-5] = ' ';
        char *esc_uri = js_escape_string(file_uri);
        char *esc_name = js_escape_string(theme_name);
        char *js = g_strdup_printf(
            "if (typeof receiveImportedTheme === 'function') { receiveImportedTheme('%s', '%s'); }", esc_name, esc_uri);
        webkit_web_view_evaluate_javascript(c->webview_target, js, -1, NULL, NULL, NULL, NULL, NULL);
        g_free(js); g_free(esc_uri); g_free(esc_name); g_free(theme_name); g_free(name); g_free(file_uri);
    } else if (c->mode == CHOOSER_MODE_FACEPLATE) {
        /* Send file:// URI of the selected image back to theme-browser.html */
        char *file_uri = g_strdup_printf("file://%s", c->selected_path);
        char *esc_uri = js_escape_string(file_uri);
        char *js = g_strdup_printf(
            "if (typeof receiveSelectedFaceplate === 'function') { receiveSelectedFaceplate('%s'); }", esc_uri);
        webkit_web_view_evaluate_javascript(c->webview_target, js, -1, NULL, NULL, NULL, NULL, NULL);
        g_free(js); g_free(esc_uri); g_free(file_uri);
    }
    gtk_window_destroy(GTK_WINDOW(c->window));
}

/* ─────────────────────────────────────────────────────────────────
 * HTS ART CHOOSER — stained-glass sidebar + thumbnail grid
 *
 * Thumbnails use the FreeDesktop cache standard:
 * ~/.cache/thumbnails/large/<md5(file:///path)>.png
 * ffmpeg extracts a frame automatically on first open.
 * Nautilus/Nemo share the same cache so thumbnails only
 * ever get generated once.
 * ───────────────────────────────────────────────────────────────── */

static char *thumb_cache_path(const char *filepath) {
    char *uri  = g_strdup_printf("file://%s", filepath);
    char *md5  = g_compute_checksum_for_string(G_CHECKSUM_MD5, uri, -1);
    char *dir  = g_build_filename(g_get_user_cache_dir(), "thumbnails", "large", NULL);
    char *path = g_strdup_printf("%s/%s.png", dir, md5);
    g_free(uri); g_free(md5); g_free(dir);
    return path;
}

/* ── Async thumbnail generation ────────────────────────────────────
 * ffmpeg is called in a GTask worker thread so the UI never blocks.
 * The card gets a spinner immediately; the real image swaps in when
 * ffmpeg finishes (via g_idle_add back on the main thread).
 * ─────────────────────────────────────────────────────────────────*/
typedef struct {
    char *filepath;   /* source video  */
    char *thumb_path; /* cache dest    */
    GtkWidget *image; /* the GtkImage placeholder to update */
} ThumbJob;

static void thumb_job_free(ThumbJob *j) {
    g_free(j->filepath); g_free(j->thumb_path); g_free(j);
}

/* Runs on main thread after ffmpeg finishes — swaps spinner for real thumb */
static gboolean thumb_swap_idle(gpointer data) {
    ThumbJob *j = (ThumbJob *)data;
    if (j->image && GTK_IS_WIDGET(j->image) &&
        g_file_test(j->thumb_path, G_FILE_TEST_IS_REGULAR)) {
        GdkTexture *tex = gdk_texture_new_from_filename(j->thumb_path, NULL);
        if (tex) {
            GtkWidget *parent = gtk_widget_get_parent(j->image);
            if (parent) {
                GtkWidget *pic = gtk_picture_new_for_paintable(GDK_PAINTABLE(tex));
                gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
                gtk_widget_set_hexpand(pic, TRUE);
                gtk_widget_set_vexpand(pic, TRUE);
                gtk_box_append(GTK_BOX(parent), pic);
                gtk_box_remove(GTK_BOX(parent), j->image);
            }
            g_object_unref(tex);
        }
    }
    thumb_job_free(j);
    return G_SOURCE_REMOVE;
}

/* Runs in worker thread */
static void thumb_worker(GTask *task, gpointer src, gpointer task_data, GCancellable *cancel) {
    ThumbJob *j = (ThumbJob *)task_data;
    char *dir = g_path_get_dirname(j->thumb_path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    char *tmp  = g_strdup_printf("%s.tmp.png", j->thumb_path);
    char *qsrc = g_shell_quote(j->filepath);
    char *qdst = g_shell_quote(tmp);
    char *cmd  = g_strdup_printf(
        "ffmpeg -y -ss 10 -i %s -vframes 1"
        " -vf scale=256:256"
        " -f image2 %s 2>/dev/null"
        " || ffmpeg -y -i %s -vframes 1"
        " -vf scale=256:256"
        " -f image2 %s 2>/dev/null",
        qsrc, qdst, qsrc, qdst);
    if (system(cmd) == 0 && g_file_test(tmp, G_FILE_TEST_IS_REGULAR))
        g_rename(tmp, j->thumb_path);
    g_free(cmd); g_free(qsrc); g_free(qdst); g_free(tmp);
    g_task_return_boolean(task, TRUE);
}

static void thumb_task_done(GObject *src, GAsyncResult *res, gpointer data) {
    g_idle_add(thumb_swap_idle, data); /* hop back to main thread */
}

/* Returns a cached pixbuf immediately (NULL if not cached yet).
   If not cached, kicks off async ffmpeg and updates image_placeholder when done. */
static GdkPixbuf *make_thumbnail_or_async(const char *full_path, gboolean is_dir,
                                           GtkWidget *image_placeholder) {
    if (is_dir) {
        const char *names[] = { "preview.png","thumbnail.png","poster.png",
                                 "preview.jpg","thumbnail.jpg", NULL };
        for (int i = 0; names[i]; i++) {
            char *p = g_build_filename(full_path, names[i], NULL);
            gboolean ok = g_file_test(p, G_FILE_TEST_IS_REGULAR);
            GdkPixbuf *pb = ok ? gdk_pixbuf_new_from_file_at_scale(p, 160, 120, FALSE, NULL) : NULL;
            g_free(p);
            if (pb) return pb;
        }
        return NULL;
    }

    /* DIRECT IMAGE LOAD: Bypass FFmpeg for image files to ensure instant rendering */
    if (is_image_file(full_path)) {
        return gdk_pixbuf_new_from_file_at_scale(full_path, 160, 160, FALSE, NULL);
    }

    char *thumb = thumb_cache_path(full_path);

    /* Check if cached thumbnail is fresh */
    gboolean valid = FALSE;
    if (g_file_test(thumb, G_FILE_TEST_IS_REGULAR)) {
        GStatBuf ss, ts;
        valid = (g_stat(full_path, &ss) == 0 && g_stat(thumb, &ts) == 0)
                    ? (ts.st_mtime >= ss.st_mtime) : TRUE;
    }

    if (valid) {
        /* Cache hit — return immediately, no async needed */
        GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(thumb, 160, 120, FALSE, NULL);
        g_free(thumb);
        return pb;
    }

    /* Cache miss — fire async ffmpeg, placeholder stays as spinner */
    ThumbJob *j = g_new0(ThumbJob, 1);
    j->filepath   = g_strdup(full_path);
    j->thumb_path = thumb; /* ownership transferred */
    j->image      = image_placeholder;
    GTask *task = g_task_new(NULL, NULL, thumb_task_done, j);
    g_task_set_task_data(task, j, NULL); /* don't free yet — done in callback */
    g_task_run_in_thread(task, thumb_worker);
    g_object_unref(task);
    return NULL; /* caller gets NULL → shows placeholder */
}

/* ─────────────────────────────────────────────────────────────────
 * HTS CAPTIVE SMART CHOOSER
 * Clean modal window with thumbnail grid and neon UI
 * ───────────────────────────────────────────────────────────────── */

static void on_chooser_destroy(GtkWidget *w, gpointer data) {
    HTSFileChooser *c = (HTSFileChooser *)data; if (!c) return;
    log_debug("CHOOSER CLOSED: selected=%s", c->selected_path ? c->selected_path : "(none)");
    if (c->webview_target)
        webkit_web_view_evaluate_javascript(c->webview_target,
            "window.postMessage({type:'HTS_CMD',action:'chooser_closed'},'*');",
            -1, NULL, NULL, NULL, NULL, NULL);
    g_free(c->root_path); g_free(c->current_path); g_free(c->selected_path); g_free(c);
    active_chooser = NULL;
}

static void on_card_clicked(GtkWidget *btn, gpointer data) {
    HTSFileChooser *c = active_chooser; if (!c) return;
    char *filepath = (char *)data;
    if (g_file_test(filepath, G_FILE_TEST_IS_DIR)) { hts_chooser_scan_directory(c, filepath); return; }

    /* Deselect all — remove from the inner .video-card box */
    GtkWidget *child = gtk_widget_get_first_child(c->flowbox);
    while (child) {
        GtkWidget *fbc = gtk_flow_box_child_get_child(GTK_FLOW_BOX_CHILD(child));
        if (fbc) {
            GtkWidget *card = gtk_button_get_child(GTK_BUTTON(fbc));
            if (card) gtk_widget_remove_css_class(card, "card-selected");
        }
        child = gtk_widget_get_next_sibling(child);
    }
    /* Add selection highlight to the card box inside the button */
    GtkWidget *card = gtk_button_get_child(GTK_BUTTON(btn));
    if (card) gtk_widget_add_css_class(card, "card-selected");
    g_free(c->selected_path); c->selected_path = g_strdup(filepath);
    gtk_widget_set_sensitive(c->select_btn, TRUE);
    gtk_widget_add_css_class(c->select_btn, "btn-ready");
}

static void on_select_btn_clicked(GtkWidget *btn, gpointer data) { chooser_commit((HTSFileChooser *)data); }

static void hts_chooser_scan_directory(HTSFileChooser *c, const char *path) {
    g_free(c->current_path); c->current_path = g_strdup(path);

    /* Path label — friendly name at root */
    if (c->path_label) {
        if (g_strcmp0(path, c->root_path) == 0)
            gtk_label_set_text(GTK_LABEL(c->path_label), "SYSTEM // MEDIA VAULT");
        else
            gtk_label_set_text(GTK_LABEL(c->path_label), path);
    }

    /* Lock UP button at jail root */
    if (c->up_btn)
        gtk_widget_set_sensitive(c->up_btn, g_strcmp0(path, c->root_path) != 0);

    /* Clear grid */
    GtkWidget *child = gtk_widget_get_first_child(c->flowbox);
    while (child) { GtkWidget *next = gtk_widget_get_next_sibling(child); gtk_flow_box_remove(GTK_FLOW_BOX(c->flowbox), child); child = next; }
    g_free(c->selected_path); c->selected_path = NULL;
    gtk_widget_set_sensitive(c->select_btn, FALSE);

    GDir *dir = g_dir_open(path, 0, NULL); if (!dir) return;
    const char *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (name[0] == '.') continue;
        char *full = g_build_filename(path, name, NULL);
        gboolean is_dir = g_file_test(full, G_FILE_TEST_IS_DIR);

        if ((c->mode == CHOOSER_MODE_VIDEO || c->mode == CHOOSER_MODE_DOCTOR) && (!is_dir && !is_video_file(name))) { g_free(full); continue; }
        if (c->mode == CHOOSER_MODE_SCREENSAVER && (!is_dir || (!is_screensaver_folder(full) && strcmp(c->current_path, c->root_path) != 0))) { g_free(full); continue; }
        if (c->mode == CHOOSER_MODE_THEME && (!is_dir && !is_html_file(name))) { g_free(full); continue; }
        if (c->mode == CHOOSER_MODE_FACEPLATE && (!is_dir && !is_image_file(name))) { g_free(full); continue; }

        /* Card — fixed square so flowbox wraps into a true grid */
        GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(card, 160, 160);
        gtk_widget_set_hexpand(card, FALSE);
        gtk_widget_set_vexpand(card, FALSE);
        gtk_widget_add_css_class(card, "video-card");

        /* Icon/thumbnail area — square */
        GtkWidget *icon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_size_request(icon_box, 160, 120);
        gtk_widget_add_css_class(icon_box, "icon-area");

        /* Spinner placeholder — shown until async ffmpeg finishes */
        GtkWidget *placeholder = gtk_image_new_from_icon_name(
            is_dir ? "folder" : "media-playback-start");
        gtk_image_set_pixel_size(GTK_IMAGE(placeholder), 48);
        gtk_widget_set_halign(placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand(placeholder, TRUE);
        gtk_box_append(GTK_BOX(icon_box), placeholder);

        /* Try cache first; if miss, async ffmpeg will swap in the real image */
        GdkPixbuf *pb = make_thumbnail_or_async(full, is_dir, placeholder);
        if (pb) {
            /* Cache hit — replace placeholder immediately */
            char *_tmp = g_strdup_printf("%s/hts_hit_%p.png", g_get_tmp_dir(), (void*)pb);
            gdk_pixbuf_save(pb, _tmp, "png", NULL, NULL);
            GdkTexture *tex = gdk_texture_new_from_filename(_tmp, NULL);
            g_remove(_tmp); g_free(_tmp);
            if (tex) {
                GtkWidget *pic = gtk_picture_new_for_paintable(GDK_PAINTABLE(tex));
                gtk_picture_set_content_fit(GTK_PICTURE(pic), GTK_CONTENT_FIT_COVER);
                gtk_widget_set_hexpand(pic, TRUE);
                gtk_widget_set_vexpand(pic, TRUE);
                gtk_box_append(GTK_BOX(icon_box), pic);
                gtk_box_remove(GTK_BOX(icon_box), placeholder);
                g_object_unref(tex);
            }
            g_object_unref(pb);
        }

        GtkWidget *lbl = gtk_label_new(name);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(lbl, "card-label");

        gtk_box_append(GTK_BOX(card), icon_box);
        gtk_box_append(GTK_BOX(card), lbl);

        GtkWidget *btn = gtk_button_new();
        gtk_button_set_child(GTK_BUTTON(btn), card);
        gtk_widget_add_css_class(btn, "card-btn");
        g_signal_connect_data(btn, "clicked", G_CALLBACK(on_card_clicked), g_strdup(full), (GClosureNotify)g_free, 0);
        gtk_flow_box_insert(GTK_FLOW_BOX(c->flowbox), btn, -1);
        g_free(full);
    }
    g_dir_close(dir);
}

static void on_up_clicked(GtkWidget *btn, gpointer data) {
    HTSFileChooser *c = (HTSFileChooser *)data;
    if (!c || g_strcmp0(c->current_path, c->root_path) == 0) return;
    char *parent = g_path_get_dirname(c->current_path);
    hts_chooser_scan_directory(c, parent);
    g_free(parent);
}

static GtkCssProvider *chooser_css_provider = NULL;

static void open_hts_custom_chooser(GtkWidget *parent_widget, WebKitWebView *wv, ChooserMode mode) {
    if (active_chooser) { gtk_window_present(GTK_WINDOW(active_chooser->window)); return; }

    HTSFileChooser *c = g_new0(HTSFileChooser, 1);
    active_chooser = c;
    c->webview_target = wv;
    c->mode = mode;

    /* Jail root — strict anchor enforcement */
    if (mode == CHOOSER_MODE_SCREENSAVER) {
        /* System screensavers stay in the read-only app container */
        c->root_path = g_build_filename(DATADIR, "data", "screensavers", NULL);
    } else {
        /* Universal Routing: Videos, Faceplates, and Themes all funnel through the Videos directory */
        const char *v_dir = g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS);
        c->root_path = g_strdup(v_dir ? v_dir : g_build_filename(g_get_home_dir(), "Videos", NULL));
        g_mkdir_with_parents(c->root_path, 0755); /* Force creation to prevent jailbreak */
    }

    const char *title = (mode == CHOOSER_MODE_DOCTOR)    ? "HTS MEDIA DOCTOR"
                      : (mode == CHOOSER_MODE_SCREENSAVER) ? "HTS SCREENSAVER CHOOSER"
                      : (mode == CHOOSER_MODE_THEME)       ? "HTS THEME IMPORTER"
                      : (mode == CHOOSER_MODE_FACEPLATE)   ? "HTS FACEPLATE SELECTOR"
                      : "HTS CAPTIVE MEDIA CHOOSER";
    const char *commit_label = (mode == CHOOSER_MODE_DOCTOR)    ? "EXAMINE PATIENT"
                              : (mode == CHOOSER_MODE_SCREENSAVER) ? "INSTALL SCREENSAVER"
                              : (mode == CHOOSER_MODE_THEME)       ? "IMPORT THEME"
                              : (mode == CHOOSER_MODE_FACEPLATE)   ? "SELECT FACEPLATE"
                              : "IMPORT TO VAULT";

    c->window = gtk_window_new();
    gtk_widget_add_css_class(c->window, "chooser-win");
    gtk_window_set_title(GTK_WINDOW(c->window), title);
    gtk_window_set_default_size(GTK_WINDOW(c->window), 1100, 780);
    gtk_window_set_decorated(GTK_WINDOW(c->window), FALSE);
    gtk_window_set_modal(GTK_WINDOW(c->window), TRUE);
    g_signal_connect(c->window, "destroy", G_CALLBACK(on_chooser_destroy), c);

    /* Load CSS once */
    if (!chooser_css_provider) {
        chooser_css_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_string(chooser_css_provider,
            /* Window & structure */
            "window.chooser-win { background-color: #0a0a0f; } "
            "flowboxchild { padding: 0; } "

            /* Top bar */
            ".chooser-topbar { background-color: #0d0d14; "
            "  border-bottom: 2px solid #1a1a2e; padding: 12px 20px; } "
            ".path-text { color: #00d4ff; font-family: monospace; font-size: 12px; "
            "  font-weight: bold; letter-spacing: 1px; } "
            ".btn-up { background-color: #1a1a2e; color: #6688aa; "
            "  border: 1px solid #2a2a44; border-radius: 6px; "
            "  padding: 8px 18px; font-family: monospace; font-size: 11px; "
            "  font-weight: bold; letter-spacing: 1px; } "
            ".btn-up:hover { background-color: #222240; color: #00d4ff; "
            "  border-color: #00d4ff; } "
            ".btn-up:disabled { background-color: #111118; color: #333348; "
            "  border-color: #1a1a28; } "

            /* Grid area */
            ".chooser-grid-area { background-color: #0a0a0f; } "

            /* Cards */
            ".video-card { background-color: #12121e; "
            "  border: 2px solid #1e1e30; border-radius: 10px; "
            "  transition: border-color 150ms; } "
            ".video-card:hover { border-color: #334466; background-color: #161626; } "
            ".icon-area { background-color: #0d0d18; "
            "  border-radius: 8px 8px 0 0; } "
            ".card-btn { background-color: transparent; border: none; padding: 4px; "
            "  border-radius: 12px; } "
            ".card-btn:hover { background-color: transparent; } "
            ".card-label { color: #556677; font-family: monospace; font-size: 10px; "
            "  font-weight: bold; letter-spacing: 0px; padding: 7px 6px 8px; } "

            /* Selected state — strong cyan glow */
            ".video-card.card-selected { background-color: #001828; "
            "  border-color: #00d4ff; border-width: 2px; } "
            ".video-card.card-selected .icon-area { background-color: #001422; } "
            ".video-card.card-selected .card-label { color: #00d4ff; "
            "  font-weight: bold; } "

            /* Scrollbar */
            "scrollbar { background-color: #0d0d14; min-width: 8px; } "
            "scrollbar slider { background-color: #2a2a44; border-radius: 4px; "
            "  min-width: 6px; min-height: 30px; margin: 1px; } "
            "scrollbar slider:hover { background-color: #00d4ff; } "

            /* Footer */
            ".chooser-footer { background-color: #0d0d14; "
            "  border-top: 2px solid #1a1a2e; padding: 12px 20px; } "
            ".btn-cancel { background-color: #1a1a2e; color: #556677; "
            "  border: 1px solid #2a2a44; border-radius: 6px; "
            "  padding: 10px 24px; font-family: monospace; font-size: 11px; "
            "  font-weight: bold; letter-spacing: 1px; } "
            ".btn-cancel:hover { background-color: #2a0a0a; color: #ff4444; "
            "  border-color: #662222; } "
            ".btn-commit { background-color: #1a1a2e; color: #334455; "
            "  border: 1px solid #2a2a44; border-radius: 6px; "
            "  padding: 10px 28px; font-family: monospace; font-size: 11px; "
            "  font-weight: bold; letter-spacing: 1px; } "
            ".btn-commit:disabled { background-color: #111118; color: #222233; "
            "  border-color: #191926; } "
            ".btn-commit.btn-ready { background-color: #00d4ff; color: #000810; "
            "  border-color: #00d4ff; } "
            ".btn-commit.btn-ready:hover { background-color: #33ddff; } "
        );
        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(chooser_css_provider), 800);
    }

    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Top bar */
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_margin_start(top_bar, 20);
    gtk_widget_set_margin_end(top_bar, 20);
    gtk_widget_set_margin_top(top_bar, 14);
    gtk_widget_set_margin_bottom(top_bar, 14);
    gtk_widget_add_css_class(top_bar, "chooser-topbar");

    c->up_btn = gtk_button_new_with_label("⬆  UP FOLDER");
    gtk_widget_add_css_class(c->up_btn, "btn-up");
    g_signal_connect(c->up_btn, "clicked", G_CALLBACK(on_up_clicked), c);

    c->path_label = gtk_label_new("");
    gtk_widget_add_css_class(c->path_label, "path-text");
    gtk_widget_set_hexpand(c->path_label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(c->path_label), PANGO_ELLIPSIZE_START);
    gtk_label_set_xalign(GTK_LABEL(c->path_label), 0.0f);

    gtk_box_append(GTK_BOX(top_bar), c->up_btn);
    gtk_box_append(GTK_BOX(top_bar), c->path_label);
    gtk_box_append(GTK_BOX(main_vbox), top_bar);

    /* Scrollable grid */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_add_css_class(scroll, "chooser-grid-area");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_widget_set_vexpand(scroll, TRUE);

    c->flowbox = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(c->flowbox), 8);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(c->flowbox), 2);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(c->flowbox), FALSE);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(c->flowbox), GTK_SELECTION_NONE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(c->flowbox), 12);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(c->flowbox), 12);
    gtk_widget_set_halign(c->flowbox, GTK_ALIGN_START);
    gtk_widget_set_margin_top(c->flowbox, 20);
    gtk_widget_set_margin_bottom(c->flowbox, 20);
    gtk_widget_set_margin_start(c->flowbox, 20);
    gtk_widget_set_margin_end(c->flowbox, 20);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), c->flowbox);
    gtk_box_append(GTK_BOX(main_vbox), scroll);

    /* Footer */
    GtkWidget *footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start(footer, 20);
    gtk_widget_set_margin_end(footer, 20);
    gtk_widget_set_margin_top(footer, 14);
    gtk_widget_set_margin_bottom(footer, 14);
    gtk_widget_set_halign(footer, GTK_ALIGN_END);
    gtk_widget_add_css_class(footer, "chooser-footer");

    GtkWidget *cancel_btn = gtk_button_new_with_label("✕  CANCEL");
    gtk_widget_add_css_class(cancel_btn, "btn-cancel");
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_window_destroy), c->window);

    c->select_btn = gtk_button_new_with_label(commit_label);
    gtk_widget_add_css_class(c->select_btn, "btn-commit");
    gtk_widget_set_sensitive(c->select_btn, FALSE);
    g_signal_connect(c->select_btn, "clicked", G_CALLBACK(on_select_btn_clicked), c);

    gtk_box_append(GTK_BOX(footer), cancel_btn);
    gtk_box_append(GTK_BOX(footer), c->select_btn);
    gtk_box_append(GTK_BOX(main_vbox), footer);

    gtk_window_set_child(GTK_WINDOW(c->window), main_vbox);

    /* Scan and open */
    if (mode == CHOOSER_MODE_SCREENSAVER) {
        char *start = g_build_filename(c->root_path, "hts", NULL);
        hts_chooser_scan_directory(c, g_file_test(start, G_FILE_TEST_IS_DIR) ? start : c->root_path);
        g_free(start);
    } else {
        hts_chooser_scan_directory(c, c->root_path);
    }

    log_debug("CHOOSER OPEN: mode=%d title=%s", mode, title);
    webkit_web_view_evaluate_javascript(wv,
        "window.postMessage({type:'HTS_CMD',action:'chooser_opened'},'*');",
        -1, NULL, NULL, NULL, NULL, NULL);
    gtk_window_present(GTK_WINDOW(c->window));
}

static void open_hts_file_chooser(GtkWidget *parent_widget, WebKitWebView *wv) {
    is_doctor_mode = FALSE;
    open_hts_custom_chooser(parent_widget, wv, CHOOSER_MODE_VIDEO);
}

static void open_hts_screensaver_chooser(GtkWidget *parent_widget, WebKitWebView *wv) {
    /* Navigate to the dedicated screensaver chooser page — it has its own
       grid, preview iframe, type filters, and import button. The generic
       file-picker (CHOOSER_MODE_SCREENSAVER) is only used when the chooser
       page itself triggers "import_screensaver" to pick a folder. */
    char uri[512];
    snprintf(uri, sizeof(uri), "file://%s/data/pages/screensaver-chooser.html", DATADIR);
    webkit_web_view_load_uri(wv, uri);
}

void sync_and_scan_themes(WebKitWebView *web_view) {}
void scan_faceplates(WebKitWebView *web_view) {}

static void on_browser_sync_request(WebKitUserContentManager *m, JSCValue *r, gpointer web_view) {
    char *msg = jsc_value_to_string(r); if (!msg) return;
    log_debug("BRIDGE MSG: %s", msg);
    if (g_str_has_prefix(msg, "save_theme|")) {
        char **parts = g_strsplit(msg, "|", 2); if (parts && parts[1]) save_active_theme(parts[1]); g_strfreev(parts);
    } 
    else if (g_str_has_prefix(msg, "delete_video|")) {
        char **parts = g_strsplit(msg, "|", 2);
        if (parts && parts[1]) {
            char *vault_dir = g_build_filename(g_get_user_config_dir(), "HTS_Video_Vault", NULL);
            char *file_path = g_build_filename(vault_dir, parts[1], NULL);
            g_mutex_lock(&copy_mutex); if (current_copy_destination && g_strcmp0(current_copy_destination, file_path) == 0) cleanup_partial_copy(); g_mutex_unlock(&copy_mutex);
            if (g_file_test(file_path, G_FILE_TEST_EXISTS)) g_remove(file_path);
            g_free(file_path); g_free(vault_dir);
        }
        g_strfreev(parts);
    } 
    /* THIS IS THE FIX: Instantly catch set_exit_btn signals to hide the GTK window */
    else if (g_str_has_prefix(msg, "set_exit_btn|")) {
        char **parts = g_strsplit(msg, "|", 2);
        if (parts && parts[1]) {
            gboolean show = (g_strcmp0(parts[1], "1") == 0);
            if (exit_btn) gtk_widget_set_visible(exit_btn, show);
            log_debug("HUD: EXIT button visibility set to %s", show ? "TRUE" : "FALSE");
        }
        g_strfreev(parts);
    }
    else if (g_strcmp0(msg, "shutdown_system") == 0) {
        GApplication *app = g_application_get_default(); if (app) g_application_quit(app);
    }
    else if (g_strcmp0(msg, "import_video") == 0) {
        open_hts_file_chooser(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(web_view))), WEBKIT_WEB_VIEW(web_view));
    }
    else if (g_strcmp0(msg, "import_theme") == 0) {
        open_hts_custom_chooser(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(web_view))), WEBKIT_WEB_VIEW(web_view), CHOOSER_MODE_THEME);
    }
    else if (g_strcmp0(msg, "open_plate_selector") == 0) {
        open_hts_custom_chooser(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(web_view))), WEBKIT_WEB_VIEW(web_view), CHOOSER_MODE_FACEPLATE);
    }
    else if (g_strcmp0(msg, "import_screensaver") == 0) {
        open_hts_screensaver_chooser(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(web_view))), WEBKIT_WEB_VIEW(web_view));
    }
    else if (g_strcmp0(msg, "doctor_select_video") == 0) {
        open_hts_custom_chooser(GTK_WIDGET(gtk_widget_get_root(GTK_WIDGET(web_view))), WEBKIT_WEB_VIEW(web_view), CHOOSER_MODE_DOCTOR);
    }
    else if (g_strcmp0(msg, "overlay_hide") == 0) {
        if (hud_box)    gtk_widget_set_visible(hud_box,    FALSE);
        if (logo_image) gtk_widget_set_visible(logo_image, FALSE);
    }
    else if (g_strcmp0(msg, "overlay_show") == 0) {
        if (hud_box)    gtk_widget_set_visible(hud_box,    TRUE);
        if (logo_image) gtk_widget_set_visible(logo_image, TRUE);
    }
    else if (g_strcmp0(msg, "overlay_logo_hide") == 0) {
        if (logo_image) gtk_widget_set_visible(logo_image, FALSE);
    }
    else if (g_strcmp0(msg, "overlay_logo_show") == 0) {
        if (logo_image) gtk_widget_set_visible(logo_image, TRUE);
    }
    g_free(msg);
}

/* Helper: spawn xdg-open for a URI — most reliable cross-desktop method */
static void open_uri_in_browser(const char *uri) {
    log_debug("EXTERNAL LINK: spawning xdg-open for: %s", uri);
    char *argv[] = { "xdg-open", (char *)uri, NULL };
    GError *err = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)) {
        log_warn("xdg-open failed: %s -- trying gio open", err ? err->message : "?");
        if (err) g_error_free(err);
        /* Fallback: gio open (available in all GNOME/Flatpak environments) */
        char *argv2[] = { "gio", "open", (char *)uri, NULL };
        GError *err2 = NULL;
        g_spawn_async(NULL, argv2, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err2);
        if (err2) g_error_free(err2);
    }
}

/* Intercept both normal navigation AND new-window (target=_blank) to external URLs */
static gboolean on_decide_policy(WebKitWebView *wv, WebKitPolicyDecision *decision,
                                  WebKitPolicyDecisionType type, gpointer user_data) {
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION ||
        type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        WebKitNavigationPolicyDecision *nav = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action(nav);
        WebKitURIRequest *req = webkit_navigation_action_get_request(action);
        const char *uri = webkit_uri_request_get_uri(req);
        if (uri && (g_str_has_prefix(uri, "https://") || g_str_has_prefix(uri, "http://"))) {
            if (!g_str_has_prefix(uri, "http://127.0.0.1") && !g_str_has_prefix(uri, "http://localhost")) {
                open_uri_in_browser(uri);
                webkit_policy_decision_ignore(decision);
                return TRUE;
            }
        }
    }
    return FALSE;
}

/* Safety net: if WebKit tries to create a new WebView for a link, open in browser instead */
static WebKitWebView *on_create_web_view(WebKitWebView *wv, WebKitNavigationAction *action,
                                          gpointer user_data) {
    WebKitURIRequest *req = webkit_navigation_action_get_request(action);
    const char *uri = webkit_uri_request_get_uri(req);
    if (uri && strlen(uri) > 0) open_uri_in_browser(uri);
    return NULL; /* Prevent new WebView from being created */
}

static void on_load_changed(WebKitWebView *web_view, WebKitLoadEvent load_event, gpointer user_data) {
    if (load_event != WEBKIT_LOAD_FINISHED) return;
    const char *uri = webkit_web_view_get_uri(web_view);
    log_debug("PAGE LOAD FINISHED: %s", uri ? uri : "(null)");

    char *port_js = g_strdup_printf("window.postMessage({type: 'HTS_PORT_SYNC', port: %d}, '*');", hts_server_port);
    webkit_web_view_evaluate_javascript(web_view, port_js, -1, NULL, NULL, NULL, NULL, NULL);
    g_free(port_js);
    inject_saved_theme(web_view);

    char *uri_base = uri ? g_path_get_basename(uri) : NULL;
    log_debug("  uri_base=%s", uri_base ? uri_base : "(null)");

    /* EXIT BUTTON EXCLUSION LIST
     * Add page basenames here to hide EXIT SYSTEM on that page.
     * All other pages show the button automatically.                */
    const char *hide_exit_on[] = {
        "doctor.html",
        "screensaver-chooser.html",
        "theme-browser.html",
        NULL
    };
    gboolean hide_exit = FALSE;
    for (int i = 0; hide_exit_on[i]; i++)
        if (g_strcmp0(uri_base, hide_exit_on[i]) == 0) { hide_exit = TRUE; break; }
    if (exit_btn) gtk_widget_set_visible(exit_btn, !hide_exit);

    /* LOGO EXCLUSION LIST
     * Add page basenames here to hide the GTK logo on that page.
     * (e.g. pages that have their own branded logo in the HTML)    */
    const char *hide_logo_on[] = {
        "doctor.html",
        "index.html",
        NULL
    };
    gboolean hide_logo = FALSE;
    for (int i = 0; hide_logo_on[i]; i++)
        if (g_strcmp0(uri_base, hide_logo_on[i]) == 0) { hide_logo = TRUE; break; }
    if (logo_image) gtk_widget_set_visible(logo_image, !hide_logo);

    if (uri_base && g_strcmp0(uri_base, "index.html") == 0) {
        log_debug("  -> index.html: hiding clock, logo in HTML");
        if (hud_box)     gtk_widget_set_visible(hud_box,     TRUE);
        if (clock_label) gtk_widget_set_visible(clock_label, FALSE);
    } else if (uri_base && g_strcmp0(uri_base, "screensaver.html") == 0) {
        log_debug("  -> screensaver.html: clock in JS nav");
        if (hud_box)     gtk_widget_set_visible(hud_box,     TRUE);
        if (clock_label) gtk_widget_set_visible(clock_label, FALSE);
    } else {
        log_debug("  -> other page: hiding hud_box");
        if (hud_box) gtk_widget_set_visible(hud_box, FALSE);
    }
    g_free(uri_base);
}

/* Global F5 handler — fires at GTK level before any widget sees the key,
   so focus state (WebView, iframe, button, etc.) is irrelevant. */
static gboolean on_global_key_pressed(GtkEventControllerKey *ctrl,
                                       guint keyval, guint keycode,
                                       GdkModifierType state, gpointer web_view) {
    if (keyval == GDK_KEY_F5) {
        log_debug("GLOBAL F5: resetting theme to default");
        /* 1. Delete active_theme.conf on the C side */
        save_active_theme("");
        /* 2. Tell the JS side to reset localStorage and reload */
        webkit_web_view_evaluate_javascript(
            WEBKIT_WEB_VIEW(web_view),
            "(() => {"
            "  const D='file:///app/bin/data/pages/theme_original.html';"
            "  localStorage.setItem('hts_theme_preference',D);"
            "  localStorage.removeItem('hts_faceplate_path');"
            "  const f=document.getElementById('furniture-frame');"
            "  if(f) f.src=D;"
            "  setTimeout(()=>location.reload(),300);"
            "})()",
            -1, NULL, NULL, NULL, NULL, NULL);
        return TRUE; /* swallow — don't let GTK do a page reload too */
    }
    return FALSE;
}

static gboolean restore_app_volume(gpointer user_data) {
    /* PulseAudio/PipeWire remembers per-app volume. If GNOME silently set
       HTS to 0%, fix it -- but ONLY if it is 0%. Never touch a volume the
       user has deliberately chosen. */
    log_debug("VOLUME: Checking HTS stream volume via pactl...");
    /* Write the awk script to a temp file to avoid nested-quote hell in C */
    const char *script_path = "/tmp/hts_vol_check.sh";
    FILE *f = fopen(script_path, "w");
    if (f) {
        fprintf(f, "#!/bin/sh\n");
        fprintf(f, "IDX=\"\"\n");
        fprintf(f, "FOUND=0\n");
        fprintf(f, "while IFS= read -r line; do\n");
        fprintf(f, "  case \"$line\" in\n");
        fprintf(f, "    *\"Sink Input #\"*) IDX=\"${line##*#}\" ;;\n");
        fprintf(f, "    *\"hts_time\"*|*\"storcke\"*) FOUND=1 ;;\n");
        fprintf(f, "    *\"Volume:\"*)\n");
        fprintf(f, "      if [ \"$FOUND\" = \"1\" ]; then\n");
        fprintf(f, "        PCT=\"$(echo \"$line\" | grep -oP \'[0-9]+(?=%%)\'  | head -1)\"\n");
        fprintf(f, "        if [ \"$PCT\" = \"0\" ]; then\n");
        fprintf(f, "          pactl set-sink-input-volume \"$IDX\" 100%% 2>/dev/null\n");
        fprintf(f, "        fi\n");
        fprintf(f, "        FOUND=0\n");
        fprintf(f, "      fi ;;\n");
        fprintf(f, "  esac\n");
        fprintf(f, "done < <(pactl list sink-inputs 2>/dev/null)\n");
        fclose(f);
        chmod(script_path, 0755);
        int r = system(script_path);
        log_debug("VOLUME: restore script exit=%d", r);
    } else {
        log_warn("VOLUME: could not write temp script");
    }
    return G_SOURCE_REMOVE; /* One-shot */
}

static void activate(GtkApplication *app, gpointer user_data) {
    start_micro_server();
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_fullscreen(GTK_WINDOW(window));
    GtkWidget *overlay = gtk_overlay_new();
    gtk_window_set_child(GTK_WINDOW(window), overlay);
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        /* ── Base ── */
        "window { background-color: #000; } "

        /* ── HUD ── */
        "#hts-hud { margin: 10px; background: transparent; } "
        "#hts-clock { color: #00d4ff; font-family: monospace; font-size: 15px; font-weight: bold; background: transparent; } "
        "button#hts-exit { background: rgba(0,0,0,0.8); color: #ff3131; border: 1px solid #ff3131; "
        "  padding: 4px 12px; border-radius: 4px; font-weight: bold; font-size: 12px; "
        "  box-shadow: none; } "
        "button#hts-exit label { color: #ff3131; font-weight: bold; font-size: 12px; } "
        "button#hts-exit:hover { background: #ff3131; } "
        "button#hts-exit:hover label { color: #000; } "


    );
    g_object_unref(css);

    WebKitSettings *settings = webkit_settings_new();
    webkit_settings_set_allow_file_access_from_file_urls(settings, TRUE);
    webkit_settings_set_allow_universal_access_from_file_urls(settings, TRUE);
    webkit_settings_set_media_playback_requires_user_gesture(settings, FALSE);
    /* Allow file:// pages to load http://127.0.0.1 iframes (screensaver previews).
       Without this WebKit blocks the mixed content silently. */
    webkit_settings_set_allow_top_navigation_to_data_urls(settings, TRUE);
    g_object_set(settings, "allow-running-insecure-content", TRUE, NULL);
    WebKitUserContentManager *manager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(manager, "hts_browser_sync", NULL);
    WebKitWebView *web_view = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW, "user-content-manager", manager, "settings", settings, NULL));

    /* WebKitGTK 6.0 in Flatpak: the URI is always explicitly set below,
       so no session restore can override it. */
    gtk_overlay_set_child(GTK_OVERLAY(overlay), GTK_WIDGET(web_view));

    /* Attach global key controller to the TOP-LEVEL WINDOW so F5 fires
       regardless of which widget (WebView, iframe, button) has focus */
    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_global_key_pressed), web_view);
    gtk_widget_add_controller(window, key_ctrl);
    
    hud_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); 
    gtk_widget_set_halign(hud_box, GTK_ALIGN_END);
    gtk_widget_set_valign(hud_box, GTK_ALIGN_START);
    gtk_widget_set_name(hud_box, "hts-hud");
    
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(top_row, GTK_ALIGN_END);
    clock_label = gtk_label_new("");
    gtk_widget_set_name(clock_label, "hts-clock");
    gtk_box_append(GTK_BOX(top_row), clock_label);
    exit_btn = gtk_button_new_with_label("EXIT SYSTEM");
    gtk_widget_set_name(exit_btn, "hts-exit");
    GtkCssProvider *exit_css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(exit_css,
        "button#hts-exit, button#hts-exit > label {"
        "  color: #ff3131;"
        "  background: rgba(0,0,0,0.8);"
        "  border: 1px solid #ff3131;"
        "  border-radius: 4px;"
        "  font-family: monospace;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "button#hts-exit:hover, button#hts-exit:hover > label {"
        "  background: #ff3131;"
        "  color: #000;"
        "}");
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(exit_btn),
        GTK_STYLE_PROVIDER(exit_css),
        GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
    g_object_unref(exit_css);
    g_signal_connect_swapped(exit_btn, "clicked", G_CALLBACK(g_application_quit), app);
    /* exit_btn added as its own overlay below */
    gtk_box_append(GTK_BOX(hud_box), top_row);
    
    // CHANGED: Use GtkImage and set_pixel_size to FORCE a tiny scale
    char *logo_path = g_build_filename(DATADIR, "data", "logo.png", NULL);
    logo_image = gtk_image_new_from_file(logo_path);
    // Logo size matches the embedded logos removed from individual pages
    gtk_image_set_pixel_size(GTK_IMAGE(logo_image), 160);
    gtk_widget_set_halign(logo_image, GTK_ALIGN_END);
    g_free(logo_path);
    /* logo added as its own overlay below */
    
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), hud_box);
    /* Logo badge — own overlay, sits below the nav bar */
    GtkWidget *logo_overlay_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(logo_overlay_box, GTK_ALIGN_END);
    gtk_widget_set_valign(logo_overlay_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(logo_overlay_box, 60);
    gtk_widget_set_margin_end(logo_overlay_box, 4);
    gtk_box_append(GTK_BOX(logo_overlay_box), logo_image);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), logo_overlay_box);
    /* EXIT button is its own overlay — visible on all pages independently of hud_box */
    GtkWidget *exit_overlay_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(exit_overlay_box, GTK_ALIGN_END);
    gtk_widget_set_valign(exit_overlay_box, GTK_ALIGN_START);
    gtk_widget_set_margin_top(exit_overlay_box, 10);
    gtk_widget_set_margin_end(exit_overlay_box, 10);
    gtk_box_append(GTK_BOX(exit_overlay_box), exit_btn);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), exit_overlay_box);
    g_signal_connect(manager, "script-message-received::hts_browser_sync", G_CALLBACK(on_browser_sync_request), web_view);
    g_signal_connect(web_view, "load-changed", G_CALLBACK(on_load_changed), NULL);
    g_signal_connect(web_view, "decide-policy", G_CALLBACK(on_decide_policy), NULL);
    g_signal_connect(web_view, "create", G_CALLBACK(on_create_web_view), NULL);
    char uri[2048];
    snprintf(uri, sizeof(uri), "file://%s/data/pages/%s", DATADIR, is_screensaver ? "screensaver.html" : "index.html");
    log_debug("APP START: loading URI: %s", uri);
    webkit_web_view_load_uri(web_view, uri); 
    gtk_window_present(GTK_WINDOW(window)); 
    g_timeout_add_seconds(1, update_clock_hud, NULL);
    /* Restore volume 1.5s after startup — PulseAudio stream must be registered first */
    g_timeout_add(1500, restore_app_volume, NULL);
}

int main(int argc, char **argv) {
    /* 1. ALWAYS print the HTS timestamp — even in CLI-only mode */
    print_hts();

    /* 2. Scan our own flags BEFORE GTK sees argv.
     * GTK does not know about --gui and will error on unknown options.
     * We check for our flags here, then strip --gui out so GTK never
     * encounters it. --screensaver is kept in argv because nothing
     * inside GTK/GApplication will choke on an unrecognised flag when
     * G_APPLICATION_DEFAULT_FLAGS is used — but we strip it too for
     * cleanliness and re-set is_screensaver ourselves. */
    gboolean launch_gui = FALSE;

    /* Build a clean argv with --gui and --screensaver removed */
    int clean_argc = 0;
    char **clean_argv = g_new0(char *, argc + 1);
    clean_argv[clean_argc++] = argv[0]; /* always keep argv[0] (program name) */

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--gui") == 0) {
            launch_gui = TRUE;          /* flag seen — do NOT forward to GTK */
        } else if (g_strcmp0(argv[i], "--screensaver") == 0) {
            launch_gui = TRUE;
            is_screensaver = TRUE;      /* set global — do NOT forward to GTK */
        } else {
            clean_argv[clean_argc++] = argv[i]; /* forward anything else */
        }
    }

    /* 3. CLI mode — timestamp already printed above, just exit */
    if (!launch_gui) {
        g_free(clean_argv);
        return 0;
    }

    /* 4. GUI LAUNCH — only reached when --gui or --screensaver was passed */
    char *log_path = g_build_filename(g_get_user_data_dir(), "hts_debug.log", NULL);
    g_mkdir_with_parents(g_get_user_data_dir(), 0755);
    debug_log = fopen(log_path, "a");
    g_free(log_path);
    if (debug_log) { fprintf(debug_log, "\n========================================\n=== HTS SESSION START ===\n========================================\n"); fflush(debug_log); }
    session_start_time = time(NULL);
    g_mutex_init(&copy_mutex);
    g_mutex_init(&log_mutex);

    GtkApplication *app = gtk_application_new("com.storcke64.hts_time", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), clean_argc, clean_argv);
    g_object_unref(app);
    g_free(clean_argv);
    g_mutex_clear(&copy_mutex);
    g_mutex_clear(&log_mutex);
    if (debug_log) fclose(debug_log);
    return status;
}
