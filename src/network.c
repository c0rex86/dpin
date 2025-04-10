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
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <curl/curl.h>
#include <ncurses.h>
#include "network.h"
#include "dpin.h"
#include "ui.h"

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

#ifndef STATUS_COMPLETE
#define STATUS_COMPLETE STATUS_FINISHED
#endif

static pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;

static volatile int test_running = 0;

static pthread_t *worker_threads = NULL;
static config_t *current_config = NULL;

static SSL_CTX *ssl_ctx = NULL;
static int ssl_initialized = 0;

static network_stats_t stats;
static pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_COLOR_RED "\033[31m"
#define ANSI_COLOR_ORANGE "\033[38;5;208m"
#define ANSI_COLOR_RESET "\033[0m"

static int init_openssl(void) {
    if (ssl_initialized) {
        return 0;
    }
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ssl_ctx) {
        ui_log("[c0re} Ошибка создания контекста SSL");
        return -1;
    }
    
    SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    
    if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
        ui_log("[c0re} Ошибка загрузки корневых сертификатов");
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
        return -2;
    }
    
    ssl_initialized = 1;
    ui_log("[c0re} OpenSSL инициализирован");
    return 0;
}

static void cleanup_openssl(void) {
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
    
    ERR_free_strings();
    EVP_cleanup();
    
    ssl_initialized = 0;
    ui_log("[c0re} OpenSSL освобожден");
}

static double time_diff(struct timespec *start, struct timespec *end) {
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

response_t http_request(const char *url) {
    response_t response = {0};
    
    struct timespec start_time, end_time;
    char *host = NULL;
    int port = 80;
    char *path = NULL;
    
    char *protocol = parse_url(url, &host, &port, &path);
    if (!protocol || !host || !path) {
        response.status_code = -1;
        return response;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        free(host);
        free(path);
        response.status_code = -2;
        return response;
    }
    
    struct hostent *server = gethostbyname(host);
    if (!server) {
        close(sock);
        free(host);
        free(path);
        response.status_code = -3;
        return response;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        free(host);
        free(path);
        response.status_code = -4;
        return response;
    }
    
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: dpin/1.0\r\n"
             "Connection: close\r\n\r\n",
             path, host);
    
    ui_log("[c0re} Отправка HTTP запроса на %s:%d%s", host, port, path);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (write(sock, request, strlen(request)) < 0) {
        close(sock);
        free(host);
        free(path);
        response.status_code = -5;
        return response;
    }
    
    char buffer[MAX_RESPONSE_SIZE];
    ssize_t n;
    size_t total_bytes = 0;
    int status_parsed = 0;
    
    while ((n = read(sock, buffer + total_bytes, MAX_RESPONSE_SIZE - total_bytes - 1)) > 0) {
        total_bytes += n;
        buffer[total_bytes] = '\0';
        
        if (!status_parsed && strstr(buffer, "\r\n")) {
            if (strncmp(buffer, "HTTP/1.", 7) == 0) {
                char *status_start = strchr(buffer, ' ') + 1;
                response.status_code = atoi(status_start);
                status_parsed = 1;
            }
        }
        
        if (total_bytes >= MAX_RESPONSE_SIZE - 1) {
            break;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    close(sock);
    
    response.response_time = time_diff(&start_time, &end_time);
    
    if (total_bytes > 0) {
        char *headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end) {
            size_t headers_size = headers_end - buffer + 4;
            response.headers = (char *)malloc(headers_size + 1);
            if (response.headers) {
                memcpy(response.headers, buffer, headers_size);
                response.headers[headers_size] = '\0';
            }
            
            size_t body_size = total_bytes - headers_size;
            response.body = (char *)malloc(body_size + 1);
            if (response.body) {
                memcpy(response.body, headers_end + 4, body_size);
                response.body[body_size] = '\0';
                response.body_length = body_size;
            }
        }
    }
    
    free(host);
    free(path);
    
    return response;
}

response_t https_request(const char *url) {
    response_t response = {0};
    struct timespec start_time, end_time;
    
    char *host = NULL;
    int port = 443;
    char *path = NULL;
    
    char *protocol = parse_url(url, &host, &port, &path);
    if (!protocol || !host || !path) {
        response.status_code = -1;
        return response;
    }
    
    if (!ssl_initialized && init_openssl() != 0) {
        free(host);
        free(path);
        response.status_code = -2;
        return response;
    }
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        free(host);
        free(path);
        response.status_code = -3;
        return response;
    }
    
    struct hostent *server = gethostbyname(host);
    if (!server) {
        close(sock);
        free(host);
        free(path);
        response.status_code = -4;
        return response;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(port);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        free(host);
        free(path);
        response.status_code = -5;
        return response;
    }
    
    ui_log("[c0re} Отправка HTTPS запроса на %s:%d%s", host, port, path);
    
    SSL *ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        close(sock);
        free(host);
        free(path);
        response.status_code = -6;
        return response;
    }
    
    if (SSL_set_fd(ssl, sock) != 1) {
        SSL_free(ssl);
        close(sock);
        free(host);
        free(path);
        response.status_code = -7;
        return response;
    }
    
    SSL_set_tlsext_host_name(ssl, host);
    
    if (SSL_connect(ssl) != 1) {
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        ui_log("[c0re} Ошибка SSL соединения: %s", err_buf);
        
        SSL_free(ssl);
        close(sock);
        free(host);
        free(path);
        response.status_code = -8;
        return response;
    }
    
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: dpin/1.0\r\n"
             "Connection: close\r\n\r\n",
             path, host);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (SSL_write(ssl, request, strlen(request)) < 0) {
        SSL_free(ssl);
        close(sock);
        free(host);
        free(path);
        response.status_code = -9;
        return response;
    }
    
    char buffer[MAX_RESPONSE_SIZE];
    int n;
    size_t total_bytes = 0;
    int status_parsed = 0;
    
    while ((n = SSL_read(ssl, buffer + total_bytes, MAX_RESPONSE_SIZE - total_bytes - 1)) > 0) {
        total_bytes += n;
        buffer[total_bytes] = '\0';
        
        if (!status_parsed && strstr(buffer, "\r\n")) {
            char *status_line = strtok(buffer, "\r\n");
            if (status_line) {
                char proto[10];
                if (sscanf(status_line, "%s %d", proto, &response.status_code) != 2) {
                    response.status_code = -10;
                }
                status_parsed = 1;
            }
        }
        
        if (total_bytes >= MAX_RESPONSE_SIZE - 1) {
            break;
        }
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    response.response_time = time_diff(&start_time, &end_time);
    response.body_length = total_bytes;
    
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    free(host);
    free(path);
    
    return response;
}

