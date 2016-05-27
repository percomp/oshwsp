/****************************************************************************
 * ii2c.c Configuración y lectura de datos del dispositivo de sensores
 *            Arduino/MPU6000
 * 
 * ii2c [-A 0|1|2|3] [-G 0|1|2|3] [-F 31.25-8000][-m|-g] [-d|-r|-f]\n\n",
 * -A            Fondo de escala del acelerómetro: 0 (2g), 1 (4g),"
 *                2 (8g), 3 (16g).\n"
 * -G            Fondo de escala del giróscopo: 0 (250 º/s), 1 (500 º/s),\n"
 *                2 (1000 º/s), 3 (2000 º/s).\n"
 * -F            Frecuencia de muestreo interna del MPU (31.25 a 8000).\n"
 *                Seguramente, distinta de la del dispositivo.\n"
 * -m|-g       m/(s*s) o g (9.8 m/(s*s))\n"
 * -d|-r|-f   [º/s] o [rad/s] o [Hz]\n");
 * -l            [Dispositivo USB]   para "cu -l dev"         <not implemented yet>
 * -s            [velocidad del canal serie] para "cu -s" <not implemented yet>
 * 
 * Ejemplo:
 * 
 * Autor: César Llamas Bello, 2014
 *            PerComp - UVa.
 ***************************************************************************/

/***************************************************************************
 * El programa interactua mediante el mandato cu, que se encarga de la
 * entrada y salida a bajo nivel. Mediante el, le envía los parámetros de
 * configuración del dispositivo de sensorización y se encarga de
 * recibir los datos de salida codificados y los imprime en formato flotante
 * ya normalizados según los parámetros de muestreo especificados.
 **************************************************************************/
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <math.h>
#include <time.h>
#include <signal.h>

#define UNUSED(x) (void)(x)

#include "caratula.h"
#include "mongoose.h"

#define DEPURACION_0

#ifdef DEPURACION_0
#define DEP_0(x) (void) printf("\nDEP-0 [%s] :%s:", funName, (x)); fflush(stdout);
#else
#define DEP_0(x) ;
#endif

#define DEPURACION_1

#ifdef DEPURACION_1
#define DEP_1(x) (void) printf("\n\tDEP-1 [%s] :%s:", funName, (x)); fflush(stdout);
#else
#define DEP_1(x) ;
#endif

#define DEPURACION_2

#ifdef DEPURACION_2
#define DEP_2(x) (void) printf("\n\t\tDEP-2 [%s] :%s:", funName, (x)); fflush(stdout);
#else
#define DEP_2(x) ;
#endif
/*#####################
 * Variable de flujo de estado del GUI de entrada web.
 * IDLE: sin hacer nada.
 * READING: leyendo del Arduino, mediante dos procesos intermedios: el "proceso lector" hijo del actual
 *          interactúa con el proceso "cu" que es el que sondea el canal serie del arduino. El proceso lector
 *          es el que envía los datos al archivo de datos.
 * IDLE:      esperando las acciones de lectura.
 * El diagrama de estado está en la cabecera del web_event_handler(), que es invocado por el servidor
 * web embebido "mongoose", quien lleva la cuenta de la interacción desde el dispositivo móvil (o pc) que
 * sirve de interfaz de usuario de alto nivel con la plataforma de sensorización.
 */
static enum EstadoProceso { IDLE, READING_READ, READING_WAIT, ERROR } estadoProceso = IDLE;

/**
 * Cada proceso tiene su bucle de salida.
 * (SIGINT) (^C) afecta al bucle Web, y 
 * (SIGUSR1)       afecta al bucle de lectura dataDump.
 * Como el manejador de señales es único, debe contemplar los dos casos :D
 */
static int keepRunningWeb   = -1; /* true */
static int keepRunningDump = -1; /* true */

#define EXIT_STATUS_OK 0 /* sin error */
#define EXIT_FAIL_ARGMNT    1 /* fallo en argumentos */
#define EXIT_FAIL_FORK      2 /* fallo en fork */
#define EXIT_FAIL_SHMEM     3 /* fallo en memoria compartida */
#define EXIT_FAIL_CANAL     4 /* fallo en E/S (dup2 o pipe) */
#define EXIT_FAIL_EXEC      5 /* fallo en la aplicación del execvp(el cu o lo que sea) */
#define EXIT_FAIL_FOPEN     6 /* fallo en la apertura/creación de archivo */

static int exit_status = EXIT_STATUS_OK; /* siempre viene bien para salidas por la vía rapida */


/* indicadores de procesos */
pid_t pLector = 0;

/*####################
 * Comunicación proceso main() y proceso lectorMain().
 */
/* talla de la memoria compartida que se necesita para el siguiente catálogo */
#define SHMSZ    37
/* STATUS del proceso de lectura para el gestor de señales del webserver cuando
 * se elige CHECK */
#define SIDLE     "IDLE"             /* el sensor no ha sido utilizado aún */
#define SERRFORK  "ERROR-FORK"       /* error en el fork de lectorMain() */
#define SERRCANAL "ERROR-CANAL"      /* error en la creación de streams y dup2() */
#define SERREXEC  "ERROR-EXEC"       /* error en el exec del cu */
#define SREADY    "READY"            /* preparado para leer */
#define SACQUIRE  "ACQUIRE"          /* leyendo datos */
#define SPAUSE    "PAUSE"            /* pausa en la lectura */
/* ORDENES del webserver hacia el proceso de lectura, que afectan a la adquisición
 * de datos de Arduino. */
#define DORESUME  "DORESUME"         /* pedir a Arduino leer datos */
#define DOPAUSE   "DOPAUSE"          /* pedir a Arduino parar la lectura */
#define TRANSITO  "TRANSITO"         /* valor transitorio tras pedir a Arduino una acción */

/* Puntero a la memoria compartida entre el WebServer y el intHandler
 * contiene uno de los mensajes anteriores.
 * Cuando no está activo lectorMain() debe ser "IDLE".
 * Se reserva su memoria en main().
 * http://stackoverflow.com/questions/230062/whats-the-best-way-to-check-if-a-file-exists-in-c-cross-platform
 */
char *shm;
/* para comunicar en la memoria compartida */
#define comSHM(x)   strcpy(shm, (x))   

/*#####################
 * Estructura de almacenamiento de datos en archivo.
 */
struct DataFile {
    char nombre[256];     /* nombre completo del archivo actual utilizado */
    char nombreParte[256];/* nombre de la parte del archivo.
                           * Es como el nombre pero con un número adicional */
    char nombreBase[128]; /* nombre base del archivo */
    char nombrePath[128]; /* nombre directorio del archivo */
    char extension[8];    /* extensión del nombre del archivo */
    int turnoNombre ;
} dataFile;    /* se inicializan con initFile() */

