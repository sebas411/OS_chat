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

#define MAX_INPUT 200
#define MESSAGE_LEN 100

struct sockaddr_in server;
int fd, conn, port;
char message[100] = "";
char user[20];
char server_ip[15];

int main(int argc, char *argv[]) {
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


  fd = socket(AF_INET, SOCK_STREAM, 0);

  server.sin_family = AF_INET;
  server.sin_port = htons(port);

  inet_pton(AF_INET, server_ip, &server.sin_addr);

  connect(fd, (struct sockaddr*) &server, sizeof(server));

  char inp[MAX_INPUT], *token;
  while(1) {
    printf("<chatsrv>$ ");
    fgets(inp, MAX_INPUT, stdin);
    char * offset = strstr( inp, "\n" );  if (NULL != offset) *offset = '\0'; //quitar \n del final del string
    token = strtok(inp, " ");
    if (token == NULL) continue;


    printf("Command is: %s\n", token);
    if (strcmp(token, "message")==0) {
      token = strtok(NULL, "\0");
      printf("%s\n", token);
      send(fd, token, strlen(token), 0);
    } else if (strcmp(token,"exit")==0) break;
  }
  return (0);
}