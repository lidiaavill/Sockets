# Servicio Morse sobre Sockets - Aplicación en Red

## 📋 Descripción General

Implementación de una aplicación cliente-servidor que proporciona un servicio de conversión de texto a código Morse a través de la red, utilizando sockets Berkeley en C. El servidor soporta tanto conexiones **TCP** como **UDP**, permitiendo que múltiples clientes se conecten simultáneamente.

Este proyecto forma parte de la asignatura **Redes I (GII)** en la Universidad de Salamanca, bajo la dirección de Moreno, A.M.y Bravo, S.

## 🎯 Objetivos

- Implementar una aplicación en red como usuario del nivel de transporte (capa 4)
- Aplicar la arquitectura cliente-servidor en la programación de sockets
- Manejar conexiones TCP y UDP simultáneamente en el servidor
- Implementar un protocolo de comunicación personalizado
- Gestionar múltiples conexiones concurrentes con procesos hijo
- Registrar todas las operaciones en un archivo de log

## 🔧 Características Principales

### Servidor (`servidor.c`)
- **Modo daemon**: Se ejecuta como proceso independiente en background
- **Soporte dual**: Maneja conexiones TCP y UDP simultáneamente usando `select()`
- **Multiproceso**: Crea procesos hijo para atender a cada cliente TCP
- **Conversión Morse**: Convierte frases a código Morse según el estándar internacional
- **Logging sincronizado**: Registra todas las operaciones usando semáforos
- **Manejo de señales**: Implementa `SIGTERM` para cierre ordenado y `SIGCHLD` para evitar procesos zombie

### Cliente (`cliente.c`)
- **Protocolo dual**: Soporta conexiones TCP y UDP
- **Lectura de ficheros**: Lee comandos desde ficheros de órdenes
- **Gestión de puertos efímeros**: Obtiene el puerto local asignado automáticamente
- **Logging de respuestas**: Guarda todas las respuestas en ficheros con nombre del puerto
- **Reintentos UDP**: Implementa reintentos con timeouts para UDP

## 📡 Protocolo de Comunicación

### Especificaciones

- **Mensajes**: Líneas de caracteres terminadas con `CR-LF` (`\r\n`)
- **Tamaño máximo**: 516 bytes (incluyendo `CR-LF`)
- **Códigos de respuesta**:
  - `220`: Servicio preparado (al conectar)
  - `240`: OK (respuesta a HOLA)
  - `250 MORSE`: Respuesta a FRASE con código Morse
  - `221`: Cerrando el servicio (respuesta a FIN)
  - `500`: Error de sintaxis

### Flujo de Comunicación

```
1. Cliente → Servidor: HOLA <dominio>
   Servidor → Cliente: 240 OK

2. Cliente → Servidor: FRASE <texto>
   Servidor → Cliente: 250 MORSE <código>
   
3. Cliente → Servidor: FIN
   Servidor → Cliente: 221 Cerrando el servicio
```

#### Ejemplo de diálogo:
```
S: 220 Servicio preparado
C: HOLA usal.es
S: 240 OK
C: FRASE SOS
S: 250 MORSE .../---/...
C: FRASE Hoy es fiesta
S: 250 MORSE ..../---/-.--/./.../..-./.././.../-/.-
C: FIN
S: 221 Cerrando el servicio
```

### Codificación Morse

- **Letras**: Separadas por `/` (ej: `SOS` → `.../---/...`)
- **Espacios en palabras**: Representados por `/` (ej: `SOS es` → `.../---/..././...`)
- **Caracteres soportados**: A-Z (0-9 también disponibles)
- **Mayúsculas automáticas**: El servidor convierte automáticamente a mayúsculas

## 📦 Estructura del Proyecto

```
.
├── servidor.c           # Implementación del servidor
├── cliente.c            # Implementación del cliente
├── clientcp.c           # Cliente TCP (referencia alternativa)
├── clientudp.c          # Cliente UDP (referencia alternativa)
├── Makefile             # Compilación automática
├── lanzaServidor.sh     # Script para ejecutar servidor y clientes de prueba
├── ordenes.txt          # Fichero de órdenes de prueba 1
├── ordenes1.txt         # Fichero de órdenes de prueba 2
├── ordenes2.txt         # Fichero de órdenes de prueba 3
├── peticiones.log       # Fichero de log de servidor (generado)
└── README.md            # Este fichero
```

## 🛠️ Compilación

### Requisitos
- **SO**: Debian GNU/Linux 11 (bullseye) o compatible
- **Compilador**: GCC
- **Herramientas**: Make
- **Librerías**: POSIX threads, semáforos del sistema (`librt`)

### Instrucciones

```bash
# Compilar todos los programas
make

# Compilar solo el servidor
make servidor

# Compilar solo el cliente
make cliente

# Limpiar ficheros objeto y ejecutables
make clean
```

## 🚀 Ejecución

### Iniciar el Servidor

```bash
./servidor
```

El servidor se ejecuta como daemon y comienza a escuchar en el puerto **53278** (TCP y UDP).

### Ejecutar un Cliente (Manual)

```bash
# Cliente TCP
./cliente <servidor> TCP <fichero_ordenes>
./cliente nogal TCP ordenes.txt

# Cliente UDP
./cliente <servidor> UDP <fichero_ordenes>
./cliente nogal UDP ordenes.txt
```

#### Parámetros:
- `<servidor>`: Nombre de host o dirección IP del servidor (ej: `nogal`, `localhost`)
- `<protocolo>`: `TCP` o `UDP`
- `<fichero_ordenes>`: Ruta al fichero con los comandos a ejecutar