/*######################
 * Descriptor de archivo del log de la aplicación. El nombre del archivo log es FLOGNAME
 * Ver logInit, LogPrint (que es un macro) y logClose.
 * Toda función donde se haga LogPrint debe haber definido char *logHandle
 */
#define FLOGNAME "./ii3.log"
FILE *logHandle=NULL;
#define LogPrint(x) logPrint(funName, (x))
    
/*######################
 * Parámetros de la sensorización y lectura de datos.
 */
/* Estructura que sustenta los parámetros de adquisición de los sensores */
struct SensParm {
    float fMuestreo;             /* frecuencia de muestreo */
    unsigned int iMuestreo;   /* configuración del registro de la IMU para la fmuestreo anterior */
    char *modoAcc ;   /*(+-2g)*/
    char *modoGyr ;   /*(+-250º/s)*/
    char *sRate    ;   /*(100 muestras/s)*/
    int c;
    int unidadesAcc; /* 0: g, 1: m/(s*s) */
    int unidadesGyr; /* 0: º/s, 1: rad/s, 2: Hz */
     /*********
      * sensParm struct initialization
      ********/
          
    char *uAceleracion;
    char *uGiroscopo;

    /* para la sensibilidad del acelerometro de MPU6000
    *   por defecto en g */
    float normaA;

    /* para la sensibilidad del giroscopo de MPU6000
    *   por defecto en º/s */
    float normaG;

    /* para la frecuencia de muestreo del MP6000
    *   por defecto en Hz.
    */
    float Rate;
} sensParm ;


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Method declaration section
 *~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void caratula(struct mg_connection *, const char*);
static int web_event_handler(struct mg_connection *, enum mg_event);

int dataDump(FILE *, FILE *);
int lectorMain();
void goRead();
void goWait();

unsigned int hexa2binInt(unsigned char c) ;
unsigned int hexa2binShort(unsigned char c) ;
short aCorto(unsigned char *s) ;
unsigned int aInt(unsigned char *s) ;
char * printFecha(char *) ;
void ayuda(char *) ;

/*****************************
 *****************************
 * Métodos.
 ****************************/

/**************************************************************************************
 * logInit() Crea el archivo de log y lo deja abierto para que cualquier método envíe
 * un mensaje al log. Si el archivo existe lo pisa con nuevos contenidos.
 */
int logInit() {
  extern FILE * logHandle;
  
  return (logHandle = fopen(FLOGNAME, "a")) ? -1 : 0;
}

/**************************************************************************************
 * logClose() Cierra el archivo de log.
 */
int logClose() {
  extern FILE * logHandle;
  
  return fclose(logHandle);
}

/**************************************************************************************
 * logPrint() Imprime mediante LogPrint, la fecha el nombre de la función y el mensaje.
 */
void logPrint(char *funcion, char *mensaje) {
  char linea[40];
  extern FILE * logHandle;
  
  (void) fprintf(logHandle, "%s:%s:%s\n", (char *) printFecha(linea), funcion, mensaje);
  (void) fflush(logHandle);
}

/**************************************************************************************
 * initFileName() inicializa el campo nombre con los campos de la estructura dataFile
 * En realidad solo se usa una vez en el main, pues no se cambiará (en principio...)
 * También inicializa el campo nombreParte.
 */
void initFileName() {
  char * funName = "initFileName";
  
  extern struct DataFile dataFile;
  
  dataFile.turnoNombre = 0;
  sprintf(dataFile.nombre, "%s%s%s",
          dataFile.nombrePath, dataFile.nombreBase, dataFile.extension);
  sprintf(dataFile.nombreParte, "%s%s-%.2d%s",
          dataFile.nombrePath, dataFile.nombreBase, dataFile.turnoNombre,
          dataFile.extension);
}

/**************************************************************************************
 * initFilePart() inicializa el nombre con los campos de la estructura dataFile
 * En realidad solo se usa una vez en el main, pues no se cambiará (en principio...)
 */
void initFilePart() {
  char * funName = "initFilePart";
  
  extern struct DataFile dataFile;
  
  dataFile.turnoNombre = 0;
  sprintf(dataFile.nombreParte, "%s%s-%.2d%s",
          dataFile.nombrePath, dataFile.nombreBase, dataFile.turnoNombre,
          dataFile.extension);
}


/**************************************************************************************
 * incFilePart() incrementa el nombre de la parte del nombre del archivo para multi-parte
 */
