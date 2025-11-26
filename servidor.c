/*
 *          		S E R V I D O R
 *
 *	This is an example program that demonstrates the use of
 *	sockets TCP and UDP as an IPC mechanism.  
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <semaphore.h> 
#include <fcntl.h>  // Para O_CREAT





#define PUERTO 53278  
#define ADDRNOTFOUND	0xffffffff	/* return address for unfound host */
#define BUFFERSIZE	1024	/* maximum size of packets to be received */
#define TAM_BUFFER 516
#define MAXHOST 128
#define LOG_FILE "peticiones.log"

#define STATE_WAIT_HOLA    102  // Esperando comando HOLA
#define STATE_READY        103  // HOLA recibido, esperando FRASE o FIN
#define STATE_DONE         104  // FIN recibido, cerrando conexión

// Tabla de conversión a código Morse
typedef struct {
    char caracter;
    char *codigo;
} MorseMap;

MorseMap tablaMorse[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
    {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
    {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
    {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
    {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
    {'Y', "-.--"},  {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
    {'8', "---.."}, {'9', "----."},
    {'\0', NULL}  // Marca de fin
};

extern int errno;

/*
 *			M A I N
 *
 *	This routine starts the server.  It forks, leaving the child
 *	to do all the work, so it does not have to be run in the
 *	background.  It sets up the sockets.  It
 *	will loop forever, until killed by a signal.
 *
 */
void convertirAMorse(char *texto, char *resultado);
char * AnalisisEstados(char *mensaje, int *estado);

void serverTCP(int s, struct sockaddr_in peeraddr_in, sem_t *sem); //Añadimos parámetro sem_t *sem
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in, sem_t*sem);
void errout(char *);		/* declare error out routine */
void escribirLog(char *mensaje, sem_t *sem);



int FIN = 0;             /* Para el cierre ordenado */
void finalizar(){ FIN = 1; }

int main(argc, argv)
int argc;
char *argv[];
{

    int s_TCP, s_UDP;		//Descriptor de socket (como "handles" o IDs)  - socket individual para cada cliente que se conecta 
    int ls_TCP;				//Guardará el ID del socket - Socket de escucha para TCP - es el socket que escucha conexiones nuevas
    
    int cc;				    /* contains the number of bytes read */
     
    struct sigaction sa = {.sa_handler = SIG_IGN}; /* used to ignore SIGCHLD */
    
    struct sockaddr_in myaddr_in; 	/* for local socket address */
    struct sockaddr_in clientaddr_in;	/* for peer socket address */
	socklen_t addrlen;
	
    fd_set readmask;
    int numfds,s_mayor;
    
    char buffer[BUFFERSIZE];	/* buffer for packets to be read into */
    
    struct sigaction vec;

	sem_t *log_semaphore;  // Semáforo para acceso al log


	/* CREATE THE LISTEN SOCKET - TCP*/

	ls_TCP = socket (AF_INET, SOCK_STREAM, 0);// Devuelve un nº (como DNI socket) o -1 si no se pudo crear
	//AF_INET = IPv4
	//SOCK_STREAM = TCP 
	//0 = protocolo por defecto, pilla cualquier puerto, sino ponemos uno específico
	if (ls_TCP == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket TCP\n", argv[0]);
		exit(1);
	}


	/* clear out address structures */ 
	memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));  //Limpia la estructura
   	memset ((char *)&clientaddr_in, 0, sizeof(struct sockaddr_in));

    addrlen = sizeof(struct sockaddr_in);

	//Configurar datos

		/* Set up address structure for the listen socket. */
	myaddr_in.sin_family = AF_INET;  //IPv4 - le dice voy a usar IPv4
		/* The server should listen on the wildcard address,
		 * rather than its own internet address.  This is
		 * generally good practice for servers, because on
		 * systems which are connected to more than one
		 * network at once will be able to have one server
		 * listening on all networks at once.  Even when the
		 * host is connected to only one network, this is good
		 * practice, because it makes the server program more
		 * portable.
		 */
	myaddr_in.sin_addr.s_addr = INADDR_ANY; //Cualquier IP de mi PC - Escucha todas las interfaces de red
	myaddr_in.sin_port = htons(PUERTO); //Puerto constante que es 17278 , htons (Host to Network Short) conversor big endian


	//ASOCIA EL PUERTO AL SOCKET//
	/* Bind the listen address to the socket. */
	if (bind(ls_TCP, (const struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
				perror(argv[0]);
		fprintf(stderr, "%s: unable to bind address TCP\n", argv[0]);
		exit(1);
	}


		/* Initiate the listen on the socket so remote users
		 * can connect.  The listen backlog is set to 5, which
		 * is the largest currently supported.
		 */
	//ESCUCHA CONEXIONES ENTRANTES
	if (listen(ls_TCP, 5) == -1) { 
		//5 Indica el tamaño de la cola de conexiones clientes, cuantos clienets puedes esperar mientras atiendes a otro
		perror(argv[0]); 
		fprintf(stderr, "%s: unable to listen on socket\n", argv[0]);
		exit(1);
	}
	
	
	/* Create the socket UDP. */
	s_UDP = socket (AF_INET, SOCK_DGRAM, 0);
	if (s_UDP == -1) {
		perror(argv[0]);
		printf("%s: unable to create socket UDP\n", argv[0]);
		exit(1);
	   }
	/* Bind the server's address to the socket. */
	if (bind(s_UDP, (struct sockaddr *) &myaddr_in, sizeof(struct sockaddr_in)) == -1) {
		perror(argv[0]);
		printf("%s: unable to bind address UDP\n", argv[0]);
		exit(1);
	    }

		/* Crear semáforo compartido entre procesos */
	log_semaphore = sem_open("/morse_log_sem", O_CREAT | O_EXCL, 0644, 1);
	if (log_semaphore == SEM_FAILED) {
		// Si ya existe (de una ejecución anterior), intentar abrirlo

				log_semaphore = sem_open("/morse_log_sem", 0);
		if (log_semaphore == SEM_FAILED) {
			perror("Error al crear semáforo");
			exit(1);
		}
	}	

		/* Now, all the initialization of the server is
		 * complete, and any user errors will have already
		 * been detected.  Now we can fork the daemon and
		 * return to the user.  We need to do a setpgrp
		 * so that the daemon will no longer be associated
		 * with the user's control terminal.  This is done
		 * before the fork, so that the child will not be
		 * a process group leader.  Otherwise, if the child
		 * were to open a terminal, it would become associated
		 * with that terminal as its control terminal.  It is
		 * always best for the parent to do the setpgrp.
		 */
	setpgrp(); //Desvincula el proceso del terminal del usuario y hace que el proceso sea independiente

	switch (fork()) { //Creamos proceso hijo
	case -1:		/* ERROR - Unable to fork, for some reason. */
		perror(argv[0]);
		fprintf(stderr, "%s: unable to fork daemon\n", argv[0]);
		exit(1);

	case 0:     /* Proceso HIJO -  The child process (daemon) comes here. */

			/* Close stdin and stderr so that they will not
			 * be kept open.  Stdout is assumed to have been
			 * redirected to some logging file, or /dev/null.
			 * From now on, the daemon will not report any
			 * error messages.  This daemon will loop forever,
			 * waiting for connections and forking a child
			 * server to handle each one.
			 */

	    
	fclose(stdin);
	//fclose(stderr);

			/* Set SIGCLD to SIG_IGN, in order to prevent
			 * the accumulation of zombies as each child
			 * terminates.  This means the daemon does not
			 * have to make wait calls to clean them up.
			 */
		if ( sigaction(SIGCHLD, &sa, NULL) == -1) { 
			//SIGCHLD: Señal que envía cuando un proceso hijo termina, evita procesos zombie
            perror(" sigaction(SIGCHLD)");
            fprintf(stderr,"%s: unable to register the SIGCHLD signal\n", argv[0]);
            exit(1);
            }
            
		    /* Registrar SIGTERM para la finalizacion ordenada del programa servidor */
        vec.sa_handler = (void *) finalizar;
        vec.sa_flags = 0;
        if ( sigaction(SIGTERM, &vec, (struct sigaction *) 0) == -1) { 
			//SIGTERM: Señal de terminación "amigable" - llama a finalizar para cerrar todo ordenadamente y envia kill pid
            perror(" sigaction(SIGTERM)");
            fprintf(stderr,"%s: unable to register the SIGTERM signal\n", argv[0]);
            exit(1);
            }
        
		while (!FIN) {
            FD_ZERO(&readmask);

			//FD_SET espera conexiones con select ()
            FD_SET(ls_TCP, &readmask); 
            FD_SET(s_UDP, &readmask);
            

    	    if (ls_TCP > s_UDP) s_mayor=ls_TCP;
    		else s_mayor=s_UDP;

            if ( (numfds = select(s_mayor+1, &readmask, (fd_set *)0, (fd_set *)0, NULL)) < 0) {
				//Select permite al servidor esperar en múltiples sockets simultáneamente, a TCP y UDP
                if (errno == EINTR) {
                    FIN=1;
		            close (ls_TCP);
		            close (s_UDP);
                    perror("\nFinalizando el servidor. Se�al recibida en elect\n "); 
                }
            }
           else { 

				//SI HAY CLIENTE FD_ISSET (ls_TCP) SE ACTIVA, es decir si llega conexión TCP:
                if (FD_ISSET(ls_TCP, &readmask)) { 
					//Si me llega petición la acepto en el socket de abajo s_TCP
                    /* Note that addrlen is passed as a pointer
                     * so that the accept call can return the
                     * size of the returned address.
                     */
    				/* This call will block until a new
    				 * connection arrives.  Then, it will
    				 * return the address of the connecting
    				 * peer, and a new socket descriptor, s,
    				 * for that connection.
    				 */

				//Acepta al cliente
    			s_TCP = accept(ls_TCP, (struct sockaddr *) &clientaddr_in, &addrlen); 
				//**`accept()`** es **bloqueante**: el servidor se queda "esperando" aquí hasta que llegue un cliente.

    			if (s_TCP == -1) exit(1);
    			switch (fork()) { //Crea proceso hijo para atenderlo
        			case -1:	/* Can't fork, just exit. */
        				exit(1);
        			case 0:		/* Child process comes here. */
                    			close(ls_TCP); /* Close the listen socket inherited from the daemon. */
        				serverTCP(s_TCP, clientaddr_in, log_semaphore);
        				exit(0);
        			default:	/* Daemon process comes here. */
        					/* The daemon needs to remember
        					 * to close the new accept socket
        					 * after forking the child.  This
        					 * prevents the daemon from running
        					 * out of file descriptor space.  It
        					 * also means that when the server
        					 * closes the socket, that it will
        					 * allow the socket to be destroyed
        					 * since it will be the last close.
        					 */
        				close(s_TCP);
        			}
             } /* De TCP*/
          /* Comprobamos si el socket seleccionado es el socket UDP */
          if (FD_ISSET(s_UDP, &readmask)) { //Si la petición es de UDP
                /* This call will block until a new
                * request arrives.  Then, it will
                * return the address of the client,
                * and a buffer containing its request.
                * BUFFERSIZE - 1 bytes are read so that
                * room is left at the end of the buffer
                * for a null character.
                */
                cc = recvfrom(s_UDP, buffer, BUFFERSIZE - 1, 0,
                   (struct sockaddr *)&clientaddr_in, &addrlen); //Guardar dir del cliente
                if ( cc == -1) {
                    perror(argv[0]);
                    printf("%s: recvfrom error\n", argv[0]);
                    exit (1);
                    }
                /* Make sure the message received is
                * null terminated.
                */
                buffer[cc]='\0';
                serverUDP (s_UDP, buffer, clientaddr_in, log_semaphore);
                }
          }
		}   /* Fin del bucle infinito de atenci�n a clientes */
        /* Cerramos los sockets UDP y TCP */
        close(ls_TCP);
        close(s_UDP);

		// Cerrar y eliminar el semáforo
		sem_close(log_semaphore); //Cierra el semáforo en este proceso
		sem_unlink("/morse_log_sem"); //Elimina el semáforo del sistema (para que no quede "huérfano").
    
        printf("\nFin de programa servidor!\n");
        
	default:		/* Parent process comes here. */
		exit(0);
	}

}

/*
 *				S E R V E R T C P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */
void serverTCP(int s, struct sockaddr_in clientaddr_in, sem_t *sem)
{
	int reqcnt = 0;		/* keeps count of number of requests */
	char buf[TAM_BUFFER];		/* This example uses TAM_BUFFER byte messages. */
	char hostname[MAXHOST];		/* remote host's name string */

	int len, len1, status;
    //struct hostent *hp;		/* pointer to host info for remote host */
    long timevar;			/* contains time returned by time() */
    
    struct linger linger;		/* allow a lingering, graceful close; */
    				            /* used when setting SO_LINGER */
	int estado = STATE_WAIT_HOLA;
	char * response = NULL;
	char bufenv [TAM_BUFFER];
	char log_msg[512];  // Buffer para mensajes de log


				
	/* Look up the host information for the remote host
	 * that we have connected with.  Its internet address
	 * was returned by the accept call, in the main
	 * daemon loop above.
	 */
	 
     status = getnameinfo((struct sockaddr *)&clientaddr_in,sizeof(clientaddr_in),
                           hostname,MAXHOST,NULL,0,0);
     if(status){
           	/* The information is unavailable for the remote
			 * host.  Just format its internet address to be
			 * printed out in the logging information.  The
			 * address will be shown in "internet dot format".
			 */
			 // inet_ntop para interoperatividad con IPv6 
            if (inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST) == NULL)
            	perror(" inet_ntop \n");
             }
	
	
    /* Log a startup message. */
    time (&timevar);
		/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
	printf("Startup from %s port %u at %s",
		hostname, ntohs(clientaddr_in.sin_port), (char *) ctime(&timevar));

		/* Set the socket for a lingering, graceful close.
		 * This will cause a final close of this socket to wait until all of the
		 * data sent on it has been received by the remote host.
		 */
	linger.l_onoff  =1;
	linger.l_linger =1;
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &linger,
					sizeof(linger)) == -1) {
		errout(hostname);
	}
	

		/* Go into a loop, receiving requests from the remote
		 * client.  After the client has sent the last request,
		 * it will do a shutdown for sending, which will cause
		 * an end-of-file condition to appear on this end of the
		 * connection.  After all of the client's requests have
		 * been received, the next recv call will return zero
		 * bytes, signalling an end-of-file condition.  This is
		 * how the server will know that no more requests will
		 * follow, and the loop will be exited.
		 */

	// LOG: Comunicación realizada
		snprintf(log_msg, sizeof(log_msg), 
         "Comunicación realizada - Host: %s, IP: %s, Protocolo: TCP, Puerto: %u",
         hostname, 
         inet_ntoa(clientaddr_in.sin_addr),
         ntohs(clientaddr_in.sin_port));
		escribirLog(log_msg, sem);

	//Enviar mensaje de bienvenida
	memset (bufenv, 0, TAM_BUFFER);
	strcpy (bufenv, "220 Servicio preparado\r\n");
	//printf ("[SERVIDOR] Enviando bienvenida: %s", bufenv);
	
	if (send(s, bufenv, TAM_BUFFER, 0) != TAM_BUFFER){
		errout (hostname);
	}



	//RECV = RECIBIR
	while ((len = recv(s, buf, TAM_BUFFER, 0)) > 0 && estado != STATE_DONE ) {
		if (len == -1) errout(hostname); /* error from recv */
			/* The reason this while loop exists is that there
			 * is a remote possibility of the above recv returning
			 * less than TAM_BUFFER bytes.  This is because a recv returns
			 * as soon as there is some data, and will not wait for
			 * all of the requested data to arrive.  Since TAM_BUFFER bytes
			 * is relatively small compared to the allowed TCP
			 * packet sizes, a partial receive is unlikely.  If
			 * this example had used 2048 bytes requests instead,
			 * a partial receive would be far more likely.
			 * This loop will keep receiving until all TAM_BUFFER bytes
			 * have been received, thus guaranteeing that the
			 * next recv at the top of the loop will start at
			 * the begining of the next request.
			 */
		//Asegurar recepción completa
		while (len < TAM_BUFFER) {
			len1 = recv(s, &buf[len], TAM_BUFFER-len, 0);
			if (len1 == -1) errout(hostname);
			if (len1 == 0) break;
			len += len1;
		}

		//NO SÉ SI HAY QUE PONER ESTA LINEA --> AÑADE LIDIA
		buf [TAM_BUFFER-1] = '\0';
		//printf ("[SERVIDOR] Mensaje recibido: %s", buf);
		response = AnalisisEstados (buf, &estado);
		if (response == NULL){
			printf("[ERROR] AnalisisEstados devolvió NULL\n");
			errout (hostname);
		}

		// LOG: Frase recibida (solo si es comando FRASE)
		if (strncmp(buf, "FRASE ", 6) == 0) {
			snprintf(log_msg, sizeof(log_msg),
					"Frase recibida - Host: %s, IP: %s, Protocolo: TCP, Puerto: %u, Frase: %s",
					hostname,
					inet_ntoa(clientaddr_in.sin_addr),
					ntohs(clientaddr_in.sin_port),
					buf);
			escribirLog(log_msg, sem);
		}

        //printf("[SERVIDOR] Estado: %d, Respuesta: %s", estado, response);

			/* Increment the request count. */
		reqcnt++;
			/* This sleep simulates the processing of the
			 * request that a real server might do.
			 */
		sleep(1); 
			/* Send a response back to the client. */



		//ENVIAR
		memset (bufenv, 0, TAM_BUFFER);
		strncpy (bufenv, response, TAM_BUFFER-1);
		if (send(s, bufenv, TAM_BUFFER, 0) != TAM_BUFFER) errout(hostname);

		printf("\nSent response number %d currentSTATE: %d", reqcnt,estado);
		printf("\nResponse: %s", response);
		
		if (response!=NULL){
			free (response);
			response = NULL;
		}
		

		// LOG: Respuesta enviada
		snprintf(log_msg, sizeof(log_msg),
				"Respuesta enviada - Host: %s, IP: %s, Protocolo: TCP, Puerto: %u, Respuesta: %s",
				hostname,
				inet_ntoa(clientaddr_in.sin_addr),
				ntohs(clientaddr_in.sin_port),
				response);
		escribirLog(log_msg, sem);
			}

		/* The loop has terminated, because there are no
		 * more requests to be serviced.  As mentioned above,
		 * this close will block until all of the sent replies
		 * have been received by the remote host.  The reason
		 * for lingering on the close is so that the server will
		 * have a better idea of when the remote has picked up
		 * all of the data.  This will allow the start and finish
		 * times printed in the log file to reflect more accurately
		 * the length of time this connection was used.
		 */
	close(s);

		/* Log a finishing message. */
	time (&timevar);
		/* The port number must be converted first to host byte
		 * order before printing.  On most hosts, this is not
		 * necessary, but the ntohs() call is included here so
		 * that this program could easily be ported to a host
		 * that does require it.
		 */
	

	printf("Completed %s port %u, %d requests, at %s\n",
		hostname, ntohs(clientaddr_in.sin_port), reqcnt, (char *) ctime(&timevar));

	// LOG: Comunicación finalizada
	snprintf(log_msg, sizeof(log_msg),
			"Comunicación finalizada - Host: %s, IP: %s, Protocolo: TCP, Puerto: %u",
			hostname,
			inet_ntoa(clientaddr_in.sin_addr),
			ntohs(clientaddr_in.sin_port));
	escribirLog(log_msg, sem);

}

/*
 *	This routine aborts the child process attending the client.
 */
void errout(char *hostname)
{
	printf("Connection with %s aborted on error\n", hostname);
	exit(1);     
}


/*
 *				S E R V E R U D P
 *
 *	This is the actual server routine that the daemon forks to
 *	handle each individual connection.  Its purpose is to receive
 *	the request packets from the remote client, process them,
 *	and return the results to the client.  It will also write some
 *	logging information to stdout.
 *
 */
void serverUDP(int s, char * buffer, struct sockaddr_in clientaddr_in, sem_t *sem)
{
   // struct in_addr reqaddr;	/* for requested host's address */
   // struct hostent *hp;		/* pointer to host info for requested host */
   // int nc, errcode;

	//struct addrinfo hints, *res;

	char respuesta [TAM_BUFFER];
	char hostname [MAXHOST];
	char log_msg [512];
	int status;

	int addrlen;
    
   	addrlen = sizeof(struct sockaddr_in);

	// Obtener hostname del cliente
    status = getnameinfo((struct sockaddr *)&clientaddr_in, sizeof(clientaddr_in),
                         hostname, MAXHOST, NULL, 0, 0);
	if (status) {
        inet_ntop(AF_INET, &(clientaddr_in.sin_addr), hostname, MAXHOST);
    }
    
    printf("\n[UDP] Mensaje recibido de %s:%u - '%s'\n", 
           hostname, ntohs(clientaddr_in.sin_port), buffer);
    
    // LOG: Comunicación realizada
    snprintf(log_msg, sizeof(log_msg), 
             "Comunicación realizada - Host: %s, IP: %s, Protocolo: UDP, Puerto: %u",
             hostname, 
             inet_ntoa(clientaddr_in.sin_addr),
             ntohs(clientaddr_in.sin_port));
    escribirLog(log_msg, sem);

	
	//Limpiar buffer de respuesta
	memset (respuesta,0,TAM_BUFFER);

	//Creamos copias del buffer para procesarlo (eliminar \r\n)
	char buffer_limpio [TAM_BUFFER];
	strncpy (buffer_limpio, buffer, TAM_BUFFER-1);
	buffer_limpio[TAM_BUFFER-1]='\0';

	//Eliminamos \r\n del final si existe
	int len = strlen (buffer_limpio);
	if (len >=2 && buffer_limpio[len-2] == '\r' && buffer_limpio[len-1] == '\n' ){
    	buffer_limpio[len-2] = '\0';
	}
	if (len >= 1 && buffer_limpio[len-1] == '\n') {
    	buffer_limpio[len-1] = '\0';
	}

	// Procesar comando según su tipo
	if (strcmp(buffer_limpio, "") == 0 || strcmp(buffer, "\r\n") == 0) {
    	strcpy(respuesta, "220 Servicio preparado\r\n");
    	printf("[UDP] Respuesta: 220 Servicio preparado\n");
	}

	else if (strncmp(buffer_limpio, "HOLA ", 5) == 0) {
    	char *dominio = buffer_limpio + 5;

    	if (strlen(dominio) > 0) {
        	strcpy(respuesta, "240 OK\r\n");
        	printf("[UDP] HOLA recibido de dominio: %s\n", dominio);
    	} 

		else {
        strcpy(respuesta, "500 Error de sintaxis\r\n");
        printf("[UDP] Error: HOLA sin dominio\n");
    	}
	}
	
	else if (strncmp(buffer_limpio, "FRASE ", 6) == 0) {
    	char *frase = buffer_limpio + 6;
    
    	if (strlen(frase) > 0) {
        	char codigoMorse[400];
        	memset(codigoMorse, 0, sizeof(codigoMorse));        
        	convertirAMorse(frase, codigoMorse);        
        	sprintf(respuesta, "250 MORSE %s\r\n", codigoMorse);
       		printf("[UDP] Frase convertida: '%s' -> '%s'\n", frase, codigoMorse);
        
        	// LOG: Frase recibida
        	snprintf(log_msg, sizeof(log_msg),
                 "Frase recibida - Host: %s, IP: %s, Protocolo: UDP, Puerto: %u, Frase: %s",
                 hostname,
                 inet_ntoa(clientaddr_in.sin_addr),
                 ntohs(clientaddr_in.sin_port),
                 frase);
        	escribirLog(log_msg, sem);
    	} 
		else {
        	strcpy(respuesta, "500 Error de sintaxis\r\n");
        	printf("[UDP] Error: FRASE vacía\n");
    	}
	}

	else if (strcmp(buffer_limpio, "FIN") == 0) {
    	strcpy(respuesta, "221 Cerrando el servicio\r\n");
    	printf("[UDP] FIN recibido\n");
	}

	else {
    	strcpy(respuesta, "500 Error de sintaxis\r\n");
    	printf("[UDP] Error de sintaxis: '%s'\n", buffer_limpio);
	}

	//Enviamos respuesta
	if (sendto (s, respuesta, strlen (respuesta),0, (struct sockaddr *)&clientaddr_in, addrlen) ==-1){
		perror ("serverUDP - sendto");
	}
	printf("[UDP] Respuesta enviada: %s", respuesta);

	// LOG: Respuesta enviada
    snprintf(log_msg, sizeof(log_msg),
             "Respuesta enviada - Host: %s, IP: %s, Protocolo: UDP, Puerto: %u, Respuesta: %s",
             hostname,
             inet_ntoa(clientaddr_in.sin_addr),
             ntohs(clientaddr_in.sin_port),
             respuesta);
    escribirLog(log_msg, sem);
	 
}


/*
 * Función: convertirAMorse
 * ------------------------
 * Convierte un texto en castellano a código Morse
 * 
 * texto: Cadena a convertir
 * resultado: Buffer donde se almacenará el código Morse
 * Los espacios se convierten en '/'
 */
void convertirAMorse(char *texto, char *resultado) {
    int i, j;
    resultado[0] = '\0';  // Inicializar cadena vacía
    
    for (i = 0; texto[i] != '\0' && texto[i] != '\r' && texto[i] != '\n'; i++) {
        char c = toupper(texto[i]);  // Convertir a mayúscula
        
        // Si es un espacio, añadir '/'
        if (c == ' ') {
            if (strlen(resultado) > 0) {
                strcat(resultado, "/");
            }
            continue;
        }
        
        // Buscar el carácter en la tabla Morse
        int encontrado = 0;
        for (j = 0; tablaMorse[j].caracter != '\0'; j++) {
            if (tablaMorse[j].caracter == c) {
                // Añadir el código morse
                if (strlen(resultado) > 0) {
                    strcat(resultado, "/");  // Separador entre letras
                }
                strcat(resultado, tablaMorse[j].codigo);
                encontrado = 1;
                break;
            }
        }
        
        // Si no se encuentra el carácter, se ignora
        if (!encontrado && c != ' ') {
            printf("Advertencia: Carácter '%c' no reconocido\n", c);
        }
    }
}

/*
 * Función: escribirLog
 * --------------------
 * Escribe un mensaje en el archivo de log de forma segura (con semáforo)
 * 
 * mensaje: Texto a escribir en el log
 * sem: Semáforo para controlar el acceso concurrente
 */
void escribirLog(char *mensaje, sem_t *sem) {
    FILE *log_file;
    time_t now;
    char timestamp[64];

    // Obtener fecha y hora actual
    time(&now);
    struct tm *t = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);


    
    // BLOQUEAR el semáforo (esperar si otro proceso está escribiendo)
    sem_wait(sem);
    
    // Abrir archivo en modo append (añadir al final)
    log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Error al abrir peticiones.log");
        sem_post(sem);  // DESBLOQUEAR aunque haya error
        return;
    }
	 
    
    // Escribir en el log: [FECHA HORA] mensaje
    fprintf(log_file, "[%s] %s\n", timestamp, mensaje);


    fflush(log_file);  // Asegurar que se escribe inmediatamente
    fclose(log_file);

    
    // DESBLOQUEAR el semáforo (permitir que otro proceso escriba)
    sem_post(sem);
}
 
