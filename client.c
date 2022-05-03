#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "cjson/cJSON.h"
#include "cjson/cJSON.c"

#define MAX_INPUT_SIZE 200
#define MESSAGE_LEN 100
#define NAME_LEN 20

struct sockaddr_in server;
int fd, conn, port;
char message[MESSAGE_LEN] = "";
char user[NAME_LEN];
char server_ip[20];

int stablish_connection() {
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return (-1);

  server.sin_family = AF_INET;
  server.sin_port = htons(port);

  inet_pton(AF_INET, server_ip, &server.sin_addr);

  connect(fd, (struct sockaddr*) &server, sizeof(server));

  cJSON *json = cJSON_CreateObject();
  cJSON *body = cJSON_CreateObject();

  time_t rawtime;
  struct tm *timeinfo;
  char timestr[6];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  sprintf(timestr, "%d:%d", timeinfo->tm_hour, timeinfo->tm_min);

  cJSON_AddStringToObject(body, "connect_time", timestr);
  cJSON_AddStringToObject(body, "user_id", user);

  cJSON_AddStringToObject(json, "request", "INIT_CONEX");
  cJSON_AddItemToObject(json, "body", body);

  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  return (1);
}

int send_disconnect() {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "request", "END_CONEX");
  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  return (1);
}

int send_message(char *dest, char *msg) {
  cJSON *json = cJSON_CreateObject();
  cJSON *body = cJSON_CreateObject();

  time_t rawtime;
  struct tm *timeinfo;
  char timestr[6];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  sprintf(timestr, "%d:%d", timeinfo->tm_hour, timeinfo->tm_min);

  cJSON_AddStringToObject(body, "message", msg);
  cJSON_AddStringToObject(body, "from", user);
  cJSON_AddStringToObject(body, "delivered_at", timestr);
  cJSON_AddStringToObject(body, "to", dest);

  cJSON_AddStringToObject(json, "request", "POST_CHAT");
  cJSON_AddItemToObject(json, "body", body);

  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  return (1);
}

int get_message(char *from) {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "request", "GET_CHAT");
  cJSON_AddStringToObject(json, "body", from);
  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  return (1);
}

int get_user(char *from) {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "request", "GET_USER");
  cJSON_AddStringToObject(json, "body", from);
  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  return (1);
}

int set_status(int status) {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "request", "PUT_STATUS");
  cJSON_AddNumberToObject(json, "body", status);
  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  return (1);
}

int main(int argc, char *argv[]) {
  // Initialization with arguments
  if(argc != 4) {
    printf("Número incorrecto de argumentos para iniciar el cliente\n");
    return(-1);
  }

  strcpy(user, argv[1]);
  strcpy(server_ip, argv[2]);

  port = atoi(argv[3]);

  if(port <= 1023 || port > 65535) {
    printf("Número de puerto inválido\n");
    return(-1);
  }  

  if(stablish_connection() < 0) {
    printf("Error estableciendo la conexión al servidor\n");
    return (-1);
  }

  char inp[MAX_INPUT_SIZE], *token;
  while(1) {
    printf("<chatsrv>$ ");
    fgets(inp, MAX_INPUT_SIZE, stdin);
    char * offset = strstr( inp, "\n" );  if (NULL != offset) *offset = '\0'; //quitar \n del final del string
    token = strtok(inp, " ");
    if (token == NULL) continue;

    //message <user> <message>
    if (strcmp(token, "message")==0) {
      char dest_user[NAME_LEN];
      token = strtok(NULL, " ");
      strcpy(dest_user, token);
      token = strtok(NULL, "\0");
      strcpy(message, token);
      printf("to:%s -- message:%s\n", dest_user, message);
      send_message(dest_user, message);

    //status <status>
    } else if (strcmp(token, "status")==0) {
      token = strtok(NULL, "\0");
      int status = atoi(token);
      set_status(status);

    //getmsg <user>
    }else if (strcmp(token, "getglobal")==0){
      token = strtok(NULL, "");
      char from_user[NAME_LEN];
      strcpy(from_user, token);
      get_message(from_user);

    //getglobalmsg
    }else if (strcmp(token, "getglobalmsg")==0){
      get_message("all");

    //exit
    } else if (strcmp(token, "exit")==0) {
      if(send_disconnect() < 0) {
        printf("Error enviando solicitud de desconectar\n");
        return (-1);
      }
      break;

    } else {
      printf("Comando desconocido\n");
    }
  }
  return (0);
}