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
#include <signal.h>
#include "cjson/cJSON.h"
#include "cjson/cJSON.c"

#define MAX_BUFF 500
#define NAME_LEN 20
#define MAX_CONN 5

static _Atomic unsigned int num_conns;

struct client_conn {
  struct sockaddr_in socket;
  char user[NAME_LEN];
  int status;
  int fd;
};

struct client_conn *conns[MAX_CONN];


pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

void queue_add(struct client_conn *cl){
	pthread_mutex_lock(&client_mutex);

	for(int i=0; i < MAX_CONN; ++i){
		if(!conns[i]){
			conns[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&client_mutex);
}

void queue_remove(char *uid){
	pthread_mutex_lock(&client_mutex);

	for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
			if(strcmp(conns[i]->user, uid) == 0){
				conns[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&client_mutex);
}

void print_users() {
  printf("lista de usuarios conectados: ");
  for (int i = 0; i < MAX_CONN; i++) {
    if(conns[i] != NULL) {
      printf("%s", conns[i]->user);
    } else {
      printf("vacio");
    }
    if (i < MAX_CONN - 1) printf(", ");
  }
  printf("\n");
}

void simple_response(int fd, char *response, int code) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "response", response);
  cJSON_AddNumberToObject(json, "code", code);
  char *string = cJSON_Print(json);
  send(fd, string, strlen(string), 0);
  cJSON_Delete(json);
}

int send_message(char *buff) {
  char u_name[NAME_LEN];
  char from_name[NAME_LEN];
  cJSON *json = cJSON_Parse(buff);
  cJSON *body = NULL;
  cJSON *to = NULL;
  cJSON *from = NULL;

  body = cJSON_GetObjectItemCaseSensitive(json, "body");
  if (!cJSON_IsObject(body)) return (-1);

  to = cJSON_GetObjectItemCaseSensitive(body, "to");
  from = cJSON_GetObjectItemCaseSensitive(body, "from");

  if(!cJSON_IsString(to) || to->valuestring == NULL || !cJSON_IsString(from) || from->valuestring == NULL) return (-1);
  strcpy(u_name, to->valuestring);
  strcpy(from_name, from->valuestring);

  
  cJSON *newjson = cJSON_CreateObject();
  cJSON *newbody = cJSON_Duplicate(body, 1);
  cJSON_Delete(json);
  int sendall = 0;
  int sendsucc = 0;


  cJSON_AddItemToObject(newjson, "body", newbody);
  cJSON_AddStringToObject(newjson, "request", "NEW_MESSAGE");
  char *string = cJSON_Print(newjson);
  cJSON_Delete(newjson);



  if(strcmp(u_name, "all") == 0) sendall = 1;

  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
			if((sendall || strcmp(conns[i]->user, u_name) == 0) && strcmp(conns[i]->user, from_name) != 0){
				sendsucc = 1;
        send(conns[i]->fd, string, strlen(string), 0);
				if(!sendall) break;
			}
		}
	}
  if (sendsucc == 1) return(1);
  return(-1);
}

void put_status(int status, char *user) {
  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
			if(strcmp(conns[i]->user, user) == 0){
        conns[i]->status = status;
				break;
			}
		}
	}
}

void get_users(int fd) {
  cJSON *json = cJSON_CreateObject();
  cJSON *users = cJSON_AddArrayToObject(json, "body");
  char *string;
  
  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
      cJSON *user = cJSON_CreateString(conns[i]->user);
			cJSON_AddItemToArray(users, user);
		}
	}
  cJSON_AddStringToObject(json, "response", "GET_USER");
  cJSON_AddNumberToObject(json, "code", 200);
  string = cJSON_Print(json);
  cJSON_Delete(json);
  send(fd, string, strlen(string), 0);
}

void get_user(int fd, char *user) {
  cJSON *json = cJSON_CreateObject();
  char ipstr[20];
  char *string;

  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
      if(strcmp(conns[i]->user, user) == 0){
				sprintf(ipstr, "%d.%d.%d.%d",
        conns[i]->socket.sin_addr.s_addr & 0xff,
        (conns[i]->socket.sin_addr.s_addr & 0xff00) >> 8,
        (conns[i]->socket.sin_addr.s_addr & 0xff0000) >> 16,
        (conns[i]->socket.sin_addr.s_addr & 0xff000000) >> 24);
				break;
			}
		}
	}
  cJSON_AddStringToObject(json, "response", "GET_USER");
  cJSON_AddNumberToObject(json, "code", 200);
  cJSON_AddStringToObject(json, "body", ipstr);
  string = cJSON_Print(json);
  cJSON_Delete(json);
  send(fd, string, strlen(string), 0);
}

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
    cJSON *body = NULL;

    request = cJSON_GetObjectItemCaseSensitive(json, "request");
    body = cJSON_GetObjectItemCaseSensitive(json, "body");

    if (!cJSON_IsString(request) || request->valuestring == NULL
              || strncmp(request->valuestring, "INIT_CONEX", 10) != 0
              || !cJSON_IsObject(body)) goto end1;
    
    u_name = cJSON_GetObjectItemCaseSensitive(body, "user_id");

    if (!cJSON_IsString(u_name) || u_name->valuestring == NULL) goto end1;

    strcpy(username, u_name->valuestring);
    int isUserUsed = 0;

    for (int i = 0; i < MAX_CONN; i++) {
      if(conns[i] != NULL) {
        if (strcmp(conns[i]->user, username) == 0) {
          isUserUsed = 1;
          break;
        }
      }
    }  

    if (isUserUsed) {
      printf("Usuario ya existe\n");
      simple_response(client_connection->fd, "INIT_CONEX", 101);
      leave_flag = 1;
      goto end1;
    }  

    strcpy(client_connection->user, username);

    //print_users();

    printf("El usuario '%s' se ha conectado\n", username);

    simple_response(client_connection->fd, "INIT_CONEX", 200);

