/******************************************************************************
* lisod.c                                                                     *
*                                                                             *
* Description: This file contains the C source code for an liso server.  The  *
*              server runs on a user-coded port and offers simple HTTP        *
*              requests.  It supports concurrent clients.                     *
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
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>

#include "log.h"
#include "parse.h"
#include "hash_table.h"

#define BUF_SIZE 8192
#define HEADER_BUF_SIZE 8192
#define TABLE_SIZE 1024
#define MAX_CLIENT FD_SETSIZE
#define _POSIX_C_SOURCE 200112L
#define MIN(x, y) x < y ? x : y
#define WAIT 1
#define CLOSE_SOCKET_FAILURE 2

int num_client = 0;
int sock = 0;
int client_sock = 0;

/***** Daemonize code *****/

int close_socket_client()
{
    if (close(client_sock))
    {
        fprintf(stderr, "Failed closing socket. %d\n", client_sock);
        return 1;
    }
    client_sock = 0;
    return 0;
}

int close_socket_main()
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket. %d\n", sock);
        return 1;
    }
    sock = 0;
    return 0;
}

void lisod_shutdown(int ret)
{
    if (client_sock != 0)
        close_socket_client();
    if (sock != 0)
        close_socket_main();
    exit(ret);
}

/**
 * internal signal handler
 */
void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGHUP:
        /* rehash the server */
        printf("handling sighup!\n");

        break;
    case SIGTERM:
        /* finalize and shutdown the server */
        printf("handling sigterm!\n");
        lisod_shutdown(EXIT_SUCCESS);
        break;
    default:
        printf("handling signal %d\n", sig);
        break;
        /* unhandled signal */
    }
}

/** 
 * internal function daemonizing the process
 */
int daemonize(char *lock_file, Log *log)
{
    printf("Starting to daemonize\n");
    /* drop to having init() as parent */
    int i, lfp, pid = fork();
    char str[256] = {0};
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    setsid();

    for (i = getdtablesize(); i >= 0; i--)
        close(i);

    i = open("/dev/null", O_RDWR);
    dup(i); /* stdout */
    dup(i); /* stderr */
    umask(027);

    lfp = open(lock_file, O_RDWR | O_CREAT, 0640);

    if (lfp < 0)
        exit(EXIT_FAILURE); /* can not open */

    if (lockf(lfp, F_TLOCK, 0) < 0)
        exit(EXIT_SUCCESS); /* can not lock */

    /* only first instance continues */
    sprintf(str, "%d\n", getpid());
    write(lfp, str, strlen(str)); /* record pid to lockfile */

    signal(SIGCHLD, SIG_IGN); /* child terminate signal */

    signal(SIGHUP, signal_handler);  /* hangup signal */
    signal(SIGTERM, signal_handler); /* software termination signal from kill */

    // log --> "Successfully daemonized lisod process, pid %d."
    char *digest = malloc(SIZE);
    sprintf(digest, "Successfully daemonized lisod process, pid %d\n", getpid());
    write_log(log, digest);
    free(digest);

    return EXIT_SUCCESS;
}

