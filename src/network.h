/*
 *  /\_/\
 * ( o.o )
 *  > ^ <
 * 
 * meow
 * modded by c0re
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include "dpin.h"

#define MAX_RESPONSE_SIZE 4096

typedef struct {
    int status_code;
    double response_time;
    unsigned long body_length;
    char *body;
    char *headers;
    http_header_t *parsed_headers;
} response_t;

typedef struct {
    int total;
    int completed;
    int successful;
    int failed;
    double min_time;
    double max_time;
    double total_time;
} network_stats_t;

response_t http_request(const char *url);
response_t http_request_with_params(const char *url, const http_params_t *params);
response_t https_request(const char *url);
response_t https_request_with_params(const char *url, const http_params_t *params);

response_t tcp_test(const char *host, int port);
response_t udp_test(const char *host, int port);

response_t http_get(const char *url);
response_t http_post(const char *url, const char *post_data, const char *content_type);
response_t http_put(const char *url, const char *put_data, const char *content_type);
response_t http_delete(const char *url);
response_t http_delete_with_data(const char *url, const char *delete_data, const char *content_type);
response_t http_head(const char *url);
response_t http_options(const char *url);
response_t http_patch(const char *url, const char *patch_data, const char *content_type);

char *get_header_value(const response_t *response, const char *header_name);
void add_response_header(response_t *response, const char *name, const char *value);
void free_response(response_t *response);

void *worker_thread(void *arg);
char *parse_url(const char *url, char **host, int *port, char **path);
void log_request(response_t *response, int thread_id);

int start_test(config_t *config);
void stop_test(config_t *config);
void reset_results(config_t *config);

network_stats_t get_network_stats(void);
void start_network_test(config_t *config);

int network_init(void);
void network_cleanup(void);

response_t http_get(const char *url);
response_t http_post(const char *url, const char *post_data, const char *content_type);
response_t http_put(const char *url, const char *put_data, const char *content_type);
response_t http_delete(const char *url);
response_t http_delete_with_data(const char *url, const char *delete_data, const char *content_type);
response_t http_head(const char *url);
response_t http_options(const char *url);
response_t http_patch(const char *url, const char *patch_data, const char *content_type);

response_t http_request_with_headers(const char *url, const char *method, const char *data,
                                   const char *content_type, const char **headers, int header_count);

response_t https_get(const char *url);
response_t https_post(const char *url, const char *post_data, const char *content_type);
response_t https_put(const char *url, const char *put_data, const char *content_type);
response_t https_delete(const char *url, const char *delete_data, const char *content_type);
response_t https_head(const char *url, const char *data, const char *content_type);
response_t https_options(const char *url, const char *data, const char *content_type);
response_t https_patch(const char *url, const char *patch_data, const char *content_type);

response_t curl_http_request_full(const char *url, const char *method, const char *data, 
                                const char *content_type, struct curl_slist *custom_headers);

char *get_json_response(const response_t *response);

#endif /* NETWORK_H */