response_t tcp_test(const char *host, int port) {
    response_t response = {0};
    struct timespec start_time, end_time;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        response.status_code = -1;
        return response;
    }
    
    struct hostent *server = gethostbyname(host);
    if (!server) {
        close(sock);
        response.status_code = -2;
        return response;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(port);
    
    ui_log("[c0re} Тестирование TCP соединения с %s:%d", host, port);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        response.status_code = -3;
        return response;
    }
    
    char test_data[] = "DPIN TCP TEST";
    if (write(sock, test_data, sizeof(test_data)) < 0) {
        close(sock);
        response.status_code = -4;
        return response;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    response.response_time = time_diff(&start_time, &end_time);
    response.body_length = sizeof(test_data);
    response.status_code = 0;
    
    close(sock);
    return response;
}

response_t udp_test(const char *host, int port) {
    response_t response = {0};
    struct timespec start_time, end_time;
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        response.status_code = -1;
        return response;
    }
    
    struct hostent *server = gethostbyname(host);
    if (!server) {
        close(sock);
        response.status_code = -2;
        return response;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(port);
    
    ui_log("[c0re} Тестирование UDP соединения с %s:%d", host, port);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    char test_data[] = "DPIN UDP TEST";
    if (sendto(sock, test_data, sizeof(test_data), 0, 
               (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        response.status_code = -3;
        return response;
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    response.response_time = time_diff(&start_time, &end_time);
    response.body_length = sizeof(test_data);
    response.status_code = 0;
    
    close(sock);
    return response;
}

char *parse_url(const char *url, char **host, int *port, char **path) {
    if (!url || !host || !port || !path) {
        return NULL;
    }
    
    *host = calloc(256, sizeof(char));
    *path = calloc(1024, sizeof(char));
    
    if (!*host || !*path) {
        if (*host) free(*host);
        if (*path) free(*path);
        return NULL;
    }
    
    char *protocol;
    if (strncmp(url, "http://", 7) == 0) {
        protocol = "http";
        url += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        protocol = "https";
        url += 8;
        *port = 443;
    } else {
        protocol = "http";
    }
    
    const char *host_end = strchr(url, '/');
    if (!host_end) {
        strcpy(*path, "/");
        strcpy(*host, url);
    } else {
        size_t host_len = host_end - url;
        strncpy(*host, url, host_len);
        (*host)[host_len] = '\0';
        
        strcpy(*path, host_end);
    }
    
    char *port_str = strchr(*host, ':');
    if (port_str) {
        *port_str = '\0';
        port_str++;
        *port = atoi(port_str);
    }
    
    if (strlen(*path) == 0) {
        strcpy(*path, "/");
    }
    
    return protocol;
}

void log_request(response_t *response, int thread_id) {
    if (!response) return;
    
    char status_color[20] = "";
    if (response->status_code >= 200 && response->status_code < 300) {
        strcpy(status_color, ANSI_COLOR_GREEN);
    } else if (response->status_code >= 300 && response->status_code < 400) {
        strcpy(status_color, ANSI_COLOR_YELLOW);
    } else if (response->status_code >= 400 && response->status_code < 500) {
        strcpy(status_color, ANSI_COLOR_ORANGE);
    } else if (response->status_code >= 500) {
        strcpy(status_color, ANSI_COLOR_RED);
    } else {
        strcpy(status_color, ANSI_COLOR_RED);
    }
    
    ui_log("[c0re} Поток %d: код %s%d%s, время %.3f сек, размер %lu байт",
           thread_id, status_color, response->status_code, ANSI_COLOR_RESET,
           response->response_time, response->body_length);
}

void *monitor_thread_func(void *arg) {
    config_t *cfg = (config_t *)arg;
    
    for (int i = 0; i < cfg->threads; i++) {
        pthread_join(worker_threads[i], NULL);
    }
    
    if (cfg->results.completed_requests > 0) {
        cfg->results.avg_time = cfg->results.total_time / cfg->results.completed_requests;
        cfg->results.requests_per_second = cfg->results.completed_requests / cfg->results.total_time;
        cfg->results.bytes_per_second = cfg->results.total_bytes / cfg->results.total_time;
    }
    
    if (test_running) {
        cfg->status = STATUS_FINISHED;
    }
    
    test_running = 0;
    
    free(worker_threads);
    worker_threads = NULL;
    
    pthread_exit(NULL);
}

void *worker_thread(void *arg) {
    thread_args_t *thread_args = (thread_args_t *)arg;
    int thread_id = thread_args->thread_id;
    config_t *config = thread_args->config;
    
    double min_time = -1;
    double max_time = 0;
    double total_time = 0;
    int completed = 0;
    int failed = 0;
    unsigned long total_bytes = 0;
    
    int requests_per_thread = config->requests / config->threads;
    if (thread_id < config->requests % config->threads) {
        requests_per_thread++;
    }
    
    for (int i = 0; i < requests_per_thread && test_running; i++) {
        response_t response = {0};
        
        switch (config->type) {
            case TEST_HTTP:
                switch (config->http_params.method) {
                    case HTTP_GET:
                        response = http_get(config->url);
                        break;
                    case HTTP_POST:
                        response = http_post(config->url, 
                                           config->http_params.post_data, 
                                           config->http_params.content_type);
                        break;
                    case HTTP_PUT:
                        response = http_put(config->url, 
                                          config->http_params.post_data, 
                                          config->http_params.content_type);
                        break;
                    case HTTP_DELETE:
                        if (config->http_params.post_data) {
                            response = http_delete_with_data(config->url, 
                                                          config->http_params.post_data,
                                                          config->http_params.content_type);
                        } else {
                            response = http_delete(config->url);
                        }
                        break;
                    case HTTP_HEAD:
                        response = http_head(config->url);
                        break;
                    case HTTP_OPTIONS:
                        response = http_options(config->url);
                        break;
                    case HTTP_PATCH:
                        response = http_patch(config->url, 
                                            config->http_params.post_data,
                                            config->http_params.content_type);
                        break;
                    default:
                        response = http_request(config->url);
                        break;
                }
                break;
            case TEST_HTTPS:
                switch (config->http_params.method) {
                    case HTTP_GET:
                        response = https_get(config->url);
                        break;
                    case HTTP_POST:
                        response = https_post(config->url, 
                                            config->http_params.post_data, 
                                            config->http_params.content_type);
                        break;
                    case HTTP_PUT:
                        response = https_put(config->url, 
                                           config->http_params.post_data, 
                                           config->http_params.content_type);
                        break;
                    case HTTP_DELETE:
                        response = https_delete(config->url, 
                                              config->http_params.post_data,
                                              config->http_params.content_type);
                        break;
                    case HTTP_HEAD:
                        response = https_head(config->url, NULL, NULL);
                        break;
                    case HTTP_OPTIONS:
                        response = https_options(config->url, NULL, NULL);
                        break;
                    case HTTP_PATCH:
                        response = https_patch(config->url, 
                                             config->http_params.post_data,
                                             config->http_params.content_type);
                        break;
                    default:
                        response = https_request(config->url);
                        break;
                }
                break;
            case TEST_TCP:
                {
                    char *host = NULL;
                    int port = 80;
                    char *path = NULL;
                    parse_url(config->url, &host, &port, &path);
                    if (host) {
                        response = tcp_test(host, port);
                        free(host);
                        free(path);
                    } else {
                        response.status_code = -100;
                    }
                }
                break;
            case TEST_UDP:
                {
                    char *host = NULL;
                    int port = 80;
                    char *path = NULL;
                    parse_url(config->url, &host, &port, &path);
                    if (host) {
                        response = udp_test(host, port);
                        free(host);
                        free(path);
                    } else {
                        response.status_code = -100;
                    }
                }
                break;
            default:
                ui_log("[c0re} Неизвестный тип теста");
                response.status_code = -999;
                break;
        }
        
        if (response.status_code >= 0) {
            completed++;
            if (min_time < 0 || response.response_time < min_time) {
                min_time = response.response_time;
            }
            if (response.response_time > max_time) {
                max_time = response.response_time;
            }
            total_time += response.response_time;
            total_bytes += response.body_length;
        } else {
            failed++;
        }
        
        log_request(&response, thread_id);
        
        usleep(10000);
    }
    
    pthread_mutex_lock(&results_mutex);
    
    config->results.completed_requests += completed;
    config->results.failed_requests += failed;
    config->results.total_bytes += total_bytes;
    
    if (min_time > 0 && (config->results.min_time <= 0 || min_time < config->results.min_time)) {
        config->results.min_time = min_time;
    }
    
    if (max_time > config->results.max_time) {
        config->results.max_time = max_time;
    }
    
    config->results.total_time = total_time > config->results.total_time ? 
                                 total_time : config->results.total_time;
    
    pthread_mutex_unlock(&results_mutex);
    
    pthread_exit(NULL);
}

int start_test(config_t *config) {
    if (!config || !config->url || config->threads <= 0 || config->requests <= 0) {
        return -1;
    }
    
    if (config->type == TEST_MEOW) {
        ui_log("[c0re} Запуск тестирования в режиме MEOW");
        config->status = STATUS_RUNNING;
        perform_meow_test(config);
        config->status = STATUS_FINISHED;
        return 0;
    }
    
    if (test_running) {
        stop_test(config);
    }
    
    reset_results(config);
    config->results.total_requests = config->requests;
    config->status = STATUS_RUNNING;
    test_running = 1;
    
    current_config = config;
    
    worker_threads = (pthread_t *)malloc(config->threads * sizeof(pthread_t));
    if (!worker_threads) {
        test_running = 0;
        config->status = STATUS_ERROR;
        return -2;
    }
    
    thread_args_t *thread_args = (thread_args_t *)malloc(config->threads * sizeof(thread_args_t));
    if (!thread_args) {
        free(worker_threads);
        worker_threads = NULL;
        test_running = 0;
        config->status = STATUS_ERROR;
        return -3;
    }
    
    for (int i = 0; i < config->threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].config = config;
        
        if (pthread_create(&worker_threads[i], NULL, worker_thread, &thread_args[i]) != 0) {
            for (int j = 0; j < i; j++) {
                pthread_cancel(worker_threads[j]);
            }
            free(worker_threads);
            free(thread_args);
            worker_threads = NULL;
            test_running = 0;
            config->status = STATUS_ERROR;
            return -4;
        }
    }
    
    pthread_t monitor_thread;
    
    if (pthread_create(&monitor_thread, NULL, monitor_thread_func, config) != 0) {
        for (int i = 0; i < config->threads; i++) {
            pthread_cancel(worker_threads[i]);
        }
        free(worker_threads);
        free(thread_args);
        worker_threads = NULL;
        test_running = 0;
        config->status = STATUS_ERROR;
        return -5;
    }
    
    pthread_detach(monitor_thread);
    
    free(thread_args);
    
    return 0;
}

void stop_test(config_t *config) {
    test_running = 0;
    
    if (config) {
        if (config->status == STATUS_RUNNING) {
            config->status = STATUS_FINISHED;
        }
    }
    
    usleep(100000);
}

void reset_results(config_t *config) {
    if (!config) return;
    
    config->results.total_requests = 0;
    config->results.completed_requests = 0;
    config->results.failed_requests = 0;
    config->results.min_time = 0.0;
    config->results.max_time = 0.0;
    config->results.avg_time = 0.0;
    config->results.total_time = 0.0;
    config->results.requests_per_second = 0.0;
    config->results.bytes_per_second = 0.0;
    config->results.total_bytes = 0;
    
    config->status = STATUS_IDLE;
    
    ui_log("[c0re} Результаты сброшены");
}

void network_cleanup(void) {
    if (ssl_initialized) {
        cleanup_openssl();
    }
    
    curl_global_cleanup();
    
    ui_log("[c0re} Сетевая подсистема освобождена");
}

typedef struct {
    char *data;
    size_t size;
    char *headers;
    size_t headers_size;
} curl_response_data_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    curl_response_data_t *resp = (curl_response_data_t *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) {
        ui_log("[c0re} Ошибка выделения памяти для ответа");
        return 0;
    }
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = '\0';
    
    return realsize;
}

static size_t header_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    curl_response_data_t *resp = (curl_response_data_t *)userp;
    
    char *ptr = realloc(resp->headers, resp->headers_size + realsize + 1);
    if (!ptr) {
        ui_log("[c0re} Ошибка выделения памяти для заголовков");
        return 0;
    }
    
    resp->headers = ptr;
    memcpy(&(resp->headers[resp->headers_size]), contents, realsize);
    resp->headers_size += realsize;
    resp->headers[resp->headers_size] = '\0';
    
    return realsize;
}

