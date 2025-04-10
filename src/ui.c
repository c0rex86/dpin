/*
 *  /\_/\
 * ( o.o )
 *  > ^ <
 * 
 * meow
 * modded by c0re
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <wchar.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>
#include "ui.h"
#include "dpin.h"
#include "network.h"

static WINDOW *header_win;
static WINDOW *status_win;
static WINDOW *results_win;
static WINDOW *menu_win;
static WINDOW *log_win = NULL;
static int term_height, term_width;
static int show_logs = 0;
static int theme_id = 0;

static char log_entries[MAX_LOG_ENTRIES][LOG_LINE_LENGTH];
static int log_count = 0;
static int log_start_idx = 0;

typedef struct {
    short header_fg, header_bg;
    short menu_fg, menu_bg;
    short status_fg, status_bg;
    short results_fg, results_bg;
    short accent_fg, accent_bg;
    short title_fg, title_bg;
    short progress_fg, progress_bg;
    short log_fg, log_bg;
} color_theme_t;

static color_theme_t themes[] = {
    {
        COLOR_WHITE, COLOR_BLUE,
        COLOR_WHITE, COLOR_BLUE,
        COLOR_BLACK, COLOR_CYAN,
        COLOR_WHITE, COLOR_BLACK,
        COLOR_YELLOW, COLOR_BLACK,
        COLOR_MAGENTA, COLOR_BLACK,
        COLOR_GREEN, COLOR_BLACK,
        COLOR_WHITE, COLOR_BLACK
    },
    {
        COLOR_BLACK, COLOR_GREEN,
        COLOR_BLACK, COLOR_GREEN,
        COLOR_BLACK, COLOR_CYAN,
        COLOR_GREEN, COLOR_BLACK,
        COLOR_YELLOW, COLOR_BLACK,
        COLOR_CYAN, COLOR_BLACK,
        COLOR_GREEN, COLOR_BLACK,
        COLOR_GREEN, COLOR_BLACK
    },
    {
        COLOR_CYAN, COLOR_BLACK,
        COLOR_CYAN, COLOR_BLACK,
        COLOR_WHITE, COLOR_BLACK,
        COLOR_WHITE, COLOR_BLACK,
        COLOR_YELLOW, COLOR_BLACK,
        COLOR_MAGENTA, COLOR_BLACK,
        COLOR_GREEN, COLOR_BLACK,
        COLOR_CYAN, COLOR_BLACK
    },
    {
        COLOR_BLACK, COLOR_MAGENTA,
        COLOR_BLACK, COLOR_MAGENTA,
        COLOR_MAGENTA, COLOR_BLACK,
        COLOR_CYAN, COLOR_BLACK,
        COLOR_YELLOW, COLOR_BLACK,
        COLOR_GREEN, COLOR_BLACK,
        COLOR_MAGENTA, COLOR_BLACK,
        COLOR_MAGENTA, COLOR_BLACK
    }
};

static const int num_themes = sizeof(themes) / sizeof(themes[0]);

void set_color_theme(int id) {
    if (id < 0 || id >= num_themes) {
        return;
    }
    
    theme_id = id;
    color_theme_t *theme = &themes[id];
    
    init_pair(UI_COLOR_HEADER_BG, theme->header_fg, theme->header_bg);
    init_pair(UI_COLOR_MENU_BG, theme->menu_fg, theme->menu_bg);
    init_pair(UI_COLOR_STATUS_BG, theme->status_fg, theme->status_bg);
    init_pair(UI_COLOR_RESULTS_BG, theme->results_fg, theme->results_bg);
    init_pair(UI_COLOR_ACCENT, theme->accent_fg, theme->accent_bg);
    init_pair(UI_COLOR_TITLE, theme->title_fg, theme->title_bg);
    init_pair(UI_COLOR_PROGRESS, theme->progress_fg, theme->progress_bg);
    init_pair(UI_COLOR_LOG_BG, theme->log_fg, theme->log_bg);
    
    init_pair(UI_COLOR_GRAD_1, COLOR_GREEN, COLOR_BLACK);
    init_pair(UI_COLOR_GRAD_2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(UI_COLOR_GRAD_3, COLOR_RED, COLOR_BLACK);
}

void ui_log(const char *fmt, ...) {
    va_list args;
    char buffer[LOG_LINE_LENGTH];
    time_t now;
    struct tm *tm_now;
    
    time(&now);
    tm_now = localtime(&now);
    
    int time_len = strftime(buffer, sizeof(buffer), "[%H:%M:%S] ", tm_now);
    
    va_start(args, fmt);
    vsnprintf(buffer + time_len, sizeof(buffer) - time_len, fmt, args);
    va_end(args);
    
    if (log_count < MAX_LOG_ENTRIES) {
        strcpy(log_entries[log_count++], buffer);
    } else {
        strcpy(log_entries[log_start_idx], buffer);
        log_start_idx = (log_start_idx + 1) % MAX_LOG_ENTRIES;
    }
    
    if (show_logs && log_win) {
        draw_logs();
    }
}

void draw_gradient_bar(WINDOW *win, int y, int x, int width, float percentage) {
    int filled_width = (int)(percentage * width / 100.0);
    int i;
    
    mvwprintw(win, y, x, "[");
    
    for (i = 0; i < filled_width; i++) {
        if (i < width / 3) {
            wattron(win, COLOR_PAIR(UI_COLOR_GRAD_1));
        } else if (i < width * 2 / 3) {
            wattron(win, COLOR_PAIR(UI_COLOR_GRAD_2));
        } else {
            wattron(win, COLOR_PAIR(UI_COLOR_GRAD_3));
        }
        waddch(win, UI_BLOCK);
        wattroff(win, COLOR_PAIR(UI_COLOR_GRAD_1 | UI_COLOR_GRAD_2 | UI_COLOR_GRAD_3));
    }
    
    for (; i < width; i++) {
        waddch(win, ' ');
    }
    
    mvwprintw(win, y, x + width + 1, "] %5.1f%%", percentage);
}

void animate_progress(WINDOW *win, int y, int x, int width, float from, float to, int steps) {
    float step_size = (to - from) / steps;
    float current = from;
    
    for (int i = 0; i < steps; i++) {
        current += step_size;
        draw_progress_bar(win, y, x, width, current);
        wrefresh(win);
        usleep(10000);
    }
    
    draw_progress_bar(win, y, x, width, to);
    wrefresh(win);
}

void draw_progress_bar(WINDOW *win, int y, int x, int width, float percentage) {
    int filled_width = (int)(percentage * width / 100.0);
    int i;
    
    mvwprintw(win, y, x, "[");
    wattron(win, COLOR_PAIR(UI_COLOR_PROGRESS));
    
    for (i = 0; i < filled_width; i++) {
        waddch(win, UI_BLOCK);
    }
    
    wattroff(win, COLOR_PAIR(UI_COLOR_PROGRESS));
    
    for (; i < width; i++) {
        waddch(win, ' ');
    }
    
    mvwprintw(win, y, x + width + 1, "] %5.1f%%", percentage);
}

void draw_shadow(WINDOW *win) {
    int height, width, i;
    getmaxyx(win, height, width);
    
    int starty, startx;
    getbegyx(win, starty, startx);
    
    for (i = 0; i < height; i++) {
        mvaddch(starty + i, startx + width, ' ' | A_REVERSE);
    }
    
    for (i = 0; i < width + 1; i++) {
        mvaddch(starty + height, startx + i, ' ' | A_REVERSE);
    }
    
    refresh();
}

void draw_fancy_border(WINDOW *win) {
    int height, width;
    getmaxyx(win, height, width);
    
    mvwaddch(win, 0, 0, UI_ULCORNER);
    mvwaddch(win, 0, width - 1, UI_URCORNER);
    mvwaddch(win, height - 1, 0, UI_LLCORNER);
    mvwaddch(win, height - 1, width - 1, UI_LRCORNER);
    
    for (int i = 1; i < width - 1; i++) {
        mvwaddch(win, 0, i, UI_HLINE);
        mvwaddch(win, height - 1, i, UI_HLINE);
    }
    
    for (int i = 1; i < height - 1; i++) {
        mvwaddch(win, i, 0, UI_VLINE);
        mvwaddch(win, i, width - 1, UI_VLINE);
    }
}

void draw_centered_text(WINDOW *win, int y, const char *text, int attr) {
    int width;
    getmaxyx(win, /* unused */ (int){0}, width);
    
    int x = (width - strlen(text)) / 2;
    if (x < 0) x = 0;
    
    wattron(win, attr);
    mvwprintw(win, y, x, "%s", text);
    wattroff(win, attr);
}

