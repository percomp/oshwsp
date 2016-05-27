/*######################
 * Descriptor de archivo del log de la aplicación. El nombre del archivo log
 * es FLOGNAME
 * Ver logInit, LogPrint (que es un macro) y logClose.
 * Toda función donde se haga LogPrint debe haber definido char *logHandle
 */
#define FLOGNAME "./ii3.log"
FILE *logHandle=NULL;
#define LogPrint(x) logPrint(funName, (x))

/******************************************************************************
 * logInit() Crea el archivo de log y lo deja abierto para que cualquier
 *           método envíe un mensaje al log.
 *           Si el archivo existe lo pisa con nuevos contenidos.
 */
int logInit() {
  extern FILE * logHandle;

  return (logHandle = fopen(FLOGNAME, "a")) ? -1 : 0;
}

/******************************************************************************
 * logClose() Cierra el archivo de log.
 */
int logClose() {
  extern FILE * logHandle;

  return fclose(logHandle);
}

/******************************************************************************
 * logPrint() Imprime mediante LogPrint, la fecha el nombre de la función
 *            y el mensaje.
 */
void logPrint(char *funcion, char *mensaje) {
  char linea[40];
  extern FILE * logHandle;

  (void) fprintf(logHandle, "%s:%s:%s\n",
                    (char *) printFecha(linea), funcion, mensaje);
  (void) fflush(logHandle);
}
