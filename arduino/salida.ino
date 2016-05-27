/*******
*  mecanismo de escape para los delimitadores de tramas
*  y los caracteres especiales.
*
*  inicio de la trama de tiempo 0x7E ('~')
*  inicio de la trama de datos  0x23 ('#')
*  final de las tramas          0x24 ('$')
*  Caracter de escape: 7D
*  traducción arduino-side a canal
*  0x7D -> 0x7D 0x00 (por ser caracter de escape)
*  0x7E -> 0x7D 0x01 (por coincidir con inicio tiempo)
*  0x23 -> 0x7D 0x02 (por coincidir con inicio datos)
*  0x24 -> 0x7D 0x03 (por coincidir con final tramas)
*  0x11 -> 0x7D 0x04 (por lo normal es, soft XON)
*  0x13 -> 0x7D 0x05 (por lo normal es, soft XOFF)
*/

/********************************************************
*  Escapar la secuencia de bytes de miniTiempo
*     lo hago todo externo porque son arrays volatile
*     y espero que vaya superrapido!!
********************************************************/
void escaparMiniTiempo() {
  // tamaño minimo de la trama de tiempo
  extern int bytesTiempoOutput;
  bytesTiempoOutput=2+sizeof(struct MiniTiempo);

  byte *i = (byte *) &miniTiempo;
  byte *o = (byte *) miniTiempoOutput;
  
  *o++ = 0x7E;  // inicio trama tiempo

  for (int j=0; j<sizeof(struct MiniTiempo); j++, i++) {
    switch (*i) {
      case 0x7D: *o++ = 0x7D; *o++ = 0x00; bytesTiempoOutput++; break;
    // suponemos que no hay perdidas ni inserciones
    //case 0x7E:  es otro inicio de trama y solo esperamos el final
    //case 0x23:  es inicio trama de datos y solo esperamos el final
      case 0x24: *o++ = 0x7D; *o++ = 0x03; bytesTiempoOutput++; break;
      case 0x11: *o++ = 0x7D; *o++ = 0x04; bytesTiempoOutput++; break;
      case 0x13: *o++ = 0x7D; *o++ = 0x05; bytesTiempoOutput++; break;
      default  : *o++ = *i;
    }
  }

  *o = 0x24;    // fin trama tiempo/datos
}
/********************************************************
*  Escapar la secuencia de bytes de bloque[isensor]
*     lo hago todo externo porque son arrays volatile
*     y espero que vaya superrapido!!
********************************************************/
void escaparMiniTrama(int isensor) {
  // tamaño minimo de la trama de datos
  extern int bytesTramaOutput;
  bytesTramaOutput=2+sizeof(MiniTrama);

  byte *i = (byte *) &(bloque[isensor]);
  byte *o = (byte *) bloqueOutput[isensor];
  
  *o++ = 0x23;  // inicio trama sensores

  for (int j=0; j<sizeof(MiniTrama); j++, i++) {
    switch (*i) {
      case 0x7D: *o++ = 0x7D; *o++ = 0x00; bytesTramaOutput++; break;
    // suponemos que no hay perdidas ni inserciones
    //case 0x7E:  es otro inicio de trama y solo esperamos el final
    //case 0x23:  es inicio trama de datos y solo esperamos el final
      case 0x24: *o++ = 0x7D; *o++ = 0x03; bytesTramaOutput++; break;
      case 0x11: *o++ = 0x7D; *o++ = 0x04; bytesTramaOutput++; break;
      case 0x13: *o++ = 0x7D; *o++ = 0x05; bytesTramaOutput++; break;
      default  : *o++ = *i;
    }
  }

  *o = 0x24;    // fin trama tiempo/datos
}

