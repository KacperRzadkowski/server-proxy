#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/errno.h>

/*
 * This is a file that implements the operation on TCP sockets that are used by
 * all of the programs used in this assignment.
 *
 * *** YOU MUST IMPLEMENT THESE FUNCTIONS ***
 *
 * The parameters and return values of the existing functions must not be changed.
 * You can add function, definition etc. as required.
 */
#include "connection.h"

int check_error(int value, char * msg){
    if(value < 0){
        fprintf(stderr,"%s ; %s\n",msg,strerror(errno));
        return -1;
    }
    return value;
}

struct sockaddr_in tcp_create_default_address(int port){
    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    return sock_addr;
}

int tcp_connect( char* hostname, int port )
{
    int connect_socket,write_counter;
    connect_socket = socket(AF_INET,SOCK_STREAM,0);
    if((check_error(connect_socket,"socket creation failed in tcp_connect\n")) == -1){
        tcp_close(connect_socket);
        return -1;
    }
    struct sockaddr_in dest_addr = tcp_create_default_address(port);
    write_counter = inet_pton(AF_INET,hostname,&dest_addr.sin_addr.s_addr);
    if((check_error(write_counter," inet_pton failed\n")) == -1 || (check_error(write_counter," inet_pton invalid IP format")) == 0){
        tcp_close(connect_socket);
        return -1;
    }
    write_counter = connect(connect_socket,(struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if((check_error(write_counter,"connect failed\n")) == -1){
        tcp_close(connect_socket);
        return -1;
    }
    return connect_socket;
}

int tcp_read( int sock, char* buffer, int n )
{
    int read_counter = read(sock, buffer, n);
    if(check_error(read_counter,"read failed") == -1)return -1;
    else if(read_counter == 0){
        fprintf(stderr,"EOF - nothing to read\n");
        return 0;
    }
    buffer[read_counter] = '\0';
    return read_counter;
}

int tcp_write( int sock, char* buffer, int bytes )
{
    int write_counter = write(sock, buffer, bytes);
    if(check_error(write_counter,"write failed") == -1){
        printf("freeing buffer");
        return -1;
    }
    else if(write_counter == 0){
        perror("tcp_write wrote 0 bytes\n");
        return 0;
    }
    return write_counter;
}

int tcp_write_loop( int sock, char* buffer, int bytes )
{
    int write_counter,bytes_sent = 0;
    while(bytes_sent < bytes){
        write_counter = write(sock, buffer, bytes);
        if(check_error(write_counter,"write failed") == -1){
            return -1;
        }
        bytes_sent = bytes_sent+write_counter;
    }
    return write_counter;
}

void tcp_close( int sock )
{
    close(sock);
    printf("closed successfully socket: %d\n",sock);
}

int tcp_create_and_listen( int port )
{
    int server_socket = socket(AF_INET,SOCK_STREAM,0);
    if(check_error(server_socket,"creating socket failed in tcp_create_and_listen\n") == -1){
        tcp_close(server_socket);
        return -1;
    }
    struct sockaddr_in sock_addr = tcp_create_default_address(port);
    if(check_error(bind(server_socket,(struct sockaddr*)&sock_addr,sizeof(struct sockaddr_in)),"bind in tcp_create_and_listen failed\n") == -1){
        tcp_close(server_socket);
        return -1;
    }
    if(check_error(listen(server_socket,0),"socket failed to listen in tcp_create_and_listen\n") == -1){
        tcp_close(server_socket);
        return -1;
    }
    return server_socket;
}

int tcp_accept( int server_sock )
{
    int accepted_socket = accept(server_sock,NULL,NULL);
    if(check_error(accepted_socket,"accept failed") == -1){
        tcp_close(accepted_socket);
        return -1;
    }
    return accepted_socket;
}

int tcp_wait( fd_set* waiting_set, int wait_end )
{
    int ready_counter;
    if((ready_counter = check_error(select(FD_SETSIZE,waiting_set,NULL,NULL,NULL),"select failed in tcp_wait")) < 0)
    return -1;
    if(ready_counter > 0 && ready_counter <= wait_end)return ready_counter;
    else{
        fprintf(stderr,"Maximum amount of clients reached\n");
        return -1;
    }
}

int tcp_wait_timeout( fd_set* waiting_set, int wait_end, int seconds )
{
    int ready_counter;
    struct timeval time_interval;
    time_interval.tv_sec = seconds;
    time_interval.tv_usec = 0;
    printf("Sleeping for : %d seconds\n",seconds);
    if((ready_counter = check_error(select(wait_end,waiting_set,NULL,NULL,&time_interval),"select failed in tcp_wait_timeout")) == -1)return -1;
    else if(ready_counter == 0){
        printf("time limit expired\n");
        return 0;
    }
    else if(ready_counter < wait_end){
        return ready_counter;
    }
    else{
        fprintf(stderr,"Maximum amount of clients reached\n");
        return -1;
    }
}