void incFilePart() {
  char * funName = "incFilePart";
  
  extern struct DataFile dataFile;
  
  dataFile.turnoNombre ++;
  sprintf(dataFile.nombreParte, "%s%s-%.2d%s", 
          dataFile.nombrePath, dataFile.nombreBase, dataFile.turnoNombre,
          dataFile.extension);
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

/**************************************************************************************
 * updateError() actualiza el string de entrada con el último mensaje de error, si es
 * este el caso, que se recibe de la memoria compartida con el Arduino.
 * Acto seguido resetea el valor del Arduino a 0 en la esperanza de que se haya caído
 * el proceso lector.
 * No debe usarse en la función de parada STOP, pues borraríamos el PID del proceso
 * lector.
 */
int updateError(char *m) {
  char * funName = "updateError";
  
      if (shm[0] == 'E') { /* la memoria compartida contiene un mensaje de error */
         sprintf(m, "STATUS: ERROR: [%s], going IDLE", shm);
         
         /* limpieza de cutis */
         comSHM(SIDLE);
         pLector = 0;
         estadoProceso = IDLE;
         
         return -1;   /* no hubo un mensaje de error de los sensores */
      } else {
         return 0;    /* hubo un mensaje de error de los sensores */
      }
}

/**************************************************************************************
 * web_event_handler() es invocado en el marco de mongoose cada vez que el servidor
 * debe despachar un mensaje.
 * 
 * State flow del manejo de eventos:
 * Estados: IDLE, READING {READ | WAIT}, ERROR
 * Inputs:   START, STOP, PAUSE, CHECK y RESET
 * Flowchart:
 *       init: IDLE
 *  A    when: IDLE    
 *                     and START go READING/READ
 *                               do { startReading();
 *                                    goRead();  }
 *                               output "status" 
 *  B                  and STOP  go IDLE
 *                               do { }
 *                               output "warning, already stopped"
 *  C                  and PAUSE go IDLE
 *                               do { }
 *                               output "warning, already stopped"
 *  D                  and CHECK go IDLE
 *                               do { check(); }
 *                               output "status"
 *  E                  and RESET go IDLE
 *                               do { resetReading(); }
 *                               output "status"
 *  F    when: READING/READ
 *                     and START go READING/READ
 *                               do { }
 *                               output "warning, already reading" 
 *  G                  and STOP  go IDLE
 *                               do { goWait();
 *                                    stopReading();
 *                                    check(); }
 *                               output "status"
 *  H                  and PAUSE go READING/WAIT
 *                               do { goWait(): }
 *                               output "status"
 *  I                  and CHECK go READING/READ
 *                               do { check(); }
 *                               output "status"
 *  J                  and RESET go READING/READ
 *                               do { }
 *                               output "warning, can't do reset while reading"
 *  L    when: READING/WAIT
 *                     and START go READING/READ
 *                               do { goRead() }
 *                               output "status" 
 *  M                  and STOP  go IDLE
 *                               do { stopReading();
 *                                    check(); }
 *                               output "status"
 *  N                  and PAUSE go READING/WAIT
 *                               do { }
 *                               output "warning, already paused"
 *  O                  and CHECK go READING/WAIT
 *                               do { check(); }
 *                               output "status"
 *  P                  and RESET go READ/WAIT
 *                               do {}
 *                               output "warning, can't do reset while reading"
 */
static int web_event_handler(struct mg_connection *conn, enum mg_event ev) {
  char * funName = "web_event_handler";
  
   extern struct DataFile dataFile;
   extern int pLector;
   char mensaje[64]; /* string para lo que sea menester */
   
   if (ev == MG_AUTH) {
      return MG_TRUE;    // Authorize all requests
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/hello")) {
      mg_printf_data(conn, "%s", "Hello world");
      return MG_TRUE;    // Mark as processed
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/index")) { /* página de inicio */
      DEP_0("WebCall:index");
      caratula(conn, "STATUS: OK.");
      return MG_TRUE;    // Mark as processed
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/datos")) {
      DEP_0("WebCall:Get archivo datos");
      if (access( dataFile.nombre, R_OK ) != -1) {
         mg_send_file(conn, dataFile.nombre, NULL);  /* Also could be a dir, or CGI */
         return MG_MORE;                           /* It is important to return MG_MORE after mg_send_file! */
      } else {
         caratula(conn, "ERROR: Archivo de datos vacío.");
      }
      return MG_TRUE;
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/start")) {
      DEP_0("WebCall:Get Start");
      if (updateError(mensaje)) {
         caratula(conn, mensaje);
         return MG_TRUE;
      }
      /*  A    when: IDLE    
       *                     and START go READING/READ
       *                               do { startReading();
       *                                    goRead();  }
       *                               output "status" 
       *  F    when: READING/READ
       *                     and START go READING/READ
       *                               do { }
       *                               output "warning, already reading" 
       *  L    when: READING/WAIT
       *                     and START go READING/READ
       *                               do goRead()
       *                               output "status" 
       */   
      switch (estadoProceso) {
        case IDLE: /*       and START go READING/READ
                    *                 do { startReading();
                    *                      goRead();  }
                    *                 output "status" 
                    */
                    {
                      int retornoLector; /* variable para el proceso hijo (case 0) */
                      switch (pLector=fork()) {
                      case 0: // hijo: proceso lector del Arduino
                           switch (retornoLector = lectorMain()) { /* al log de error */
                                  /* lectorMain() al ponerse en marcha hace un startReading()
                                   * y un goRead() por defecto, por intermediación de
                                   * dataDump(). Si no puede, devuelve error.
                                   */
                            case EXIT_STATUS_OK:  /* preparado para ampliaciones */
                            case EXIT_FAIL_CANAL:
                            case EXIT_FAIL_EXEC:
                            case EXIT_FAIL_FORK:
                                 DEP_0("lectorMain fallido");
                                 DEP_0(shm);
                                 break;
                           }
                           exit(retornoLector);
                           break;
                       case -1: // fallo
                           /* fallo en el fork */
                           exit_status = EXIT_FAIL_FORK;
                           keepRunningWeb = 0;
                           caratula(conn, "STATUS: E-Arrancando: fork fallido.");
                           break;
                       default: // padre: hilo actual del main web.
                           estadoProceso = READING_READ;
                           DEP_0("estadoProceso=READING_READ");
                           caratula(conn, "STATUS: OK: Arrancado.");
                      }
                    }
                    break;
        case READING_READ:
                    /*     and START go READING/READ
                     *               do { }
                     *               output "warning, already reading" 
                     */
                    caratula(conn, "STATUS: WARNING: Proceso ya leyendo.");
                    DEP_0("Proceso ya leyendo");
                    DEP_0(shm);
                    break;
        case READING_WAIT:
                    /*     and START go READING/READ
                     *               do goRead()
                     *               output "status" 
                     */
                    goRead();
                    DEP_0("estadoProceso=Init-READING_READ");
                    DEP_0(shm);
                    estadoProceso = READING_READ;
                    caratula(conn, "STATUS: OK: Iniciando lectura.");
        default:    ;
      }
      return MG_TRUE;    // Mark as processed
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/stop")) {
      DEP_0("WebCall:Get Stop");
      if (updateError(mensaje)) {
         caratula(conn, mensaje);
         return MG_TRUE;
      }
      /*  B when: IDLE    and STOP  go IDLE
       *                            do { }
       *                            output "warning, already stopped"
       *  G when: READING/READ
       *                  and STOP  go IDLE
       *                            do { goWait();
       *                                 stopReading();
       *                                 check(); }
       *                            output "status"
       *  M when: READING/WAIT
       *                  and STOP  go IDLE
       *                            do { stopReading();
       *                                 check(); }
       *                            output "status"
       * 
       */
      switch (estadoProceso) {
        case IDLE:/*      and STOP  go STOP
                   *                do { }
                   *                output "warning, already stopped"
                   */
                  caratula(conn, "STATUS: WARNING: Proceso ya parado.");
                  DEP_0("Proceso ya parado");
                  DEP_0(shm);
                  break;
        case READING_READ:
                  /*      and STOP  go IDLE
                   *                do { goWait();
                   *                     stopReading();
                   *                     check(); }
                   *                output "status"
                   */
                  goWait();
                  /* espero que en un segundo haya parado Arduino */
                  while (strcmp(shm, SPAUSE) && strcmp(shm, SIDLE))
                    sleep(1);
                  
                   /* Trata de desembarazarse del proceso Lector */
                  int proc_status;

                  /* Matar el proceso lector desencadena la muerte del proceso cu también */
                  kill(pLector, SIGUSR1);
                  while (wait(&proc_status) != -1) ; /* pick up dead children (proces lector) */
                  pLector=0;
                  estadoProceso = IDLE;
                  DEP_0("estadoProceso=IDLE");
                  DEP_0(shm);
                  caratula(conn, "STATUS: OK: Lector parado.");
                  break;
        case READING_WAIT: /* ya sé que es parte del anterior... pero así es más robusto */
                  /*      and STOP  go IDLE
                   *                do { stopReading();
                   *                     check(); }
                   *                output "status"
                   */
                   /* Trata de desembarazarse del proceso Lector */
                   {
                     int proc_status;

                     /* Matar el proceso lector desencadena la muerte del proceso cu también */
                     kill(pLector, SIGUSR1);
                     while (wait(&proc_status) != -1) ; /* pick up dead children (proces lector) */
                     pLector=0;
                     estadoProceso = IDLE;
                     DEP_0("estadoProceso=IDLE");
                     DEP_0(shm);
                     caratula(conn, "STATUS: OK: Lector parado.");
                   }
        default:   ;
      }
      return MG_TRUE;    // Mark as processed
      
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/pause")) {
      DEP_0("WebCall:Get Pause");
      if (updateError(mensaje)) {
         caratula(conn, mensaje);
         return MG_TRUE;
      }
      /*  C    when: IDLE    and PAUSE go IDLE
       *                               do { }
       *                               output "warning, already stopped"
       *  H    when: READING/READ
       *                     and PAUSE go READING/WAIT
       *                               do { goWait(); }
       *                               output "status"
       *  N    when: READING/WAIT
       *                     and PAUSE go READING/WAIT
       *                               do { }
       *                               output "warning, already paused"
       */      
      switch (estadoProceso) {
        case IDLE:
                  /*      and PAUSE go IDLE
                   *                do { }
                   *                output "warning, already stopped"
                   */
                  caratula(conn, "STATUS: WARNING: Proceso ya parado.");
                  DEP_0("Proceso ya parado");
                  DEP_0(shm);
                  break;
        case READING_READ:
                  /*      and PAUSE go READING/WAIT
                   *                do { goWait(); }
                   *                output "status"
                   */
                  goWait();
                  /* espero que en un segundo haya parado Arduino */
                  while (strcmp(shm, SPAUSE) && strcmp(shm, SIDLE))
                    sleep(1);
                  estadoProceso = READING_WAIT;
                  caratula(conn, "STATUS: OK: Lector en espera.");
                  DEP_0("estadoProceso=READING_WAIT");
                  DEP_0(shm);
                  break;
        case READING_WAIT:
                  /*      and PAUSE go READING/WAIT
                   *                do { }
                   *                output "warning, already paused"
                   */
                  caratula(conn, "STATUS: WARNING: Lectura ya pausada.");
                  DEP_0("Lectura ya Pausada");
                  DEP_0(shm);
                  break;
        default:  ;
      }
      return MG_TRUE;    // Mark as processed
      
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/check")) {
      /*  D    when: IDLE    and CHECK go IDLE
       *                               do {check(); }
       *                               output "status"
       *  I    when: READING/READ
       *                     and CHECK go READING/READ
       *                               do { check(); }
       *                               output "status"
       *  O    when: READING/WAIT
       *                     and CHECK go READING/WAIT
       *                               do { check(); }
       *                               output "status"
       */
      DEP_0("WebCall:Get Check");
      if (updateError(mensaje)) {
         caratula(conn, mensaje);
         return MG_TRUE;
      }
      switch (estadoProceso) {
        case IDLE:
                  /*      and CHECK go IDLE
                   *                do {check(); }
                   *                output "status"
                   */
                   caratula(conn, "STATUS: OK: Lector parado.");
                   break;
        case READING_READ:
                  /* STATUS del proceso de lectura para el gestor de señales del
                   * webserver cuando se elige CHECK. Sólo va a adquirir uno de estos tres:
                   * READY, ACQUIRE, PAUSE
                   * que son los mensajes admisibles cuando el lector lectorMain() está sano,
                   * puesto que si hubiera sido un mensaje de error habría sido capturado
                   * en cualquier pulsación de la interfaz, y como deviene en un error,
                   * se limpia inmediatamente y  el estado cambia a IDLE.
                   */
                  /*      and CHECK go READING/READ
                   *                do {check(); }
                   *                output "status"
                   */
        case READING_WAIT:
                  /*      and CHECK go READING/WAIT
                   *                do {check(); }
                   *                output "status"
                   */
                  sprintf(mensaje, "STATUS: LECTOR: [%s].", shm);
                  caratula(conn, mensaje);
        default:  ;
      }
      return MG_TRUE;    // Mark as processed
      
   } else if (ev == MG_REQUEST && !strcmp(conn->uri, "/reset")) {
      /*  E    when: IDLE    and RESET go IDLE
       *                               do { resetReading(); }
       *                               output "status"
       *  J    when: READING/READ
       *                     and RESET go READING/READ
       *                               do { }
       *                               output "warning, can't do reset while reading"
       *  P    when: READING/WAIT
       *                     and RESET go READ/WAIT
       *                               do {}
       *                               output "warning, can't do reset while reading"
       */
      DEP_0("WebCall:Get Reset");
      if (updateError(mensaje)) {
         caratula(conn, mensaje);
         return MG_TRUE;
      }
      switch (estadoProceso) {
        case IDLE:
                  /*  and RESET go IDLE
                   *            do { resetReading(); }
                   *            output "status"
                   */
                  (void) unlink(dataFile.nombre); /* tanto si existe como si no */
                  initFilePart();
                  caratula(conn, "STATUS: OK: Archivo reiniciado.");
                  DEP_0("Reset del archivo");
                  DEP_0(shm);
                  break;
        case READING_READ:
                  /*  and RESET go READING/READ
                   *            do { }
                   *            output "warning, can't do reset while reading"
                   */
        case READING_WAIT:
                  /*  and RESET go READING/WAIT
                   *            do { }
                   *            output "warning, can't do reset while reading"
                   */
                  caratula(conn, "STATUS: WARNING: Pare la lectura antes de reiniciar.");
                  DEP_0("Reset en la lectura");
                  DEP_0(shm);
        default:  ;
      }
      return MG_TRUE;    // Mark as processed
      
   } else {
      return MG_FALSE;   // Rest of the events are not processed
   }
}

/**************************************************************************************
 * intHandler() es invocado en el proceso lector de datos del sensor a causa de
 * algún kill en el proceso controlador de la interfaz.
 */
void intHandler(int sig) {
  char * funName = "intHandler";
  
    /**
      * Cada proceso tiene su bucle de salida.
      * (SIGINT)  (^C) afecta al bucle Web, y 
      * (SIGUSR1)      afecta al bucle de lectura de volcado de datos del "proceso lector", que en este
      *                caso se encarga de .
      * Como el manejador de señales es único, debe contemplar los dos casos :D
      */
      if (sig == SIGUSR1)
         keepRunningDump = 0; /* false: stop lectura */
      else { 
         if (READING_READ == estadoProceso || READING_WAIT == estadoProceso) {
            /* Trata de desembarazarse del proceso Lector */
            int proc_status;

               /* Matar el proceso lector desencadena la muerte del proceso cu también */
            kill(pLector, SIGUSR1);
            while (wait(&proc_status) != -1) ; /* pick up dead children (proces lector) */
         }
         keepRunningWeb = 0; /* false: stop todo */
      }
}

/**************************************************************************************
 * dataDump() es la parte principal del bucle que atiende la lectura del stream del cu
 * y lo vuelca a archivo.
 */
int dataDump(FILE * stream_in, FILE * stream_out) {
  char * funName = "dataDump";
  
      extern struct DataFile dataFile;

      extern struct SensParm sensParm; /* via extern es tan feo como forzarlo como argumento XD */
      unsigned char linea[40];
      unsigned char *s;
      int c;

      /* conseguir un apuntador a archivo adecuado, en primera instancia.
       * Sobre dataFile.nombre.
       */
      DEP_1(dataFile.nombre);
      FILE *arch;
      if (NULL == (arch = fopen(dataFile.nombre, "a"))) {
          exit_status = EXIT_FAIL_CANAL;
          comSHM(SERRCANAL);
          return -1;
      }
      DEP_1("Abierto archivo de datos");
      
      
      /**********
       * modoAcc y modoGyr se imprimen como caracter aunque son un dígito.
       * iMuestreo es el número calculado del divisor del reloj, segun el manual
       * del MPU, y este sí que se envia como número.
       */
       /* Aquí esperamos que toda la basurilla que pudiera haberse enviado desde
        * el arduino, se disipe, y empiece a enviar '-'.
        */
       while ((c = fgetc (stream_in)) != '-') {
          // fputc (c, stderr); fflush(stderr);
         ;
       }
       /* fprintf (stderr, "%c\n", c); fflush(stderr); */


       DEP_1("Enviando parámetros al sensor");
       fprintf(stderr, "Sending parameters %c, %c, %u\n", sensParm.modoAcc[0], sensParm.modoGyr[0], sensParm.iMuestreo);
       fflush(stderr);
       fprintf(stream_out, "%c, %c, %u", sensParm.modoAcc[0], sensParm.modoGyr[0], sensParm.iMuestreo);
       fflush(stream_out);   /* this line is extremely importante, te lo juro */
       /* printf("Writing...\n"); fflush(stdout); */

       DEP_1("Esperando datos @");
       while ((c = fgetc (stream_in)) != '@') {
          // fputc (c, stderr); fflush(stderr);
         ;
       }
       /* fprintf (stderr, "%c\n", c); fflush(stderr); */
       
       DEP_1("Enviando cabecera al archivo de datos");
       /********
         * Cabecera de datos para LiveGraph
         *******/
       fprintf(arch, "##:##\n");
       fprintf(arch, "@ParametrosAdquisicion modoAcc %c, modoGyr %c, fMuestreo %f\n",
                           sensParm.modoAcc[0], sensParm.modoGyr[0], sensParm.fMuestreo);
       fprintf(arch, "@FechaAdquisicion %s", printFecha(linea));
       fprintf(arch, "\n@Tiempo (ms):Sensor: xAcc %s: yAcc %s: zAcc %s: xGir %s: yGir %s: zGir %s\n",
                      sensParm.uAceleracion, sensParm.uAceleracion, sensParm.uAceleracion, sensParm.uGiroscopo, sensParm.uGiroscopo, sensParm.uGiroscopo);
       fprintf(arch, "# Parte: %s\n", dataFile.nombreParte);
       incFilePart(); /* preparado para la siguiente adquisición */
       fprintf(arch, "# Comienzo de la adquisición\n");
       fflush(arch); 
       
       LogPrint("Comienzo de la adquisición");

       DEP_1("Comienza la adquisición");
       /* fprintf(stderr, "Bucle de lectura desde CU\n"); fflush(stderr); */
       /* el bucle de lectura del proceso va todo seguido sin dialogo hacia el dispositivo,
         * dado que cualquier condición de comunicación hacia el Arduino implica un retardo en
         * la llamada desde este en el loop(), lo que retrasa el muestreo, de modo que no hay diálogo
         * hacia el dispositivo más allá de las señales del canal serie.
         */
       
       /* Este bucle siempre funciona a las órdenes de arduino, es decir:
        *  - el estado inicial de arduino es enviar datos.
        *  - arduino puede parar o reanudar mediante el boton hardware.
        *  - PC puede parar al arduino enviandole una señal de parada, pero el estado
        *    pasa a corte cuando arduino se lo dice.
        *  - PC puede reanudar al arduino enviandole una señal de lectura, pero el estado pasa a
        *    lectura cuando lo reconoce el arduino.
        *  - El quiz de la cuestión es que este bucle no se bloquea pues el arduino sigue enviando
        *    lineas aunque no esté adquiriendo datos. Aunque a un ritmo más bajo.
        */
       int corte = 0;
       comSHM(SACQUIRE);
       while (NULL != fgets((char *) linea, 40, stream_in) &&   keepRunningDump) {
          // DEP_2(linea);
          /**** DECODE Arduino ****/
          switch (linea[0]) {
          case '#' :  /* identifica lineas con datos */
                s = linea+1;
                /* printf("%.9u %c (%+.7hd, %+.7hd, %+.7hd) (%+.7hd, %+.7hd, %+.7hd)\n",
                           aInt(s), s[8], aCorto(s+9), aCorto(s+13), aCorto(s+17),
                                                                  aCorto(s+21), aCorto(s+25), aCorto(s+29));
                */
                fprintf(arch, "%.9u:%c:%+14.7f:%+14.7f:%+14.7f:%+14.7f:%+14.7f:%+14.7f\n",
                           aInt(s), s[8],
                                  (float) (aCorto(s+9)/sensParm.normaA),
                                     (float) (aCorto(s+13)/sensParm.normaA),
                                     (float) (aCorto(s+17)/sensParm.normaA),
                                  (float) (aCorto(s+21)/sensParm.normaG),
                                     (float) (aCorto(s+25)/sensParm.normaG),
                                     (float) (aCorto(s+29)/sensParm.normaG)); fflush(arch);
           DEP_2(linea);
                break;
          case '|' : /* se recibe una señal de corte desde el arduino */
               if (0 == corte) {
                  LogPrint("Corte en la adquisición");
                  fprintf(arch, "# %s\n", linea);
                  fflush(arch);
                  corte = -1;
                  comSHM(SPAUSE);
               }
               break;
          case '@' : /* se recibe una señal de reanudacion desde el arduino */
                  LogPrint("Reanudacion en la adquisicion");
                  fprintf(arch, "# %s\n", linea);
                  fflush(arch);
                  corte = 0;
                  comSHM(SACQUIRE);
               break;
          case '-' : /* se recibe un mensaje vacío desde el arduino */
                 // DEP_1("(nil)");
               break;
          case 'P' : /* error de protocolo desde Arduino */
                  DEP_1("Error de protocolo desde arduino")
                  DEP_1(linea);
               break;
          default:
                fprintf(arch, "# %s", linea);
                fflush(arch);
          } /** end decode Arduino **/
          
          /**** DECODE WEB COMMAND ****/
          if (!strcmp(shm, DORESUME)) {
             if (!corte) { /* Arduino está adquiriendo. ¿!? */
               LogPrint("Petición de reanudación en marcha ¿!?");
               DEP_1("Petición de reanudación en marcha ¿!?");
               break; /* romper el bucle de lectura, sorry! */
             } else {
               fprintf(stream_out, "%c", 0x41 ); /* 'A' */
               fflush(stream_out);
               LogPrint("Petición de reanudación de adquisición");
               DEP_1("Petición de reanudación Arduino");
               comSHM(TRANSITO);
             }
          } else if (!strcmp(shm, DOPAUSE)) {
             if (!corte) { /* Arduino está adquiriendo. */
               fprintf(stream_out, "%c", 0x70 ); /* 'p' */
               fflush(stream_out);
               LogPrint("Petición de parada de adquisición");
               DEP_1("Petición de parada Arduino");
               comSHM(TRANSITO);
             } else {
               LogPrint("Petición de parada en parada ¿!?");
               DEP_1("Petición de parada en parada ¿!?");
               break; /* romper el bucle de lectura, sorry! */
             }
          } /** end decode Web **/
       }
       LogPrint("Fin de la adquisición");
       
       DEP_1("Fin de la adquisición");
       comSHM(SREADY);
       fprintf(arch, "# Fin de la adquisición\n");
       fflush(arch);
       fclose(arch);
       return 0;
}

/**************************************************************************************
 * goRead() Es invocado por el proceso web para indicar a Arduino que prosiga su lectura.
 * Debe ponerse en contacto con el proceso de lectura local (dataDump) por memoria
 * compartida, para que envíe un caracter de reanudación al arduino.
 * Posteriormente, debe seguir leyendo líneas del arduino como es normal.
 */
void goRead() {
  char * funName = "goRead";
  
  LogPrint("Indicando DoResume");
  DEP_1("Reanudación de la adquisición");
  comSHM(DORESUME);
  /* no espera por la reanudación, se supone que la interfaz tiene una orden de check() */
}

/**************************************************************************************
 * goWait() Es invocado por el proceso web para indicar a Arduino que detenga su lectura.
 * Debe ponerse en contacto con el proceso de lectura local (dataDump) por memoria
 * compartida, para que envíe un caracter de parada al arduino.
 * Posteriormente, debe seguir leyendo líneas del arduino como es normal.
 */
void goWait() {
  char * funName = "goWait";
  
  LogPrint("Indicando DoPause");
  DEP_1("Parada de la adquisición");
  comSHM(DOPAUSE);
  /* no espera por la reanudación, se supone que la interfaz tiene una orden de check() */
}


/**************************************************************************
 * main()
 *****************/

int main(int argc, char **argv) {
  char * funName = "main";

    /****************************
     * #Preamble.
     *      * Log preparations
     *      * Sensor declaration
     *      * getopt() decoding
     *      * shared memory initialization
     *      * web service initialization
     *      * sensParm struct initialization
     *      * dataFile struct initialization
     ****************************/
    /*********
     * Log preparations
     */
    if (!logInit()) {
         perror("Inicialización del log");
         exit_status = EXIT_FAIL_FOPEN; /* fallo en apertura de fichero */
         
         return exit_status;
    }
    
    /*********
     * Sensor declarations
     ********/
    extern struct SensParm sensParm;
          sensParm.modoAcc         = "0";    /*(+-2g)*/
          sensParm.modoGyr         = "0";    /*(+-250º/s)*/
          sensParm.sRate            = "100"; /*(100 muestras/s)*/
          sensParm.unidadesAcc   = 0; /* 0: g, 1: m/(s*s) */
          sensParm.unidadesGyr   = 0; /* 0: º/s, 1: rad/s, 2: Hz */
          sensParm.uAceleracion = "g";
          sensParm.uGiroscopo    = "º/s";

          /* para la sensibilidad del acelerometro de MPU6000
          *   por defecto en g */
          sensParm.normaA          = 1.0;

          /* para la sensibilidad del giroscopo de MPU6000
          *   por defecto en º/s */
          sensParm.normaG          = 1.0;

          /* para la frecuencia de muestreo del MP6000
          *   por defecto en Hz.
          */
          sensParm.Rate             = 100.0;

    opterr = 0;

    /*******************
     * getopt decoding
     *******************/
    int c;
    while ((c = getopt (argc, argv, "rdfgmF:A:G:")) != -1)
       switch (c) {
          /*   Variables de la adquisición de los datos */
          case 'A':
             sensParm.modoAcc = optarg; break;
          case 'G':
             sensParm.modoGyr = optarg; break;
          case 'F':
             sensParm.sRate = optarg; break ;
          /*   Unidades de medida de los datos adquiridos */
          case 'g':   /* g */
             sensParm.normaA = 1.0;            sensParm.uAceleracion = "g";       break;
          case 'm':   /* m/s^2 dado g */
             sensParm.normaA = 1.0/9.8;      sensParm.uAceleracion = "m/s^2"; break;
          case 'd':   /* Degrees/s - º/s */
             sensParm.normaG = 1.0;            sensParm.uGiroscopo    = "º/s";    break;
          case 'f':   /* Frequency-Hz   dado º/s */
             sensParm.normaG = 1.0/360.0;   sensParm.uGiroscopo    = "Hz";      break;
          case 'r':   /* Angle-rad/s dado º/s*/
             sensParm.normaG = M_PI/180.0; sensParm.uGiroscopo    = "rad/s"; break;
          case '?':
             if (optopt == 'c') {
                fprintf (stderr, "La opción -%c requiere 2g, 4g, 8g o 16g.\n", optopt);
             } else if (isprint (optopt)) {
                fprintf (stderr, "Opción desconocida `-%c'.\n", optopt);
                ayuda(argv[0]);
             } else {
                fprintf (stderr,
                              "Caracter de opción, desconocido`\\x%x'.\n",
                              optopt);
                ayuda(argv[0]);
             }
             exit_status = EXIT_FAIL_ARGMNT ; /* fallo en argumentos */
             return exit_status ;
          default:
             /* this must never be reached */
             abort ();
    }
                
    /*********
     * Shared memory to comm status of adquisition.
     ********/
    /* Antes de crear los pipes vamos a crear una zona de memoria compartida para
     * el webServerHandler y el proceso escritor del archivo de adquisición.
     * (http://www.cs.cf.ac.uk/Dave/C/node27.html)
     */
    int shmid;
    /* Nombre de nuestro segmento de memoria. "5678". */
    key_t key = 5678;
    /* Create the segment. */
    if ((shmid = shmget(key, SHMSZ, IPC_CREAT | 0666)) < 0) {
       perror("shmget");
       exit_status = EXIT_FAIL_SHMEM; /* fallo en memoria compartida */
       return exit_status;
    }

    /* Now we attach the segment to our data space. */
    if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
       perror("shmat");
       exit_status = EXIT_FAIL_SHMEM; /* fallo en memoria compartida */
       return exit_status;
    }
    comSHM(SIDLE);
      
    /********
     * Signal handler.
     *******/
     signal(SIGINT, intHandler);   /* Señal de interrupción lanzada desde un terminal, por ejemplo */
     signal(SIGUSR1, intHandler);  /* Señal lanzada desde el gestor del WebServer */
          
    /*********
     * Web service declarations
     ********/
    struct mg_server *server = mg_create_server(NULL, web_event_handler);
    mg_set_option(server, "document_root", ".");
    mg_set_option(server, "listening_port", "8080");
    
    
    /*********
     * sensParm struct initialization
     ********/

    /* Valores de normalización para el acelerómetro de InvenSense MPU6000,
    *   en función de la precisión del dato recibido    float fMuestreo = atof(sRate);
    *   en el programa del controlador.
    */
    switch (sensParm.modoAcc[0]) {
      case '0': sensParm.normaA *= 16384.0; break;
      case '1': sensParm.normaA *= 8192.0;   break;
      case '2': sensParm.normaA *= 4096.0;   break;
      case '3': sensParm.normaA *= 2048.0;   break;
      default:
         ayuda(argv[0]);
         exit_status = EXIT_FAIL_ARGMNT; /* fallo en argumentos */
         return exit_status;
    }
    
    /* Valores de normalización para el giróscopo de InvenSense MPU6000,
    *   en función de la precisión del dato recibido. Debe coincidir con el valor indicado
    *   en el programa del controlador.
    */
    switch (sensParm.modoGyr[0]) {
      case '0': sensParm.normaG *= 131.0; break;
      case '1': sensParm.normaG *= 65.5;   break;
      case '2': sensParm.normaG *= 32.8;   break;
      case '3': sensParm.normaG *= 16.4;   break;
      default:
         ayuda(argv[0]);
         exit_status = EXIT_FAIL_ARGMNT; /* fallo en argumentos */
         return exit_status;
    }


    /* Frecuencia de adquisición de datos para el giróscopo de InvenSense MPU6000.
    *   admite más, pero para nuestra configuración con hasta 6 sensores, en Arduino
    *   soporta hasta 100 muestras por segundo de cada sensor giróscopo y acelerómetro.
    *   en total son: 100 x 6 x 2 x 3 = 3600 enteros por segundo.
    *   
    *   de la pág. 12 del libro de Registros: la tasa de salida del acelerómetro es fija de 1kHz.
    *   La tasa de muestreo global viene entonces limitada por la tasa de salida del giróscopo.
    *   La tasa de salida del giróscopo es 8kHz con cierto filtro DLPF desabilitado, y
    *   1kHz con el filtro habilitado. En nuestro caso estará deshabilitado, con lo que
    *   la tasa de muestreo global se calcula con la siguiente fórmula:
    *       Tasa Muestreo = [Tasa de salida del giróscopo]/(1 + SMPLRT_DIV)
    *                            = 8000 / (1 + SMPLRT_DIV)
    *       SMPLRT_DIV = (8000 / Tasa_Muestreo ) - 1
    *                         = (Divisor de la tasa de muestreo).
    *   en función de la precisión del dato recibido. Debe coincidir con el valor indicado
    *   en el programa del controlador.
    */

    sensParm.fMuestreo = atof(sensParm.sRate);
    sensParm.iMuestreo = (unsigned int) ((8000.0 / sensParm.fMuestreo) - 1.0);
    
    if (sensParm.fMuestreo < 31.25 || sensParm.fMuestreo > 8000.0) {
       ayuda(argv[0]);
       exit_status = EXIT_FAIL_ARGMNT;
       /* fallocaratula(conn, "STATUS: OK: Proceso leyendo."); en argumentos */
       return exit_status;
    }

    /* Depuración para comprobar valores
    printf("ModoAcelerometro = %c, ModoGiroscopo = %c, iMuestreo = %u\n",
               modoAcc[0], modoGyr[0], iMuestreo);
    printf("%f, %f, %f\n", fMuestreo, normaA, normaG);
    */
    
    /*********
     * dataFile struct initialization, con nuestros valores por defecto.
     * Con ellos se inicializará cuando sea necesario el nombre del archivo con initFileName()
     ********/
    extern struct DataFile dataFile;
    strcpy(dataFile.nombrePath, "./");
    strcpy(dataFile.nombreBase, "archivo");
    strcpy(dataFile.extension,  ".datos");
    initFileName();
    
    /****************************
     * #Main Loop.
     *      Es en esencia un bucle de servicio web.
     ***************************/
    DEP_0("Entrando en bucle de servicio Web");
    DEP_0("estadoProceso=IDLE");
    for (;keepRunningWeb;) {
       mg_poll_server(server, 1000);   // Infinite loop, Ctrl-C to stop
    }
    mg_destroy_server(&server);
    
    /* hay que matar al poceso lector si es que estuviera activo !!*/
    if (pLector == 0) {
       /* y luego esperar por él */
       int proc_status;

       kill(pLector, SIGUSR1);
       while (wait(&proc_status) != -1) ; /* pick up dead children (proces lector) */
    }

    return exit_status;
}

