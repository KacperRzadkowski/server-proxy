
#include "xmlfile.h"
#include "connection.h"
#include "record.h"
#include "recordToFormat.h"
#include "recordFromFormat.h"

#include <arpa/inet.h>
#include <sys/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/ioctl.h>


#define BUFFSIZE 16384
#define MAX_CONNECTIONS 100


/* This struct should contain the information that you want
 * keep for one connected client.
 */
struct Client
{
    int socket;
    struct Client* prev;
    struct Client* next;
    char dest;
    char id;
    char type;
};

typedef struct Client Client;

void usage( char* cmd )
{
    fprintf( stderr, "Usage: %s <port>\n"
                     "       This is the proxy server. It takes as imput the port where it accepts connections\n"
                     "       from \"xmlSender\", \"binSender\" and \"anyReceiver\" applications.\n"
                     "       <port> - a 16-bit integer in host byte order identifying the proxy server's port\n"
                     "\n",
                     cmd );
    exit( -1 );
}

Client* createClient(int socket){
    Client * client = malloc(sizeof(Client));
    if(client == NULL){
        fprintf(stderr,"handleNewClient failed to alloc client returning NULL\n");
        return NULL;
    }
    client -> socket = socket;
    client -> next = NULL;
    client -> prev = NULL;
    client -> id = '$';
    client -> dest = '$';
    return client;
}

/*
 * This function is called when a new connection is noticed on the server
 * socket.
 * The proxy accepts a new connection and creates the relevant data structures.
 *
 * *** The parameters and return values of this functions can be changed. ***
 */
Client* handleNewClient( int server_sock, Client * prev_client, int* max_fd_value)
{
    int socket = tcp_accept(server_sock);
    if(socket == -1){
        fprintf(stderr,"handleNewClient returned NULL\n");
        return NULL;
    }
    Client * client = createClient(socket);
    if(client == NULL){
        tcp_close(socket);
        fprintf(stderr,"Failed to alloc client in newClient\n");
        return NULL;
    }
    if(prev_client != NULL){
        client -> prev = prev_client;
        prev_client -> next = client;
    }

    if(client -> socket > *max_fd_value){
        *max_fd_value = socket;
    }

    printf("New client with socket : %d\n",client -> socket);
    return client;
}

int freeClient(Client ** client){
    if(*client != NULL){
        if((*client )-> socket >= 0){
            tcp_close((*client) -> socket);
            free(*client);
            client = NULL;
            return 1;
        }
        fprintf(stderr,"Negative socket value in freeClient, client socket: %d freeing Client\n", (*client) -> socket);
        free(client);
        *client = NULL;
        return -1;
    }
    else{
        return -1;
    }
}


/*
 * This function is called when a connection is broken by one of the connecting
 * clients. Data structures are clean up and resources that are no longer needed
 * are released.
 *
 * *** The parameters and return values of this functions can be changed. ***
 */
int removeClient( Client** client,int max_fd_value)
{
    if ((*client)->next) {
        (*client)->next->prev = (*client)->prev;
    }
    if ((*client)->prev) {
        (*client)->prev->next = (*client)->next;
    }
    if((*client) -> socket == max_fd_value){
        max_fd_value--;
    }
    if(check_error(freeClient(client),"freeClient failed") == -1){
        return -1;
    }
    *client = NULL;
    return max_fd_value;
}

/*
 * This function is called when the proxy received enough data from a sending
 * client to create a Record. The 'dest' field of the Record determines the
 * client to which the proxy should send this Record.
 *
 * If no such client is connected to the proxy, the Record is discarded without
 * error. Resources are released as appropriate.
 *
 * If such a client is connected, this functions find the correct socket for
 * sending to that client, and determines if the Record must be converted to
 * XML format or to binary format for sendig to that client.
 *
 * It does then send the converted messages.
 * Finally, this function deletes the Record before returning.
 *
 * *** The parameters and return values of this functions can be changed. ***
 */
Client* findClientByDest(Client* sender_client, Record* record){
    Client* tempClient;
    if(sender_client -> next != NULL){
        tempClient = sender_client -> next;
        while(tempClient -> next != NULL){
            if(record -> dest == tempClient -> id && sender_client -> type == tempClient -> dest){
                return tempClient;
            }
            tempClient = tempClient -> next;
        }
        if(record -> dest == tempClient -> id && sender_client -> type == tempClient -> dest){
            return tempClient;
        }
    }
    if(sender_client -> prev != NULL){
        tempClient = sender_client -> prev;
        while(tempClient -> prev != NULL){
            if(record -> dest == tempClient -> id && sender_client -> type == tempClient -> dest){
                return tempClient;
            }
            tempClient = tempClient -> prev;
        }
        if(record -> dest == tempClient -> id && sender_client -> type == tempClient -> dest){
            return tempClient;
        }
    }
    return NULL;
}



