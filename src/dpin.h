/*
 *  /\_/\
 * ( o.o )
 *  > ^ <
 * 
 * meow
 * modded by c0re
 */

#ifndef DPIN_H
#define DPIN_H


typedef enum {
    TEST_HTTP,
    TEST_HTTPS,
    TEST_TCP,
    TEST_UDP,
    TEST_MEOW  
} test_type_t;

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_HEAD,
    HTTP_OPTIONS,
    HTTP_PATCH
} http_method_t;

// Статус теста
typedef enum {
    STATUS_IDLE,
    STATUS_RUNNING,
    STATUS_FINISHED,
    STATUS_ERROR,
    STATUS_COMPLETE
} test_status_t;

// Результаты теста
typedef struct {
    int total_requests;
    int completed_requests;
    int failed_requests;
    double min_time;
    double max_time;
    double avg_time;
    double total_time;
    double requests_per_second;
    double bytes_per_second;
    unsigned long total_bytes;
} result_t;

// хуууууууй
typedef struct http_header {
    char *name;
    char *value;
    struct http_header *next;
} http_header_t;


typedef struct {
    http_method_t method;
    char *post_data;
    size_t post_data_size;
    http_header_t *headers;
    char *content_type;
    int follow_redirects;
    int timeout;
    char *auth_username;
    char *auth_password;
} http_params_t;


typedef struct {
    char *url;
    int requests;
    int threads;
    test_type_t type;
    test_status_t status;
    http_params_t http_params;
    result_t results;
} config_t;


typedef struct {
    int thread_id;
    config_t *config;
} thread_args_t;


int start_test(config_t *config);
void stop_test(config_t *config);
void reset_results(config_t *config);


void init_http_params(http_params_t *params);
void add_http_header(http_params_t *params, const char *name, const char *value);
void set_post_data(http_params_t *params, const char *data, size_t size);
void free_http_params(http_params_t *params);

#endif /* DPIN_H */ 