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
#include <fcntl.h>
#include <errno.h>

#include "parse.h"
#include "log.h"
#include "hash_table.h"

#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define HEADER_BUF_SIZE 8192
#define TABLE_SIZE 1024
#define MAX_CLIENT FD_SETSIZE

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int sock;
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    fprintf(stdout, "----- Echo Server -----\n");

    Log *log = log_init_default("liso.log");

    Table *table = create_table(TABLE_SIZE);

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        error_log(log, "", "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
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

        if (select(max_sd + 1, &newfds, NULL, NULL, NULL) < 0)
        {
            close(sock);
            error_log(log, "", "Error select.\n");
            return EXIT_FAILURE;
        }

        // todo implement timeout
        // todo handle large buffer requests

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

                    FD_SET(new_socket, &readfds);

                    insert_table(table, new_socket, temp_addr);

                    if (new_socket > max_sd)
                    {
                        max_sd = new_socket;
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

                        if (request == NULL)
                        {
                            printf("Buffer:");
                            for (int i = 0; i < BUF_SIZE; ++i)
                            {
                                printf("%d,", buf[i]);
                            }
                            printf("\n");

                            char res[] = "HTTP/1.1 400 Bad Request\r\n";
                            printf("processing request\n");
                            if (send(i, res, strlen(res), 0) != strlen(res))
                            {
                                close_socket(i);
                                close_socket(sock);
                                error_log(log, "", "Error sending to client.\n");
                                return EXIT_FAILURE;
                            }

                            printf("reached checkpoint 1\n");

                            struct sockaddr_in *new_addr = (struct sockaddr_in *)lookup_table(table, i);

                            printf("reached here\n");

                            access_log(log, inet_ntoa(new_addr->sin_addr), "", res, 400, strlen(res));

                            printf("done\n");
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
                                    // return 411;
                                    // code = 411;
                                    // phrase = "Length Required";
                                }
                                else
                                {
                                    realloc(new_buf, val);
                                    for (k = BUF_SIZE; k < val; k += BUF_SIZE)
                                    {
                                        readret = recv(i, buf, BUF_SIZE, 0);
                                        if (readret < 1)
                                            break;
                                        memcpy(new_buf + k, buf, min(BUF_SIZE, val - k));
                                    }
                                    if (readret >= 1)
                                        request->body = new_buf;
                                }
                                // TODO read more stuff from body
                            }

                            handle_request(request, log);
                            if (send(i, buf, readret, 0) != readret)
                            {
                                close_socket(i);
                                close_socket(sock);
                                error_log(log, "", "Error sending to client.\n");
                                return EXIT_FAILURE;
                            }

                            struct sockaddr_in *new_addr = (struct sockaddr_in *)lookup_table(table, i);

                            access_log(log, inet_ntoa(new_addr->sin_addr), "", buf, 200, strlen(buf));
                        }
                        printf("reaching end\n");
                        memset(buf, 0, BUF_SIZE);
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

                        fprintf(stderr, "Socket reaching end %d.\n", i);
                    }
                }

                // printf("Closing client %d's socket\n", sd);
                // if (close_socket(sd))
                // {
                //     close_socket(sock);
                //     fprintf(stderr, "Error closing client socket.\n");
                //     return EXIT_FAILURE;
                // }
                // else
                // {
                //     client_sock[i] = 0;
                // }
            }
        }
    }

    printf("Token\n");

    close_socket(sock);

    return EXIT_SUCCESS;
}

