#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <unistd.h>

#define SUCCESS 0
#define LOG_FAILURE 2;

//Header field
typedef struct
{
    char *app_name;
    time_t time;
    pid_t pid;
} Log_header;

//HTTP Request Header
typedef struct
{
    Log_header *header;
    FILE *file;

} Log;

Log *log_init_default(const char *file_name);

int write_log(Log *log, const char *buf);

int error_log(Log *log, char *ip_buf, const char *err_msg);

int access_log(Log *log, char *ip_buf, const char *usr, const char *request, int req_num, int size);

int close_log(Log *log);