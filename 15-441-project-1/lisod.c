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

#define HEADER_BUF_SIZE 8192
#define TABLE_SIZE 1024
#define MAX_CLIENT FD_SETSIZE
#define MIN(x, y) x < y ? x : y
#define MAX(x, y) x < y ? y : x
#define WAIT 1
#define CLOSE_SOCKET_FAILURE 2

int num_client = 0;
int sock = 0;
int client_sock = 0;
int https_sock = 0;
Table *table;
SSL_CTX *ssl_context;

/***** Daemonize code *****/

int close_socket_client()
{
    if (close(client_sock))
    {
        fprintf(stderr, "Failed closing client socket. %d\n", client_sock);
        return 1;
    }
    client_sock = 0;
    return 0;
}

int close_socket_main()
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing main socket. %d\n", sock);
        return 1;
    }
    sock = 0;
    return 0;
}

int close_socket_https()
{
    if (close(https_sock))
    {
        fprintf(stderr, "Failed closing https socket. %d\n", https_sock);
        return 1;
    }
    https_sock = 0;
    return 0;
}

void lisod_shutdown(int ret)
{
    // TODO close all sockets when shutting down!
    // put fd_set as global!
    if (client_sock != 0)
        close_socket_client();
    if (sock != 0)
        close_socket_main();
    if (https_sock != 0)
        close_socket_https();
    remove_all_entries_in_table(table);
    if (ssl_context != NULL)
        SSL_CTX_free(ssl_context);
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

int check_uri(char *uri)
{
    const char *pre = "/cgi/";
    size_t lenpre = strlen(pre),
           lenstr = strlen(uri);
    return lenstr < lenpre ? 0 : memcmp(pre, uri, lenpre) == 0;
}

Response *handle_request(Request *request, int pre_assigned_code, const char *www_folder)
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

    // timeout and other special cases

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

    // ********** CGI ***********
    else if (check_uri(request->http_uri))
    {
        return NULL; // pass to the other handler
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

            printf("Request buffer: %s\n", request->buf);

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
                char *body = request->buf + request->header_length;

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

        time_t last_modified = (info->st_mtimespec).tv_sec;
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

int send_reply(Request *request, Response *response, Log *log, Table *table, fd_set **readfds, int mode)
{
    // TODO put request digest generation out of send_reply()!

    int socket_num = client_sock;

    // check which connection it is from the table!

    struct sockaddr_in *new_addr = (struct sockaddr_in *)lookup_table(table, socket_num);
    char *addr = new_addr == NULL ? "N/A" : inet_ntoa(new_addr->sin_addr);

    printf("Sending reply to socket %d\n", socket_num);

    // log correctly the request!

    char *request_digest = malloc(BUF_SIZE);
    bzero(request_digest, BUF_SIZE);
    if (request != NULL)
        sprintf(request_digest, "%s %s %s", request->http_method, request->http_uri, request->http_version);
    else
        sprintf(request_digest, "CANNOT RECOGNIZE THIS REQUEST");

    access_log(log, addr, "", request_digest, response->code, response->size);

    free(request_digest);

    // send depending on the mode
    int num;
    if (mode == 0)
    {
        num = write(socket_num, response->buf, response->real_size);
    }
    else if (mode == 1)
    {
        printf("real size: %ld\n", response->real_size);
        num = SSL_write(lookup_table_context(table, client_sock), response->buf, response->real_size);
    }

    if (num != response->real_size)
    {
        //  securely delete context
        printf("send num: %d, errno: %d, mode: %d\n", num, errno, mode);
        if (mode == 1)
            printf("Error indicator:%d\n", SSL_get_error(lookup_table_context(table, client_sock), num));
        error_log(log, addr, "Error sending to client.\n");
        lisod_shutdown(EXIT_FAILURE);
        return EXIT_FAILURE;
    }

    // close socket
    // 1. When connection closes
    // 2. When the server errors
    // 3. When client timed out after establishing connection
    // While the third happens, we would send client a close notice
    if (response->close == 0)
    {
        if (close_socket_client())
        {
            lisod_shutdown(EXIT_FAILURE);
        }
        FD_CLR(socket_num, *readfds);
        remove_table(table, socket_num);
        num_client--;
    }
    printf("Successfully sent reply! Close: %d\n", response->close);

    // free up the request
    if (request != NULL)
    {
        if (request->buf != NULL)
            free(request->buf);
        if (request->headers != NULL)
        {
            free(request->headers);
        }
        free(request);
    }

    return SUCCESS;
}

int receive(int i, char *buf, SSL *client_context)
{
    if (client_context == NULL)
        return recv(i, buf, BUF_SIZE, 0);
    else
        return SSL_read(client_context, buf, BUF_SIZE);
}

/* error messages stolen from: http://linux.die.net/man/2/execve */
void execve_error_handler()
{
    switch (errno)
    {
    case E2BIG:
        fprintf(stderr, "The total number of bytes in the environment \
(envp) and argument list (argv) is too large.\n");
        return;
    case EACCES:
        fprintf(stderr, "Execute permission is denied for the file or a \
script or ELF interpreter.\n");
        return;
    case EFAULT:
        fprintf(stderr, "filename points outside your accessible address \
space.\n");
        return;
    case EINVAL:
        fprintf(stderr, "An ELF executable had more than one PT_INTERP \
segment (i.e., tried to name more than one \
interpreter).\n");
        return;
    case EIO:
        fprintf(stderr, "An I/O error occurred.\n");
        return;
    case EISDIR:
        fprintf(stderr, "An ELF interpreter was a directory.\n");
        return;
        //     case ELIBBAD:
        //         fprintf(stderr, "An ELF interpreter was not in a recognised \
// format.\n");
        //         return;
    case ELOOP:
        fprintf(stderr, "Too many symbolic links were encountered in \
resolving filename or the name of a script \
or ELF interpreter.\n");
        return;
    case EMFILE:
        fprintf(stderr, "The process has the maximum number of files \
open.\n");
        return;
    case ENAMETOOLONG:
        fprintf(stderr, "filename is too long.\n");
        return;
    case ENFILE:
        fprintf(stderr, "The system limit on the total number of open \
files has been reached.\n");
        return;
    case ENOENT:
        fprintf(stderr, "The file filename or a script or ELF interpreter \
does not exist, or a shared library needed for \
file or interpreter cannot be found.\n");
        return;
    case ENOEXEC:
        fprintf(stderr, "An executable is not in a recognised format, is \
for the wrong architecture, or has some other \
format error that means it cannot be \
executed.\n");
        return;
    case ENOMEM:
        fprintf(stderr, "Insufficient kernel memory was available.\n");
        return;
    case ENOTDIR:
        fprintf(stderr, "A component of the path prefix of filename or a \
script or ELF interpreter is not a directory.\n");
        return;
    case EPERM:
        fprintf(stderr, "The file system is mounted nosuid, the user is \
not the superuser, and the file has an SUID or \
SGID bit set.\n");
        return;
    case ETXTBSY:
        fprintf(stderr, "Executable was open for writing by one or more \
processes.\n");
        return;
    default:
        fprintf(stderr, "Unkown error occurred with execve().\n");
        return;
    }
}

char *ENVP[] = {
    "CONTENT_LENGTH=",
    "CONTENT-TYPE=",
    "GATEWAY_INTERFACE=CGI/1.1",
    "QUERY_STRING=",
    "REMOTE_ADDR=",
    "REMOTE_HOST=", // ?
    "REQUEST_METHOD=",
    "SCRIPT_NAME=/cgi",
    "HOST_NAME=", // ?
    "SERVER_PORT=80",
    "SERVER_PROTOCOL=HTTP/1.1",
    "SERVER_SOFTWARE=Liso/1.0",
    "HTTP_ACCEPT=",
    "HTTP_REFERER=",
    "HTTP_ACCEPT_ENCODING=",
    "HTTP_ACCEPT_LANGUAGE=",
    "HTTP_ACCEPT_CHARSET=",
    "HTTP_COOKIE=",
    "HTTP_USER_AGENT=",
    "HTTP_CONNECTION=",
    "HTTP_HOST=",
    "REQUEST_URI=",
    "PATH_INFO=",
    NULL};

char **get_env_ptrs(Request *request, int my_sock, char *addr)
{

    /*************** BEGIN ENVIRONMENT VARIABLES **************/

    // parse URI
    char *uri = malloc(strlen(request->http_uri));

    strcpy(uri, request->http_uri);

    char *ptr = strchr(uri, '?') + 1;

    char *query = malloc(strlen(ptr));
    bzero(query, strlen(ptr));
    strcpy(query, ptr);
    bzero(ptr - 1, strlen(query) + sizeof(char));

    // QUERY_STRING
    ENVP[3] = malloc(BUF_SIZE);
    bzero(ENVP[3], BUF_SIZE);
    sprintf(ENVP[3], "QUERY_STRING=%s", query);
    free(query);

    // REQUEST URI
    ENVP[21] = malloc(BUF_SIZE);
    bzero(ENVP[21], BUF_SIZE);
    sprintf(ENVP[21], "REQUEST_URI=%s", uri);

    // PATH INFO
    ENVP[22] = malloc(BUF_SIZE);
    bzero(ENVP[22], BUF_SIZE);
    sprintf(ENVP[22], "REQUEST_URI=%s", uri + 4);
    free(uri);

    // REMOTE_ADDR
    ENVP[4] = malloc(BUF_SIZE);
    bzero(ENVP[4], BUF_SIZE);
    sprintf(ENVP[4], "REMOTE_ADDR=%s", addr);

    // REQUEST_METHOD
    ENVP[6] = malloc(BUF_SIZE);
    bzero(ENVP[6], BUF_SIZE);
    sprintf(ENVP[6], "REQUEST_METHOD=%s", request->http_method);

    // SERVER PORT: decide whether it is HTTP or HTTPS
    ENVP[9] = malloc(BUF_SIZE);
    bzero(ENVP[9], BUF_SIZE);
    sprintf(ENVP[9], "SERVER_PORT=%d", my_sock);

    int k;
    for (k = 0; k < request->header_count; ++k)
    {
        Request_header header = request->headers[k];
        // CONTENT_LENGTH
        if (strcmp(header.header_name, "Content-Length") == 0)
        {
            ENVP[0] = malloc(BUF_SIZE);
            bzero(ENVP[0], BUF_SIZE);
            sprintf(ENVP[0], "CONTENT_LENGTH=%s", header.header_value);
            break;
        }

        // CONTENT_TYPE
        else if (strcmp(header.header_name, "Content-Type") == 0)
        {
            ENVP[1] = malloc(BUF_SIZE);
            bzero(ENVP[1], BUF_SIZE);
            sprintf(ENVP[1], "CONTENT-TYPE=%s", header.header_value);
            break;
        }
        // HTTP_ACCEPT
        else if (strcmp(header.header_name, "Accept") == 0)
        {

            ENVP[12] = malloc(BUF_SIZE);
            bzero(ENVP[12], BUF_SIZE);
            sprintf(ENVP[12], "HTTP_ACCEPT=%s", header.header_value);
            break;
        }
        // HTTP_REFERER
        else if (strcmp(header.header_name, "Referer") == 0)
        {

            ENVP[13] = malloc(BUF_SIZE);
            bzero(ENVP[13], BUF_SIZE);
            sprintf(ENVP[13], "HTTP_REFERER=%s", header.header_value);
            break;
        }
        // HTTP_ACCEPT_ENCODING
        else if (strcmp(header.header_name, "Accept-Encoding") == 0)
        {

            ENVP[14] = malloc(BUF_SIZE);
            bzero(ENVP[14], BUF_SIZE);
            sprintf(ENVP[14], "HTTP_ACCEPT_ENCODING=%s", header.header_value);
            break;
        }
        // HTTP_ACCEPT_LANGUAGE
        else if (strcmp(header.header_name, "Accept-Language") == 0)
        {

            ENVP[15] = malloc(BUF_SIZE);
            bzero(ENVP[15], BUF_SIZE);
            sprintf(ENVP[15], "HTTP_ACCEPT_LANGUAGE=%s", header.header_value);
            break;
        }
        // HTTP_ACCEPT_CHARSET
        else if (strcmp(header.header_name, "Accept-Charset") == 0)
        {

            ENVP[16] = malloc(BUF_SIZE);
            bzero(ENVP[16], BUF_SIZE);
            sprintf(ENVP[16], "HTTP_ACCEPT_CHARSET=%s", header.header_value);
            break;
        }
        // COOKIE
        else if (strcmp(header.header_name, "Cookie") == 0)
        {

            ENVP[17] = malloc(BUF_SIZE);
            bzero(ENVP[17], BUF_SIZE);
            sprintf(ENVP[17], "HTTP_COOKIE=%s", header.header_value);
            break;
        }
        // USER-AGENT
        else if (strcmp(header.header_name, "User-Agent") == 0)
        {

            ENVP[18] = malloc(BUF_SIZE);
            bzero(ENVP[18], BUF_SIZE);
            sprintf(ENVP[18], "HTTP_USER_AGENT=%s", header.header_value);
            break;
        }
        // CONNECTION
        else if (strcmp(header.header_name, "Connection") == 0)
        {

            ENVP[19] = malloc(BUF_SIZE);
            bzero(ENVP[19], BUF_SIZE);
            sprintf(ENVP[19], "HTTP_CONNECTION=%s", header.header_value);
            break;
        }
        // HOST
        else if (strcmp(header.header_name, "Host") == 0)
        {

            ENVP[20] = malloc(BUF_SIZE);
            bzero(ENVP[20], BUF_SIZE);
            sprintf(ENVP[20], "HTTP_HOST=%s", header.header_value);
            break;
        }
    }
    return ENVP;

    /*************** END ENVIRONMENT VARIABLES **************/
}

int handle_cgi_request(Request *request, Log *log, char *addr, char *cgi_folder, int my_sock)
{

    char **ENVP = get_env_ptrs(request, my_sock, addr);

    /*************** BEGIN VARIABLE DECLARATIONS **************/
    pid_t pid;
    int stdin_pipe[2];
    int stdout_pipe[2];
    char buf[BUF_SIZE];
    char *ARGV[] = {
        cgi_folder,
        NULL};
    int readret;
    /*************** END VARIABLE DECLARATIONS **************/

    /*************** BEGIN PIPE **************/
    /* 0 can be read from, 1 can be written to */
    if (pipe(stdin_pipe) < 0)
    {
        // TODO change the error messages into log reports and return gracefully
        fprintf(stderr, "Error piping for stdin.\n");
        return EXIT_FAILURE;
    }

    if (pipe(stdout_pipe) < 0)
    {
        fprintf(stderr, "Error piping for stdout.\n");
        return EXIT_FAILURE;
    }
    /*************** END PIPE **************/
    /*************** BEGIN FORK **************/
    pid = fork();
    /* not good */
    if (pid < 0)
    {
        fprintf(stderr, "Something really bad happened when fork()ing.\n");
        return EXIT_FAILURE;
    }

    /* child, setup environment, execve */
    if (pid == 0)
    {
        /*************** BEGIN EXECVE ****************/
        close(stdout_pipe[0]);
        close(stdin_pipe[1]);
        dup2(stdout_pipe[1], fileno(stdout));
        dup2(stdin_pipe[0], fileno(stdin));
        /* you should probably do something with stderr */

        /* pretty much no matter what, if it returns bad things happened... */
        if (execve(cgi_folder, ARGV, ENVP))
        {
            execve_error_handler();
            fprintf(stderr, "Error executing execve syscall.\n");
            return EXIT_FAILURE;
        }
        /*************** END EXECVE ****************/
    }

    if (pid > 0)
    {
        fprintf(stdout, "Parent: Heading to select() loop.\n");
        close(stdout_pipe[1]);
        close(stdin_pipe[0]);

        // then change client_sock to stdin_pipe[1], the place to write

        client_sock = stdin_pipe[1];
        printf("client_sock: %d\n", stdin_pipe[1]);

        // return the other fd for log

        return stdout_pipe[1];
    }
    /*************** END FORK **************/

    fprintf(stderr, "Process exiting, badly...how did we get here!?\n");
    return EXIT_FAILURE;
}

Response *forward_cgi_request(Request *request)
{
    Response *ret = malloc(sizeof(Response));
    int val = 0;

    for (int k = 0; k < request->header_count; ++k)
    {
        Request_header header = request->headers[k];
        if (strcmp(header.header_name, "Content-Length") == 0)
        {
            val = atoi(header.header_value);
            break;
        }
    }

    // val should never be 0 at this point

    ret->buf = request->buf;
    ret->real_size = request->header_length + val;
    ret->size = -1;
    ret->close = -1;
    ret->code = -1;

    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 9)
    {
        printf("Usage: ./lisod [HTTP Port] [HTTPS Port] [log file] [lock file] "
               "[www file] [cgi file] [private key file] [certificate file]\n");
        printf("%d\n", argc);
        return -1;
    }
    SSL *client_context;
    int http_port = atoi(argv[1]);
    int https_port = atoi(argv[2]);
    char *log_file = argv[3];
    char *lock_file = argv[4];
    char *www_file = argv[5];
    char *cgi_file = argv[6];
    char *private_key_file = argv[7];
    char *cert_file = argv[8];
    printf("%d %d %s %s %s %s %s %s\n", http_port, https_port, log_file, lock_file,
           www_file, cgi_file, private_key_file, cert_file);
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr;
    char buf[BUF_SIZE + 10];
    bzero(buf, BUF_SIZE + 10);

    Log *log = log_init_default(log_file);

    // daemonize(lock_file, log);

    log_refresh(log);

    /************ SSL INIT ************/
    SSL_load_error_strings();
    SSL_library_init();

    if ((ssl_context = SSL_CTX_new(TLS_server_method())) == NULL)
    {
        error_log(log, "", "Error creating SSL context.\n");
        return EXIT_FAILURE;
    }

    /* register private key */
    if (SSL_CTX_use_PrivateKey_file(ssl_context, private_key_file, SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(ssl_context);
        error_log(log, "", "Error associating private key.\n");
        return EXIT_FAILURE;
    }

    /* register public key (certificate) */
    if (SSL_CTX_use_certificate_file(ssl_context, cert_file, SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(ssl_context);
        error_log(log, "", "Error associating certificate.\n");
        return EXIT_FAILURE;
    }
    /************ END SSL INIT ************/

    fprintf(stdout, "----- Liso Server v1.0 -----\n");

    /************ CREATE HTTP SERVER ************/

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
        SSL_CTX_free(ssl_context);
        error_log(log, "", "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket_main();
        SSL_CTX_free(ssl_context);
        error_log(log, "", "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    /************ END CREATE HTTP SERVER ************/

    /************ CREATE HTTPS SERVER ************/

    if ((https_sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        SSL_CTX_free(ssl_context);
        close_socket_main();
        error_log(log, "", "Failed creating HTTPS socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(https_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(https_sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        close_socket_https();
        close_socket_main();
        SSL_CTX_free(ssl_context);
        error_log(log, "", "Failed binding HTTPS socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(https_sock, 5))
    {
        close_socket_https();
        close_socket_main();
        SSL_CTX_free(ssl_context);
        error_log(log, "", "Error listening on HTTPS socket.\n");
        return EXIT_FAILURE;
    }

    /************ END CREATE HTTPS SERVER ************/

    /************ MAIN LOOP ************/
    table = create_table(TABLE_SIZE);

    fd_set *readfds = malloc(sizeof(fd_set));
    int max_sd = MAX(sock, https_sock);

    FD_ZERO(readfds);
    FD_SET(sock, readfds);
    FD_SET(https_sock, readfds);

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
            error_log(log, "", "Error select.\n");
            lisod_shutdown(EXIT_FAILURE);
            return EXIT_FAILURE;
        }

        free(timeout);

        // handling timeout

        if (select_val == 0)
        {
            for (int i = 0; i < max_sd + 1; i++)
            {
                // TODO do NOT send a timeout response to the CGI script!
                if (i != sock && lookup_table(table, i) != NULL)
                {
                    printf("Send a timeout response!\n");
                    // send to that client that we have timed out!
                    client_sock = i;
                    Response *response = handle_request(NULL, 408, www_file);
                    int k = send_reply(NULL, response, log, table, &readfds, 0);
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
                client_sock = i;

                if (i == sock || i == https_sock)
                {
                    // accept HTTP and HTTPS connections

                    struct sockaddr *temp_addr;
                    temp_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
                    cli_size = sizeof(temp_addr);

                    int new_socket;
                    if ((new_socket = accept(i, temp_addr,
                                             &cli_size)) == -1)
                    {
                        error_log(log, "", "Error accepting connection.\n");
                        lisod_shutdown(EXIT_FAILURE);
                        return EXIT_FAILURE;
                    }

                    // set sockets as non blocking
                    // TODO: check for SSL errors like non-block early read/write returns
                    // fcntl(i, F_SETFL, O_NONBLOCK);
                    // fcntl(new_socket, F_SETFL, O_NONBLOCK);

                    // return 503 code when unable to accept more connections

                    if (num_client == MAX_CLIENT)
                    {
                        Response *response = handle_request(NULL, 503, www_file);
                        insert_table(table, i, temp_addr, i);
                        if (send_reply(NULL, response, log, table, &readfds, 0) == EXIT_FAILURE)
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
                        if (i == sock)
                        {
                            insert_table(table, new_socket, temp_addr, sock);
                        }
                        else if (i == https_sock)
                        {
                            /************ WRAP SOCKET WITH SSL ************/
                            if ((client_context = SSL_new(ssl_context)) == NULL)
                            {
                                error_log(log, "", "Error creating client SSL context.\n");
                                lisod_shutdown(EXIT_FAILURE);
                                return EXIT_FAILURE;
                            }

                            if (SSL_set_fd(client_context, new_socket) == 0)
                            {
                                error_log(log, "", "Error creating client SSL context.\n");
                                lisod_shutdown(EXIT_FAILURE);
                                return EXIT_FAILURE;
                            }

                            if (SSL_accept(client_context) <= 0)
                            {

                                error_log(log, "", "Error accepting (handshake) client SSL context.\n");
                                lisod_shutdown(EXIT_FAILURE);
                                return EXIT_FAILURE;
                            }
                            /************ END WRAP SOCKET WITH SSL ************/
                            insert_table_with_context(table, new_socket, temp_addr, https_sock, client_context);
                        }
                        num_client++;
                        FD_SET(new_socket, readfds);
                        if (new_socket > max_sd)
                        {
                            max_sd = new_socket;
                        }
                    }
                }
                else
                {
                    // handle client sock
                    Request *request = NULL;

                    printf("Potato*******************************\n");

                    // check the type of connection from table
                    int mode_sock = lookup_table_connection(table, i);

                    if (mode_sock == sock)
                    {
                        client_context = NULL;
                    }
                    else if (mode_sock == https_sock)
                    {
                        printf("Received an SSL connection!\n");
                        client_context = lookup_table_context(table, i);
                    }

                    // ******** Handling HTTP and HTTPS receive ********

                    if ((readret = receive(i, buf, client_context)) >= 1)
                    {
                        printf("Received!!!\n");

                        // ******** Read more stuff from the buffer ********
                        char *new_buf = malloc(readret + 1);
                        bzero(new_buf, readret + 1);
                        memcpy(new_buf, buf, readret);
                        int len = readret;

                        if (readret == BUF_SIZE)
                        {
                            printf("Start Reading a new line!!!\n");
                            while (readret == BUF_SIZE)
                            {
                                printf("Reading a new line!!!\n");
                                new_buf = realloc(new_buf, len + BUF_SIZE + 1);
                                new_buf[len] = 0;
                                readret = receive(i, new_buf + len, client_context);
                                len += readret;
                            }
                            // int k, val;
                            // for (k = 0; k < request->header_count; ++k)
                            // {
                            //     Request_header header = request->headers[k];
                            //     if (strcmp(header.header_name, "Content-Length") == 0)
                            //     {
                            //         val = atoi(header.header_value);
                            //         break;
                            //     }
                            // }
                            // if (k == request->header_count)
                            // {
                            //     // send a response of 411
                            //     printf("DID NOT SEE CONTENT LENGTH!\n");
                            //     response = handle_request(NULL, 411, www_file);
                            // }
                            // else
                            // {
                            // new_buf = realloc(new_buf, request->header_length + val + 1);
                            // new_buf[request->header_length + val] = 0;
                            // // TODO check if strlen(new_buf) is the correct starting point
                            // for (k = BUF_SIZE; k < request->header_length + val; k += BUF_SIZE)
                            // {
                            //     bzero(buf, BUF_SIZE);
                            //     readret = receive(i, buf, client_context);
                            //     if (readret < 1)
                            //         break;
                            //     memcpy(new_buf + k, buf, MIN(BUF_SIZE, val - k));
                            // }
                            // if (readret >= 1)
                            // {
                            //     request->buf = new_buf;
                            // }
                            // else
                            // {
                            //     //TODO handle error
                            // }
                            // }
                            // read more stuff from body
                        }

                        // ******** Parsing ********

                        Response *response = NULL;

                        printf("Start parsing... \n");
                        // printf("%s\n", new_buf);
                        // printf("size: %ld\n", len);

                        int mode = mode_sock == https_sock ? 1 : 0;

                        // If the received request is from a logged CGI
                        // socket, then this request is a response
                        if (lookup_table(table, i) == NULL && lookup_table_connection(table, i) != -1)
                        {
                            // pass it into a new parser and attempt to get a response
                            response = parse_response(new_buf, len, i);

                            // then set client_sock to be the original connection
                            client_sock = lookup_table_connection(table, i);

                            // then close the connection with stdout_pipe[0]
                            close(i);

                            // decrease num_client, remove it from hash table,
                            // remove it from fd_set
                            num_client--;
                            FD_CLR(i, readfds);
                            remove_table(table, i);

                            // parsing failed
                            if (response == NULL)
                            {
                                // send 400 to client!
                                new_buf = NULL;
                                response = handle_request(NULL, 500, www_file);
                                printf("Parsing response from CGI failed!\n");
                            }
                        }
                        else
                        // the request is a normal request, parse as a request
                        {
                            request = parse(new_buf, len, i);

                            printf("result of request is %p\n", request);

                            // parsing failed
                            if (request == NULL)
                            {
                                // send a response of 400
                                response = handle_request(NULL, 400, www_file);
                                printf("Parsing request failed!\n");
                            }
                            else
                            {
                                // handle request

                                printf("handling the request!\n");

                                // pre process request for particular errors
                                // then check URI for /cgi/
                                response = handle_request(request, 0, www_file);

                                /************* HANDLE CGI **************/

                                if (response == NULL)
                                {
                                    // if /cgi exists, handle it in a particular handler
                                    char *new_addr = inet_ntoa(((struct sockaddr_in *)lookup_table(table, i))->sin_addr);

                                    int n = lookup_table_connection(table, i);

                                    printf("handling CGI! connection: %d\n", n);

                                    // handling CGI requests
                                    int socket_num = handle_cgi_request(request, log, new_addr, cgi_file, n);
                                    if (socket_num == EXIT_FAILURE)
                                    {
                                        // handling error. send a response of 500
                                        response = handle_request(NULL, 500, www_file);
                                        printf("Handling CGI request failed!\n");
                                    }
                                    else
                                    {
                                        // set max socket, increase num_client
                                        max_sd = MAX(max_sd, socket_num);
                                        num_client++;

                                        // log stdout_pipe[0] socket in the hash table and fd_set
                                        insert_table(table, socket_num, NULL, i);
                                        FD_SET(socket_num, readfds);

                                        mode = 0;

                                        // draft a special response that forwards request to stdin_pipe[1]
                                        response = forward_cgi_request(request);

                                        printf("ready to pass response! \nBuf: %s\nSize: %zd\n", response->buf, response->real_size);
                                    }
                                }

                                /************* END HANDLE CGI **************/
                            }
                        }
                        // printf("reaching end\n");
                        // memset(buf, 0, BUF_SIZE);

                        // ******** Send Reply ********

                        // TODO check if send_reply works properly with CGI and new logics!
                        if (send_reply(request, response, log, table, &readfds, mode) == EXIT_FAILURE)
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

                    if (readret < 0)
                    {
                        // handling SSL read errors and normal read errors
                        if (client_context != NULL)
                        {
                            char msg[100];
                            sprintf(msg, "Error SSL reading from client socket. Error number: %d\n",
                                    SSL_get_error(client_context, readret));
                            error_log(log, "", msg);
                        }
                        error_log(log, "", "Error reading from client socket.\n");
                        lisod_shutdown(EXIT_FAILURE);

                        return EXIT_FAILURE;
                    }
                    if (readret == 0)
                    {
                        if (close_socket_client())
                        {
                            error_log(log, "", "Error closing client socket.\n");
                            lisod_shutdown(EXIT_FAILURE);
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
    lisod_shutdown(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}