/**************************************************************************************
 * lectorMain() lanza los procesos para lectura y escritura del dispositivo
 * (truculenti a tope)
 *****************/
int lectorMain() {
  char * funName = "lectorMain";
  
    /* parte común con los pipes */
    int to_cu_pipe[2];
    int from_cu_pipe[2];

    /** mandato del SO de tipo "connect unix" que es lo más sencillito para
      *   interactuar con el canal serie del Arduino a alta velocidad.
      *   ¡ojo con la velocidad del USB en el lado del Arduino!
      **/
    char *cmd[] = { "/usr/bin/cu", "-l", "/dev/ttyACM0", "-s", "230400", 0 };
    /* char *cmd[] = { "/bin/cat", 0 }; * perhaps some more easy to debug */

    /* Create the pipes.   */
    if (pipe (to_cu_pipe)) {
         fprintf (stderr, "Pipe to_cu_pipe failed.\n");
         exit_status = EXIT_FAIL_CANAL; /* fallo en dup2 o pipe */
         comSHM(SERRCANAL);
         return exit_status;
    }
    if (pipe (from_cu_pipe)) {
         fprintf (stderr, "Pipe from_cu_pipe failed.\n");
         exit_status = EXIT_FAIL_CANAL; /* fallo en dup2 o pipe */
         comSHM(SERRCANAL);
         return exit_status;
    }

    extern int exit_status;

    /* fprintf(stderr, "voy a hacer fork\n"); fflush(stderr); */
    /* Create the child process.   */
    switch (fork ()) {
    case 0:            /* CHILD: this is our CU/device !! */
       dup2(to_cu_pipe[0], 0) ;    /* stdin to reading from the parent, and... */
       dup2(from_cu_pipe[1], 1) ; /* stdout to write to the parent. */
       close(to_cu_pipe[1]);
       close(from_cu_pipe[0]);
       /* fprintf(stderr, "justo antes de execvp\n"); fflush(stderr); */
       /* ejecutar el CU */
       execvp(cmd[0], cmd);
       perror(cmd[0]); /* execvp failed */
                               /* las razones son peculiares de cu */
       exit_status = EXIT_FAIL_EXEC; /* fallo en el proceso de lectura cu */
       comSHM(SERREXEC);
       break;
    case -1: /* The fork failed.   */
       /* fprintf (stderr, "Fork failed.\nPor alguna razón\n"); */
       exit_status = EXIT_FAIL_FORK;
       comSHM(SERRFORK);
       break;
    default:          /* This is the parent process.   */
       {
          FILE *stream_out; /* stream to send conf-parameters to CU/device */
          stream_out = fdopen (to_cu_pipe[1], "w");
   
   
          FILE *stream_in; /* stream to read data from CU/device */
          stream_in = fdopen (from_cu_pipe[0], "r");
   
          close(to_cu_pipe[0]);
          close(from_cu_pipe[1]);
          /* fprintf(stderr, "Hacia CU desde el padre\n"); fflush(stderr); */
          /* give CU/device some time to wake-up? */
          /* (void) sleep(1); */
          DEP_1("Antes de dataDump");
          dataDump(stream_in, stream_out);   /********** DATA_DUMP()  ***********/
          
          DEP_1("Despues de dataDump");
          comSHM(SREADY); /* listo para la siguiente adquisición */
          /* matamos al proceso hijo elegantemente cerrando un pipe */
          fclose (stream_out);
   
          int proc_status;
   
          while (wait(&proc_status) != -1) ; /* pick up dead children */
             
          exit_status = EXIT_STATUS_OK; /* sin fallo */
          comSHM(SREADY); /* nuestra forma de indicar que no ha habido fallo */
          break;
       }
    }
    return exit_status;
}