end1:
    bzero(buff, MAX_BUFF);
    cJSON_Delete(json);
  } else {
    leave_flag = 1;
  }

  while(1) {
    if(leave_flag) {
      break;
    }

    int receive = recv(client_connection->fd, buff, MAX_BUFF, 0);

    if(receive > 0) {

      cJSON *json = cJSON_Parse(buff);
      cJSON *request = NULL;
      request = cJSON_GetObjectItemCaseSensitive(json, "request");

      if (!cJSON_IsString(request) || request->valuestring == NULL) {
        leave_flag = 1;
        continue;
      }
      //end connection
      if(strcmp(request->valuestring, "END_CONEX") == 0) {
        leave_flag = 1;
        printf("Cliente ha enviado petición de desconexión.\n");
        simple_response(client_connection->fd, "END_CONEX", 200);

      // send message
      } else if (strcmp(request->valuestring, "POST_CHAT") == 0) {
        send_message(buff);
      } else if (strncmp(request->valuestring, "PUT_STATUS", 10) == 0) {
        cJSON *status = NULL;
        status = cJSON_GetObjectItemCaseSensitive(json, "body");
        if (cJSON_IsNumber(status)) {
          put_status(status->valueint, client_connection->user);
          simple_response(client_connection->fd, "PUT_STATUS", 200);
        }
      } else if (strncmp(request->valuestring, "GET_USER", 8) == 0) {
        cJSON *body = NULL;
        char dest_user[NAME_LEN];

        body = cJSON_GetObjectItemCaseSensitive(json, "body");

        if (cJSON_IsString(body) && body->valuestring != NULL) {
          if (strncmp(body->valuestring, "all", 3) == 0)
          get_users(client_connection->fd);
          else {
            strcpy(dest_user, body->valuestring);
            get_user(client_connection->fd, dest_user);
          }
        }

      }

      cJSON_Delete(json);

      bzero(buff, MAX_BUFF);
    } else if (receive == 0) {
      printf("Usuario %s se desconecto\n", client_connection->user);
      leave_flag = 1;
    } else {
      printf("ERROR\n");
    }
  }

  close(client_connection->fd);
  queue_remove(client_connection->user);
  //print_users();
  free(client_connection);
  num_conns--;
  pthread_detach(pthread_self());

  return NULL;
}

int main(int argc, char *argv[]) {

  struct sockaddr_in server;
  struct sockaddr_in client;
  int fd, conn, port;
  pthread_t tid;

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

  if (fd < 0) {
    printf("Error en creación de socket\n");
    return (-1);
  }

  if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
    printf("Error en enlace de socket a ip\n");
    return (-1);
  }

  if (listen(fd, MAX_CONN) < 0) {
    printf("Error en escucha de conexiones\n");
    return (-1);
  }

  while(1){
    socklen_t client_len = sizeof(client);
    conn = accept(fd, (struct sockaddr *)&client, &client_len);

    if (conn < 0) {
      printf("Conexión no aceptada\n");
      return (-1);
    }

    if((num_conns + 1) >= MAX_CONN) {
      printf("No se pueden agregar más clientes, cantidad máxima alcanzada.\n");
      simple_response(conn, "INIT_CONEX", 105);
      close(conn);
      continue;
    }

    struct client_conn *client_connection = (struct client_conn*)malloc(sizeof(struct client_conn));
    client_connection->socket = client;
    client_connection->fd = conn;
    client_connection->status = 0;
    strcpy(client_connection->user, "");

    int continue_flag = 0;
    /*
    for (int i = 0; i < MAX_CONN; i++) {
      if(conns[i] != NULL) {
        if (conns[i]->socket.sin_addr.s_addr == client_connection->socket.sin_addr.s_addr) {
          simple_response(client_connection->fd, "INIT_CONEX", 105);
          free(client_connection);
          continue_flag = 1;
        }
      }
    }  */
    if (continue_flag) continue;

    queue_add(client_connection);

    pthread_create(&tid, NULL, &handle_conn, (void*)client_connection);

    sleep(1);
  }

  return (0);
}