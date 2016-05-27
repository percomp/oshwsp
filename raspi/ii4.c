#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#define random rand
#else
#include <unistd.h>
#endif

#ifdef CMAKE_BUILD
#include "lws_config.h"
#endif

static char *nombre = "IDUS-22-01-2015";

#include "libwebsockets.h"

static unsigned int opts;
static int se_cerro;  /* sirve para marcar el cierre del websocket desde el servidor */

static int deny_deflate;
static int deny_mux;
static struct libwebsocket *wsi_mirror;
static int mirror_lifetime = 0;
static volatile int force_exit = 0;
static int longlived = 0;

#define ARCHIVIN "./leaf.jpg"
#define CANTIDAD    1024L     /* cantidad de bytes que se transfieren en cada envío */
static char *nombreArchivo = ARCHIVIN;
static FILE *fdatos = NULL;
static long int ifdatos   = 0;        /* numero de fragmento a enviar a continuacion */
static long int nfdatos   = 0;    /* datos que quedan por transferir */
static long int tfdatos   = CANTIDAD; /* datos a transferir en cada envio */

static int websockeservertID = 0; /* numero que le da el servidor a este cliente */
/*
 *  RaspiToPC-protocol:  we connect to the server and print the primitive
 *            we are given
 */

enum mis_protocolos {

   PROTOCOLO_SENSORES,

   /* always last */
   DEMO_PROTOCOL_COUNT
};


/* dumb_increment protocol */

static int
callback_protocolo_sensores(struct libwebsocket_context *context,
                            struct libwebsocket *wsi,
                            enum libwebsocket_callback_reasons evento,
                            void *user, void *in, size_t len) {
   unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 4096 +
                     LWS_SEND_BUFFER_POST_PADDING];
   int nbytes_netos    = 0;
   int nbytes_enviados = 0;

   switch (evento) {
   case LWS_CALLBACK_CLIENT_ESTABLISHED:
      fprintf(stderr, "callback_protocolo_sensores: LWS_CALLBACK_CLIENT_ESTABLISHED\n");
      fprintf(stderr, "Saludo inicial...\n"); fflush(stderr);
     /*
      * start the ball rolling,
      * LWS_CALLBACK_CLIENT_WRITEABLE will come next service
      */
      nbytes_netos    = sprintf((char *)&buf[LWS_SEND_BUFFER_PRE_PADDING],
                                "/saludo %s", nombre);
      nbytes_enviados = libwebsocket_write(wsi,
         &buf[LWS_SEND_BUFFER_PRE_PADDING], nbytes_netos, opts | LWS_WRITE_TEXT);

      printf("saludo enviado\n"); fflush(stdout);
      /*
      if (nbytes_enviados < 0)
         return -1;
      if (nbytes_enviados < nbytes_netos) {
         lwsl_err("Escritura parcial LWS_CALLBACK_CLIENT_WRITEABLE\n");
         return -1;
      }
      */
      fprintf(stderr, "Esperando peticiones del pc\n"); fflush(stderr);

      break;

   case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      fprintf(stderr, "LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
      se_cerro = 1;
      break;

   case LWS_CALLBACK_CLOSED:
      fprintf(stderr, "LWS_CALLBACK_CLOSED\n");
      se_cerro = 1;
      break;

   case LWS_CALLBACK_CLIENT_RECEIVE:
      fprintf(stderr, "rx %d '%*s'\n", (int)len, (int) len, (char *)in); fflush(stderr);

 /*RET00*/
      if (len >= 5 && !memcmp(in, "RET00", 5)) {        /* respuesta de /saludo */
         *((char *)in+len)='\0'; /* construye un NTS */
         sscanf((char *) in + 5, "%d", &websockeservertID);
         printf("El identificador del servidor es: %d\n", websockeservertID);
 /*FILE-*/
      /* Aquí hay que procesar FILE-
       * 1º enviar FILE con /06 talla blocksize
       * 2º esperar /RET60 (archivo abierto listo para recibir)
       *          o /RET61 (error leyendo parametros de transferencia)
       */
      } else if (len >= 5 && !memcmp(in, "FILE-", 5)) {        /* -> /06 */
        struct stat infoFile;

        printf("Hay que abrir el archivo\n"); fflush(stdout);
        if (fdatos || (fdatos = fopen(nombreArchivo, "rb"))) {
          fstat(fileno(fdatos), &infoFile);
          nfdatos = infoFile.st_size;
          tfdatos = CANTIDAD;
          printf("Fichero abierto para enviar\n");
        } else {
          printf("Imposible abrir archivo\n");
          return -1;
        }


        nbytes_netos    = sprintf((char *)&buf[LWS_SEND_BUFFER_PRE_PADDING],
                                  "/06 %ld %ld", nfdatos, CANTIDAD);
        nbytes_enviados = libwebsocket_write(wsi,
           &buf[LWS_SEND_BUFFER_PRE_PADDING], nbytes_netos, opts | LWS_WRITE_TEXT);

 /*RET60*/
      } else if (len >= 5 && !memcmp(in, "RET60", 5)) { /* /06 -> */
         printf("Invocado el OK para empezar a enviar datos\n");

         tfdatos = (tfdatos < nfdatos ) ? tfdatos : nfdatos;

         if (tfdatos != fread((void *) buf+LWS_SEND_BUFFER_PRE_PADDING,
                            1, tfdatos, fdatos)) {
            printf("Imposible leer datos\n");
         }
         nbytes_enviados = libwebsocket_write(wsi,
                    &buf[LWS_SEND_BUFFER_PRE_PADDING], tfdatos, opts | LWS_WRITE_BINARY);

         nfdatos -= tfdatos;
         ifdatos++;
         if (nfdatos <= 0) {
            fclose(fdatos);
            fdatos  = NULL ;
            nfdatos = 0;
            ifdatos = 0;
         } else {
            libwebsocket_callback_on_writable(context, wsi);
         }
 /*RET61*/
      } else if (len >= 5 && !memcmp(in, "RET61", 5)) { /* /06 -> */
         printf("El PC no esta listo para recibir datos\n"); fflush(stdout);
         nfdatos = 0;
         fclose(fdatos);
         ifdatos = 0;
 /*START*/
      } else if (len >= 5 && !memcmp(in, "START", 5)) { /* -> /00 */
      } else if (len >= 5 && !memcmp(in, "STOP-", 5)) { /* -> /01 */
      } else if (len >= 5 && !memcmp(in, "PAUSE", 5)) { /* -> /02 */
      } else if (len >= 5 && !memcmp(in, "CHECK", 5)) { /* -> /03 */
      } else if (len >= 5 && !memcmp(in, "RESET", 5)) { /* -> /04 */
      } else if (len >= 5 && !memcmp(in, "DATOS", 5)) { /* -> /05 */
      } else {  /* -> /10 */
         printf("Mandato de la Raspi no conocido\n"); fflush(stdout);
        return -1;
      }
      break;

   case LWS_CALLBACK_CLIENT_WRITEABLE:
     printf("---%ld,\t", nfdatos); fflush(stdout);
     /* si fdatos != NULL es que se esta dando una transferencia.
      */
     if (fdatos) {
         tfdatos = (tfdatos < nfdatos ) ? tfdatos : nfdatos;
         if (tfdatos != fread((void *) buf + LWS_SEND_BUFFER_PRE_PADDING,
                            1, tfdatos, fdatos)) {
            printf("Imposible leer datos\n");
         }
         nbytes_enviados = libwebsocket_write(wsi,
                    &buf[LWS_SEND_BUFFER_PRE_PADDING], tfdatos, opts | LWS_WRITE_BINARY);

         nfdatos -= tfdatos;
         ifdatos++;
         if (nfdatos <= 0) {
            fclose(fdatos);
            fdatos  = NULL ;
            nfdatos = 0;
            ifdatos = 0;
         } else {
            libwebsocket_callback_on_writable(context, wsi);
         }
     }
     break;

   /* si somos el primer elemento de protocolos[0] ... */
   case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
      if ((strcmp(in, "deflate-stream") == 0) && deny_deflate) {
         fprintf(stderr, "denied deflate-stream extension\n");
         return 1;
      }
      if ((strcmp(in, "deflate-frame") == 0) && deny_deflate) {
         fprintf(stderr, "denied deflate-frame extension\n");
         return 1;
      }
      if ((strcmp(in, "x-google-mux") == 0) && deny_mux) {
         fprintf(stderr, "denied x-google-mux extension\n");
         return 1;
      }
      break;

   default: break;
   }

   return 0;
}