/**************************************************************************************
 * aCorto() Pasa de hexadecimal codificado en ASCII a short.
 */
short aCorto(unsigned char *s) {
    short int i = 0x0000;
         i =                  hexa2binShort(s[0]);
         i = (i << 4) | hexa2binShort(s[1]);
         i = (i << 4) | hexa2binShort(s[2]);
         i = (i << 4) | hexa2binShort(s[3]);
    return i;
}

/**************************************************************************************
 * aInt() Pasa de hexadecimal codificado en ASCII a unsigned int.
 */
unsigned int aInt(unsigned char *s) {
    unsigned int t=0x00000000;
         t =                  hexa2binInt(s[0]);
         t = (t << 4) | hexa2binInt(s[1]);
         t = (t << 4) | hexa2binInt(s[2]);
         t = (t << 4) | hexa2binInt(s[3]);
         t = (t << 4) | hexa2binInt(s[4]);
         t = (t << 4) | hexa2binInt(s[5]);
         t = (t << 4) | hexa2binInt(s[6]);
         t = (t << 4) | hexa2binInt(s[7]);
    return t;
}

/**************************************************************************************
 * hexa2binShort() helper de aShort().
 */
unsigned int hexa2binShort(unsigned char c) {
    switch (c) {
       case '0': return 0x0000;
       case '1': return 0x0001;
       case '2': return 0x0002;
       case '3': return 0x0003;
       case '4': return 0x0004;
       case '5': return 0x0005;
       case '6': return 0x0006;
       case '7': return 0x0007;
       case '8': return 0x0008;
       case '9': return 0x0009;
       case 'A': return 0x000A;
       case 'B': return 0x000B;
       case 'C': return 0x000C;
       case 'D': return 0x000D;
       case 'E': return 0x000E;
       case 'F': /* se supone que solo debe haber estos valores */
       default : return 0x000F;
    }
}