response_t curl_http_request_full(const char *url, const char *method, const char *data, 
                                const char *content_type, struct curl_slist *custom_headers) {
    response_t response = {0};
    CURL *curl;
    CURLcode res;
    long response_code = 0;
    struct timespec start_time, end_time;
    curl_response_data_t resp_data = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        ui_log("[c0re} Ошибка инициализации CURL");
        response.status_code = -1;
        return response;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "dpin/1.0");
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp_data);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&resp_data);
    
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (strcmp(method, "GET") != 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    }
    
    if (data && (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0 || 
                strcmp(method, "PATCH") == 0 || strcmp(method, "DELETE") == 0)) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
    }
    
    struct curl_slist *headers = NULL;
    
    if (content_type && strlen(content_type) > 0) {
        char content_type_header[256];
        snprintf(content_type_header, sizeof(content_type_header), "Content-Type: %s", content_type);
        headers = curl_slist_append(headers, content_type_header);
    }
    
    if (custom_headers) {
        struct curl_slist *temp = custom_headers;
        while (temp) {
            headers = curl_slist_append(headers, temp->data);
            temp = temp->next;
        }
    }
    
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    ui_log("[c0re} Отправка %s запроса на %s", method, url);
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    res = curl_easy_perform(curl);
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    if (res != CURLE_OK) {
        ui_log("[c0re} CURL ошибка: %s", curl_easy_strerror(res));
        response.status_code = -2;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        response.status_code = (int)response_code;
        
        double content_length;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
        
        if (resp_data.data && resp_data.size > 0) {
            response.body = (char *)malloc(resp_data.size + 1);
            if (response.body) {
                memcpy(response.body, resp_data.data, resp_data.size);
                response.body[resp_data.size] = '\0';
                response.body_length = resp_data.size;
            }
        }
        
        if (resp_data.headers && resp_data.headers_size > 0) {
            response.headers = (char *)malloc(resp_data.headers_size + 1);
            if (response.headers) {
                memcpy(response.headers, resp_data.headers, resp_data.headers_size);
                response.headers[resp_data.headers_size] = '\0';
            }
        }
        
        response.response_time = time_diff(&start_time, &end_time);
    }
    
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    if (resp_data.data) {
        free(resp_data.data);
    }
    
    if (resp_data.headers) {
        free(resp_data.headers);
    }
    
    curl_easy_cleanup(curl);
    
    return response;
}

