/*******************************************************************************
 * dataDump() es la parte principal del bucle que atiende la lectura del
 *            stream del cu y lo vuelca a archivo.
 */
int dataDump(FILE * stream_in, FILE * stream_out, const char * nombre) {
  char * funName = "dataDump";

      /* extern struct DataFile dataFile; */

      extern struct SensParm sensParm;
                    /* extern es tan feo como forzarlo como argumento XD */
      unsigned char linea[40];
      unsigned char *s;
      int c;

      /* conseguir un apuntador a archivo adecuado, en primera instancia.
       * Sobre dataFile.nombre.
       */
      DEP_1(nombre);
      FILE *arch;
      if (NULL == (arch = fopen(nombre, "w"))) {
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
          fputc (c, stderr); fflush(stderr);
         ;
       }
       fprintf(stream_out, "%c", 0x52 ); /* 'R' */ fflush(stream_out);
       /* fprintf (stderr, "%c", c); fflush(stderr); */


       /******************

       DEP_1("Enviando parámetros al sensor");
       fprintf(stderr, "Sending parameters %c, %c, %u\n",
               sensParm.modoAcc[0], sensParm.modoGyr[0], sensParm.iMuestreo);
       fflush(stderr);
       fprintf(stream_out, "%c, %c, %u", sensParm.modoAcc[0],
                                         sensParm.modoGyr[0],
                                         sensParm.iMuestreo);
       fflush(stream_out);   // this line is extremely importante, te lo juro */
       /* printf("Writing...\n"); fflush(stdout); */

       // DEP_1("Esperando datos @");
       /*
       while ((c = fgetc (stream_in)) != '@') {
          // fputc (c, stderr); fflush(stderr);
         ;
       }
       */
       /* fprintf (stderr, "%c\n", c); fflush(stderr); */

       DEP_1("Enviando cabecera al archivo de datos");
       /********
         * Cabecera de datos para LiveGraph. Esto lo sacaré algún día de aquí.
         *******/
       fprintf(arch, "##:##\n");
       fprintf(arch,
            "@ParametrosAdquisicion modoAcc %c, modoGyr %c, fMuestreo %f\n",
             sensParm.modoAcc[0], sensParm.modoGyr[0], sensParm.fMuestreo);
       fprintf(arch, "@FechaAdquisicion %s", printFecha(linea));
       fprintf(arch, "\n@Tiempo (ms):Sensor: xAcc %s: yAcc %s: zAcc %s:"
                     " xGir %s: yGir %s: zGir %s\n",
                      sensParm.uAceleracion, sensParm.uAceleracion,
                      sensParm.uAceleracion, sensParm.uGiroscopo,
                      sensParm.uGiroscopo, sensParm.uGiroscopo);
       /* fprintf(arch, "# Parte: %s\n", dataFile.nombreParte); */
       fprintf(arch, "# Parte: %s\n", nombre);
       /* incFilePart(); /* preparado para la siguiente adquisición */
       fprintf(arch, "# Comienzo de la adquisición\n");
       fflush(arch);

       LogPrint("Comienzo de la adquisición");

       DEP_1("Comienza la adquisición");
       /* fprintf(stderr, "Bucle de lectura desde CU\n"); fflush(stderr); */
  
       /* El bucle de lectura del proceso todo lo rápido que puede.
        * Arduino comprueba que haya datos disponibles, y cuando los hay son
        * una letra con una orden: A (adquirir) y p (pausar).
        * Cuando Arduino tiene a bien parar o adquirir, envía una línea con la
        * indicación de su acción, pues la parte crítica es que Arduino decida
        * y así vaya todo lo rápido que pueda.
        */

       /******************* BUCLE **********************************************
        * Este bucle siempre funciona a las órdenes de arduino, es decir:
        *  - el estado inicial de arduino es enviar datos.
        *  - arduino puede parar o reanudar mediante el boton hardware.
        *  - PC puede parar al arduino enviandole una señal de parada, pero
        *    el estado pasa a corte cuando arduino se lo dice.
        *  - PC puede reanudar al arduino enviandole una señal de lectura,
        *    pero el estado pasa a lectura cuando lo reconoce el arduino.
        *  - El quiz de la cuestión es que este bucle no se bloquea pues el
        *    arduino sigue enviando lineas aunque no esté adquiriendo datos.
        *    Aunque a un ritmo más bajo.
        */
       int corte = 0;
       comSHM(SACQUIRE);
       //        fprintf(stream_out, "%c", 0x41 ); /* 'A' */
       //        fflush(stream_out);
       //        fprintf(stream_out, "%c", 0x41 ); /* 'A' */
       //        fflush(stream_out);
       //        LogPrint("Petición de reanudación de adquisición");
       keepRunningDump = -1;
       printf("voy a por ello chaval!"); fflush(stdout); 
       printf("%s", fgets((char *) linea, 40, stream_in));
       printf("%s", fgets((char *) linea, 40, stream_in));
       printf("%s", fgets((char *) linea, 40, stream_in));
       while (NULL != fgets((char *) linea, 40, stream_in) && keepRunningDump) {
          DEP_2(linea);
          putchar('.'); fflush(stdout);

          if (! corte) {
                s = linea+1;
                fprintf(arch,
                   "%.9u:%c:%+14.7f:%+14.7f:%+14.7f:%+14.7f:%+14.7f:%+14.7f\n",
                   aInt(s), s[8], (float) (aCorto(s+9)/sensParm.normaA),
                                     (float) (aCorto(s+13)/sensParm.normaA),
                                     (float) (aCorto(s+17)/sensParm.normaA),
                                  (float) (aCorto(s+21)/sensParm.normaG),
                                     (float) (aCorto(s+25)/sensParm.normaG),
                                     (float) (aCorto(s+29)/sensParm.normaG));
                fflush(arch);
          }

          /**** DECODE WEB COMMAND ****/
          if (!strcmp(shm, DORESUME)) {
             if (corte) { /* Arduino no está adquiriendo. */
               corte = 0;
               LogPrint("Petición de reanudación de adquisición");
               DEP_1("Petición de reanudación Arduino");
               comSHM(TRANSITO);
             }
          } else if (!strcmp(shm, DOPAUSE)) {
             if (!corte) { /* Arduino está adquiriendo. */
               corte = 1;
               LogPrint("Petición de parada de adquisición");
               DEP_1("Petición de parada Arduino");
               comSHM(TRANSITO);
             } 
          } else if (!strcmp(shm, DOSTOP)) {
             keepRunningDump = 0;
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
