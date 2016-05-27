#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "caratula.h"
#include "mongoose.h"


static const char *s_no_cache_header =
  "Cache-Control: max-age=0, post-check=0, "
  "pre-check=0, no-store, no-cache, must-revalidate\r\n";
static int s_signal_received = 0;
static struct mg_server *s_server = NULL;
static int nosvamos = 0;

static void handle_websocket_message(struct mg_connection *conn) ;

/* Data associated with each websocket connection
 * La idea es interesante pues habiendo varios websockets podemos
 * muestrear de varias raspi.
 */
//#define ARCHIVIN "leaf.jpg"
#define ARCHIVIN "webdatos.dat"

struct conn_data {
  unsigned  iconexion;
  int       transmision;   /* !=0 (true) se ha pedido la transferencia o
                            * se está transfiriendo.
                            * en cualquier otro caso debe valer 0 (false). */
  int       nfdatos;       /* datos que quedan por transferir */
  FILE      *fdatos;       /* descriptor del archivo  mientras hay
                            * transferencia. Si no, debe ser NULL */
  char      identidad[16];
  unsigned  blocksize ;    /* tamaño de bloque que se transmitirá */
  struct    mg_connection *conexion_websocket;
};

#define MAXDISPOSITIVOS 10
static struct conn_data conn_abiertas[MAXDISPOSITIVOS];
static unsigned short iconexiones = 0;
/* conexión estable del servidor con la raspi es la 0 en este caso.
 **/
#define RASPI 0 
static unsigned short iconexion_actual = RASPI;
/* XDDDD porque sólo tenemos la raspi actualmente */

static void signal_handler(int sig_num) {
  // signal(sig_num, signal_handler);  // Reinstantiate signal handler
  s_signal_received = sig_num;
  if (sig_num == SIGTERM || sig_num == SIGINT) nosvamos = 1;
}

/**************************************************************************************
 * caratula() fabrica una página web en el marco de mongoose (mg_connection) con una línea
 * (mensaje) de estatus, y con un contenido (cabecera y cola) que figuran en caratula.h.
 */
void caratula(struct mg_connection *conn, const char *mensaje) {
  char * funName = "caratula";

      mg_printf_data(conn, "%s", cabecera);
      mg_printf_data(conn, "%s\n", mensaje);
      mg_printf_data(conn, "%s", cola);
}

static void handle_raspi(struct mg_connection *conn) { caratula(conn, "RASPI-callback!!!"); }
static void handle_start(struct mg_connection *conn) { caratula(conn, "START-callback!!!"); }
static void handle_stop(struct mg_connection *conn)  { caratula(conn, "STOP--callback!!!"); }

