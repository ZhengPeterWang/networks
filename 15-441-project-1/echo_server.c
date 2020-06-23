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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "parse.h"

#define ECHO_PORT 9999
#define BUF_SIZE 4096
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
    int sock, client_sock[MAX_CLIENT];
    ssize_t readret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
    {
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        return EXIT_FAILURE;
    }

    if (listen(sock, 5))
    {
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        client_sock[i] = 0;
    }

    fd_set readfds;

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int max_sd = sock;

        //add child sockets to set
        for (int i = 0; i < MAX_CLIENT; i++)
        {
            //socket descriptor
            int sd = client_sock[i];

            //if valid socket descriptor then add to read list
            if (sd > 0)
                FD_SET(sd, &readfds);

            //highest file descriptor number, need it for the select function
            if (sd > max_sd)
                max_sd = sd;
        }

        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            close(sock);
            fprintf(stderr, "Error select.\n");
            return EXIT_FAILURE;
        }
        if (FD_ISSET(sock, &readfds))
        {
            cli_size = sizeof(cli_addr);
            int new_socket;
            if ((new_socket = accept(sock, (struct sockaddr *)&cli_addr,
                                     &cli_size)) == -1)
            {
                close(sock);
                fprintf(stderr, "Error accepting connection.\n");
                return EXIT_FAILURE;
            }
            printf("New connection , socket fd is %d with port %d\n", new_socket, ntohs(cli_addr.sin_port));

            for (int i = 0; i < MAX_CLIENT; i++)
            {
                if (client_sock[i] == 0)
                {
                    client_sock[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENT; i++)
        {
            int sd = client_sock[i];

            readret = 0;

            if (FD_ISSET(sd, &readfds))
            {
                printf("Potato*******************************\n");
                while ((readret = recv(sd, buf, BUF_SIZE, 0)) >= 1)
                {
                    printf("Start parsing... \n");

                    char new_arr[] = "GET / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n";

                    for (int i = 0; i < strlen(new_arr); ++i)
                    {
                        printf("%d,", new_arr[i]);
                    }
                    printf("\n");

                    Request *request = parse(new_arr, strlen(new_arr), sd);

                    // Request *request = parse(buf, strlen(buf), sd);

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
                        if (send(sd, res, readret, 0) != readret)
                        {
                            close_socket(sd);
                            close_socket(sock);
                            fprintf(stderr, "Error sending to client.\n");
                            return EXIT_FAILURE;
                        }
                        printf("done\n");
                        // parsing failed
                        // send a response of 400
                    }

                    else
                    {
                        printf("Parsing succeeded!\n");
                        if (send(sd, buf, readret, 0) != readret)
                        {
                            close_socket(sd);
                            close_socket(sock);
                            fprintf(stderr, "Error sending to client.\n");
                            return EXIT_FAILURE;
                        }
                    }
                    printf("reaching end\n");
                    memset(buf, 0, BUF_SIZE);
                    if (readret < BUF_SIZE)
                        break;
                }

                if (readret == -1)
                {
                    close_socket(sd);
                    close_socket(sock);
                    fprintf(stderr, "Error reading from client socket.\n");
                    return EXIT_FAILURE;
                }
                printf("Closing client %d's socket\n", sd);
                if (close_socket(sd))
                {
                    close_socket(sock);
                    fprintf(stderr, "Error closing client socket.\n");
                    return EXIT_FAILURE;
                }
                else
                {
                    client_sock[i] = 0;
                }
            }
        }
    }

    printf("Token\n");

    close_socket(sock);

    return EXIT_SUCCESS;
}
