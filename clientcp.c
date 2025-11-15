/*
 *			C L I E N T C P
 *
 *	This is an example program that demonstrates the use of
 *	stream sockets as an IPC mechanism.  This contains the client,
 *	and is intended to operate in conjunction with the server
 *	program.  Together, these two programs
 *	demonstrate many of the features of sockets, as well as good
 *	conventions for using these features.
 *
 *
 */
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <time.h>

#define PUERTO 53278 
#define TAM_BUFFER 516

/*
 *			M A I N
 *
 *	This routine is the client which request service from the remote.
 *	It creates a connection, sends a number of
 *	requests, shuts down the connection in one direction to signal the
 *	server about the end of data, and then receives all of the responses.
 *	Status will be written to stdout.
 *
 *	The name of the system to which the requests will be sent is given
 *	as a parameter to the command.
 */
int main(argc, argv)
int argc;
char *argv[];
{
    int s;				/* connected socket descriptor */
   	struct addrinfo hints, *res;
    long timevar;			/* contains time returned by time() */
    struct sockaddr_in myaddr_in;	/* for local socket address */
    struct sockaddr_in servaddr_in;	/* for server socket address */
	int addrlen, i, j, errcode;
    /* This example uses TAM_BUFFER byte messages. */
	char buf[TAM_BUFFER];

	if (argc != 2) {
		fprintf(stderr, "Usage:  %s <remote host>\n", argv[0]);
		exit(1);
	}

	/* Create the socket. */
	s = socket (AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to create socket\n", argv[0]);
		exit(1);
	}
	
	/* clear out address structures */
	memset ((char *)&myaddr_in, 0, sizeof(struct sockaddr_in));
	memset ((char *)&servaddr_in, 0, sizeof(struct sockaddr_in));

	/* Set up the peer address to which we will connect. */
	servaddr_in.sin_family = AF_INET;
	
	/* Get the host information for the hostname that the
	 * user passed in. */
      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;
 	 /* esta funci�n es la recomendada para la compatibilidad con IPv6 gethostbyname queda obsoleta*/
    errcode = getaddrinfo (argv[1], NULL, &hints, &res); 
    if (errcode != 0){
			/* Name was not found.  Return a
			 * special value signifying the error. */
		fprintf(stderr, "%s: No es posible resolver la IP de %s\n",
				argv[0], argv[1]);
		exit(1);
        }
    else {
		/* Copy address of host */
		servaddr_in.sin_addr = ((struct sockaddr_in *) res->ai_addr)->sin_addr;
	    }
    freeaddrinfo(res);

    /* puerto del servidor en orden de red*/
	servaddr_in.sin_port = htons(PUERTO);

		/* Try to connect to the remote server at the address
		 * which was just built into peeraddr.
		 */
	if (connect(s, (const struct sockaddr *)&servaddr_in, sizeof(struct sockaddr_in)) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to connect to remote\n", argv[0]);
		exit(1);
	}
		/* Since the connect call assigns a free address
		 * to the local end of this connection, let's use
		 * getsockname to see what it assigned.  Note that
		 * addrlen needs to be passed in as a pointer,
		 * because getsockname returns the actual length
		 * of the address.
		 */
	addrlen = sizeof(struct sockaddr_in);
	if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to read socket address\n", argv[0]);
		exit(1);
	 }

	/* Print out a startup message for the user. */
	time(&timevar);
	/* The port number must be converted first to host byte
	 * order before printing.  On most hosts, this is not
	 * necessary, but the ntohs() call is included here so
	 * that this program could easily be ported to a host
	 * that does require it.
	 */
	printf("Connected to %s on port %u at %s",
			argv[1], ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));

	printf("Connected to %s on port %u at %s",
        argv[1], ntohs(myaddr_in.sin_port), (char *) ctime(&timevar));

	// ============ ESTILO LUIS - CLIENTE INTERACTIVO MORSE ============
	char bufr[TAM_BUFFER];  // Buffer separado para recibir

