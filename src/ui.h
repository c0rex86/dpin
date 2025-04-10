/*
 *  /\_/\
 * ( o.o )
 *  > ^ <
 * 
 * meow
 * modded by c0re
 */

#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include "dpin.h"

#define UI_COLOR_NORMAL     1
#define UI_COLOR_HIGHLIGHT  2
#define UI_COLOR_ERROR      3
#define UI_COLOR_SUCCESS    4
#define UI_COLOR_INFO       5
#define UI_COLOR_HEADER_BG  6
#define UI_COLOR_MENU_BG    7
#define UI_COLOR_STATUS_BG  8
#define UI_COLOR_RESULTS_BG 9
#define UI_COLOR_ACCENT     10
#define UI_COLOR_TITLE      11
#define UI_COLOR_WARNING    12
#define UI_COLOR_PROGRESS   13
#define UI_COLOR_LOG_BG     14
#define UI_COLOR_STATS_BG   15
#define UI_COLOR_GRAD_1     16
#define UI_COLOR_GRAD_2     17
#define UI_COLOR_GRAD_3     18

#define UI_HLINE       0x2500
#define UI_VLINE       0x2502
#define UI_LTEE        0x251c
#define UI_RTEE        0x2524
#define UI_BTEE        0x2534
#define UI_TTEE        0x252c
#define UI_ULCORNER    0x250c
#define UI_URCORNER    0x2510
#define UI_LLCORNER    0x2514
#define UI_LRCORNER    0x2518
#define UI_PLUS        0x253c
#define UI_BLOCK       0x2588
#define UI_BLOCK_MID   0x2592
#define UI_BLOCK_LIGHT 0x2591
#define UI_RARROW      0x2192
#define UI_LARROW      0x2190
#define UI_UARROW      0x2191
#define UI_DARROW      0x2193
#define UI_BULLET      0x2022
#define UI_CHECK       0x2713
#define UI_CROSS       0x2717

#define UI_PROGRESS_BLOCK      0x2588
#define UI_PROGRESS_BLOCK_75   0x2593
#define UI_PROGRESS_BLOCK_50   0x2592
#define UI_PROGRESS_BLOCK_25   0x2591

#define MAX_LOG_ENTRIES 100
#define LOG_LINE_LENGTH 256

int init_ui(void);
void cleanup_ui(void);
void update_ui(config_t *config);
void draw_header(void);
void draw_status(config_t *config);
void draw_results(config_t *config);
void draw_menu(void);
void handle_input(config_t *config);
void draw_progress_bar(WINDOW *win, int y, int x, int width, float percentage);
void draw_fancy_border(WINDOW *win);
void draw_centered_text(WINDOW *win, int y, const char *text, int attr);
void draw_gauge(WINDOW *win, int y, int x, int width, float value, float max_value, int attr);
void draw_gradient_bar(WINDOW *win, int y, int x, int width, float percentage);
void draw_shadow(WINDOW *win);

void ui_log(const char *fmt, ...);
void draw_logs(void);
void toggle_log_window(void);
void clear_logs(void);

int handle_keypress(int ch, config_t *config);

void show_http_method_selector(config_t *config);
void show_data_input(config_t *config);
void manage_http_headers(config_t *config);
void perform_meow_test(config_t *config);

void animate_progress(WINDOW *win, int y, int x, int width, float from, float to, int steps);
void show_splash_screen(void);
void draw_info_box(const char *title, const char *message);
bool draw_warning_box(const char *title, const char *message);
void set_color_theme(int theme_id);

#endif /* UI_H */