/**************************************************************************************
 * hexa2binInt() helper de aInt().
 */
unsigned int hexa2binInt(unsigned char c) {
    switch (c) {
       case '0': return 0x00000000;
       case '1': return 0x00000001;
       case '2': return 0x00000002;
       case '3': return 0x00000003;
       case '4': return 0x00000004;
       case '5': return 0x00000005;
       case '6': return 0x00000006;
       case '7': return 0x00000007;
       case '8': return 0x00000008;
       case '9': return 0x00000009;
       case 'A': return 0x0000000A;
       case 'B': return 0x0000000B;
       case 'C': return 0x0000000C;
       case 'D': return 0x0000000D;
       case 'E': return 0x0000000E;
       case 'F': /* se supone que solo debe haber estos valores */
       default : return 0x0000000F;
    }
}

/**************************************************************************************
 * printFecha() Imprime la fecha según la Wikipedia,
 * lo hace an la posición actual sin retorno de carro
 * 
 * http://en.wikipedia.org/wiki/C_date_and_time_functions
 */
char * printFecha(char * buffer) {
  char * funName = "printFecha";
  
      time_t current_time;
      char* c_time_string;
 
      /* Obtain current time as seconds elapsed since the Epoch. */
      current_time = time(NULL);
 
      
      if (current_time == ((time_t)-1))
      {
            /* (void) fprintf(stderr, "Fallo en el cálculo del tiempo actual\n"); */
            strcpy(buffer, "**fallo en printFecha**");
      }
 
      /* Convert to local time format. */
      c_time_string = ctime(&current_time);
 
      if (c_time_string == NULL)
      {
            /* (void) fprintf(stderr, "Fallo en la conversión de tiempo a hora\n"); */
            strcpy(buffer, "**fallo en printFecha**");
      }
 
      /* Print to stdout. */
      strcpy(buffer, c_time_string);
      return buffer;
}

/**************************************************************************************
 * ayuda() Es una función que sirve de ayuda de las opciones del programa para lanzar
 * el servidor web.
 */
void ayuda(char *nombre_prog) {
  char * funName = "ayuda";
  
      fprintf(stderr, "Utilice:\n"
                              "%s [-A 0|1|2|3] [-G 0|1|2|3] [-F 31.25-8000][-m|-g] [-d|-r|-f]\n\n",
                  basename(nombre_prog));
      fprintf(stderr, "-A         Fondo de escala del acelerómetro: 0 (2g), 1 (4g),"
                                  "2 (8g), 3 (16g).\n"
                      "-G         Fondo de escala del giróscopo: 0 (250 º/s), 1 (500 º/s),\n"
                      "           2 (1000 º/s), 3 (2000 º/s).\n"
                      "-F         Frecuencia de muestreo interna del MPU (31.25 a 8000).\n"
                      "           Seguramente, distinta de la del dispositivo.\n"
                      "-m|-g      m/(s*s) o g (9.8 m/(s*s))\n"
                      "-d|-r|-f   [º/s] o [rad/s] o [Hz]\n");
}

