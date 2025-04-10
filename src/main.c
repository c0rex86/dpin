/*
 *  /\_/\
 * ( o.o )
 *  > ^ <
 * 
 * meow
 * modded by c0re
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ncurses.h>
#include <pthread.h>
#include <time.h>
#include "dpin.h"
#include "ui.h"
#include "network.h"

static volatile int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT) {
        running = 0;
    }
}

void print_version(void) {
    printf("dpin v1.0.0\n");
    printf("Автор: c0re (c0re@valx.pw)\n");
}

void print_help(void) {
    printf("Использование: dpin [ОПЦИИ]\n\n");
    printf("Опции:\n");
    printf("  -h, --help           Показать эту справку\n");
    printf("  -v, --version        Показать версию\n");
    printf("  -u, --url URL        URL для тестирования\n");
    printf("  -c, --count N        Количество запросов (по умолчанию: 100)\n");
    printf("  -t, --threads N      Количество потоков (по умолчанию: 10)\n");
    printf("  --http               Использовать протокол HTTP (по умолчанию)\n");
    printf("  --https              Использовать протокол HTTPS\n");
    printf("  --tcp                Тестировать TCP соединение\n");
    printf("  --udp                Тестировать UDP соединение\n");
}

int main(int argc, char *argv[]) {
    config_t config = {
        .url = NULL,
        .requests = 100,
        .threads = 10,
        .type = TEST_HTTP,
        .status = STATUS_IDLE
    };
    
    init_http_params(&config.http_params);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--url") == 0) {
            if (i + 1 < argc) {
                config.url = argv[++i];
                printf("[c0re} URL установлен: %s\n", config.url);
            }
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--count") == 0) {
            if (i + 1 < argc) {
                config.requests = atoi(argv[++i]);
                printf("[c0re} Количество запросов: %d\n", config.requests);
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--threads") == 0) {
            if (i + 1 < argc) {
                config.threads = atoi(argv[++i]);
                printf("[c0re} Количество потоков: %d\n", config.threads);
            }
        } else if (strcmp(argv[i], "--http") == 0) {
            config.type = TEST_HTTP;
            printf("[c0re} Выбран протокол HTTP\n");
        } else if (strcmp(argv[i], "--https") == 0) {
            config.type = TEST_HTTPS;
            printf("[c0re} Выбран протокол HTTPS\n");
        } else if (strcmp(argv[i], "--tcp") == 0) {
            config.type = TEST_TCP;
            printf("[c0re} Выбрано тестирование TCP\n");
        } else if (strcmp(argv[i], "--udp") == 0) {
            config.type = TEST_UDP;
            printf("[c0re} Выбрано тестирование UDP\n");
        } else if (strcmp(argv[i], "--method") == 0) {
            if (i + 1 < argc) {
                const char *method = argv[++i];
                if (strcasecmp(method, "get") == 0) {
                    config.http_params.method = HTTP_GET;
                } else if (strcasecmp(method, "post") == 0) {
                    config.http_params.method = HTTP_POST;
                } else if (strcasecmp(method, "put") == 0) {
                    config.http_params.method = HTTP_PUT;
                } else if (strcasecmp(method, "delete") == 0) {
                    config.http_params.method = HTTP_DELETE;
                } else if (strcasecmp(method, "head") == 0) {
                    config.http_params.method = HTTP_HEAD;
                } else if (strcasecmp(method, "options") == 0) {
                    config.http_params.method = HTTP_OPTIONS;
                } else if (strcasecmp(method, "patch") == 0) {
                    config.http_params.method = HTTP_PATCH;
                }
                printf("[c0re} Выбран метод HTTP: %s\n", method);
            }
        } else if (strcmp(argv[i], "--header") == 0 || strcmp(argv[i], "-H") == 0) {
            if (i + 1 < argc) {
                const char *header = argv[++i];
                char *separator = strchr(header, ':');
                if (separator) {
                    *separator = '\0';
                    char *name = header;
                    char *value = separator + 1;
                    while (*value == ' ') value++;
                    
                    add_http_header(&config.http_params, name, value);
                    printf("[c0re} Добавлен заголовок: %s: %s\n", name, value);
                    
                    *separator = ':';
                }
            }
        } else if (strcmp(argv[i], "--data") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                const char *data = argv[++i];
                set_post_data(&config.http_params, data, strlen(data));
                printf("[c0re} Установлены данные для запроса\n");
            }
        } else if (strcmp(argv[i], "--content-type") == 0) {
            if (i + 1 < argc) {
                if (config.http_params.content_type) {
                    free(config.http_params.content_type);
                }
                config.http_params.content_type = strdup(argv[++i]);
                printf("[c0re} Установлен Content-Type: %s\n", config.http_params.content_type);
            }
        }
    }

    if (!config.url) {
        fprintf(stderr, "[c0re} Ошибка: URL не указан. Используйте -u или --url.\n");
        print_help();
        return 1;
    }

    signal(SIGINT, signal_handler);

    printf("[c0re} Инициализация интерфейса...\n");
    fflush(stdout);
    
    network_init();
    
    if (init_ui() != 0) {
        fprintf(stderr, "[c0re} Ошибка инициализации интерфейса\n");
        return 1;
    }
    
    ui_log("[c0re} Запуск интерфейса");
    
    while (running) {
        update_ui(&config);
        usleep(100000);
    }

    cleanup_ui();
    network_cleanup();
    
    free_http_params(&config.http_params);
    
    return 0;
} 