static int ev_handler(struct mg_connection *conn, enum mg_event ev) {
  switch (ev) {
    case MG_AUTH: return MG_TRUE;
    case MG_REQUEST:
      /******************************
      ***** primitivas Websocket. ***
      *******************************/
      if (conn->is_websocket) {
        struct conn_data *conexion = (struct conn_data *) conn->connection_param;
        /* de momento solo tenemos la raspi */
           if (conexion->iconexion != RASPI) {
               printf("Intento de comunicación desde fuera de la Raspi: [%.48s]\n",
                       conn->remote_ip);
               return MG_TRUE;
           } else {
              (void) handle_websocket_message(conn);
              return MG_TRUE;
           }
      /******************************
      ***** primitivas Web. *********
      *******************************/
      } else {
        if (!memcmp(conn->uri, "/raspi", 6)) {
           mg_send_file(conn, "index.html", NULL);  // Return MG_MORE after!
           return MG_MORE;
        } else if (!memcmp(conn->uri, "/data.txt", 6)) {
           if (access(ARCHIVIN, R_OK) != -1) {
              mg_send_file(conn, ARCHIVIN, NULL); /* podría ser un directorio o CGI...*/
              return MG_MORE;
           } else {
              printf("Datos no preparados.\n");
              return MG_MORE;
           }
        } else if (!memcmp(conn->uri, "/start", 6)) {
           /* enviar START por el websocket 
            */
           caratula(conn, "START!!!");
           printf("enviando mensaje START a \"%.16s\" mediante websocket\n",
                   conn_abiertas[iconexion_actual].identidad); fflush(stdout);
           mg_websocket_printf(conn_abiertas[iconexion_actual].conexion_websocket, WEBSOCKET_OPCODE_TEXT, "START");
           return MG_MORE; /* he quitado MG_MORE */
           //return MG_TRUE; /* he quitado MG_MORE */
        } else if (!memcmp(conn->uri, "/stop", 5)) {
           /* enviar STOP por el websocket 
            */
           printf("enviando mensaje STOP- a \"%.16s\" mediante websocket\n",
                   conn_abiertas[iconexion_actual].identidad); fflush(stdout);
           mg_websocket_printf(conn_abiertas[iconexion_actual].conexion_websocket, WEBSOCKET_OPCODE_TEXT, "STOP-");
           caratula(conn, "STOP!!!!");
           // return MG_TRUE;
           return MG_MORE;
        } else if (!memcmp(conn->uri, "/pause", 5)) {
           /* enviar STOP por el websocket 
            */
           printf("enviando mensaje PAUSE a \"%.16s\" mediante websocket\n",
                   conn_abiertas[iconexion_actual].identidad); fflush(stdout);
           mg_websocket_printf(conn_abiertas[iconexion_actual].conexion_websocket, WEBSOCKET_OPCODE_TEXT, "PAUSE");
           caratula(conn, "PAUSE!!!!");
           return MG_TRUE;
        } else if (!memcmp(conn->uri, "/resum", 5)) {
           /* enviar STOP por el websocket 
            */
           printf("enviando mensaje RESUM a \"%.16s\" mediante websocket\n",
                   conn_abiertas[iconexion_actual].identidad); fflush(stdout);
           mg_websocket_printf(conn_abiertas[iconexion_actual].conexion_websocket, WEBSOCKET_OPCODE_TEXT, "RESUM");
           caratula(conn, "RESUME!!!!");
           return MG_TRUE;
        } else if (!memcmp(conn->uri, "/file", 5)) {
           /* pide archivo desde el dispositivo al PC.
            */
           if (conn_abiertas[iconexion_actual].transmision) {
             caratula(conn, "ESPERE, transmision ya iniciada");
             printf("transmision ya solicitada"); fflush(stdout);
           } else {
             caratula(conn, "INICIANDO recepcion");
             printf("enviando mensaje FILE- a \"%.16s\" mediante websocket\n",
                   conn_abiertas[iconexion_actual].identidad); fflush(stdout);
             mg_websocket_printf(conn_abiertas[iconexion_actual].conexion_websocket, WEBSOCKET_OPCODE_TEXT, "FILE-");
           }
           return MG_TRUE;
        } else {
           printf("Primitiva no reconocida en la interfaz web\n");
           return MG_FALSE;
        }
      }
      break;
    case MG_WS_CONNECT:
      {
         struct conn_data *conexion_actual = conn_abiertas + iconexiones;
         memset(conexion_actual, 0x00, sizeof(struct conn_data));
         // New websocket connection. Send connection ID back to the client.
         conexion_actual->iconexion = iconexiones;   /* indice inverso */
         conexion_actual->conexion_websocket = conn; /* referencia inversa */
         conexion_actual->identidad[0] = '\0';       /* inicializar nombre */
         conn->connection_param = conexion_actual;
         iconexiones++;
         printf("Comunicación #%d establecida con <dispositivo> desde [%.48s]\n",
                iconexiones, conn->remote_ip);
      }
      return MG_FALSE;
    case MG_CLOSE:
      {
         if (conn->is_websocket) {
            iconexiones--;
            memset((void *) &(conn->connection_param), 0, sizeof(struct conn_data));
            printf("limpiado websocket\n"); fflush(stdout);
         }
         return MG_TRUE;
      }
    default: return MG_FALSE;
  }
}