void draw_gauge(WINDOW *win, int y, int x, int width, float value, float max_value, int attr) {
    float percentage = value / max_value * 100.0;
    int filled_width = (int)(percentage * width / 100.0);
    
    wattron(win, attr);
    for (int i = 0; i < filled_width; i++) {
        mvwaddch(win, y, x + i, UI_BLOCK);
    }
    wattroff(win, attr);
    
    for (int i = filled_width; i < width; i++) {
        mvwaddch(win, y, x + i, UI_BLOCK_LIGHT);
    }
    
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.1f/%.1f", value, max_value);
    mvwprintw(win, y, x + width + 2, "%s", value_str);
}

int init_ui(void) {
    setlocale(LC_ALL, "");
    
    initscr();
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "[c0re} Ваш терминал не поддерживает цвета\n");
        return 1;
    }
    
    start_color();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(100);
    
    getmaxyx(stdscr, term_height, term_width);
    
    init_pair(UI_COLOR_NORMAL, COLOR_WHITE, COLOR_BLACK);
    init_pair(UI_COLOR_HIGHLIGHT, COLOR_BLACK, COLOR_CYAN);
    init_pair(UI_COLOR_ERROR, COLOR_RED, COLOR_BLACK);
    init_pair(UI_COLOR_SUCCESS, COLOR_GREEN, COLOR_BLACK);
    init_pair(UI_COLOR_INFO, COLOR_CYAN, COLOR_BLACK);
    
    set_color_theme(theme_id);
    
    header_win = newwin(3, term_width, 0, 0);
    status_win = newwin(3, term_width, 3, 0);
    results_win = newwin(term_height - 8, term_width, 6, 0);
    menu_win = newwin(2, term_width, term_height - 2, 0);
    
    keypad(header_win, TRUE);
    keypad(status_win, TRUE);
    keypad(results_win, TRUE);
    keypad(menu_win, TRUE);
    
    show_splash_screen();
    
    return 0;
}

