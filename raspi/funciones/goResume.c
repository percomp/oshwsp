/**************************************************************************************
 * goResume() reanuda la lectura en el interior del bucle de lectura de dataDump-supersoft.
 */
void goResume() {
  char * funName = "goResume";

  LogPrint("Indicando DoResume");
  DEP_1("Reanudando la adquisición");
  comSHM(DORESUME);
}

