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

