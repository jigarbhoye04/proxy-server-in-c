#ifndef PROXY_HTTP_PARSER_H
#define PROXY_HTTP_PARSER_H

#include "../proxy_parse.h"

// HTTP Parser Module
// Handles HTTP request parsing and header manipulation

// Parser interface functions
struct ParsedRequest* ParsedRequest_create(void);
void ParsedRequest_destroy(struct ParsedRequest *pr);
int ParsedRequest_parse(struct ParsedRequest *parse, const char *buf, int buflen);
int ParsedRequest_unparse_headers(struct ParsedRequest *pr, char *buf, size_t buflen);

// Header manipulation functions
int ParsedHeader_set(struct ParsedRequest *pr, const char *key, const char *value);
struct ParsedHeader* ParsedHeader_get(struct ParsedRequest *pr, const char *key);

// HTTP request validation and utilities
int validate_http_request(const char* request, int length);
int extract_host_port(const char* host_header, char* host, int* port);

#endif // PROXY_HTTP_PARSER_H