int forwardMessage( Record*record, Client* sender_client, int bytes_read)
{
    if(record == NULL){
        fprintf(stderr,"record is null in forwardMessage\n");
        deleteRecord(record);
        return -1;
    }
    if(sender_client == NULL){
        fprintf(stderr,"Client is null in forwardMessage\n");
        deleteRecord(record);
        return -1;
    }
    printf("forwarding message sent by Client with ID: %c\n",sender_client -> id);
    Client* reciever;
    if((reciever = findClientByDest(sender_client,record)) == NULL){
        deleteRecord(record);
        fprintf(stderr,"Could not find corresponding reciever to sender with DEST: %c\n", record -> dest);
        return -1;
    }
    printf("FOUND RECEIVER\n");
    char * data;
    if(reciever -> dest == 'X'){
        if((data = recordToXML(record,&bytes_read)) == NULL){
            deleteRecord(record);
            free(data);
            return -1;
        }
        if((bytes_read = check_error(tcp_write(reciever -> socket, data, strlen(data)),"tcp_write failed in forwardMessage")) == -1){
            deleteRecord(record);
            free(data);
            return -1;
        }
        free(data);
    }
    else if(reciever -> dest == 'B'){
        if((data = recordToBinary(record,&bytes_read)) == NULL){
            deleteRecord(record);
            free(data);
            return -1;
        }
        if((bytes_read = check_error(tcp_write(reciever -> socket, data, bytes_read),"tcp_write failed in forwardMessage")) == -1){
            deleteRecord(record);
            free(data);
            return -1;
        }
        free(data);
    }
    else{
        fprintf(stderr,"Receiver has an invalid type\n");
        deleteRecord(record);
        return -1;
    }
    deleteRecord(record);
    return 1;
}



int extractIdAndDest(Client * client, char * buffer, int read_count){
    if((read_count = check_error(tcp_read(client -> socket,buffer,1),"tcp_read failed in proxy")) == -1){
        return -1;
    }
    if(read_count == 1){
        if((buffer[0] <= 'Z' && buffer[0] >= 'A')){
            client -> dest = buffer[0];
        }
        else{
            fprintf(stderr,"first byte that client sent is not a valid dest: %c\n", buffer[0]);
            return -1;
        }
    }
    else{
        while(read_count == 0){
            if((read_count = check_error(tcp_read(client -> socket,buffer,1),"tcp_read failed in proxy")) == -1){
                return -1;
            }
        }
        client -> dest = buffer[0];
    }

    if((read_count = check_error(tcp_read(client -> socket,buffer,1),"tcp_read failed in proxy")) == -1){
        return -1;
    }
    if(read_count == 1){
        if((buffer[0] <= 'Z' || buffer[0] >= 'A')){
            client -> id = buffer[0];
        }
        else{
            fprintf(stderr,"second byte that client sent is not a valid dest: %c\n", buffer[0]);
            return -1;
        }
    }
    else{
        while(read_count == 0){
            if((read_count = check_error(tcp_read(client -> socket,buffer,1),"tcp_read failed in proxy")) == -1){
                return -1;
            }
        }
        client -> id = buffer[0];
    }
    return read_count;
}


int handleClient( Client* client )
{
    if(client == NULL){
        printf("client NULL in handleClient\n");
        return -1;
    }
    int read_count;
    Record * record = NULL;
    char * buffer = malloc(BUFFSIZE);
    if(buffer == NULL){
        fprintf(stderr,"Failed to allocate buffer in handleClient");
        return -1;
    }
    memset(buffer,0,BUFFSIZE);
    if(client -> dest == '$' && client -> id == '$'){
        if((check_error(extractIdAndDest(client,buffer,read_count),"extractIdAndDest failed in handleClient\n")) == -1){
            free(buffer);
            fprintf(stderr,"Failed to set client ID and Dest\n");
            return -1;
        }
        free(buffer);
        printf("Set ID an Dest for client %d ID: %c Dest: %c\n",client -> socket, client -> id, client ->dest);
        return 1;
    }
    //? alternativt bara fortsätt läsa?
    if((read_count = tcp_read(client -> socket,buffer,BUFFSIZE)) == -1){
        free(buffer);
        return -1;
    }
    if(read_count == 0){
        free(buffer);
        return 0;
    }
    do{
        if(read_count != 0){
            if(buffer[0] == '<'){
                record = XMLtoRecord(buffer,BUFFSIZE,&read_count);
                client -> type = 'X';
            }else{
                //* this happens because of memset in parsers, the buffer may consist only of null bytes and thus is done being handled
                if(client -> type){
                    if(client -> type == 'X'){
                        break;
                    }
                }
                record = BinaryToRecord(buffer,BUFFSIZE,&read_count);
                client -> type = 'B';
            }
            if(record != NULL ){
                if(check_error(forwardMessage(record,client,read_count),"forwardMessage failed") == -1){
                    deleteRecord(record);
                    break;
                }
            }
        }
    }
    while(read_count);
    free(buffer);
    if(record){
        deleteRecord(record);
    }
    return 1;
}