void show_splash_screen(void) {
    clear();
    refresh();
    
    int splash_width = 60;
    int splash_height = 10;
    int start_y = (term_height - splash_height) / 2;
    int start_x = (term_width - splash_width) / 2;
    
    WINDOW *splash = newwin(splash_height, splash_width, start_y, start_x);
    wbkgd(splash, COLOR_PAIR(UI_COLOR_HEADER_BG));
    
    draw_fancy_border(splash);
    
    draw_centered_text(splash, 2, "DPIN - Network Load Testing Tool", A_BOLD);
    draw_centered_text(splash, 4, "Distributed Performance Investigation Network", COLOR_PAIR(UI_COLOR_ACCENT));
    draw_centered_text(splash, 6, "Version 1.0.0 - By c0re", A_NORMAL);
    draw_centered_text(splash, 8, "Press any key to continue...", COLOR_PAIR(UI_COLOR_HIGHLIGHT));
    
    draw_shadow(splash);
    
    wrefresh(splash);
    
    timeout(-1);
    getch();
    timeout(100);
    
    for (int i = 1; i <= splash_width / 2; i++) {
        mvwvline(splash, 1, i, ' ', splash_height - 2);
        mvwvline(splash, 1, splash_width - i - 1, ' ', splash_height - 2);
        wrefresh(splash);
        usleep(5000);
    }
    
    delwin(splash);
    clear();
    refresh();
}

void cleanup_ui(void) {
    if (log_win) {
        delwin(log_win);
    }
    delwin(header_win);
    delwin(status_win);
    delwin(results_win);
    delwin(menu_win);
    endwin();
}

void draw_header(void) {
    wclear(header_win);
    wbkgd(header_win, COLOR_PAIR(UI_COLOR_HEADER_BG));
    draw_fancy_border(header_win);
    
    char title[] = " dpin - Distributed Performance Investigation Network ";
    draw_centered_text(header_win, 1, title, A_BOLD);
    
    wrefresh(header_win);
}

void draw_status(config_t *config) {
    wclear(status_win);
    wbkgd(status_win, COLOR_PAIR(UI_COLOR_NORMAL));
    draw_fancy_border(status_win);
    
    char status_str[32];
    int color = UI_COLOR_NORMAL;
    
    switch (config->status) {
        case STATUS_IDLE:
            strcpy(status_str, "Готов");
            color = UI_COLOR_NORMAL;
            break;
        case STATUS_RUNNING:
            strcpy(status_str, "Тестирование...");
            color = UI_COLOR_INFO;
            break;
        case STATUS_FINISHED:
            strcpy(status_str, "Завершено");
            color = UI_COLOR_SUCCESS;
            break;
        case STATUS_ERROR:
            strcpy(status_str, "Ошибка");
            color = UI_COLOR_ERROR;
            break;
    }
    
    wattron(status_win, COLOR_PAIR(color) | A_BOLD);
    mvwprintw(status_win, 1, 2, "Статус: %s", status_str);
    wattroff(status_win, COLOR_PAIR(color) | A_BOLD);
    
    wattron(status_win, COLOR_PAIR(UI_COLOR_ACCENT));
    mvwprintw(status_win, 1, term_width - strlen(config->url ? config->url : "не указан") - 6, "URL: ");
    wattroff(status_win, COLOR_PAIR(UI_COLOR_ACCENT));
    
    wattron(status_win, A_UNDERLINE | COLOR_PAIR(UI_COLOR_HIGHLIGHT));
    wprintw(status_win, "%s", config->url ? config->url : "не указан");
    wattroff(status_win, A_UNDERLINE | COLOR_PAIR(UI_COLOR_HIGHLIGHT));
    
    wrefresh(status_win);
}

