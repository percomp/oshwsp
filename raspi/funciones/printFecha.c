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
 
   if (current_time == ((time_t)-1)) {
      /* (void) fprintf(stderr, "Fallo en el cálculo del tiempo actual\n"); */
      strcpy(buffer, "**fallo en printFecha**");
   }
 
   /* Convert to local time format. */
   c_time_string = ctime(&current_time);
 
   if (c_time_string == NULL) {
      /* (void) fprintf(stderr, "Fallo en la conversión de tiempo a hora\n"); */
      strcpy(buffer, "**fallo en printFecha**");
   }
 
   /* Print to stdout. */
   strcpy(buffer, c_time_string);
   return buffer;
}
