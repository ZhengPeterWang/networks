#include <unistd.h>
#include "parse.h"

extern int yyparse();
extern void yyrestart(FILE *);
extern void set_parsing_options(char *, size_t, Request *);
extern void set_parsing_options_response(char *, size_t, Response *);

/**
* Given a char buffer returns the parsed request headers
*/
Request *parse(char *buffer, int size, int socketFd)
{
	//Differant states in the state machine
	enum
	{
		STATE_START = 0,
		STATE_CR,
		STATE_CRLF,
		STATE_CRLFCR,
		STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	char buf[8192];
	memset(buf, 0, 8192);

	state = STATE_START;
	while (state != STATE_CRLFCRLF)
	{
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state)
		{
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if (ch == expected)
			state++;
		else
			state = STATE_START;
	}

	//Valid End State
	if (state == STATE_CRLFCRLF)
	{
		Request *request = malloc(sizeof(Request));
		request->header_count = 0;
		request->buf = buffer;
		//TODO You will need to handle resizing this in parser.y
		request->headers = malloc(sizeof(Request_header) * 1);

		set_parsing_options(buf, i, request);

		yyrestart(NULL);
		if (yyparse() == SUCCESS)
		{
			printf("Parsing succeeded!\n");

			request->header_length = i;

			// if (i < size)
			// {
			// 	request->header_length = i;
			// 	// request->body = malloc(size - i + 1);
			// 	// bzero(request->body, size - i + 1);
			// 	// memcpy(request->body, buffer + i, size - i);
			// }
			return request;
		}
		else
		{
			// parsing failed
			free(request->headers);
			free(request);
		}
	}
	//TODO Handle Malformed Requests
	printf("Parsing Request Failed.\n");

	return NULL;
}

Response *parse_response(char *buffer, int size, int socketFd)
{
	//Differant states in the state machine
	enum
	{
		STATE_START = 0,
		STATE_CR,
		STATE_CRLF,
		STATE_CRLFCR,
		STATE_CRLFCRLF
	};

	int i = 0, state;
	size_t offset = 0;
	char ch;
	char buf[8192];
	memset(buf, 0, 8192);

	state = STATE_START;
	while (state != STATE_CRLFCRLF)
	{
		char expected = 0;

		if (i == size)
			break;

		ch = buffer[i++];
		buf[offset++] = ch;

		switch (state)
		{
		case STATE_START:
		case STATE_CRLF:
			expected = '\r';
			break;
		case STATE_CR:
		case STATE_CRLFCR:
			expected = '\n';
			break;
		default:
			state = STATE_START;
			continue;
		}

		if (ch == expected)
			state++;
		else
			state = STATE_START;
	}

	//Valid End State
	if (state == STATE_CRLFCRLF)
	{
		Response *response = malloc(sizeof(Response));
		response->close = -1;
		response->code = -1;
		response->real_size = 0; // TODO
		response->buf = buffer;
		response->size = i;
		//TODO You will need to handle resizing this in parser.y

		set_parsing_options_response(buf, i, response);

		yyrestart(NULL);
		if (yyparse() == SUCCESS)
		{
			printf("Parsing response succeeded!\n");

			return response;
		}
		else
		{
			// parsing response failed
			free(response);
			return NULL;
		}
	}
	printf("Parsing response failed.\n");
	return NULL;
}
