#include <log.h>
#define SIZE 1024

Log *log_init_default(const char *file)
{
    Log_header *log_header = (Log_header *)malloc(sizeof(Log_header));
    log_header->pid = getpid();
    log_header->app_name = "Liso";
    log_header->time = time(NULL);

    Log *log = (Log *)malloc(sizeof(Log));
    log->header = log_header;
    log->file = fopen(file, "w");

    if (file == NULL)
    {
        fprintf(stderr, "Error opening file.\n");
        return NULL;
    }

    // const char *first_buf;

    // sprintf(first_buf, "Liso server v 0.2!~ (゜-゜)つロ\n %t, %ld\n", log_header->time, (long)log_header->pid);

    // int err_num;
    // if ((err_num = write_log(log, first_buf)) != SUCCESS)
    // {
    //     fprintf(stderr, "Error processing file %s\n", log->file_name);
    //     return NULL;
    // }

    return log;
}

int error_log(Log *log, char *ip_buf, const char *err_msg)
{
    char *first_buf;

    time_t t = time(NULL);

    struct tm *tt = localtime(&t);

    char *new_time = (char *)malloc(SIZE);

    strftime(new_time, SIZE, "%d/%b/%Y:%H:%M:%S %z", tt);
    sprintf(first_buf, "[%s] [error] [client %s] %s\n", new_time, ip_buf, err_msg);

    int err_num;
    if ((err_num = write_log(log, first_buf)) != SUCCESS)
    {
        fprintf(stderr, "Error processing file\n");
        exit(0);
    }
    return SUCCESS;
}

int access_log(Log *log, char *ip_buf, const char *usr, const char *request, int req_num, int size)
{
    printf("in access log\n");

    char *first_buf = (char *)malloc(SIZE);

    time_t t = time(NULL);

    struct tm *tt = localtime(&t);

    char *new_time = (char *)malloc(SIZE);

    printf("before strftime\n");

    strftime(new_time, SIZE, "%d/%b/%Y:%H:%M:%S %z", tt);

    printf("in strftime\n");

    sprintf(first_buf, "%s - %s [%s] \"%s\" %d %d\n", ip_buf, usr, new_time, request, req_num, size);

    printf("after strftime\n");

    int err_num;
    if ((err_num = write_log(log, first_buf)) != SUCCESS)
    {
        fprintf(stderr, "Error processing file\n");
        exit(0);
    }
    return SUCCESS;
}

int write_log(Log *log, const char *buf)
{
    FILE *file = log->file;
    if (file == NULL)
    {
        fprintf(stderr, "Error opening file.\n");
        return LOG_FAILURE;
    }
    int err_num;
    if ((err_num = fputs(buf, file)) == 0)
    {
        fprintf(stderr, "Error writing to file with error number %d.\n", err_num);
        return LOG_FAILURE;
    }
    if ((err_num = fflush(file)) != 0)
    {
        fprintf(stderr, "Error flushing file with error number %d.\n", err_num);
        return LOG_FAILURE;
    }
    return SUCCESS;
}

int close_log(Log *log)
{
    FILE *file = log->file;
    int err_num;
    if ((err_num = fclose(file)) != 0)
    {
        fprintf(stderr, "Error closing file with error number %d.\n", err_num);
        return LOG_FAILURE;
    }
    free(log->header);
    free(log);
    return SUCCESS;
}