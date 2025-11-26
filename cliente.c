#define _GNU_SOURCE
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
#include <sys/errno.h>
#include <signal.h>
#include <ctype.h> 

extern int errno;

#define PUERTO 53278 
#define TAM_BUFFER 516
#define RETRIES 5
#define TIMEOUT 6
#define MAXHOST 512

int obtener_ip_servidor (char *nombre_servidor, struct sockaddr_in *servaddr_in, char *nombre_programa) {
    struct addrinfo hints, *res;
    int errcode;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    
    errcode = getaddrinfo(nombre_servidor, NULL, &hints, &res); //Resuelve el nombre del servidor usando DNS
    if (errcode != 0) {
        fprintf(stderr, "%s: No es posible resolver la IP de %s\n",
                nombre_programa, nombre_servidor);
        return -1;
    }
    
    servaddr_in->sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr; //Cast de sockaddr* (genérico) a sockaddr_in* (IPv4) y copiar la IP 

    freeaddrinfo(res);
    return 0;
}

FILE* crear_archivo_salida(char *nombre_programa, struct sockaddr_in *myaddr_in, 
                            char *protocolo, char *servidor, char *archivo_ordenes) {
    char nombre_archivo_salida[50];
    FILE *archivo_salida;
    
    sprintf(nombre_archivo_salida, "%u.txt", ntohs(myaddr_in->sin_port));
    archivo_salida = fopen(nombre_archivo_salida, "w"); // w (write) si el archivo existe, lo sobrescribe, sino lo crea
    
    if (archivo_salida == NULL) {
        perror(nombre_programa);
        fprintf(stderr, "%s: No se pudo crear el archivo de salida %s\n", 
                nombre_programa, nombre_archivo_salida);
        return NULL;
    }
    
    printf("Archivo de salida: %s\n", nombre_archivo_salida);
    fprintf(archivo_salida, "=== CLIENTE %s - Puerto: %u ===\n", 
            protocolo, ntohs(myaddr_in->sin_port));
    fprintf(archivo_salida, "Servidor: %s\n", servidor);
    fprintf(archivo_salida, "Protocolo: %s\n", protocolo);
    fprintf(archivo_salida, "Archivo de ordenes: %s\n\n", archivo_ordenes);
    fflush(archivo_salida);
    
    return archivo_salida;
}