### Ejecución Automática (Script)

```bash
chmod +x lanzaServidor.sh
./lanzaServidor.sh
```

Este script lanza el servidor y 6 clientes de prueba (3 TCP + 3 UDP) en paralelo.

## 📝 Formato de Ficheros de Órdenes

Cada línea contiene un comando a enviar al servidor:

```
HOLA usal.es
FRASE SOS
FRASE Hoy es fiesta
FIN
```

### Comandos Válidos:
- `HOLA <dominio>`: Inicia la conexión
- `FRASE <texto>`: Solicita conversión a Morse
- `FIN`: Cierra la conexión

## 📊 Ficheros de Salida del Cliente

Cada cliente genera un fichero con el nombre de su puerto efímero:

```
<puerto_efímero>.txt
```

**Contenido de ejemplo** (`48244.txt`):
```
=== CLIENTE TCP - Puerto: 48244 ===
Servidor: nogal
Protocolo: TCP
Archivo de órdenes: ordenes1.txt

S: 220 Servicio preparado

C: HOLA marca.com
S: 240 OK

C: FRASE SOS
S: 250 MORSE .../---/...

C: FIN
S: 221 Cerrando el servicio

=== Servidor cerró la conexión ===

=== Fin de la comunicación ===
Finalizado: Wed Nov 26 14:47:51 2025
```

## 📋 Fichero de Log del Servidor

El servidor registra todas las operaciones en `peticiones.log`:

```
[2025-11-26 14:47:45] Comunicación realizada - Host: nogal.fis.usal.es, IP: 212.128.144.105, Protocolo: TCP, Puerto: 48244
[2025-11-26 14:47:46] Respuesta enviada - Host: nogal.fis.usal.es, IP: 212.128.144.105, Protocolo: TCP, Puerto: 48244, Respuesta: 240 OK
[2025-11-26 14:47:46] Frase recibida - Host: nogal.fis.usal.es, IP: 212.128.144.105, Protocolo: TCP, Puerto: 48244, Frase: FRASE SOS
[2025-11-26 14:47:48] Respuesta enviada - Host: nogal.fis.usal.es, IP: 212.128.144.105, Protocolo: TCP, Puerto: 48244, Respuesta: 250 MORSE .../---/...
[2025-11-26 14:47:51] Comunicación finalizada - Host: nogal.fis.usal.es, IP: 212.128.144.105, Protocolo: TCP, Puerto: 48244
```

**Información registrada**:
- Fecha y hora de cada evento
- Nombre de host y dirección IP del cliente
- Protocolo de transporte (TCP/UDP)
- Puerto efímero del cliente
- Tipo de evento:
  - Comunicación realizada (nueva conexión)
  - Frase recibida (con el texto)
  - Respuesta enviada (con el código Morse)
  - Comunicación finalizada (desconexión)

## 🔐 Sincronización

### Semáforos POSIX
El servidor utiliza semáforos POSIX para garantizar el acceso seguro al fichero `peticiones.log` cuando múltiples procesos escriben simultáneamente.

```c
sem_t *log_semaphore = sem_open("/morse_log_sem", O_CREAT | O_EXCL, 0644, 1);
```

- **Operación**: `sem_wait()` bloquea antes de escribir
- **Liberación**: `sem_post()` desbloquea tras escribir

## 🧪 Pruebas Realizadas

El proyecto incluye 3 ficheros de prueba con diferentes escenarios:

### `ordenes.txt`
- Comandos correctos
- Conversión de frases simples
- Errores intencionales para probar validación

### `ordenes1.txt`
- Prueba con dominio alternativo (`marca.com`)
- Diferentes frases
- Errores de sintaxis

### `ordenes2.txt`
- Comandos HOLA incorrectos (`HELO` vs `HOLA`)
- FRASE antes de HOLA
- Múltiples intentos correctos

### Resultados
- ✅ Servidor maneja múltiples clientes TCP simultáneamente
- ✅ Servidor procesa peticiones UDP concurrentes
- ✅ Conversión correcta a código Morse
- ✅ Validación de protocolo funciona
- ✅ Log sincronizado sin condiciones de carrera
- ✅ Cierre ordenado de conexiones

## 🔄 Diferencias TCP vs UDP

| Aspecto | TCP | UDP |
|---------|-----|-----|
| **Conexión** | Conexión establecida | Sin conexión |
| **Fiabilidad** | Garantizada | Sin garantía |
| **Orden** | Conserva orden | Puede desordenarse |
| **Reintentos** | Automáticos | Manual (implementado) |
| **Overhead** | Mayor | Menor |

### Implementación UDP
- Mensaje inicial vacío para obtener respuesta 220
- Sistema de reintentos con timeouts (5 intentos, 6 segundos)
- Manejo de `SIGALRM` para detectar timeouts
`

## 📖 Referencias

- POSIX Sockets API: Berkeley sockets
- Protocolo de aplicación: Estándar personalizado Morse
- Tabla Morse: Código Morse internacional estándar
- Semáforos POSIX: `sem_open()`, `sem_wait()`, `sem_post()`
- Gestión de procesos: `fork()`, `sigaction()`, `select()`

## 👥 Créditos

**Asignatura**: Redes I - Grado en Ingeniería Informática  
**Universidad**: Universidad de Salamanca  
**Autores del enunciado**: Moreno, A.M., Bravo, S., Vázquez, A.  
**Autores de la implementación**: Carolina De Jesús Arolas y Lidia Villarreal Castán
**Fecha**: 29/10/2025

## 📄 Licencia

Proyecto educativo - Universidad de Salamanca

---

**Última actualización**: 26 de Noviembre de 2025