/*
 * Función: AnalisisEstados
 * ----------------------------------
 * Analiza los mensajes del cliente según el protocolo Morse
 * 
 * mensaje: Mensaje recibido del cliente
 * estado: Puntero al estado actual del protocolo
 * 
 * Retorna: Respuesta a enviar al cliente
 */
char * AnalisisEstados(char *mensaje, int *estado) {
	fflush (stdout);
    char *respuesta = (char *)malloc(sizeof(char) * TAM_BUFFER);
    char codigoMorse[400];
    char *frase;
    
    if (respuesta == NULL) {
        perror("Error al reservar memoria");
        return NULL;
    }

	//Copia mensaje a variable local
	char *linea = (char *)malloc(strlen(mensaje) + 1);  
    if (linea == NULL) {  
        perror("Error al reservar memoria");
        free(respuesta);
        return NULL;
    }
    strcpy(linea, mensaje);
    
    // Eliminar \r\n del final
    int len = strlen(linea);
    if (len >= 2 && linea[len-2] == '\r' && linea[len-1] == '\n') {
        linea[len-2] = '\0';
    }
    
    
    switch (*estado) {
        case STATE_WAIT_HOLA:
			//printf("[ESTADO] STATE_WAIT_HOLA\n");
            if (strncmp(linea, "HOLA ", 5) == 0) {
                char *dominio = linea + 5;
                if (strlen(dominio) > 0) {
                    //printf("[INFO] HOLA recibido de: %s\n", dominio);
                    strcpy(respuesta, "240 OK\r\n");
                    *estado = STATE_READY;
                } else {
                    strcpy(respuesta, "500 Error de sintaxis\r\n");
                }
            } else {
                strcpy(respuesta, "500 Error de sintaxis\r\n");
            }
            break;
        
        case STATE_READY:
		 	//printf("[ESTADO] STATE_READY\n");
            if (strncmp(linea, "FRASE ", 6) == 0) {
                frase = linea + 6;
                if (strlen(frase) > 0) {
                   // printf("[INFO] Convirtiendo: '%s'\n", frase);
					memset(codigoMorse, 0, sizeof(codigoMorse)); //Limpiar buffer
                    convertirAMorse(frase, codigoMorse);
                    strcpy(respuesta, "250 MORSE ");
                    strcat(respuesta, codigoMorse);
                    strcat(respuesta, "\r\n");
                    //printf("[INFO] Morse: %s\n", codigoMorse);
                } else {
                    strcpy(respuesta, "500 Error de sintaxis\r\n");
                }
            } else if (strcmp(linea, "FIN") == 0) {
                //printf("[INFO] FIN recibido\n");
                strcpy(respuesta, "221 Cerrando el servicio\r\n");
                *estado = STATE_DONE;
            } else {
                strcpy(respuesta, "500 Error de sintaxis\r\n");
            }
            break;
        
        case STATE_DONE:
			//printf("[ESTADO] STATE_DONE\n");
            strcpy(respuesta, "221 Cerrando el servicio\r\n");
            break;
        
        default:
            strcpy(respuesta, "500 Error de sintaxis\r\n");
            *estado = STATE_WAIT_HOLA;
            break;
    }
    free (linea);
    return respuesta;
}