void clienteTCP(char *servidor, char *archivo_ordenes, char *nombre_programa) {
    int s;
    struct sockaddr_in myaddr_in, servaddr_in;
    int addrlen, i, j;
    char buf[TAM_BUFFER];
    char bufr[TAM_BUFFER];
    long timevar;
    FILE *archivo_salida = NULL;
    
    FILE *archivo_ordenes_fp = NULL;
    char linea_buffer [TAM_BUFFER];
    ssize_t nread;
    
    //CREAR SOCKET
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to create socket\n", nombre_programa);
        exit(1);
    }
    
    //LIMPIAR ESTRUCTURAS 
    memset(&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset(&servaddr_in, 0, sizeof(struct sockaddr_in));
    
    //CONFIGURAR DIRECCIÓN SERVIDOR
    servaddr_in.sin_family = AF_INET;
    
    if (obtener_ip_servidor(servidor, &servaddr_in, nombre_programa) != 0) {
        close(s);
        exit(1);
    }
    
    servaddr_in.sin_port = htons(PUERTO); //Establecemos el puerto del servidor (53278)
    
    //CONECTAR
    if (connect(s, (const struct sockaddr *)&servaddr_in, 
                sizeof(struct sockaddr_in)) == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to connect to remote\n", nombre_programa);
        close(s);
        exit(1);
    }
    
    //OBTENER PUERTO LOCAL
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to read socket address\n", nombre_programa);
        close(s);
        exit(1);
    }
    
    //CREAR ARCHIVO SALIDA
    archivo_salida = crear_archivo_salida(nombre_programa, &myaddr_in, 
                                           "TCP", servidor, archivo_ordenes);
    if (archivo_salida == NULL) {
        close(s);
        exit(1);
    }
    
    //ABRIR ARCHIVO DE ÓRDENES
    archivo_ordenes_fp = fopen(archivo_ordenes, "r");
    if (archivo_ordenes_fp == NULL) {
        perror(nombre_programa);
        fprintf(stderr, "%s: No se pudo abrir el archivo %s\n", 
                nombre_programa, archivo_ordenes);
        close(s);
        fclose(archivo_salida);
        exit(1);
    }
    
    //MENSAJE CONEXIÓN
    time(&timevar);
    printf("Connected to %s on port %u at %s", servidor, 
           ntohs(myaddr_in.sin_port), ctime(&timevar));
    
    //RECIBIR MENSAJE DE BIENVENIDA (220)
    memset(bufr, 0, TAM_BUFFER);
    i = recv(s, bufr, TAM_BUFFER, 0);
    if (i == -1) { //Comprobación por si recv falló
        perror(nombre_programa);
        fprintf(stderr, "%s: Error al recibir bienvenida\n", nombre_programa);
        fprintf(archivo_salida, "[ERROR] Error al recibir respuesta\n");
        fflush(archivo_salida);
        fclose(archivo_salida);
        fclose(archivo_ordenes_fp);
        close(s);
        exit(1);
    }
    
    // Completar recepción
    while (i < TAM_BUFFER) {
        j = recv(s, &bufr[i], TAM_BUFFER-i, 0);
        if (j == -1) {
            perror(nombre_programa);
            fprintf(stderr, "%s: error reading result\n", nombre_programa);
            fprintf(archivo_salida, "[ERROR] Error al recibir respuesta completa\n");
            fflush(archivo_salida);
            fclose(archivo_salida);
            fclose(archivo_ordenes_fp);
            close(s);
            exit(1);
        }
        if (j == 0) break;
        i += j;
    }
    
    bufr[TAM_BUFFER-1] = '\0'; //Marcar final de la cadena
    printf("S: %s\n", bufr);
    fprintf(archivo_salida, "S: %s\n", bufr);
    fflush(archivo_salida);
    
    //BUCLE DE LECTURA DESDE ARCHIVO
    while (fgets(linea_buffer, TAM_BUFFER, archivo_ordenes_fp) != NULL){
        // Limpiar buffers
        memset(buf, 0, TAM_BUFFER);
        memset(bufr, 0, TAM_BUFFER);

        nread = strlen (linea_buffer);
                
        // Eliminar el salto de línea que añade getline
        if (nread > 0 && linea_buffer[nread-1] == '\n') {  
            linea_buffer[nread-1] = '\0';                 
            nread--;
        }
        
        // Verificar que la línea no esté vacía
        if (nread == 0) {
            continue;  // Saltar líneas vacías
        }
        
        // Copiar la línea al buffer
        strncpy(buf, linea_buffer, TAM_BUFFER-3);
        buf[TAM_BUFFER-3] = '\0';  // Asegurar terminación
        
        // Añadir \r\n al final 
        strcat(buf, "\r\n");
        
        // Escribir en archivo y consola lo que enviamos
        printf("C: %s", buf);
        fprintf(archivo_salida, "C: %s", buf);
        fflush(archivo_salida);
        
        // Enviar mensaje al servidor
        if (send(s, buf, TAM_BUFFER, 0) != TAM_BUFFER) {
            perror(nombre_programa);
            fprintf(stderr, "%s: Error al enviar mensaje\n", nombre_programa);
            fprintf(archivo_salida, "[ERROR] Error al enviar mensaje\n");
            fflush(archivo_salida);
            break;
        }
        
        // Recibir respuesta del servidor
        i = recv(s, bufr, TAM_BUFFER, 0);
        if (i == -1) {
            perror(nombre_programa);
            fprintf(stderr, "%s: error reading result\n", nombre_programa);
            break;
        }
        
        // Asegurar que se reciben todos los TAM_BUFFER bytes
        while (i < TAM_BUFFER) {
            j = recv(s, &bufr[i], TAM_BUFFER-i, 0);
            if (j == -1) {
                perror(nombre_programa);
                fprintf(stderr, "%s: error reading result\n", nombre_programa);
                fclose(archivo_salida);
                fclose(archivo_ordenes_fp);
                close(s);
                exit(1);
            }
            if (j == 0) break;  // Conexión cerrada
            i += j;
        }
        
        // Asegurar terminación null
        bufr[TAM_BUFFER-1] = '\0';
        
        // Escribir respuesta en archivo y consola
        printf("S: %s\n", bufr);
        fprintf(archivo_salida, "S: %s\n", bufr);
        
        // Comprobar si es el mensaje de cierre
        if (strstr(bufr, "221") != NULL) {
            printf("\nServidor cerró la conexion. Saliendo...\n");
            fprintf(archivo_salida, "\n=== Servidor cerró la conexion ===\n");
            fflush(archivo_salida);
            break;
        }
    }
    
    //CERRAR Y LIBERAR
    if (shutdown(s, 1) == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to shutdown socket\n", nombre_programa);
    }
    
        
    if (archivo_ordenes_fp) {
        fclose(archivo_ordenes_fp);
    }
    
    if (archivo_salida) {
        fprintf(archivo_salida, "\n=== Fin de la comunicacion ===\n");
        time(&timevar);
        fprintf(archivo_salida, "Finalizado: %s", ctime(&timevar));
        fclose(archivo_salida);
        printf("Respuestas guardadas en: %u.txt\n", ntohs(myaddr_in.sin_port));
    }
    
    close(s);
    time(&timevar);
    printf("All done at %s", ctime(&timevar));
}

