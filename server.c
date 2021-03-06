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
#define MAX_CONN 30
#define MAX_MESSAGES 100
#define MESSAGE_LEN 100


static _Atomic unsigned int num_conns;
static _Atomic unsigned int current_mess;

struct client_conn {
  struct sockaddr_in socket;
  char user[NAME_LEN];
  int status;
  int fd;
};

struct message_s {
  char message[MESSAGE_LEN];
  char time_s[20];
  char from[NAME_LEN];
  char to[NAME_LEN];
};

struct client_conn *conns[MAX_CONN];

struct message_s *messages[MAX_MESSAGES];


pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;
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

void add_message (struct message_s *me){
	pthread_mutex_lock(&message_mutex);

	for(int i=0; i < MAX_MESSAGES; ++i){
		if(!messages[i]){
			messages[i] = me;
			break;
		}
	}

	pthread_mutex_unlock(&message_mutex);
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
  char deliv[30];
  char messa[MESSAGE_LEN];
  cJSON *json = cJSON_Parse(buff);
  cJSON *body = NULL;

  body = cJSON_GetObjectItemCaseSensitive(json, "body");
  if (!cJSON_IsArray(body)) return (-1);

  cJSON *to;
  cJSON *from;
  cJSON *delivered;
  cJSON *mess;

  to = cJSON_GetArrayItem(body, 3);
  from = cJSON_GetArrayItem(body, 1);
  delivered = cJSON_GetArrayItem(body, 2);
  mess = cJSON_GetArrayItem(body, 0);

  if(!cJSON_IsString(to) || to->valuestring == NULL || !cJSON_IsString(from) || from->valuestring == NULL) return (-1);
  strcpy(u_name, to->valuestring);
  strcpy(from_name, from->valuestring);
  strcpy(deliv, delivered->valuestring);
  strcpy(messa, mess->valuestring);

  struct message_s *mess_stru = (struct message_s *)malloc(sizeof(struct message_s));;
  strcpy(mess_stru->to, u_name);
  strcpy(mess_stru->from, from_name);
  strcpy(mess_stru->time_s, deliv);
  strcpy(mess_stru->message, messa);

  add_message(mess_stru);

  
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
  if (sendsucc) return(200);
  return(102);
}

int put_status(int status, char *user) {
  if (!(status == 0 || status == 1 || status == 2)) return(104);
  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
			if(strcmp(conns[i]->user, user) == 0){
        conns[i]->status = status;
				break;
			}
		}
	}
  return (200);
}

void get_message(int fd, char *from, char *to) {
  int accepted = 0;
  int found = 0;
  int send_all = 0;
  if (strncmp(from, "all", 3) == 0) send_all = 1;
  cJSON *json = cJSON_CreateObject();
  cJSON *body;
  cJSON_AddStringToObject(json, "response", "GET_CHAT");

  body = cJSON_AddArrayToObject(json, "body");

  for (int i=0; i<MAX_MESSAGES;i++) {
    accepted = 0;
    if(!messages[i]) continue;
    if (send_all && strncmp(messages[i]->to, "all", 3) == 0) accepted = 1;
    else if (!send_all && strcmp(messages[i]->to, to) == 0 && strcmp(messages[i]->from, from) == 0) {
      accepted = 1;
    } else if (!send_all && strcmp(messages[i]->to, from) == 0 && strcmp(messages[i]->from, to) == 0) {
      accepted = 1;
    }
    if (accepted) {
      cJSON *subarray = cJSON_CreateArray();
      char from[NAME_LEN];
      char timestr[NAME_LEN];
      char messagestr[MESSAGE_LEN];
      strcpy(from, messages[i]->from);
      strcpy(timestr, messages[i]->time_s);
      strcpy(messagestr, messages[i]->message);
      found = 1;

      cJSON *from_s = cJSON_CreateString(from);
      cJSON *delivered = cJSON_CreateString(timestr);
      cJSON *mess = cJSON_CreateString(messagestr);
      cJSON_AddItemToArray(subarray, mess);
      cJSON_AddItemToArray(subarray, from_s);
      cJSON_AddItemToArray(subarray, delivered);

      cJSON_AddItemToArray(body, subarray);
    }
  }
  int code;
  if (found)
    code = 200;
  else
    code = 102;
  cJSON_AddNumberToObject(json, "code", code);
  char *string = cJSON_Print(json);
  cJSON_Delete(json);
  send(fd, string, strlen(string), 0);
}

