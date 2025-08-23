#include "../../include/proxy/http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Windows compatibility for strcasecmp
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

// HTTP Parser Implementation
// Note: Enhanced implementation for proper proxy functionality

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
    // Enhanced parser to extract Host header for proxy functionality
    if (!parse || !buf || buflen <= 0) {
        printf("[PARSER] Invalid parameters\n");
        return -1;
    }
    
    printf("[PARSER] Parsing request of length %d\n", buflen);
    printf("[PARSER] Request content: %.200s...\n", buf);
    
    // Store original buffer
    parse->buf = malloc(buflen + 1);
    if (!parse->buf) return -1;
    
    memcpy(parse->buf, buf, buflen);
    parse->buf[buflen] = '\0';
    parse->buflen = buflen;
    
    // Find the end of the first line (request line)
    char* line_end = strstr(parse->buf, "\r\n");
    if (!line_end) {
        printf("[PARSER] Failed to find end of request line\n");
        return -1;
    }
    
    // Parse request line (make a copy to avoid modifying original)
    int line_len = line_end - parse->buf;
    char* request_line = malloc(line_len + 1);
    strncpy(request_line, parse->buf, line_len);
    request_line[line_len] = '\0';
    
    printf("[PARSER] Request line: %s\n", request_line);
    
    // Parse method, path, version
    char* method = strtok(request_line, " ");
    char* path = strtok(NULL, " ");
    char* version = strtok(NULL, " ");
    
    if (method) {
        parse->method = malloc(strlen(method) + 1);
        strcpy(parse->method, method);
        printf("[PARSER] Method: %s\n", parse->method);
    }
    
    if (path) {
        parse->path = malloc(strlen(path) + 1);
        strcpy(parse->path, path);
        printf("[PARSER] Path: %s\n", parse->path);
    }
    
    if (version) {
        parse->version = malloc(strlen(version) + 1);
        strcpy(parse->version, version);
        printf("[PARSER] Version: %s\n", parse->version);
    }
    
    free(request_line);
    
    // Parse headers (start after the first \r\n)
    printf("[PARSER] Looking for headers...\n");
    char* headers_start = line_end + 2; // Skip \r\n
    char* current_pos = headers_start;
    
    while (current_pos < parse->buf + buflen) {
        // Find the end of current header line
        char* header_end = strstr(current_pos, "\r\n");
        if (!header_end) break;
        
        // Check for empty line (end of headers)
        if (header_end == current_pos) break;
        
        // Extract header line
        int header_len = header_end - current_pos;
        char* header_line = malloc(header_len + 1);
        strncpy(header_line, current_pos, header_len);
        header_line[header_len] = '\0';
        
        printf("[PARSER] Header line: %s\n", header_line);
        
        // Parse header name and value
        char* colon = strchr(header_line, ':');
        if (colon) {
            *colon = '\0';  // Split header name and value
            char* header_name = header_line;
            char* header_value = colon + 1;
            
            // Skip whitespace in header value
            while (*header_value == ' ') header_value++;
            
            printf("[PARSER] Header: '%s' = '%s'\n", header_name, header_value);
            
            // Check for Host header
            if (strcasecmp(header_name, "Host") == 0) {
                parse->host = malloc(strlen(header_value) + 1);
                strcpy(parse->host, header_value);
                printf("[PARSER] Found Host header: %s\n", parse->host);
                free(header_line);
                break;
            }
        }
        
        free(header_line);
        current_pos = header_end + 2; // Move to next line
    }
    
    if (!parse->host) {
        printf("[PARSER] WARNING: No Host header found!\n");
    }
    
    // Set default port if not specified
    if (!parse->port) {
        parse->port = malloc(3);
        strcpy(parse->port, "80");
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