// Handler para alarma
void handler_udp() {
    printf("Alarma recibida\n");
}

void clienteUDP(char *servidor, char *archivo_ordenes, char *nombre_programa) {
    int s;
    struct sockaddr_in myaddr_in, servaddr_in;
    socklen_t addrlen;
    long timevar;
    struct sigaction vec;
    
    char buffer[TAM_BUFFER];
    char bufResp[TAM_BUFFER];
    
    FILE *archivo_ordenes_fp = NULL;
    char linea_buffer [TAM_BUFFER];
    ssize_t nread;

    FILE *archivo_salida=NULL;
    
    //CREAR SOCKET
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to create socket\n", nombre_programa);
        exit(1);
    }
    
    //LIMPIAR ESTRUCTURAS
    memset(&myaddr_in, 0, sizeof(struct sockaddr_in));
    memset(&servaddr_in, 0, sizeof(struct sockaddr_in));
    
    //BIND SOCKET LOCAL
    myaddr_in.sin_family = AF_INET;
    myaddr_in.sin_port = 0;
    myaddr_in.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(s, (const struct sockaddr *)&myaddr_in, 
             sizeof(struct sockaddr_in)) == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to bind socket\n", nombre_programa);
        close(s);
        exit(1);
    }
    
    //OBTENER PUERTO LOCAL
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(s, (struct sockaddr *)&myaddr_in, &addrlen) == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: unable to read socket address\n", nombre_programa);
        close(s);
        exit(1);
    }

    //CREAR ARCHIVO SALIDA
    archivo_salida = crear_archivo_salida(nombre_programa, &myaddr_in, 
                                           "UDP", servidor, archivo_ordenes);
    if (archivo_salida == NULL) {
        close(s);
        exit(1);
    }

    
    //MENSAJE CONEXIÓN
    time(&timevar);
    printf("Connected to %s on port %u at %s", servidor, 
           ntohs(myaddr_in.sin_port), ctime(&timevar));
    
    //CONFIGURAR DIRECCIÓN SERVIDOR
    servaddr_in.sin_family = AF_INET;
    
    if (obtener_ip_servidor(servidor, &servaddr_in, nombre_programa) != 0) {
        close(s);
        exit(1);
    }
    
    servaddr_in.sin_port = htons(PUERTO);
    
    //REGISTRAR SIGALRM
    vec.sa_handler = (void *)handler_udp;
    vec.sa_flags = 0;
    if (sigaction(SIGALRM, &vec, NULL) == -1) {
        perror("sigaction(SIGALRM)");
        fprintf(stderr, "%s: unable to register the SIGALRM signal\n", nombre_programa);
        close(s);
        exit(1);
    }
    
    //ABRIR ARCHIVO DE ÓRDENES
    archivo_ordenes_fp = fopen(archivo_ordenes, "r");
    if (archivo_ordenes_fp == NULL) {
        perror(nombre_programa);
        fprintf(stderr, "%s: No se pudo abrir el archivo %s\n", 
                nombre_programa, archivo_ordenes);
        close(s);
        exit(1);
    }
    char mensaje_inicial[TAM_BUFFER];
    memset(mensaje_inicial, 0, TAM_BUFFER);
    strcpy(mensaje_inicial, "\r\n");
    
    if (sendto(s, mensaje_inicial, strlen(mensaje_inicial), 0,
              (struct sockaddr *)&servaddr_in,
              sizeof(struct sockaddr_in)) == -1) {
        perror(nombre_programa);
        fprintf(stderr, "%s: Error al enviar mensaje inicial\n", nombre_programa);
        fclose(archivo_ordenes_fp);
        fclose(archivo_salida);
        close(s);
        exit(1);
    }
    
    // Esperar respuesta 220 (Servicio preparado)
    printf("[Cliente UDP] Esperando respuesta inicial del servidor...\n");
    alarm(TIMEOUT);
    memset(bufResp, 0, TAM_BUFFER);
    
    if (recvfrom(s, bufResp, TAM_BUFFER, 0,
                (struct sockaddr *)&servaddr_in, &addrlen) == -1) {
        if (errno == EINTR) {
            fprintf(stderr, "[ERROR] Timeout esperando respuesta inicial del servidor\n");
            fprintf(archivo_salida, "[ERROR] Timeout esperando respuesta 220\n");
        } else {
            perror(nombre_programa);
            fprintf(stderr, "%s: Error al recibir respuesta inicial\n", nombre_programa);
            fprintf(archivo_salida, "[ERROR] Error al recibir respuesta inicial\n");
        }
        fflush(archivo_salida);
        fclose(archivo_ordenes_fp);
        fclose(archivo_salida);
        close(s);
        exit(1);
    }
    
    alarm(0);  // Cancelar alarma
    
    // Mostrar y guardar respuesta 220
    printf("S: %s", bufResp);
    fprintf(archivo_salida, "S: %s", bufResp);
    fflush(archivo_salida);
    
    // Verificar que sea el mensaje 220
    if (strncmp(bufResp, "220", 3) != 0) {
        fprintf(stderr, "[ERROR] Respuesta inicial inesperada: %s\n", bufResp);
        fprintf(archivo_salida, "[ERROR] Se esperaba '220' pero se recibió: %s\n", bufResp);
        fflush(archivo_salida);
        fclose(archivo_ordenes_fp);
        fclose(archivo_salida);
        close(s);
        exit(1);
    }
    
    //printf("[Cliente UDP] Conexión inicial completada. Enviando comandos...\n\n");
    fprintf(archivo_salida, "\n--- Inicio de comandos ---\n\n");
    fflush(archivo_salida);
    
    
    
    // BUCLE DE LECTURA DESDE ARCHIVO
    while (fgets(linea_buffer, TAM_BUFFER, archivo_ordenes_fp) != NULL) {
        // Limpiar buffers
        memset(buffer, 0, TAM_BUFFER);
        memset(bufResp, 0, TAM_BUFFER);
         nread = strlen(linea_buffer);
        
        // Eliminar el salto de línea que añade getline
        if (linea_buffer[nread-1] == '\n') {
            linea_buffer[nread-1] = '\0';
            nread--;
        }
        
        // Verificar que la línea no esté vacía
        if (nread == 0) {
            continue;  // Saltar líneas vacías
        }
        
        // Copiar la línea al buffer
        strncpy(buffer, linea_buffer, TAM_BUFFER-3);
        buffer[TAM_BUFFER-3] = '\0';  // Asegurar terminación
        
        // Añadir \r\n al final (protocolo)
        strcat(buffer, "\r\n");
        
        // Mostrar qué enviamos
        printf("C: %s", buffer);
        fprintf(archivo_salida, "C: %s", buffer);
        fflush(archivo_salida);

        
        //ENVIAR COMANDO CON REINTENTOS
        int n_retry = RETRIES;
        int respuesta_recibida = 0;
        
        while (n_retry > 0 && !respuesta_recibida) {
            // Enviar comando
            if (sendto(s, buffer, strlen(buffer), 0,
                      (struct sockaddr *)&servaddr_in,
                      sizeof(struct sockaddr_in)) == -1) {
                perror(nombre_programa);
                fprintf(stderr, "%s: unable to send message\n", nombre_programa);
                fclose(archivo_ordenes_fp);
                close(s);
                exit(1);
            }

                        
            // Activar alarma
            alarm(TIMEOUT);
            
            // Esperar respuesta (bloqueante)
            memset(bufResp, 0, TAM_BUFFER);
            if (recvfrom(s, bufResp, TAM_BUFFER, 0,
                        (struct sockaddr *)&servaddr_in, &addrlen) == -1) {
                if (errno == EINTR) {
                    // Timeout: reintentar
                    n_retry--;
                    printf("[Cliente] Timeout, reintentando... (%d intentos restantes)\n", n_retry);
                } else {
                    perror(nombre_programa);
                    fprintf(stderr, "%s: error receiving response\n", nombre_programa);
                    fclose(archivo_ordenes_fp);
                    close(s);
                    exit(1);
                }
            } else {
                // Respuesta recibida correctamente
                alarm(0);
                respuesta_recibida = 1;
                printf("S: %s", bufResp);
                fprintf(archivo_salida, "S: %s", bufResp);
                fflush(archivo_salida);
                
                // Si es mensaje de cierre (221), terminar
                if (strncmp(bufResp, "221", 3) == 0) {
                    printf("\n[Cliente] Cerrando conexion...\n");
                    break;
                }
            }
        }
        
        // Verificar si se agotaron los reintentos
        if (n_retry == 0 && !respuesta_recibida) {
            fprintf(stderr, "[ERROR] No se pudo contactar con el servidor tras %d intentos\n", RETRIES);
            break;
        }
    }
           
    if (archivo_ordenes_fp) {
        fclose(archivo_ordenes_fp);
    }

    // CERRAR ARCHIVO DE SALIDA
    if (archivo_salida) {
        fprintf(archivo_salida, "\n=== Fin de la comunicacion ===\n");
        time(&timevar);
        fprintf(archivo_salida, "Finalizado: %s", ctime(&timevar));
        fclose(archivo_salida);
    }
        
    
    close(s);
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <servidor> <protocolo> <archivo_ordenes>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s nogal TCP ordenes.txt\n", argv[0]);
        exit(1);
    }
    
    char *servidor = argv[1];
    char *protocolo = argv[2];
    char *archivo_ordenes = argv[3];
    
    // Convertir protocolo a mayúsculas para comparación
    int i;
    for (i = 0; protocolo[i]; i++) {
        protocolo[i] = toupper(protocolo[i]);
    }
    
    if (strcmp(protocolo, "TCP") == 0) {
        clienteTCP(servidor, archivo_ordenes, argv[0]);
    } else if (strcmp(protocolo, "UDP") == 0) {
        clienteUDP(servidor, archivo_ordenes, argv[0]);
    } else {
        fprintf(stderr, "Protocolo no válido: %s (usar TCP o UDP)\n", protocolo);
        exit(1);
    }
    
    return 0;
}
