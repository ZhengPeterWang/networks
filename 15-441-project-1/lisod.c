/******************************************************************************
* echo_server.c                                                               *
*                                                                             *
* Description: This file contains the C source code for an echo server.  The  *
*              server runs on a hard-coded port and simply write back anything*
*              sent to it by connected clients.  It does not support          *
*              concurrent clients.                                            *
*                                                                             *
* Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
*          Wolf Richter <wolf@cs.cmu.edu>                                     *
*                                                                             *
*******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "parse.h"
#include "log.h"
#include "hash_table.h"

#define BUF_SIZE 8192
#define HEADER_BUF_SIZE 8192
#define TABLE_SIZE 1024
#define MAX_CLIENT FD_SETSIZE / 2
#define _POSIX_C_SOURCE 200112L
#define MIN(x, y) x < y ? x : y
#define WAIT 3

int num_client = 0;

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

static const char *DAY_NAMES[] =
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONTH_NAMES[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *Rfc1123_DateTime(time_t t)
{
    const int RFC1123_TIME_LEN = 29;
    struct tm *tm;
    char *buf = malloc(RFC1123_TIME_LEN + 1);
    bzero(buf, RFC1123_TIME_LEN + 1);

    tm = gmtime(&t);
    if (tm == NULL)
        return NULL;

    strftime(buf, RFC1123_TIME_LEN + 1, "---, %d --- %Y %H:%M:%S GMT", tm);
    memcpy(buf, DAY_NAMES[tm->tm_wday], 3);
    memcpy(buf + 8, MONTH_NAMES[tm->tm_mon], 3);

    return buf;
}

const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}

Response *handle_request(Request *request, Log *log, int pre_assigned_code, const char *www_folder)
{
    printf("Parsing succeeded!\n");

    int code;
    int sz = 0;
    int close = 1; // default not close
    char *phrase = NULL;
    char *content = NULL;
    char *header = (char *)malloc(10 * BUF_SIZE);
    bzero(header, BUF_SIZE);
    struct stat *info = NULL;
    char uri_buf[HEADER_BUF_SIZE];
    bzero(uri_buf, HEADER_BUF_SIZE);

    // timeout

    if (request == NULL)
    {
        switch (pre_assigned_code)
        {
        case 408:
            code = 408;
            phrase = "Request timeout";
            break;
        case 411:
            code = 411;
            phrase = "Length Required";
            break;
        case 400:
            code = 400;
            phrase = "Bad Request";
            break;
        case 503:
            code = 503;
            phrase = "Service Unavailable";
        case 0:
            break;
        default:
            printf("Should never happen!\n");
            break;
        }
    }

    // check version. if not http 1.1, return 505. 10.5.6
    else if (strcmp(request->http_version, "HTTP/1.1") != 0)
    {
        printf("Version: %s\n", request->http_version);
        code = 505;
        phrase = "HTTP Version Not Supported";
        // version not supported. return 505.
    }

    /* -------  GET and HEAD ------- */

    else if (strcmp(request->http_method, "GET") == 0 || strcmp(request->http_method, "HEAD") == 0)
    {
        // load file from the correct directory
        // pass it into the buffer
        // if it is a folder, or a root, try to open index.html.
        // if retrieved, return 200 OK.
        // if not found, return 404 Not Found.
        // if met system call errors, return 500 Internal Server Error.

        // URI to retrieve the resource

        strcpy(uri_buf, www_folder);
        if (strcmp(request->http_uri, "/") != 0)
        {
            printf("Content of buffer: %s\n", uri_buf);
            strcat(uri_buf, request->http_uri);
        }

        info = (struct stat *)malloc(sizeof(struct stat));

        int n = stat(uri_buf, info);
        printf("stat: %d", n);

        // check if file exists
        if (access(uri_buf, F_OK) != 0)
        {
            code = 404;
            phrase = "Not Found";
        }
        else
        {
            printf("file exists!\n");
            if (S_ISDIR(info->st_mode))
            {
                strcat(uri_buf, "index.html");
                printf("concacenated \n");
                if (access(uri_buf, F_OK) != 0)
                {
                    code = 404;
                    phrase = "Not Found";
                }
                else
                {
                    code = 200;
                    phrase = "OK";
                }
            }
            else
            {
                code = 200;
                phrase = "OK";
            }
        }

        printf("URI: %s\n", uri_buf);

        if (stat(uri_buf, info) == -1)
        {
            code = 500;
            phrase = "Internal Server Error";
        }

        printf("Reached point 1. code: %d\n", code);

        // read the file

        FILE *file = fopen("./static_site/index.html", "r");

        if (file == NULL)
        {
            code = 500;
            phrase = "Internal Server Error";
        }
        // get the size of the file

        if (code == 200)
        {
            if (fseek(file, 0L, SEEK_END) == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (code == 200)
        {
            sz = ftell(file);
            if (sz == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (code == 200)
        {
            if (fseek(file, 0L, SEEK_SET) == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (code != 200)
            sz = 0;

        printf("Reached point 2. code: %d, sz: %d\n", code, sz);

        // Process the request by getting the content
        // Do not get content while HEAD

        if (strcmp(request->http_method, "GET") == 0 && code == 200)
        {
            // create the buffer
            content = (char *)malloc(sz + 1);
            bzero(content, sz);
            printf("sz: %d\n", sz);

            // read everything into the buffer
            char symbol;
            int i = 0;
            if (file != NULL)
            {
                while ((symbol = getc(file)) != EOF)
                {
                    content[i] = symbol;
                    i++;
                }
            }
            if (ferror(file))
            {
                code = 500;
                phrase = "Internal Server Error";
                printf("i: %d Ferror!\n", i);
            }
        }
        if (file != NULL)
            if (fclose(file) != 0)
            {
                printf("I am there!\n");
                code = 500;
                phrase = "Internal Server Error";
            }

        printf("Reached point 3. code: %d, sz: %d\n", code, sz);
    }

    /* -------  POST ------- */

    else if (strcmp(request->http_method, "POST") == 0)
    {
        // parse the address of the uri
        strcpy(uri_buf, www_folder);
        strncat(uri_buf, request->http_uri, strlen(request->http_uri));

        // open the file and start writing to it
        FILE *file = fopen(uri_buf, "w");

        if (file == NULL)
        {
            code = 500;
            phrase = "Internal Server Error";
        }
        // get the size of the file

        if (code == 200)
        {
            // start to process the request!
            if (fseek(file, 0L, SEEK_END) == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (code == 200)
        {
            sz = ftell(file);
            if (sz == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (code == 200)
        {
            if (fseek(file, 0L, SEEK_SET) == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }

        // put the body into the file!
        if (code == 200)
        {
            char *body = request->body;

            // put the stuff!
            if (fputs(body, file) < 0)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (file != NULL)
            if (fclose(file) != 0)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
    }

    // for all other methods, return 501.
    else
    {
        code = 501;
        phrase = "Not Implemented";
    }

    // clear up content pointer
    if (code != 200 && content != NULL)
    {
        free(content);
        content = NULL;
        sz = 0;
    }

    printf("reached point 4. code: %d\n", code);

    // returns: HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    // (header CRLF)* CRLF body
    // header implements Connection and Date (strftime()). 14.10  14.18
    // connection: close close.
    // date assign one while caching if not exist in request
    // implement Server ('Liso/1.0') 14.38
    // Content-Length: 14.13
    // Content-Type: 14.17 // MIME
    // Last-Modified: 14.29 // do not do conditional get now.

    // fill in headers

    // headers
    // 1. Date

    // TODO handle errors when date is not set
    strcpy(header, "Date: ");

    char *rfc_date = Rfc1123_DateTime(time(NULL));
    strcat(header, rfc_date);
    free(rfc_date);

    printf("Date header: %s\n", header);

    // 2. Connection

    strcat(header, "\r\n");
    strcat(header, "Connection: ");

    if (code == 500 || code == 505 || code == 408 || code == 503)
    {
        // 500 error, close connection
        // 505 wrong version, close connection
        // 408 timeout
        close = 0;
    }
    else
    {
        Request_header *tmp = request->headers;
        for (int i = 0; i < request->header_count; i++)
        {
            if (strcmp(tmp->header_name, "Connection") == 0)
            {
                if (strcmp(tmp->header_value, "close") == 0)
                {
                    close = 0;
                }
                printf("reached connection\n");
                break;
            }
            tmp++;
        }
    }
    if (close == 0)
        strcat(header, "close");
    else
        strcat(header, "keep-alive");

    printf("After 2. , close is :%d, header: %s\n", close, header);

    // 3. Server

    strcat(header, "\r\n");
    strcat(header, "Server: Liso/1.0\r\n");

    // 4. Content-Length

    strcat(header, "Content-Length: ");
    char *len = (char *)malloc(20);
    bzero(len, 20);
    *len = sprintf(len, "%d", sz);

    strcat(header, len);
    free(len);
    strcat(header, "\r\n");

    // 5. Content-Type

    if (code == 200 && (strcmp(request->http_method, "GET") == 0 || strcmp(request->http_method, "HEAD") == 0))
    {
        strcat(header, "Content-Type: ");
        // MIME types
        // text/html text/css image/png image/jpeg image/gif application/pdf

        const char *ext = get_filename_ext(uri_buf);

        if (strcmp(ext, "html") == 0)
        {
            strcat(header, "text/html");
        }
        else if (strcmp(ext, "css") == 0)
        {
            strcat(header, "text/css");
        }
        else if (strcmp(ext, "png") == 0)
        {
            strcat(header, "image/png");
        }
        else if (strcmp(ext, "jpeg") == 0)
        {
            strcat(header, "image/jpeg");
        }
        else if (strcmp(ext, "gif") == 0)
        {
            strcat(header, "image/gif");
        }
        else if (strcmp(ext, "pdf") == 0)
        {
            strcat(header, "application/pdf");
        }
        else
        {
            strcat(header, "application/octet-stream");
        }

        header = strcat(header, "\r\n");

        // 6. Last-Modified

        strcat(header, "Last-Modified: ");

        time_t last_modified = (info->st_mtimespec).tv_sec;
        char *temp_buf = Rfc1123_DateTime(last_modified);
        strcat(header, temp_buf);
        free(temp_buf);

        strcat(header, "\r\n");

        free(info);
    }

    printf("After 6. , header: %s\n", header);

    // Return a response

    Response *response = (Response *)malloc(sizeof(Response));
    char chr[BUF_SIZE];
    sprintf(chr, "HTTP/1.1 %d %s\r\n", code, phrase);
    char *final_buf = (char *)malloc(sz + strlen(header) + strlen(chr) + 1);
    bzero(final_buf, sz + strlen(header) + strlen(chr) + 1);
    strcpy(final_buf, chr);
    strcat(final_buf, header);
    strcat(final_buf, "\r\n");
    if (content != NULL)
        strcat(final_buf, content);

    response->buf = final_buf;
    response->size = strlen(final_buf);
    response->close = close;
    free(header);
    printf("Content pointer: %d\n", content);
    if (content != NULL)
        free(content);

    printf("Just before response\n");

    // return header vs contents
    return response;
}

int send_reply(int socket_num, int main_socket_num, Response *response, Log *log, Table *table, fd_set *readfds)
{
    struct sockaddr_in *new_addr = (struct sockaddr_in *)lookup_table(table, socket_num);
    char *addr = inet_ntoa(new_addr->sin_addr);

    if (send(socket_num, response->buf, response->size, 0) != response->size)
    {
        close_socket(socket_num);
        close_socket(main_socket_num);
        error_log(log, addr, "Error sending to client.\n");
        return EXIT_FAILURE;
    }

    // TODO log correctly the request!
    access_log(log, addr, "", response->buf, response->size, strlen(response->buf));

    // close socket
    // 1. When connection closes
    // 2. When the server errors
    // 3. When client timed out after establishing connection
    // While the third happens, we would send client a close notice
    if (response->close == 0)
    {
        if (close_socket(socket_num))
        {
            close_socket(main_socket_num);
            error_log(log, "", "Error closing client socket.\n");
            return EXIT_FAILURE;
        }
        FD_CLR(socket_num, readfds);
        remove_table(table, socket_num);
        num_client--;
    }
    printf("Successfully sent reply! Close: %d\n", response->close);

    return SUCCESS;
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: ./lisod [HTTP Port] [log file] [www file]\n");
        printf("%d\n", argc);
        return -1;
    }
    int http_port = atoi(argv[1]);
    char *log_file = argv[2];
    char *www_file = argv[3];
    printf("%d %s %s\n", http_port, log_file, www_file);
    int sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    fprintf(stdout, "----- Echo Server -----\n");

    Log *log = log_init_default(log_file);

    Table *table = create_table(TABLE_SIZE);

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_log(log, "", "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(http_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        close_socket(sock);
        error_log(log, "", "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        error_log(log, "", "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    fd_set readfds;
    int max_sd = sock;

    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        printf("Potato...%d\n", max_sd);

        fd_set newfds = readfds;

        // set timeout value

        struct timeval timeout;
        timeout.tv_sec = WAIT;
        timeout.tv_usec = 0;

        int select_val;

        // select

        if ((select_val = select(max_sd + 1, &newfds, NULL, NULL, &timeout)) < 0)
        {
            close(sock);
            error_log(log, "", "Error select.\n");
            return EXIT_FAILURE;
        }

        // handling timeout

        if (select_val == 0)
        {
            for (int i = 0; i < max_sd + 1; i++)
            {
                if (i != sock && lookup_table(table, i) != NULL)
                {
                    printf("Send a timeout response!\n");
                    // send to that client that we have timed out!
                    Response *response = handle_request(NULL, log, 408, www_file);
                    if (send_reply(i, sock, response, log, table, &readfds) == EXIT_FAILURE)
                    {
                        printf("1\n");
                        free(response->buf);
                        free(response);
                        printf("In 657\n");
                        return EXIT_FAILURE;
                    }
                    free(response->buf);
                    free(response);
                    printf("In 662\n");
                }
            }
            printf("Finished sending timeout responses!\n");
            continue;
        }

        for (int i = 0; i < max_sd + 1; i++)
        {
            if (FD_ISSET(i, &newfds))
            {
                printf("We got one %d\n", i);

                if (i == sock)
                {
                    struct sockaddr *temp_addr;
                    temp_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
                    cli_size = sizeof(temp_addr);

                    int new_socket;
                    if ((new_socket = accept(sock, temp_addr,
                                             &cli_size)) == -1)
                    {
                        close(sock);
                        error_log(log, "", "Error accepting connection.\n");
                        return EXIT_FAILURE;
                    }

                    // return 503 code when unable to accept more connections

                    if (num_client == MAX_CLIENT)
                    {
                        Response *response = handle_request(NULL, log, 503, www_file);
                        if (send_reply(i, sock, response, log, table, &readfds) == EXIT_FAILURE)
                        {
                            printf("2\n");
                            free(response->buf);
                            free(response);
                            printf("In 699\n");
                            return EXIT_FAILURE;
                        }
                        free(response->buf);
                        free(response);
                        printf("In 704\n");
                    }

                    else
                    {
                        num_client++;

                        FD_SET(new_socket, &readfds);

                        insert_table(table, new_socket, temp_addr);

                        if (new_socket > max_sd)
                        {
                            max_sd = new_socket;
                        }
                    }
                }
                else
                {
                    Request *request = NULL;
                    char *new_buf;

                    printf("Potato*******************************\n");
                    if ((readret = recv(i, buf, BUF_SIZE, 0)) >= 1)
                    {
                        printf("Start parsing... \n");
                        request = parse(buf, strlen(buf), i);
                        new_buf = request->body;
                        printf("result of request is %p\n", request);

                        Response *response;

                        if (request == NULL)
                        {
                            response = handle_request(NULL, log, 400, www_file);
                            printf("processing request\n");

                            // parsing failed
                            // send a response of 400
                        }
                        else
                        {
                            if (readret == BUF_SIZE)
                            {
                                int k, val;
                                for (k = 0; k < request->header_count; ++k)
                                {
                                    Request_header header = request->headers[k];
                                    if (strcmp(header.header_name, "Content-Length") == 0)
                                    {
                                        val = atoi(header.header_value);
                                        break;
                                    }
                                }
                                if (k == request->header_count)
                                {
                                    // send a response of 411
                                    response = handle_request(NULL, log, 411, www_file);
                                }
                                else
                                {
                                    new_buf = realloc(new_buf, val);
                                    // TODO check if strlen(new_buf) is the correct starting point
                                    for (k = strlen(new_buf); k < val; k += BUF_SIZE)
                                    {
                                        bzero(buf, BUF_SIZE);
                                        readret = recv(i, buf, BUF_SIZE, 0);
                                        if (readret < 1)
                                            break;
                                        memcpy(new_buf + k, buf, MIN(BUF_SIZE, val - k));
                                    }
                                    if (readret >= 1)
                                    {
                                        request->body = new_buf;
                                    }
                                }
                                // read more stuff from body
                            }
                            // handle request
                            if (response == NULL)
                            {
                                printf("handling the request!\n");
                                response = handle_request(request, log, 0, www_file);
                            }
                        }

                        // printf("reaching end\n");
                        // memset(buf, 0, BUF_SIZE);

                        if (send_reply(i, sock, response, log, table, &readfds) == EXIT_FAILURE)
                        {
                            printf("3\n");
                            free(response->buf);
                            free(response);
                            printf("In 797\n");
                            return EXIT_FAILURE;
                        }
                        free(response->buf);
                        free(response);
                        printf("In 802\n");
                    }

                    if (readret == -1)
                    {
                        close_socket(i);
                        close_socket(sock);
                        error_log(log, "", "Error reading from client socket.\n");
                        return EXIT_FAILURE;
                    }
                    if (readret == 0)
                    {
                        if (close_socket(i))
                        {
                            close_socket(sock);
                            error_log(log, "", "Error closing client socket.\n");
                            return EXIT_FAILURE;
                        }
                        FD_CLR(i, &readfds);
                        remove_table(table, i);

                        fprintf(stderr, "Socket reaching end %d.\n", i);
                    }
                }
            }
        }
    }

    printf("Token\n");

    close_socket(sock);

    return EXIT_SUCCESS;
}