void draw_menu(void) {
    wclear(menu_win);
    wbkgd(menu_win, COLOR_PAIR(UI_COLOR_MENU_BG));
    draw_fancy_border(menu_win);
    
    char *menu_items[] = {
        "S - Старт", "P - Пауза", "R - Сброс", "L - Логи",
        "M - Метод", "H - Заголовки", "D - Данные", "T - Тема", "Q - Выход"
    };
    
    int item_width = term_width / (sizeof(menu_items) / sizeof(menu_items[0]) + 1);
    int offset = 2;
    
    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {
        wattron(menu_win, A_BOLD | COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(menu_win, 1, offset, "%c", menu_items[i][0]);
        wattroff(menu_win, A_BOLD | COLOR_PAIR(UI_COLOR_ACCENT));
        
        wprintw(menu_win, "%s", menu_items[i] + 1);
        offset += strlen(menu_items[i]) + 2;
    }
    
    wrefresh(menu_win);
}

void draw_logs(void) {
    int log_width = term_width;
    int log_height = term_height / 2;
    
    if (!log_win) {
        log_win = newwin(log_height, log_width, term_height - log_height, 0);
    }
    
    wclear(log_win);
    wbkgd(log_win, COLOR_PAIR(UI_COLOR_LOG_BG));
    box(log_win, 0, 0);
    
    mvwprintw(log_win, 0, 2, "[ Журнал событий ]");
    
    int visible_logs = log_height - 2;
    
    int start_idx = (log_count <= visible_logs) ? 0 : 
                    (log_start_idx + log_count - visible_logs) % MAX_LOG_ENTRIES;
    
    for (int i = 0; i < visible_logs && i < log_count; i++) {
        int idx = (start_idx + i) % MAX_LOG_ENTRIES;
        mvwprintw(log_win, i + 1, 1, "%s", log_entries[idx]);
    }
    
    wrefresh(log_win);
}

void toggle_log_window(void) {
    show_logs = !show_logs;
    
    if (show_logs) {
        if (!log_win) {
            int log_width = term_width;
            int log_height = term_height / 2;
            log_win = newwin(log_height, log_width, term_height - log_height, 0);
        }
        
        draw_logs();
    } else {
        if (log_win) {
            delwin(log_win);
            log_win = NULL;
            
            clear();
            refresh();
        }
    }
}

void clear_logs(void) {
    log_count = 0;
    log_start_idx = 0;
    
    ui_log("[c0re} Журнал очищен");
}

void draw_info_box(const char *title, const char *message) {
    int box_width = 60;
    int box_height = 8;
    int start_y = (term_height - box_height) / 2;
    int start_x = (term_width - box_width) / 2;
    
    WINDOW *info_win = newwin(box_height, box_width, start_y, start_x);
    wbkgd(info_win, COLOR_PAIR(UI_COLOR_STATUS_BG));
    
    box(info_win, 0, 0);
    
    wattron(info_win, A_BOLD);
    mvwprintw(info_win, 0, (box_width - strlen(title) - 4) / 2, "[ %s ]", title);
    wattroff(info_win, A_BOLD);
    
    mvwprintw(info_win, 3, (box_width - strlen(message)) / 2, "%s", message);
    
    mvwprintw(info_win, 6, (box_width - 4) / 2, "[ OK ]");
    
    draw_shadow(info_win);
    wrefresh(info_win);
    
    timeout(-1);
    wgetch(info_win);
    timeout(100);
    
    delwin(info_win);
    
    clear();
    refresh();
}

bool draw_warning_box(const char *title, const char *message) {
    int box_width = 60;
    int box_height = 10;
    int start_y = (term_height - box_height) / 2;
    int start_x = (term_width - box_width) / 2;
    
    WINDOW *warn_win = newwin(box_height, box_width, start_y, start_x);
    wbkgd(warn_win, COLOR_PAIR(UI_COLOR_ERROR));
    
    box(warn_win, 0, 0);
    
    wattron(warn_win, A_BOLD);
    mvwprintw(warn_win, 0, (box_width - strlen(title) - 4) / 2, "[ %s ]", title);
    wattroff(warn_win, A_BOLD);
    
    mvwprintw(warn_win, 2, (box_width - strlen(message)) / 2, "%s", message);
    
    mvwprintw(warn_win, 4, box_width / 3 - 2, "[ Да ]");
    mvwprintw(warn_win, 4, 2 * box_width / 3 - 2, "[ Нет ]");
    
    draw_shadow(warn_win);
    wrefresh(warn_win);
    
    timeout(-1);
    bool result = false;
    
    while (1) {
        int ch = wgetch(warn_win);
        if (ch == 'y' || ch == 'Y' || ch == KEY_ENTER) {
            result = true;
            break;
        } else if (ch == 'n' || ch == 'N' || ch == 27) {
            result = false;
            break;
        }
    }
    
    timeout(100);
    delwin(warn_win);
    
    clear();
    refresh();
    
    return result;
}

int handle_keypress(int ch, config_t *config) {
    if (!config) return 0;
    
    switch (ch) {
        case 'q':
        case 'Q':
            return -1;
            
        case 's':
        case 'S':
            if (config->status != STATUS_RUNNING) {
                if (strlen(config->url) > 0) {
                    start_test(config);
                } else {
                    draw_warning_box("Ошибка", "URL не указан");
                }
            }
            break;
            
        case 'p':
        case 'P':
            if (config->status == STATUS_RUNNING) {
                stop_test(config);
            }
            break;
            
        case 'r':
        case 'R':
            reset_results(config);
            break;
            
        case 'l':
        case 'L':
            toggle_log_window();
            break;
            
        case 'c':
        case 'C':
            clear_logs();
            break;
            
        case 'm':
        case 'M':
            show_http_method_selector(config);
            break;
            
        case 'd':
        case 'D':
            if (config->http_params.method == HTTP_POST || 
                config->http_params.method == HTTP_PUT || 
                config->http_params.method == HTTP_PATCH || 
                config->http_params.method == HTTP_DELETE) {
                show_data_input(config);
            } else {
                draw_warning_box("Ошибка", "Данные можно отправлять только для POST, PUT, PATCH и DELETE");
            }
            break;
            
        case 'h':
        case 'H':
            manage_http_headers(config);
            break;
            
        case 't':
        case 'T':
            if (config->status != STATUS_RUNNING) {
                config->type = (config->type + 1) % (TEST_MEOW + 1);
                ui_log("[c0re} Тип теста изменен на %s", 
                      config->type == TEST_HTTP ? "HTTP" : 
                      config->type == TEST_HTTPS ? "HTTPS" : 
                      config->type == TEST_TCP ? "TCP" : 
                      config->type == TEST_UDP ? "UDP" : "MEOW");
            }
            break;
    }
    
    return 0;
}

void draw_results(config_t *config) {
    wclear(results_win);
    wbkgd(results_win, COLOR_PAIR(UI_COLOR_RESULTS_BG));
    draw_fancy_border(results_win);
    
    wattron(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
    mvwprintw(results_win, 1, 2, "РЕЗУЛЬТАТЫ ТЕСТИРОВАНИЯ");
    wattroff(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
    
    wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
    mvwprintw(results_win, 3, 2, "Тип теста:");
    wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
    
    wattron(results_win, A_BOLD);
    mvwprintw(results_win, 3, 14, "%s", 
              config->type == TEST_HTTP ? "HTTP" : 
              config->type == TEST_HTTPS ? "HTTPS" : 
              config->type == TEST_TCP ? "TCP" : "UDP");
    wattroff(results_win, A_BOLD);
    
    wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
    mvwprintw(results_win, 4, 2, "Потоки:");
    wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
    
    mvwprintw(results_win, 4, 14, "%d", config->threads);
    
    wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
    mvwprintw(results_win, 5, 2, "Запросы:");
    wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
    
    mvwprintw(results_win, 5, 14, "%d", config->requests);
    
    for (int i = 2; i < term_width - 2; i++) {
        mvwaddch(results_win, 6, i, UI_HLINE);
    }
    
    if (config->status == STATUS_RUNNING || config->status == STATUS_FINISHED) {
        float completion_percentage = config->results.total_requests > 0 ?
            (float)config->results.completed_requests / config->results.total_requests * 100.0 : 0.0;
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 8, 2, "Прогресс:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        draw_gradient_bar(results_win, 8, 14, 40, completion_percentage);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 9, 2, "Выполнено:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 9, 14, "%d из %d", 
                 config->results.completed_requests,
                 config->results.total_requests);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 10, 2, "Ошибки:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        if (config->results.failed_requests > 0) {
            wattron(results_win, COLOR_PAIR(UI_COLOR_ERROR));
            mvwprintw(results_win, 10, 14, "%d", config->results.failed_requests);
            wattroff(results_win, COLOR_PAIR(UI_COLOR_ERROR));
        } else {
            wattron(results_win, COLOR_PAIR(UI_COLOR_SUCCESS));
            mvwprintw(results_win, 10, 14, "%d", config->results.failed_requests);
            wattroff(results_win, COLOR_PAIR(UI_COLOR_SUCCESS));
        }
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
        mvwprintw(results_win, 12, 2, "ВРЕМЯ ОТВЕТА");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 13, 2, "Минимум:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 13, 14, "%.3f сек", config->results.min_time);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 14, 2, "Среднее:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 14, 14, "%.3f сек", config->results.avg_time);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 15, 2, "Максимум:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 15, 14, "%.3f сек", config->results.max_time);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 16, 2, "Общее:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 16, 14, "%.3f сек", config->results.total_time);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
        mvwprintw(results_win, 18, 2, "ПРОИЗВОДИТЕЛЬНОСТЬ");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 19, 2, "Запросов/сек:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 19, 18, "%.2f", config->results.requests_per_second);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 20, 2, "Данных/сек:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 20, 18, "%.2f КБ/с", config->results.bytes_per_second / 1024.0);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 21, 2, "Всего данных:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        mvwprintw(results_win, 21, 18, "%.2f КБ", (double)config->results.total_bytes / 1024.0);
        
        int right_col = term_width / 2 + 5;
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
        mvwprintw(results_win, 12, right_col, "ВИЗУАЛИЗАЦИЯ");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_TITLE) | A_BOLD);
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 14, right_col, "Запр/сек:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        float max_rps = 100.0;
        draw_gauge(results_win, 14, right_col + 12, 30, 
                  config->results.requests_per_second, 
                  max_rps,
                  COLOR_PAIR(UI_COLOR_SUCCESS));
        
        wattron(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        mvwprintw(results_win, 16, right_col, "КБ/сек:");
        wattroff(results_win, COLOR_PAIR(UI_COLOR_ACCENT));
        
        float max_bps = 1024.0;
        draw_gauge(results_win, 16, right_col + 12, 30, 
                  config->results.bytes_per_second / 1024.0,
                  max_bps,
                  COLOR_PAIR(UI_COLOR_INFO));
        
        if (config->status == STATUS_RUNNING) {
            wattron(results_win, COLOR_PAIR(UI_COLOR_INFO) | A_BOLD);
            draw_centered_text(results_win, term_height - 10, 
                           "[ Идет нагрузочное тестирование... ]", 
                           COLOR_PAIR(UI_COLOR_INFO) | A_BOLD);
            wattroff(results_win, COLOR_PAIR(UI_COLOR_INFO) | A_BOLD);
        } else if (config->status == STATUS_FINISHED) {
            wattron(results_win, COLOR_PAIR(UI_COLOR_SUCCESS) | A_BOLD);
            draw_centered_text(results_win, term_height - 10, 
                           "[ Тестирование завершено успешно ]", 
                           COLOR_PAIR(UI_COLOR_SUCCESS) | A_BOLD);
            wattroff(results_win, COLOR_PAIR(UI_COLOR_SUCCESS) | A_BOLD);
        }
    } else {
        wattron(results_win, COLOR_PAIR(UI_COLOR_INFO) | A_BOLD);
        draw_centered_text(results_win, term_height / 2 - 2, 
                       "Нажмите 'S' для начала тестирования", 
                       COLOR_PAIR(UI_COLOR_INFO) | A_BOLD);
        wattroff(results_win, COLOR_PAIR(UI_COLOR_INFO) | A_BOLD);
    }
    
    wrefresh(results_win);
}

