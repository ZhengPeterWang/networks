#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SUCCESS 0

//Header field
typedef struct
{
	char header_name[4096];
	char header_value[4096];
} Request_header;

//HTTP Request Header
typedef struct
{
	char http_version[50];
	char http_method[50];
	char http_uri[4096];
	Request_header *headers;
	char *body;
	int header_count;
} Request;

typedef struct
{
	char *buf;
	int code;
	ssize_t size;
	ssize_t real_size;
	int close; // 0 close, 1 not close
} Response;

Request *parse(char *buffer, int size, int socketFd);
