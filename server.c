#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cjson/cJSON.h"
#include "cjson/cJSON.c"

#define MAX_BUFF 500
#define MESSAGE_LEN 1000
#define MAX_CONN 5
#define NAME_LEN 20

static _Atomic unsigned int num_conns;

struct client_conn {
  struct sockaddr_in socket;
  char user[NAME_LEN];
  int status;
  int fd;
};

struct client_conn *conns[MAX_CONN];
struct sockaddr_in server;
struct sockaddr_in client;
int fd, conn, port;
char message[MESSAGE_LEN] = "";

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t tid;


void * handle_conn(void *arg) {
  char buff[MAX_BUFF];
  char username[NAME_LEN];
  int leave_flag = 0;
  num_conns++;
  struct client_conn *client_connection = (struct client_conn*) arg;

  if(recv(client_connection->fd, buff, MAX_BUFF, 0) > 0) {
    cJSON *json = cJSON_Parse(buff);
    cJSON *u_name = NULL;
    cJSON *request = NULL;
    cJSON_Delete(json);
  }

  while(1) {
    if(leave_flag) {
      break;
    }

    int receive = recv(client_connection->fd, buff, MAX_BUFF, 0);

    if(receive > 0) {
      printf("Message Received: %s\n", buff);
      bzero(buff, MAX_BUFF);
    } else if (receive == 0) {
      printf("Se desconecto\n");
      leave_flag = 1;
    } else {
      printf("ERROR\n");
    }
  }

  close(client_connection->fd);
  //free(client_connection);
  num_conns--;
  pthread_detach(pthread_self());

  return NULL;
}

int main(int argc, char *argv[]) {
  if(argc != 2) {
    printf("Número incorrecto de argumentos para iniciar el servidor\n");
    return(-1);
  }

  port = atoi(argv[1]);

  if(port <= 1023 || port > 65535) {
    printf("Número de puerto inválido\n");
    return(-1);
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = INADDR_ANY;

  fd = socket(AF_INET, SOCK_STREAM, 0);

  bind(fd, (struct sockaddr *) &server, sizeof(server));

  listen(fd, MAX_CONN);

  while(1){
    socklen_t client_len = sizeof(client);
    conn = accept(fd, (struct sockaddr *)&client, &client_len);

    if((num_conns + 1) >= MAX_CONN) {
      printf("No se pueden agregar más clientes, cantidad máxima alcanzada.\n");
      close(conn);
      continue;
    }

    struct client_conn *client_connection;
    client_connection->socket = client;
    client_connection->fd = conn;
    client_connection->status = 0;

    pthread_mutex_lock(&client_mutex);
    for (int i=0; i<MAX_CONN; i++) {
      if(!conns[i]){
        conns[i] = client_connection;
        break;
      }
    }
    pthread_mutex_unlock(&client_mutex);
    pthread_create(&tid, NULL, &handle_conn, (void*)client_connection);

    sleep(1);
  }

  return (0);
}