// *** PASO 1: RECIBIR MENSAJE DE BIENVENIDA (220) ***
	memset(bufr, 0, TAM_BUFFER);
	i = recv(s, bufr, TAM_BUFFER, 0);
	if (i == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: Error al recibir bienvenida\n", argv[0]);
		exit(1);
	}

	// Completar recepción
	while (i < TAM_BUFFER) {
		j = recv(s, &bufr[i], TAM_BUFFER-i, 0);
		if (j == -1) {
			perror(argv[0]);
			exit(1);
		}
		if (j == 0) break;
		i += j;
	}

	bufr[TAM_BUFFER-1] = '\0';
	printf("S: %s\n", bufr);

	// *** PASO 2: BUCLE INTERACTIVO (como Luis) ***
	while(1)
	{
		// Limpiar buffers
		memset(buf, 0, TAM_BUFFER);
		memset(bufr, 0, TAM_BUFFER);
		
		// Leer comando del usuario
		printf("C: ");
		fflush(stdout);
		
		if (fgets(buf, TAM_BUFFER-3, stdin) == NULL) {
			fprintf(stderr, "Error leyendo entrada\n");
			break;
		}
		
		// Quitar el \n que añade fgets
		int len = strlen(buf);
		if (len > 0 && buf[len-1] == '\n') {
			buf[len-1] = '\0';
		}
		
		// Añadir \r\n al final (protocolo)
		strcat(buf, "\r\n");
		
		// Enviar mensaje al servidor
		if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
			perror(argv[0]);
			fprintf(stderr, "%s: Error al enviar mensaje\n", argv[0]);
			exit(1);
		}

		
		
		// Recibir respuesta del servidor
        i = recv(s, bufr, TAM_BUFFER, 0);
        if (i == -1) {
            perror(argv[0]);
            fprintf(stderr, "%s: error reading result\n", argv[0]);
            exit(1);
        }
		
		// Asegurar que se reciben todos los TAM_BUFFER bytes
			while (i < TAM_BUFFER) {
				j = recv(s, &bufr[i], TAM_BUFFER-i, 0);
				if (j == -1) {
					perror(argv[0]);
					fprintf(stderr, "%s: error reading result\n", argv[0]);
					exit(1);
				}
				i += j;
			}
	
				
	
    
    // Comprobar si es el mensaje de cierre (como Luis)
    if (strstr(bufr, "221 Cerrando el servicio") != NULL) {
        printf("\nServidor cerró la conexión. Saliendo...\n");
        break;
    }

	// Asegurar terminación null
    bufr[TAM_BUFFER-1] = '\0';

	 // Mostrar respuesta del servidor
    printf("S: %s\n", bufr);
}
	/* Now, shutdown the connection for further sends.
	* This will cause the server to receive an end-of-file
	* condition after it has received all the requests that
	* have just been sent, indicating that we will not be
	* sending any further requests.
	*/

	if (shutdown(s, 1) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to shutdown socket\n", argv[0]);
		exit(1);
	}
	
	/* Check to see if the read failed. */
	if (i == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: error reading message\n", argv[0]);
		exit(1);
	}
		
    /* Print message indicating completion of task. */
	time(&timevar);
	printf("All done at %s", (char *)ctime(&timevar));
}

	/*for (i=1; i<=5; i++) {
		*buf = i;
		if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
			fprintf(stderr, "%s: Connection aborted on error ",	argv[0]);
			fprintf(stderr, "on send number %d\n", i);
			exit(1);
		}
	}*/

		/* Now, shutdown the connection for further sends.
		 * This will cause the server to receive an end-of-file
		 * condition after it has received all the requests that
		 * have just been sent, indicating that we will not be
		 * sending any further requests.
		 */

	/*
	if (shutdown(s, 1) == -1) {
		perror(argv[0]);
		fprintf(stderr, "%s: unable to shutdown socket\n", argv[0]);
		exit(1);
	}
	*/

		/* Now, start receiving all of the replys from the server.
		 * This loop will terminate when the recv returns zero,
		 * which is an end-of-file condition.  This will happen
		 * after the server has sent all of its replies, and closed
		 * its end of the connection.
		 */
	//while (i = recv(s, buf, TAM_BUFFER, 0)) {
		//Verificación
		/*if (i == -1) {
            perror(argv[0]);
			fprintf(stderr, "%s: error reading result\n", argv[0]);
			exit(1);
		}
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
			 * the begining of the next reply.
			 */
		/*while (i < TAM_BUFFER) {
			j = recv(s, &buf[i], TAM_BUFFER-i, 0);
			if (j == -1) {
                     perror(argv[0]);
			         fprintf(stderr, "%s: error reading result\n", argv[0]);
			         exit(1);
               }
			i += j;
		}
			// Print out message indicating the identity of this reply. 
		printf("Received result number %d\n", *buf);
	}*/
	// ============ CÓDIGO NUEVO - ENVIAR MENSAJES ============
// Enviar comando HOLA
/*strcpy(buf, "HOLA usal.es\r\n");
printf("Enviando: %s", buf);
if (send(s, buf, strlen(buf), 0) < 0) {
    perror(argv[0]);
    fprintf(stderr, "%s: Error al enviar HOLA\n", argv[0]);
    exit(1);
}

// Recibir respuesta del servidor
memset(buf, 0, TAM_BUFFER);
int bytes_recibidos = recv(s, buf, TAM_BUFFER-1, 0);
if (bytes_recibidos <= 0) {
    perror(argv[0]);
    fprintf(stderr, "%s: Error al recibir respuesta\n", argv[0]);
    exit(1);
}
buf[bytes_recibidos] = '\0';
printf("Respuesta del servidor: %s", buf);

// Enviar comando FRASE
strcpy(buf, "FRASE SOS\r\n");
printf("Enviando: %s", buf);
if (send(s, buf, strlen(buf), 0) < 0) {
    perror(argv[0]);
    fprintf(stderr, "%s: Error al enviar FRASE\n", argv[0]);
    exit(1);
}

// Recibir respuesta con código Morse
memset(buf, 0, TAM_BUFFER);
bytes_recibidos = recv(s, buf, TAM_BUFFER-1, 0);
if (bytes_recibidos <= 0) {
    perror(argv[0]);
    fprintf(stderr, "%s: Error al recibir morse\n", argv[0]);
    exit(1);
}
buf[bytes_recibidos] = '\0';
printf("Código Morse recibido: %s", buf);

// Enviar comando FIN
strcpy(buf, "FIN\r\n");
printf("Enviando: %s", buf);
if (send(s, buf, strlen(buf), 0) < 0) {
    perror(argv[0]);
    fprintf(stderr, "%s: Error al enviar FIN\n", argv[0]);
    exit(1);
}

// Recibir confirmación de cierre
memset(buf, 0, TAM_BUFFER);
bytes_recibidos = recv(s, buf, TAM_BUFFER-1, 0);
if (bytes_recibidos > 0) {
    buf[bytes_recibidos] = '\0';
    printf("Respuesta final: %s", buf);
}

	/* Print message indicating completion of task. */
	//time(&timevar);
	//printf("All done at %s", (char *)ctime(&timevar));
	
//}