void update_ui(config_t *config) {
    int new_height, new_width;
    getmaxyx(stdscr, new_height, new_width);
    
    if (new_height != term_height || new_width != term_width) {
        term_height = new_height;
        term_width = new_width;
        
        delwin(header_win);
        delwin(status_win);
        delwin(results_win);
        delwin(menu_win);
        
        if (log_win) {
            delwin(log_win);
            log_win = NULL;
            show_logs = 0;
        }
        
        header_win = newwin(3, term_width, 0, 0);
        status_win = newwin(3, term_width, 3, 0);
        results_win = newwin(term_height - 8, term_width, 6, 0);
        menu_win = newwin(2, term_width, term_height - 2, 0);
        
        keypad(header_win, TRUE);
        keypad(status_win, TRUE);
        keypad(results_win, TRUE);
        keypad(menu_win, TRUE);
        
        clear();
        refresh();
    }
    
    draw_header();
    draw_status(config);
    draw_results(config);
    draw_menu();
    
    if (show_logs && log_win) {
        draw_logs();
    }
    
    int ch = getch();
    if (ch != ERR) {
        handle_keypress(ch, config);
    }
}

void show_http_method_selector(config_t *config) {
    int box_width = 50;
    int box_height = 15;
    int start_y = (term_height - box_height) / 2;
    int start_x = (term_width - box_width) / 2;
    
    WINDOW *method_win = newwin(box_height, box_width, start_y, start_x);
    wbkgd(method_win, COLOR_PAIR(UI_COLOR_HEADER_BG));
    draw_fancy_border(method_win);
    
    wattron(method_win, A_BOLD);
    mvwprintw(method_win, 0, (box_width - 20) / 2, " Выбор HTTP метода ");
    wattroff(method_win, A_BOLD);
    
    const char *methods[] = {
        "GET     - Получение данных",
        "POST    - Отправка данных на сервер",
        "PUT     - Обновление ресурса",
        "DELETE  - Удаление ресурса",
        "HEAD    - Только заголовки",
        "OPTIONS - Получение опций",
        "PATCH   - Частичное обновление"
    };
    
    int current_method = config->http_params.method;
    
    for (int i = 0; i < 7; i++) {
        if (i == current_method) {
            wattron(method_win, COLOR_PAIR(UI_COLOR_HIGHLIGHT));
        }
        mvwprintw(method_win, i + 2, 2, "%s", methods[i]);
        if (i == current_method) {
            wattroff(method_win, COLOR_PAIR(UI_COLOR_HIGHLIGHT));
        }
    }
    
    mvwprintw(method_win, box_height - 2, 2, "Используйте Вверх/Вниз для выбора, Enter для подтверждения");
    
    draw_shadow(method_win);
    wrefresh(method_win);
    
    keypad(method_win, TRUE);
    int ch;
    
    while (1) {
        ch = wgetch(method_win);
        
        if (ch == KEY_UP && current_method > 0) {
            mvwprintw(method_win, current_method + 2, 2, "%s", methods[current_method]);
            current_method--;
            wattron(method_win, COLOR_PAIR(UI_COLOR_HIGHLIGHT));
            mvwprintw(method_win, current_method + 2, 2, "%s", methods[current_method]);
            wattroff(method_win, COLOR_PAIR(UI_COLOR_HIGHLIGHT));
        } else if (ch == KEY_DOWN && current_method < 6) {
            mvwprintw(method_win, current_method + 2, 2, "%s", methods[current_method]);
            current_method++;
            wattron(method_win, COLOR_PAIR(UI_COLOR_HIGHLIGHT));
            mvwprintw(method_win, current_method + 2, 2, "%s", methods[current_method]);
            wattroff(method_win, COLOR_PAIR(UI_COLOR_HIGHLIGHT));
        } else if (ch == '\n' || ch == KEY_ENTER) {
            config->http_params.method = (http_method_t)current_method;
            break;
        } else if (ch == 27) {
            break;
        }
        
        wrefresh(method_win);
    }
    
    delwin(method_win);
    clear();
    refresh();
}