response_t http_get(const char *url) {
    return curl_http_request_full(url, "GET", NULL, NULL, NULL);
}

response_t http_post(const char *url, const char *post_data, const char *content_type) {
    return curl_http_request_full(url, "POST", post_data, content_type, NULL);
}

response_t http_put(const char *url, const char *put_data, const char *content_type) {
    return curl_http_request_full(url, "PUT", put_data, content_type, NULL);
}

response_t http_delete(const char *url) {
    return curl_http_request_full(url, "DELETE", NULL, NULL, NULL);
}

response_t http_delete_with_data(const char *url, const char *delete_data, const char *content_type) {
    return curl_http_request_full(url, "DELETE", delete_data, content_type, NULL);
}

response_t http_head(const char *url) {
    return curl_http_request_full(url, "HEAD", NULL, NULL, NULL);
}

response_t http_options(const char *url) {
    return curl_http_request_full(url, "OPTIONS", NULL, NULL, NULL);
}

response_t http_patch(const char *url, const char *patch_data, const char *content_type) {
    return curl_http_request_full(url, "PATCH", patch_data, content_type, NULL);
}

response_t http_request_with_headers(const char *url, const char *method, const char *data,
                                    const char *content_type, const char **headers, int header_count) {
    struct curl_slist *custom_headers = NULL;
    
    if (headers && header_count > 0) {
        for (int i = 0; i < header_count; i++) {
            custom_headers = curl_slist_append(custom_headers, headers[i]);
        }
    }
    
    response_t response = curl_http_request_full(url, method, data, content_type, custom_headers);
    
    if (custom_headers) {
        curl_slist_free_all(custom_headers);
    }
    
    return response;
}

