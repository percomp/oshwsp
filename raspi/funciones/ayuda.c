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