void show_data_input(config_t *config) {
    int box_width = 70;
    int box_height = 20;
    int start_y = (term_height - box_height) / 2;
    int start_x = (term_width - box_width) / 2;
    
    WINDOW *data_win = newwin(box_height, box_width, start_y, start_x);
    wbkgd(data_win, COLOR_PAIR(UI_COLOR_HEADER_BG));
    draw_fancy_border(data_win);
    
    wattron(data_win, A_BOLD);
    mvwprintw(data_win, 0, (box_width - 20) / 2, " Ввод данных запроса ");
    wattroff(data_win, A_BOLD);
    
    mvwprintw(data_win, 2, 2, "Метод: %s", 
             config->http_params.method == HTTP_POST ? "POST" :
             config->http_params.method == HTTP_PUT ? "PUT" : "PATCH");
    
    mvwprintw(data_win, 4, 2, "Content-Type:");
    
    char content_type[64] = "application/x-www-form-urlencoded";
    if (config->http_params.content_type) {
        strncpy(content_type, config->http_params.content_type, sizeof(content_type) - 1);
    }
    
    mvwprintw(data_win, 6, 2, "Данные запроса:");
    
    WINDOW *edit_win = derwin(data_win, 8, box_width - 4, 8, 2);
    wbkgd(edit_win, COLOR_PAIR(UI_COLOR_NORMAL));
    
    char data[1024] = "";
    if (config->http_params.post_data) {
        strncpy(data, config->http_params.post_data, sizeof(data) - 1);
    }
    
    mvwprintw(data_win, box_height - 2, 2, "Ctrl+D для завершения ввода, ESC для отмены");
    
    draw_shadow(data_win);
    wrefresh(data_win);
    wrefresh(edit_win);
    
    echo();
    curs_set(1);
    mvwgetnstr(data_win, 4, 16, content_type, sizeof(content_type) - 1);
    
    werase(edit_win);
    wmove(edit_win, 0, 0);
    wrefresh(edit_win);
    
    int ch, pos = 0;
    
    while ((ch = wgetch(edit_win)) != 4 && ch != 27) {
        if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                data[--pos] = '\0';
                werase(edit_win);
                mvwprintw(edit_win, 0, 0, "%s", data);
            }
        } else if (ch >= 32 && ch <= 126 && pos < sizeof(data) - 1) {
            data[pos++] = ch;
            data[pos] = '\0';
            werase(edit_win);
            mvwprintw(edit_win, 0, 0, "%s", data);
        }
        
        wrefresh(edit_win);
    }
    
    noecho();
    curs_set(0);
    
    if (ch != 27) {
        if (config->http_params.content_type) {
            free(config->http_params.content_type);
        }
        config->http_params.content_type = strdup(content_type);
        
        if (data[0] != '\0') {
            set_post_data(&config->http_params, data, strlen(data));
        }
    }
    
    delwin(edit_win);
    delwin(data_win);
    clear();
    refresh();
}