/* Protocolo de la raspi al PC
 * return codigo de reconocimiento por cada primitiva enviada a la raspi.
 * 
 * /saludo codigo-identificacion -> RET00
 * /estado codigo de estatus de la raspi
 * /string string con datos a enviar de la raspi al PC en crudo.
 * /archiv archivo devuelto a petición de los datos de sensores desde el PC.
 * /cierre finalización de la comunicación desde la raspi.
 */

/* Protocolo desde el PC a la raspi
 * RET-- codigo de reconocimiento por cada primitiva enviada al PC
 *       00 : OK
 * START iniciar adquisición.  -> /00
 * STOP- parar la adquisición. -> /01
 * PAUSE pausar la adquisición.-> /02
 * CHECK averiguar el estado.  -> /03
 * RESET resetear el archivo de datos. -> /04
 * DATOS solicitar datos a la raspi desde el PC en modo continuo. -> /05 y -> /string por cada linea de datos.
 * FILE- solicitar datos a la raspi desde el PC. -> /06 TALLA BLOCKSIZE (tamaño del archivo y bytes por bloque, en modo WEBSOCKET_OPCODE_BINARY
 *       el receptor debe recomponer el archivo y dejarlo correctamente.
 */

/* se supone que solo va a ser invocado con conn_abierto
 */