static const char *DAY_NAMES[] =
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONTH_NAMES[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *Rfc1123_DateTime(time_t t)
{
    const int RFC1123_TIME_LEN = 29;
    struct tm tm;
    char *buf = malloc(RFC1123_TIME_LEN + 1);

    gmtime_s(&tm, &t);

    strftime(buf, RFC1123_TIME_LEN + 1, "---, %d --- %Y %H:%M:%S GMT", &tm);
    memcpy(buf, DAY_NAMES[tm.tm_wday], 3);
    memcpy(buf + 8, MONTH_NAMES[tm.tm_mon], 3);

    return buf;
}

char *handle_request(Request *request, Log *log)
{
    printf("Parsing succeeded!\n");

    int code;
    char *phrase;
    char *content;
    char *header;

    // check version. if not http 1.1, return 505. 10.5.6
    if (strcmp(request->http_version, "HTTP/1.1"))
    {
        code = 505;
        phrase = "HTTP Version Not Supported";
        // version not supported. return 505.
    }

    else if (strcmp(request->http_method, "GET") == 0 || strcmp(request->http_method, "HEAD") == 0)
    {
        // load file from the correct directory
        // pass it into the buffer
        // if it is a folder, or a root, try to open index.html.
        // if retrieved, return 200 OK.
        // if not found, return 404 Not Found.
        // if met system call errors, return 500 Internal Server Error.

        // returns: HTTP-Version SP Status-Code SP Reason-Phrase CRLF
        // (header CRLF)* CRLF body
        // header implements Connection and Date (strftime()). 14.10  14.18
        // connection: close close.
        // date assign one while caching if not exist in request
        // implement Server ('Liso/1.0') 14.38
        // Content-Length: 14.13
        // Content-Type: 14.17 // MIME
        // Last-Modified: 14.29 // do not do conditional get now.
        char uri_buf[HEADER_BUF_SIZE];
        uri_buf[0] = '.';
        strncat(uri_buf, request->http_uri, strlen(request->http_uri));
        FILE *file = fopen(uri_buf, "r");

        if (file == NULL)
        {
            if (uri_buf[strlen(uri_buf) - 1] != '/')
            {
                code = 404;
                phrase = "Not Found";
                // return 404
            }
            else
            {
                strncat(uri_buf, "index.html", strlen(request->http_uri));
                file = fopen(uri_buf, "r");
                if (file == NULL)
                {
                    code = 404;
                    phrase = "Not Found";
                    // return 404
                }
                else
                {
                    code = 200;
                    phrase = "OK";
                    // return 200
                }
            }
        }
        else
        {
            code = 200;
            phrase = "OK";
        }
        // return 200

        // get the size of the file
        int sz = 0;
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
        if (code != 200)
            sz = 0;

        header = strcat(header, "Date: ");
        header = strcat(header, Rfc1123_DateTime(time(NULL)));
        header = strcat(header, "\r\n");
        header = strcat(header, "Connection: ");

        Request_header *tmp = request->headers;
        for (int i = 0; i < request->header_count; i++)
        {
            Request_header rh = *tmp;
            if (strcmp(rh.header_name, "Connection") == 0)
            {
                header = strcat(header, rh.header_value);
            }
        }

        header = strcat(header, "\r\n");
        header = strcat(header, "Server: Liso/1.0\r\n");
        header = strcat(header, "Content-Length: ");
        char *len = itoa(sz);
        header = strcat(header, len);
        if (code == 200)
        {
            header = strncat(header, "\r\nContent-Type: ", HEADER_BUF_SIZE);
            // MIME types
            // text/html text/css image/png image/jpeg image/gif application/pdf

            char *ext = get_filename_ext(uri_buf);

            if (strcmp(ext, "html") == 0)
            {
                header = strcat(header, "text/html");
            }
            else if (strcmp(ext, "css") == 0)
            {
                header = strcat(header, "text/css");
            }
            else if (strcmp(ext, "png") == 0)
            {
                header = strcat(header, "image/png");
            }
            else if (strcmp(ext, "jpeg") == 0)
            {
                header = strcat(header, "image/jpeg");
            }
            else if (strcmp(ext, "gif") == 0)
            {
                header = strcat(header, "image/gif");
            }
            else if (strcmp(ext, "pdf") == 0)
            {
                header = strcat(header, "application/pdf");
            }
            else
            {
            }

            header = strncat(header, "\r\nLast-Modified: ", HEADER_BUF_SIZE);
            struct stat *info = (struct stat *)malloc(sizeof(struct stat));
            if (stat(uri_buf, info) == -1)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
            else
            {
                time_t last_modified = (info->st_mtimespec).tv_sec;
                header = strncat(header, Rfc1123_DateTime(last_modified), HEADER_BUF_SIZE);
            }
        }
        // do not send the content, but send the body.
        // this first.

        if (strcmp(request->http_method, "GET") == 0 && code == 200)
        {
            // put the stuff into buffer!
            content = (char *)malloc(sz);
            fgets(content, sz, file);
            if (ferror(file))
            {
                code = 500;
                phrase = "Internal Server Error";
            }
        }
        if (file != NULL)
            fclose(file);
    }
    else if (strcmp(request->http_method, "POST") == 0)
    {
    }
    else
    {
    }
    // for all other methods, return 501.

    // return header vs contents
}

const char *get_filename_ext(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return "";
    return dot + 1;
}
