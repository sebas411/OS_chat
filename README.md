# Chat client and server in C
## Compilation and execution
- To compile the program you must run the following command:
`make compile`  
After you have compiled the programs you will have the executable files `client` and `server`

- To run the server use:  
`./server <port>`

- And to run the client use:
`./client <u_name> <srv_ip> <srv_port>`

### Available commands in client:
- `help`: muestra este mensaje de ayuda
- `message <usuario> <mensaje>`: envía un mensaje a un usuario
- `message all <mensaje>`: envía un mensaje a todos los usuarios
- `getconn`: muestra una lista de usuarios conectados
- `getuser <usuario>`: muestra la dirección ip de un usuario
- `status <status>`: cambia el status del usuario
- `exit`: sale del programa
