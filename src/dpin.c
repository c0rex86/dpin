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
#include "dpin.h"

void init_http_params(http_params_t *params) {
    if (!params) return;
    
    params->method = HTTP_GET;
    params->post_data = NULL;
    params->post_data_size = 0;
    params->headers = NULL;
    params->content_type = NULL;
    params->follow_redirects = 1;
    params->timeout = 30;
    params->auth_username = NULL;
    params->auth_password = NULL;
}

void add_http_header(http_params_t *params, const char *name, const char *value) {
    if (!params || !name || !value) return;
    
    http_header_t *header = (http_header_t *)malloc(sizeof(http_header_t));
    if (!header) return;
    
    header->name = strdup(name);
    header->value = strdup(value);
    header->next = NULL;
    
    if (!params->headers) {
        params->headers = header;
    } else {
        http_header_t *last = params->headers;
        while (last->next) {
            last = last->next;
        }
        last->next = header;
    }
}

void set_post_data(http_params_t *params, const char *data, size_t size) {
    if (!params || !data) return;
    
    if (params->post_data) {
        free(params->post_data);
    }
    
    params->post_data = (char *)malloc(size + 1);
    if (params->post_data) {
        memcpy(params->post_data, data, size);
        params->post_data[size] = '\0';
        params->post_data_size = size;
    }
}

void free_http_params(http_params_t *params) {
    if (!params) return;
    
    if (params->post_data) {
        free(params->post_data);
        params->post_data = NULL;
    }
    
    http_header_t *header = params->headers;
    while (header) {
        http_header_t *next = header->next;
        if (header->name) free(header->name);
        if (header->value) free(header->value);
        free(header);
        header = next;
    }
    params->headers = NULL;
    
    if (params->content_type) {
        free(params->content_type);
        params->content_type = NULL;
    }
    
    if (params->auth_username) {
        free(params->auth_username);
        params->auth_username = NULL;
    }
    
    if (params->auth_password) {
        free(params->auth_password);
        params->auth_password = NULL;
    }
}

/*
 *  /\_/\
 * ( o.o )
 *  > ^ <
 * 
 * meow
 * modded by c0re
 */