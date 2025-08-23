#include "../../include/proxy/http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PHASE 6: HTTP Parser Implementation
// Note: This is a simplified implementation for modular architecture demonstration

struct ParsedRequest* ParsedRequest_create(void) {
    return (struct ParsedRequest*)calloc(1, sizeof(struct ParsedRequest));
}

void ParsedRequest_destroy(struct ParsedRequest *pr) {
    if (pr) {
        // Free allocated memory for parsed request
        if (pr->buf) free(pr->buf);
        if (pr->method) free(pr->method);
        if (pr->protocol) free(pr->protocol);
        if (pr->host) free(pr->host);
        if (pr->port) free(pr->port);
        if (pr->path) free(pr->path);
        if (pr->version) free(pr->version);
        free(pr);
    }
}

int ParsedRequest_parse(struct ParsedRequest *parse, const char *buf, int buflen) {
    // Simplified parser for demonstration - extracts basic HTTP request components
    if (!parse || !buf || buflen <= 0) {
        return -1;
    }
    
    // Store original buffer
    parse->buf = malloc(buflen + 1);
    if (!parse->buf) return -1;
    
    memcpy(parse->buf, buf, buflen);
    parse->buf[buflen] = '\0';
    parse->buflen = buflen;
    
    // Basic parsing - extract method, path, version
    char* line = strtok(parse->buf, "\r\n");
    if (!line) return -1;
    
    char* method = strtok(line, " ");
    char* path = strtok(NULL, " ");
    char* version = strtok(NULL, " ");
    
    if (method) {
        parse->method = malloc(strlen(method) + 1);
        strcpy(parse->method, method);
    }
    
    if (path) {
        parse->path = malloc(strlen(path) + 1);
        strcpy(parse->path, path);
    }
    
    if (version) {
        parse->version = malloc(strlen(version) + 1);
        strcpy(parse->version, version);
    }
    
    return 0;
}

int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen) {
    // Simplified implementation
    if (!pr || !buf) return -1;
    
    snprintf(buf, buflen, "%s %s %s\r\n\r\n", 
             pr->method ? pr->method : "GET",
             pr->path ? pr->path : "/",
             pr->version ? pr->version : "HTTP/1.1");
    
    return 0;
}

int ParsedHeader_set(struct ParsedRequest *pr, const char *key, const char *value) {
    // Simplified implementation - would need proper header linked list
    return 0;
}

struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char *key) {
    // Simplified implementation - would need proper header lookup
    return NULL;
}

int validate_http_request(const char* request, int length) {
    if (!request || length <= 0) {
        return 0; // Invalid
    }
    
    // Basic HTTP request validation
    // Must start with a valid method
    if (strncmp(request, "GET ", 4) == 0 ||
        strncmp(request, "POST ", 5) == 0 ||
        strncmp(request, "PUT ", 4) == 0 ||
        strncmp(request, "DELETE ", 7) == 0 ||
        strncmp(request, "HEAD ", 5) == 0) {
        return 1; // Valid
    }
    
    return 0; // Invalid method
}

int extract_host_port(const char* host_header, char* host, int* port) {
    if (!host_header || !host || !port) {
        return -1;
    }
    
    // Default HTTP port
    *port = 80;
    
    // Look for port separator
    char* port_separator = strchr(host_header, ':');
    if (port_separator) {
        // Copy host part
        int host_len = port_separator - host_header;
        strncpy(host, host_header, host_len);
        host[host_len] = '\0';
        
        // Extract port
        *port = atoi(port_separator + 1);
        if (*port <= 0 || *port > 65535) {
            *port = 80; // Reset to default for invalid ports
        }
    } else {
        // No port specified, copy entire host
        strcpy(host, host_header);
    }
    
    return 0;
}