void get_users(int fd) {
  cJSON *json = cJSON_CreateObject();
  cJSON *users = cJSON_AddArrayToObject(json, "body");
  int found = 0;
  int code = 200;
  char *string;
  
  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
      found = 1;
      cJSON *user = cJSON_CreateString(conns[i]->user);
			cJSON_AddItemToArray(users, user);
		}
	}
  if (!found) code = 103;
  cJSON_AddStringToObject(json, "response", "GET_USER");
  cJSON_AddNumberToObject(json, "code", code);
  string = cJSON_Print(json);
  cJSON_Delete(json);
  send(fd, string, strlen(string), 0);
}

void get_user(int fd, char *user) {
  cJSON *json = cJSON_CreateObject();
  char ipstr[20];
  char *string;
  int found = 0;
  int code = 200;

  for(int i=0; i < MAX_CONN; ++i){
		if(conns[i]){
      if(strcmp(conns[i]->user, user) == 0){
        found = 1;
				sprintf(ipstr, "%d.%d.%d.%d",
          conns[i]->socket.sin_addr.s_addr & 0xff,
          (conns[i]->socket.sin_addr.s_addr & 0xff00) >> 8,
          (conns[i]->socket.sin_addr.s_addr & 0xff0000) >> 16,
          (conns[i]->socket.sin_addr.s_addr & 0xff000000) >> 24);
				break;
			}
		}
	}
  if (!found) code = 102;

  cJSON_AddStringToObject(json, "response", "GET_USER");
  cJSON_AddNumberToObject(json, "code", code);
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
              || !cJSON_IsArray(body)) goto end1;
    
    u_name = cJSON_GetArrayItem(body, 1);

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
        printf("Cliente ha enviado petici??n de desconexi??n.\n");
        simple_response(client_connection->fd, "END_CONEX", 200);

      // send message
      } else if (strcmp(request->valuestring, "POST_CHAT") == 0) {
        int code = send_message(buff);
        simple_response(client_connection->fd, "POST_CHAT", code);
      } else if (strncmp(request->valuestring, "PUT_STATUS", 10) == 0) {
        cJSON *status = NULL;
        status = cJSON_GetObjectItemCaseSensitive(json, "body");
        if (cJSON_IsNumber(status)) {
          int code = put_status(status->valueint, client_connection->user);
          simple_response(client_connection->fd, "PUT_STATUS", code);
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

      } else if (strncmp(request->valuestring, "GET_CHAT", 8) == 0) {
        cJSON *body = NULL;
        char dest_user[NAME_LEN];
        char from_user[NAME_LEN];
        body = cJSON_GetObjectItemCaseSensitive(json, "body");
        strcpy(from_user, body->valuestring);
        strcpy(dest_user, client_connection->user);
        get_message(client_connection->fd, from_user, dest_user);
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
    printf("N??mero incorrecto de argumentos para iniciar el servidor\n");
    return(-1);
  }

  port = atoi(argv[1]);

  if(port <= 1023 || port > 65535) {
    printf("N??mero de puerto inv??lido\n");
    return(-1);
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = INADDR_ANY;

  fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    printf("Error en creaci??n de socket\n");
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
      printf("Conexi??n no aceptada\n");
      return (-1);
    }

    if((num_conns + 1) >= MAX_CONN) {
      printf("No se pueden agregar m??s clientes, cantidad m??xima alcanzada.\n");
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