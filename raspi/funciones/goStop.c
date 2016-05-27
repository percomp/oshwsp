/**************************************************************************************
 * goStop() para el bucle de lectura de dataDump-supersoft.
 */
void goStop() {
  char * funName = "goStop";

  LogPrint("Indicando DoStop");
  DEP_1("Detención de la adquisición");
  comSHM(DOSTOP);
}