/* list of supported protocols and callbacks */

static struct libwebsocket_protocols protocolos[] = {
   {
      "RaspiToPC-protocol, proprietary-sensoring-protocol",
      callback_protocolo_sensores,
      0,
      4096
   },
   { NULL, NULL, 0, 0 } /* end */
};

void sighandler(int sig)
{
   force_exit = 1;
}

int main(int argc, char **argv) {
   int n = 0;
   int ret = 0;
   int port = 80;
   int use_ssl = 0; /* sin ssl */
   struct libwebsocket_context *context;
   const char *address = "tera.infor.uva.es";
   struct libwebsocket *wsi_elPC;
   int ietf_version = -1; /* latest */

   /* para enviar el primer mensaje */
   unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 4096 +
                     LWS_SEND_BUFFER_POST_PADDING];
   int nbytes_netos    = 0;
   int nbytes_enviados = 0;

   fprintf(stderr, "Cliente de libwebsockets on Raspberry-pi\n");

   signal(SIGINT, sighandler);

   /*
    * create the websockets context.  This tracks open connections and
    * knows how to route any traffic and which protocol version to use,
    * and if each connection is client or server side.
    *
    * For this client-only demo, we tell it to not listen on any port.
    */
   struct lws_context_creation_info info;
   memset(&info, 0, sizeof info);

   info.port = CONTEXT_PORT_NO_LISTEN;
   info.protocols = protocolos;
#ifndef LWS_NO_EXTENSIONS
   info.extensions = libwebsocket_get_internal_extensions();
#endif
   info.gid = -1;
   info.uid = -1;

   context = libwebsocket_create_context(&info);
   if (context == NULL) {
      fprintf(stderr, "Creación fallida del contexto libwebsocket\n");
      return 1;
   }

   /* create a client websocket using RaspiToPC-protocol */

   wsi_elPC = libwebsocket_client_connect(context, address, port, use_ssl,
         "/", "localhost", "localhost", /* argv[optind], argv[optind], */
          protocolos[PROTOCOLO_SENSORES].name, ietf_version);

   if (wsi_elPC == NULL) {
      fprintf(stderr, "libwebsocket conexión con PC fallido\n");
      ret = 1;
   } else {
      /* sit there servicing the websocket context to handle incoming
       * packets,.... nothing happens until the client websocket connection is
       * asynchronously established
       */
      int n = 0;

      while (n >= 0 && !se_cerro && !force_exit) {
         n = libwebsocket_service(context, 1);

         if (n < 0)
            continue;
      }
   }

   fprintf(stderr, "Exiting\n");

   libwebsocket_context_destroy(context);

   return ret;
}