static void handle_websocket_message(struct mg_connection *conn) {
  struct conn_data *conexion= (struct conn_data *) conn->connection_param;
  static char *datablock= NULL;

  /* estamos hablando de un archivo */
  if (WEBSOCKET_OPCODE_BINARY & conn->wsbits) {
     if (0==conexion->transmision) {
       /* printf("[%p, %p] ", conn, conexion); // aquí se imprime la coonexión y las tramas */
       /*
       short x = conn->wsbits & 0x000f;
       printf("%s ",
          (0x000a == x)? "PONG" :
             ((0x0002 == x)? "BING" : "!"));
             */
       /* printf("[%x] ", conn->wsbits); */
       /* printf("Intento de comunicación binaria, sin transmision, o archivo cerrado");*/
       fflush(stdout);
     } else if (!datablock && !(datablock = calloc(1, conexion->blocksize))) {
       printf("Imposible reservar bloque de talla %d\n", conexion->blocksize);
       fflush(stdout);
     } else if (conexion->blocksize < conn->content_len) {
       printf("Recibida una trama mayor que el blocksize\n");
       fflush(stdout);
     } else {
       // printf("* "); fflush(stdout);
       if (conn->content_len != fwrite((void *) conn->content, 1, conn->content_len, conexion->fdatos)) {
         printf("error grabando datos\n"); fflush(stdout);
         fclose(conexion->fdatos);
         conexion->fdatos      = NULL;
         conexion->transmision = 0;
         conexion->nfdatos     = 0;
         conexion->blocksize   = 0;
         free(datablock);
       } else if ((conexion->nfdatos -= conn->content_len)<=0) { /* fin */
         printf("Fin de la transmision\n"); fflush(stdout);
         fclose(conexion->fdatos);
         conexion->fdatos      = NULL;
         conexion->transmision = 0;
         conexion->nfdatos     = 0;
         conexion->blocksize   = 0;
         free(datablock);
         datablock = NULL;
       } /* else {
         printf("."); fflush(stdout);
       } */
       
     }
  /* estamos en una primitiva */
  } else {
     /* Depuracion:
      */
     char linea[60];
     strncpy(linea, conn->content, conn->content_len);
     linea[conn->content_len] = '\0'; /* por alguna razón no me funciona */
     printf("[[%s]]\n", linea); fflush(stdout);
     if (conn->content[0] = '/') {
       if (!memcmp(conn->content, "/string ", 8)) {
         printf("%s> [%s]\n", conexion->identidad, conn->content+8);
/* /saludo id */
       } else if (!memcmp(conn->content, "/saludo ", 7)) {
         strncpy(conexion->identidad, conn->content+8, 16);
         conexion->identidad[15] = '\0'; /* trunca a NTS si rebosa */
         printf("Saludo inicial desde <dispositivo> [%s]\n", conexion->identidad);
         printf("\trespondiendo RET00\n");
         fflush(stdout);
         mg_websocket_printf(conn, WEBSOCKET_OPCODE_TEXT, "RET00 %d", iconexiones);
       } else if (!memcmp(conn->content, "/estado ", 7)) {
       } else if (!memcmp(conn->content, "/archiv ", 7)) {
       } else if (!memcmp(conn->content, "/cierre ", 7)) {
       } else if (!memcmp(conn->content, "/cierre ", 7)) {
/* Respuestas de START */
       } else if (!memcmp(conn->content, "/00 ", 3)) {
         printf("Realizado START OK <dispositivo> [%s]\n", conexion->identidad);
       } else if (!memcmp(conn->content, "/10 ", 3)) {
         printf("Realizado START ERROR <dispositivo> [%s]\n", conexion->identidad);
/* Respuesta de STOP- */
       } else if (!memcmp(conn->content, "/01 ", 3)) {
         printf("Realizado STOP- OK <dispositivo> [%s]\n", conexion->identidad);
       } else if (!memcmp(conn->content, "/02 ", 3)) {
       } else if (!memcmp(conn->content, "/03 ", 3)) {
       } else if (!memcmp(conn->content, "/04 ", 3)) {
       } else if (!memcmp(conn->content, "/05 ", 3)) {
       } else if (!memcmp(conn->content, "/06 ", 3)) { /* RET from FILE- */
         long int talla, blocksize;
         *((char *)conn->content+conn->content_len) = '\0';
         
         /* viene acompañado del tamaño del archivo y el blocksize */
         talla = atol(strtok((char *) conn->content+4, " \t,"));
         blocksize = atol(strtok(NULL, " \t,"));
         
         if (0 == talla || 0 == blocksize) {
            printf("------------- %ld, %ld--------\n", talla, blocksize);
            mg_websocket_printf(conn, WEBSOCKET_OPCODE_TEXT, "RET61");
            printf("Error leyendo parámetros de transferencia de archivo en /06\n");
            printf("\trespondiendo RET61\n");
            fflush(stdout);
         } else {
            conexion->nfdatos     = talla;
            conexion->blocksize   = blocksize;
            conexion->transmision = 1;
            conexion->fdatos = fopen(ARCHIVIN, "wb");
            printf("Abierto > %s, talla=%ld, bloque=%ld\n", ARCHIVIN, talla, blocksize);
            printf("\trespondiendo RET60, %p, %p\n", conn, conexion);
            fflush(stdout);
            mg_websocket_printf(conn, WEBSOCKET_OPCODE_TEXT, "RET60");
         }
       } else {
         printf("Mandato de la raspi no reconocido [%s]\n", conn->content); fflush(stdout);
       }
     } else {
       printf("Mandato no reconocido desde <%s>\n", conexion->identidad);
     }
  }
}

int main(void) {
  struct mg_server *server;

  // Create and configure the server
  server = mg_create_server(NULL, ev_handler);
  mg_set_option(server, "listening_port", "80");

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  
  iconexiones = 0;

  // Serve request. Hit Ctrl-C to terminate the program
  printf("Starting on port %s\n", mg_get_option(server, "listening_port"));
  for (;!nosvamos;) {
    mg_poll_server(server, 100);
    /* en restful api pone 100, pero en ws 100 */
  }

  // Cleanup, and free server instance
  mg_destroy_server(&server);

  return 0;
}