Client* findClientBySocket(int socket, Client* last_client){
    while(last_client -> prev){
        if(last_client -> socket == socket){
            return last_client;
        }
        last_client = last_client -> prev;
    }
    if(last_client -> socket == socket){
        return last_client;
    }
    else{
        fprintf(stderr, "Could not find client in findClient socket: %d\n",socket);
        return NULL;
    }
}

//* This function frees server client, closes its socket and doublechecks if any more clients exist
int cleanup(Client **first_client) {
    Client ** remove_client;
    while (*first_client) {
        if((*first_client) -> next){
            remove_client = first_client;
            first_client = &((*first_client)->next);
            if (check_error(freeClient(remove_client), "freeClient failed in cleanup") == -1) {
                return -1;
            }
        }else{
            if (check_error(freeClient(first_client), "freeClient failed in cleanup") == -1) {
                return -1;
            }
            return 1;
        }
    }
    if (check_error(freeClient(first_client), "freeClient failed in cleanup") == -1) {
        return -1;
    }
    return 1;
}


int main( int argc, char* argv[] )
{
    int port;
    int server_sock;

    if( argc != 2 )
    {
        usage( argv[0] );
    }

    port = atoi( argv[1] );

    server_sock = tcp_create_and_listen( port );
    if( server_sock < 0 ) exit( -1 );
    /* add your initialization code */
    Client * server_client;
    if((server_client = createClient(server_sock)) == NULL){
        fprintf(stderr,"Server client alloc failed\n");
        tcp_close(server_sock);
        return -1;
    }
    int max_fd_value = server_sock;
    int conn_counter = 0;
    int num_ready_sock = 0;
    int error_counter;

    fd_set sock_set;
    fd_set current_sock_set;
    FD_ZERO(&sock_set);
    FD_SET(server_sock, &sock_set);
    Client* current_client;
    Client* last_client;
    printf("proxy initialized\n");

    /*
     * The following part is the event loop of the proxy. It waits for new connections,
     * new data arriving on existing connection, and events that indicate that a client
     * has disconnected.
     *
     * This function uses handleNewClient() when activity is seen on the server socket
     * and handleClient() when activity is seen on the socket of an existing connection.
     *
     * The loops ends when no clients are connected any more.
     */

    do
    {
        current_sock_set = sock_set;
        if((num_ready_sock = check_error(tcp_wait(&current_sock_set, MAX_CONNECTIONS),"Select failed")) < 1){
            break;
        }
        if(num_ready_sock > 0){
            //* The following block handles activity on the server socket
            if(FD_ISSET(server_sock,&current_sock_set)){
                if(conn_counter == 0){
                    last_client = handleNewClient(server_sock,server_client,&max_fd_value);
                }
                else{
                    last_client = handleNewClient(server_sock,last_client,&max_fd_value);
                }
                if(last_client == NULL){
                    fprintf(stderr,"Client is NULL in main\n");
                }
                else{
                    FD_SET(last_client-> socket, &sock_set);
                    printf("New client connected to proxy : %d\n",last_client -> socket);
                    conn_counter++;
                }
            }
            //* The following block handles activity on the client sockets
            for(int sock = 0; sock <= max_fd_value; sock++ ){
                if(FD_ISSET(sock,&current_sock_set) && sock != server_sock){
                    if((current_client = findClientBySocket(sock,last_client)) != NULL){
                        printf("Handling client: %d\n", current_client -> socket);
                        if((error_counter=handleClient(current_client)) <= 0){
                            FD_CLR(current_client -> socket, &sock_set);
                            if((error_counter = check_error(removeClient(&current_client, max_fd_value),"removeClient failed")) == -1);
                            else{
                                max_fd_value = error_counter;
                            }
                            conn_counter--;
                        }
                    }
                    else{
                        FD_CLR(current_client -> socket, &sock_set);
                        conn_counter--;
                    }
                }
            }
        }
        printf("Connections: %d\n",conn_counter);
    }
    while( conn_counter > 0);

    cleanup(&server_client);

    return 0;
}