void manage_http_headers(config_t *config) {
    int box_width = 70;
    int box_height = 20;
    int start_y = (term_height - box_height) / 2;
    int start_x = (term_width - box_width) / 2;
    
    WINDOW *headers_win = newwin(box_height, box_width, start_y, start_x);
    wbkgd(headers_win, COLOR_PAIR(UI_COLOR_HEADER_BG));
    draw_fancy_border(headers_win);
    
    wattron(headers_win, A_BOLD);
    mvwprintw(headers_win, 0, (box_width - 20) / 2, " Управление заголовками ");
    wattroff(headers_win, A_BOLD);
    
    mvwprintw(headers_win, 2, 2, "Текущие заголовки:");
    
    int y = 4;
    http_header_t *curr = config->http_params.headers;
    while (curr && y < box_height - 6) {
        mvwprintw(headers_win, y++, 4, "%s: %s", curr->name, curr->value);
        curr = curr->next;
    }
    
    mvwprintw(headers_win, box_height - 5, 2, "1. Добавить заголовок");
    mvwprintw(headers_win, box_height - 4, 2, "2. Удалить заголовок");
    mvwprintw(headers_win, box_height - 3, 2, "3. Выход");
    
    draw_shadow(headers_win);
    wrefresh(headers_win);
    
    int ch;
    while (1) {
        ch = wgetch(headers_win);
        
        if (ch == '1') {
            werase(headers_win);
            draw_fancy_border(headers_win);
            
            wattron(headers_win, A_BOLD);
            mvwprintw(headers_win, 0, (box_width - 20) / 2, " Добавление заголовка ");
            wattroff(headers_win, A_BOLD);
            
            mvwprintw(headers_win, 2, 2, "Имя заголовка:");
            mvwprintw(headers_win, 4, 2, "Значение:");
            
            echo();
            curs_set(1);
            char name[64] = "";
            char value[256] = "";
            mvwgetnstr(headers_win, 2, 16, name, sizeof(name) - 1);
            mvwgetnstr(headers_win, 4, 16, value, sizeof(value) - 1);
            noecho();
            curs_set(0);
            
            if (name[0] != '\0' && value[0] != '\0') {
                add_http_header(&config->http_params, name, value);
            }
            
            werase(headers_win);
            draw_fancy_border(headers_win);
            
            wattron(headers_win, A_BOLD);
            mvwprintw(headers_win, 0, (box_width - 20) / 2, " Управление заголовками ");
            wattroff(headers_win, A_BOLD);
            
            mvwprintw(headers_win, 2, 2, "Текущие заголовки:");
            
            y = 4;
            curr = config->http_params.headers;
            while (curr && y < box_height - 6) {
                mvwprintw(headers_win, y++, 4, "%s: %s", curr->name, curr->value);
                curr = curr->next;
            }
            
            mvwprintw(headers_win, box_height - 5, 2, "1. Добавить заголовок");
            mvwprintw(headers_win, box_height - 4, 2, "2. Удалить заголовок");
            mvwprintw(headers_win, box_height - 3, 2, "3. Выход");
            
        } else if (ch == '2') {
            if (!config->http_params.headers) {
                continue;
            }
            
            werase(headers_win);
            draw_fancy_border(headers_win);
            
            wattron(headers_win, A_BOLD);
            mvwprintw(headers_win, 0, (box_width - 20) / 2, " Удаление заголовка ");
            wattroff(headers_win, A_BOLD);
            
            mvwprintw(headers_win, 2, 2, "Выберите заголовок для удаления:");
            
            int header_count = 0;
            curr = config->http_params.headers;
            while (curr) {
                mvwprintw(headers_win, 4 + header_count, 4, "%d. %s: %s", 
                         header_count + 1, curr->name, curr->value);
                header_count++;
                curr = curr->next;
            }
            
            mvwprintw(headers_win, 4 + header_count + 1, 2, "Введите номер заголовка или 0 для отмены:");
            
            echo();
            curs_set(1);
            char choice_str[8] = "";
            mvwgetnstr(headers_win, 4 + header_count + 1, 50, choice_str, sizeof(choice_str) - 1);
            noecho();
            curs_set(0);
            
            int choice = atoi(choice_str);
            if (choice > 0 && choice <= header_count) {
                if (choice == 1) {
                    http_header_t *to_remove = config->http_params.headers;
                    config->http_params.headers = to_remove->next;
                    free(to_remove->name);
                    free(to_remove->value);
                    free(to_remove);
                } else {
                    curr = config->http_params.headers;
                    for (int i = 1; i < choice - 1; i++) {
                        curr = curr->next;
                    }
                    http_header_t *to_remove = curr->next;
                    curr->next = to_remove->next;
                    free(to_remove->name);
                    free(to_remove->value);
                    free(to_remove);
                }
            }
            
            werase(headers_win);
            draw_fancy_border(headers_win);
            
            wattron(headers_win, A_BOLD);
            mvwprintw(headers_win, 0, (box_width - 20) / 2, " Управление заголовками ");
            wattroff(headers_win, A_BOLD);
            
            mvwprintw(headers_win, 2, 2, "Текущие заголовки:");
            
            y = 4;
            curr = config->http_params.headers;
            while (curr && y < box_height - 6) {
                mvwprintw(headers_win, y++, 4, "%s: %s", curr->name, curr->value);
                curr = curr->next;
            }
            
            mvwprintw(headers_win, box_height - 5, 2, "1. Добавить заголовок");
            mvwprintw(headers_win, box_height - 4, 2, "2. Удалить заголовок");
            mvwprintw(headers_win, box_height - 3, 2, "3. Выход");
            
        } else if (ch == '3' || ch == 27) {
            break;
        }
        
        wrefresh(headers_win);
    }
    
    delwin(headers_win);
    clear();
    refresh();
}