response_t https_get(const char *url) {
    return http_get(url);
}

response_t https_post(const char *url, const char *post_data, const char *content_type) {
    return http_post(url, post_data, content_type);
}

response_t https_put(const char *url, const char *put_data, const char *content_type) {
    return http_put(url, put_data, content_type);
}

response_t https_delete(const char *url, const char *delete_data, const char *content_type) {
    return http_delete_with_data(url, delete_data, content_type);
}

response_t https_head(const char *url, const char *data, const char *content_type) {
    return http_head(url);
}

response_t https_options(const char *url, const char *data, const char *content_type) {
    return http_options(url);
}

response_t https_patch(const char *url, const char *patch_data, const char *content_type) {
    return http_patch(url, patch_data, content_type);
}

char *get_json_response(const response_t *response) {
    if (!response || !response->body || response->body_length <= 0) {
        return NULL;
    }
    
    if (response->headers) {
        char *content_type = strcasestr(response->headers, "Content-Type:");
        if (content_type && strcasestr(content_type, "application/json")) {
            char *json = (char *)malloc(response->body_length + 1);
            if (json) {
                memcpy(json, response->body, response->body_length);
                json[response->body_length] = '\0';
                return json;
            }
        }
    }
    
    if (response->body[0] == '{' || response->body[0] == '[') {
        char *json = (char *)malloc(response->body_length + 1);
        if (json) {
            memcpy(json, response->body, response->body_length);
            json[response->body_length] = '\0';
            return json;
        }
    }
    
    return NULL;
}

