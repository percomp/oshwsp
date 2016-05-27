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