void perform_meow_test(config_t *config) {
    ui_log("[c0re} Запуск режима MEOW с последовательным использованием всех HTTP методов");
    
    const char *url = config->url;
    const char *data = config->http_params.post_data ? config->http_params.post_data : "meow=1";
    const char *content_type = config->http_params.content_type ? 
                              config->http_params.content_type : 
                              "application/x-www-form-urlencoded";
    
    ui_log("[c0re} MEOW: Выполнение GET запроса");
    response_t get_response = http_get(url);
    log_request(&get_response, 0);
    free_response(&get_response);
    
    ui_log("[c0re} MEOW: Выполнение POST запроса");
    response_t post_response = http_post(url, data, content_type);
    log_request(&post_response, 0);
    free_response(&post_response);
    
    ui_log("[c0re} MEOW: Выполнение PUT запроса");
    response_t put_response = http_put(url, data, content_type);
    log_request(&put_response, 0);
    free_response(&put_response);
    
    ui_log("[c0re} MEOW: Выполнение DELETE запроса");
    response_t delete_response = http_delete(url);
    log_request(&delete_response, 0);
    free_response(&delete_response);
    
    ui_log("[c0re} MEOW: Выполнение HEAD запроса");
    response_t head_response = http_head(url);
    log_request(&head_response, 0);
    free_response(&head_response);
    
    ui_log("[c0re} MEOW: Выполнение OPTIONS запроса");
    response_t options_response = http_options(url);
    log_request(&options_response, 0);
    free_response(&options_response);
    
    ui_log("[c0re} MEOW: Выполнение PATCH запроса");
    response_t patch_response = http_patch(url, data, content_type);
    log_request(&patch_response, 0);
    free_response(&patch_response);
    
    ui_log("[c0re} MEOW: Все запросы выполнены");
} 