static const char *DAY_NAMES[] =
    {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *MONTH_NAMES[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *Rfc1123_DateTime(time_t *t)
{
    const int RFC1123_TIME_LEN = 29;
    struct tm *tm;
    char *buf = malloc(RFC1123_TIME_LEN + 1);
    bzero(buf, RFC1123_TIME_LEN + 1);

    tm = gmtime(t);
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
    char *header = malloc(BUF_SIZE);
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
        sz = 0;
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
        printf("stat: %d\n", n);

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
                strcat(uri_buf, "/index.html");
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

        if (code != 404 && stat(uri_buf, info) == -1)
        {
            code = 500;
            phrase = "Internal Server Error";
        }

        printf("Reached point 1. code: %d\n", code);

        // read the file

        FILE *file = fopen(uri_buf, "r");

        if (code != 404 && file == NULL)
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
            bzero(content, sz + 1);
            printf("sz: %d\n", sz);

            // read everything into the buffer
            char symbol;
            int i;
            if (file != NULL)
            {
                printf("Start passing file here!");
                for (i = 0; i < sz; i++)
                {
                    symbol = getc(file);
                    content[i] = symbol;
                    printf("%c ", symbol);
                }
                printf("\n");
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

        // check if header contains content-length
        int k;
        int val = 0;
        for (k = 0; k < request->header_count; ++k)
        {
            Request_header header = request->headers[k];
            if (strcmp(header.header_name, "Content-Length") == 0)
            {
                val = atoi(header.header_value);
                break;
            }
        }
        // return 411 if content-length is not in the header
        if (k == request->header_count)
        {
            code = 411;
            phrase = "Length Required";
        }
        else
        {
            // parse the address of the uri
            strcpy(uri_buf, www_folder);
            strncat(uri_buf, request->http_uri, strlen(request->http_uri));

            // open the file and start writing to it
            FILE *file = fopen(uri_buf, "w");

            printf("Request body: %s\n", request->body);

            if (file == NULL)
            {
                code = 500;
                phrase = "Internal Server Error";
            }
            // get the size of the file
            else
            {
                code = 200;
                phrase = "OK";
            }

            // put the body into the file!
            if (code == 200)
            {
                char *body = request->body;

                // put the stuff!
                if (fwrite(body, 1, val, file) != val)
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
    time_t t = time(NULL);

    char *rfc_date = Rfc1123_DateTime(&t);
    strcat(header, rfc_date);
    free(rfc_date);

    printf("Date header: %s\n", header);

    // 2. Connection

    strcat(header, "\r\n");
    strcat(header, "Connection: ");

    if (code == 400)
    {
        close = 1;
    }
    else if (code == 500 || code == 505 || code == 408 || code == 503)
    {
        // 500 error, close connection
        // 505 wrong version, close connection
        // 408 timeout
        close = 0;
    }
    else if (request == NULL)
    {
        close = 1;
        printf("SHould never happen! code: %d\n", code);
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

    // 3. Server

    strcat(header, "\r\n");
    strcat(header, "Server: Liso/1.0\r\n");

    // 4. Content-Length

    strcat(header, "Content-Length: ");
    char *len = (char *)malloc(20);
    bzero(len, 20);
    sprintf(len, "%d", sz);

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

        time_t last_modified = (info->st_mtim).tv_sec;
        char *temp_buf = Rfc1123_DateTime(&last_modified);
        strcat(header, temp_buf);
        free(temp_buf);

        strcat(header, "\r\n");
    }

    if (info != NULL)
        free(info);

    strcat(header, "\r\n");

    printf("After 6, header: %s\n", header);

    // Return a response

    Response *response = (Response *)malloc(sizeof(Response));
    char chr[BUF_SIZE];
    bzero(chr, BUF_SIZE);
    sprintf(chr, "HTTP/1.1 %d %s\r\n", code, phrase);
    char *final_buf = (char *)malloc(sz + strlen(header) + strlen(chr) + 1);
    bzero(final_buf, sz + strlen(header) + strlen(chr) + 1);
    strncpy(final_buf, chr, strlen(chr));
    strncat(final_buf, header, strlen(header));

    printf("final_buf: %s\n", final_buf);

    for (int i = 0; i < sz; ++i)
    {
        final_buf[i + strlen(header) + strlen(chr)] = content[i];
    }

    response->buf = final_buf;
    response->size = sz;
    response->real_size = sz + strlen(header) + strlen(chr);
    response->code = code;
    printf("response size: %ld\n", response->size);
    response->close = close;
    free(header);
    if (content != NULL)
        free(content);

    printf("Just before response\n");

    // return header vs contents
    return response;
}

int send_reply(int socket_num, int main_socket_num, Request *request, Response *response, Log *log, Table *table, fd_set **readfds)
{
    struct sockaddr_in *new_addr = (struct sockaddr_in *)lookup_table(table, socket_num);
    char *addr = inet_ntoa(new_addr->sin_addr);

    printf("Sending reply to socket %d with main socket %d\n", socket_num, main_socket_num);

    char *request_digest = malloc(BUF_SIZE);
    bzero(request_digest, BUF_SIZE);
    if (request != NULL)
        sprintf(request_digest, "%s %s %s", request->http_method, request->http_uri, request->http_version);
    else
        sprintf(request_digest, "CANNOT RECOGNIZE THIS REQUEST");

    access_log(log, addr, "", request_digest, response->code, response->size);

    free(request_digest);

    int num = send(socket_num, response->buf, response->real_size, 0);

    if (num != response->real_size)
    {
        close_socket_client();
        close_socket_main();
        printf("send num: %d, errno: %d\n", num, errno);
        error_log(log, addr, "Error sending to client.\n");
        return EXIT_FAILURE;
    }

    // TODO log correctly the request!

    // close socket
    // 1. When connection closes
    // 2. When the server errors
    // 3. When client timed out after establishing connection
    // While the third happens, we would send client a close notice
    if (response->close == 0)
    {
        if (close_socket_client())
        {
            close_socket_main();
            error_log(log, "", "Error closing client socket.\n");
            return CLOSE_SOCKET_FAILURE;
        }
        FD_CLR(socket_num, *readfds);
        remove_table(table, socket_num);
        num_client--;
    }
    printf("Successfully sent reply! Close: %d\n", response->close);

    // free up the request
    if (request != NULL)
    {
        if (request->body != NULL)
            free(request->body);
        if (request->headers != NULL)
        {
            free(request->headers);
        }
        free(request);
    }

    return SUCCESS;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage: ./lisod [HTTP Port] [log file] [lock file] [www file]\n");
        printf("%d\n", argc);
        return -1;
    }
    int http_port = atoi(argv[1]);
    char *log_file = argv[2];
    char *lock_file = argv[3];
    char *www_file = argv[4];
    printf("%d %s %s %s\n", http_port, log_file, lock_file, www_file);
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr;
    char buf[BUF_SIZE + 10];
    bzero(buf, BUF_SIZE + 10);

    Log *log = log_init_default(log_file);

    daemonize(lock_file, log);

    log_refresh(log);

    fprintf(stdout, "----- Liso Server v1.0 -----\n");

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
        close_socket_main();
        error_log(log, "", "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket_main();
        error_log(log, "", "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    fd_set *readfds = malloc(sizeof(fd_set));
    int max_sd = sock;

    FD_ZERO(readfds);
    FD_SET(sock, readfds);

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        printf("Potato...%d\n", max_sd);

        fd_set newfds = *readfds;

        // set timeout value

        struct timeval *timeout = malloc(sizeof(struct timeval));
        timeout->tv_sec = WAIT;
        timeout->tv_usec = 0;

        int select_val;

        // select
        printf("max sd: %d\n", max_sd);

        if ((select_val = select(max_sd + 1, &newfds, NULL, NULL, timeout)) < 0)
        {
            close(sock);
            error_log(log, "", "Error select.\n");
            return EXIT_FAILURE;
        }

        free(timeout);

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
                    int k = send_reply(i, sock, NULL, response, log, table, &readfds);
                    if (k == EXIT_FAILURE)
                    {
                        printf("1\n");
                        free(response->buf);
                        free(response);
                        printf("In 657\n");
                        return EXIT_FAILURE;
                    }
                    else if (k == CLOSE_SOCKET_FAILURE)
                    {
                        printf("2\n");
                        free(response->buf);
                        free(response);
                        printf("In 657\n");
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
                        insert_table(table, i, temp_addr);
                        if (send_reply(i, sock, NULL, response, log, table, &readfds) == EXIT_FAILURE)
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

                        FD_SET(new_socket, readfds);

                        insert_table(table, new_socket, temp_addr);

                        if (new_socket > max_sd)
                        {
                            max_sd = new_socket;
                        }
                    }
                }
                else
                {

                    // handle client sock
                    client_sock = i;

                    Request *request = NULL;
                    char *new_buf;

                    printf("Potato*******************************\n");
                    if ((readret = recv(i, buf, BUF_SIZE, 0)) >= 1)
                    {
                        printf("Start parsing... \n");
                        printf("%s\n", buf);
                        printf("size: %d\n", strlen(buf));
                        request = parse(buf, strlen(buf), i);

                        printf("result of request is %p\n", request);

                        Response *response = NULL;

                        if (request == NULL)
                        {
                            new_buf = NULL;
                            response = handle_request(NULL, log, 400, www_file);
                            printf("processing request\n");

                            // parsing failed
                            // send a response of 400
                        }
                        else
                        {
                            new_buf = request->body;
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
                                    printf("DID NOT SEE CONTENT LENGTH!\n");
                                    response = handle_request(NULL, log, 411, www_file);
                                }
                                else
                                {
                                    new_buf = realloc(new_buf, val + 1);
                                    new_buf[val] = 0;
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

                        if (send_reply(i, sock, request, response, log, table, &readfds) == EXIT_FAILURE)
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
                        close_socket_client();
                        close_socket_main();
                        error_log(log, "", "Error reading from client socket.\n");
                        return EXIT_FAILURE;
                    }
                    if (readret == 0)
                    {
                        if (close_socket_client())
                        {
                            close_socket_main();
                            error_log(log, "", "Error closing client socket.\n");
                            return EXIT_FAILURE;
                        }
                        FD_CLR(i, readfds);
                        remove_table(table, i);

                        fprintf(stderr, "Socket reaching end %d.\n", i);
                    }
                }
            }
        }
    }

    printf("Token\n");

    close_socket_main();

    return EXIT_SUCCESS;
}
