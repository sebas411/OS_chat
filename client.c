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
#include <pthread.h>
#include "cjson/cJSON.h"
#include "cjson/cJSON.c"

#define MAX_BUFF 500
#define NAME_LEN 20
#define MAX_INPUT_SIZE 200
#define MESSAGE_LEN 100

struct sockaddr_in server;
int fd, conn, port;
char message[MESSAGE_LEN] = "";
char user[NAME_LEN];
char server_ip[20];
int tmpstatus = 0;
char activecolor[8] = "\x1B[32m";


int stablish_connection() {
  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return (-1);

  server.sin_family = AF_INET;
  server.sin_port = htons(port);

  inet_pton(AF_INET, server_ip, &server.sin_addr);

  if(connect(fd, (struct sockaddr*) &server, sizeof(server)) < 0) return (-1);

  cJSON *json = cJSON_CreateObject();
  cJSON *body = cJSON_CreateObject();

  time_t rawtime;
  struct tm *timeinfo;
  char timestr[6];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  sprintf(timestr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);

  cJSON_AddStringToObject(body, "connect_time", timestr);
  cJSON_AddStringToObject(body, "user_id", user);

  cJSON_AddStringToObject(json, "request", "INIT_CONEX");
  cJSON_AddItemToObject(json, "body", body);

  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);

  char inbuff[MAX_BUFF];

  return (1);
}