void add_response_header(response_t *response, const char *name, const char *value) {
    if (!response) return;
    
    http_header_t *header = (http_header_t *)malloc(sizeof(http_header_t));
    if (!header) return;
    
    header->name = strdup(name);
    header->value = strdup(value);
    header->next = NULL;
    
    if (!response->parsed_headers) {
        response->parsed_headers = header;
    } else {
        http_header_t *last = response->parsed_headers;
        while (last->next) {
            last = last->next;
        }
        last->next = header;
    }
}

char *get_header_value(const response_t *response, const char *header_name) {
    if (!response || !response->parsed_headers || !header_name) {
        return NULL;
    }
    
    http_header_t *header = response->parsed_headers;
    while (header) {
        if (strcasecmp(header->name, header_name) == 0) {
            return header->value;
        }
        header = header->next;
    }
    
    return NULL;
}

void free_response(response_t *response) {
    if (!response) return;
    
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }
    
    if (response->headers) {
        free(response->headers);
        response->headers = NULL;
    }
    
    http_header_t *header = response->parsed_headers;
    while (header) {
        http_header_t *next = header->next;
        if (header->name) free(header->name);
        if (header->value) free(header->value);
        free(header);
        header = next;
    }
    response->parsed_headers = NULL;
}

int network_init(void) {
    memset(&stats, 0, sizeof(stats));
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    if (init_openssl() != 0) {
        ui_log("[c0re} Ошибка инициализации OpenSSL");
        return -1;
    }
    
    ui_log("[c0re} Сетевая подсистема инициализирована");
    return 0;
}