int send_disconnect() {
  cJSON *json = cJSON_CreateObject();

  cJSON_AddStringToObject(json, "request", "END_CONEX");
  char *string = cJSON_Print(json);

  send(fd, string, strlen(string), 0);

  cJSON_Delete(json);
  char inbuff[MAX_BUFF];

  
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
  sprintf(timestr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);

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

void *receive_message() {
  char inbuff[MAX_BUFF];

  while(1) {
    if(recv(fd, inbuff, MAX_BUFF, 0) >0) {
      cJSON *json = cJSON_Parse(inbuff);
      cJSON *request = NULL;
      cJSON *response = NULL;

      request = cJSON_GetObjectItemCaseSensitive(json, "request");
      response = cJSON_GetObjectItemCaseSensitive(json, "response");

      //request
      if(cJSON_IsString(request) && request->valuestring != NULL) {
        //message
        if (strncmp(request->valuestring, "NEW_MESSAGE", 11) == 0) {
          cJSON *body = NULL;
          cJSON *from = NULL;
          cJSON *to = NULL;
          cJSON *delivered = NULL;
          cJSON *msg = NULL;
          char u_from[NAME_LEN];
          char u_to[NAME_LEN];
          char inmessage[MESSAGE_LEN];
          char time_at[10];

          body = cJSON_GetObjectItemCaseSensitive(json, "body");
          if(cJSON_IsObject(body)) {
            from = cJSON_GetObjectItemCaseSensitive(body, "from");
            delivered = cJSON_GetObjectItemCaseSensitive(body, "delivered_at");
            msg = cJSON_GetObjectItemCaseSensitive(body, "message");
            to = cJSON_GetObjectItemCaseSensitive(body, "to");
            if(cJSON_IsString(from) && cJSON_IsString(delivered) && cJSON_IsString(msg) && cJSON_IsString(to)
                  && from->valuestring != NULL && delivered->valuestring != NULL && msg->valuestring != NULL && to->valuestring != NULL) {
              strcpy(inmessage, msg->valuestring);
              strcpy(u_from, from->valuestring);
              strcpy(time_at, delivered->valuestring);
              strcpy(u_to, to->valuestring);
              if (strncmp(u_to, "all", 3) == 0) {
                strcpy(u_to, " (global)");
              } else {
                strcpy(u_to, "");
              }
              printf("\33[2K\r");
              printf("\x1B[32m<%s - %s%s>\x1B[0m %s\n", u_from, time_at, u_to, inmessage);
              printf("%s<chatcli>$\x1B[0m ", activecolor);
              fflush(stdout);
            }
          }
        }
      
      //response
      } else if (cJSON_IsString(response) && response->valuestring != NULL) {
        if (strncmp(response->valuestring, "GET_USER", 8) == 0) {
          cJSON *body = NULL;
          body = cJSON_GetObjectItemCaseSensitive(json, "body");

          if (cJSON_IsArray(body)) {
            cJSON *user = NULL;
            printf("\33[2K\r");
            printf("Usuarios conectados: ");
            cJSON_ArrayForEach(user, body) {
              if(cJSON_IsString(user) && user->valuestring != NULL) {
                printf("| %s ", user->valuestring);
              }
            }
            printf("|\n");
            printf("%s<chatcli>$\x1B[0m ", activecolor);
            fflush(stdout);
          } else if (cJSON_IsString(body) && body->valuestring != NULL) {
            printf("\33[2K\r");
            printf("La direccion ip del usuario es: %s\n", body->valuestring);
            printf("%s<chatcli>$\x1B[0m ", activecolor);
            fflush(stdout);
          }
        } else if (strncmp(response->valuestring, "PUT_STATUS", 10) == 0) {
          cJSON *code = NULL;
          code = cJSON_GetObjectItemCaseSensitive(json, "code");
          if (cJSON_IsNumber(code) && code->valueint == 200) {
            if (tmpstatus == 0) {
              strcpy(activecolor, "\x1B[32m");
            } else if (tmpstatus == 1) {
              strcpy(activecolor, "\x1B[33m");
            } else if (tmpstatus == 2) {
              strcpy(activecolor, "\x1B[31m");
            }
            printf("\33[2K\r");
            printf("%s<chatcli>$\x1B[0m ", activecolor);
            fflush(stdout);
          }
        }


      } else {
        printf("Mensaje del servidor inválido\n");
      }
      cJSON_Delete(json);
    } else {
      break;
    }
    bzero(inbuff, MAX_BUFF);
  }
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

  pthread_t receive_thread;

  pthread_create(&receive_thread, NULL, &receive_message, NULL);

  char inp[MAX_INPUT_SIZE], *token;
  while(1) {
    printf("%s<chatcli>$\x1B[0m ", activecolor);
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
      send_message(dest_user, message);

    //status <status>
    } else if (strcmp(token, "status")==0) {
      token = strtok(NULL, "\0");
      int status = atoi(token);
      tmpstatus = status;
      set_status(status);

    //getconn
    } else if (strncmp(token, "getconn", 7) == 0) {
      get_user("all");

    //getuser <user>
    } else if (strncmp(token, "getuser", 7) == 0) {
      char dest_user[NAME_LEN];
      token = strtok(NULL, "\0");
      strcpy(dest_user, token);
      get_user(dest_user);

    //getmsg <user>
    } else if (strcmp(token, "getglobal")==0){
      token = strtok(NULL, "");
      char from_user[NAME_LEN];
      strcpy(from_user, token);
      get_message(from_user);

    //getglobalmsg
    }else if (strcmp(token, "getglobalmsg")==0){
      get_message("all");

    //help
    }else if (strncmp(token, "help", 4)==0){
      printf("Comandos disponibles:\n- help: muestra este mensaje de ayuda\n- message <usuario> <mensaje>: envía un mensaje a un usuario\n- message all <mensaje>: envía un mensaje a todos los usuarios\n- getconn: muestra una lista de usuarios conectados\n- getuser <usuario>: muestra la dirección ip de un usuario\n- status <status>: cambia el status del usuario\n- exit: sale del programa\n");

    //exit
    } else if (strcmp(token, "exit")==0) {
      if(send_disconnect() < 0) {
        printf("Error enviando solicitud de desconectar\n");
        return (-1);
      }
      sleep(1);
      break;

    } else {
      printf("Comando desconocido\n");
    }
  }

  close(fd);
  return (0);
}
