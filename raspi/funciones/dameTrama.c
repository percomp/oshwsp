/*
* lógica de funcionamiento:
*/
#define CCOM ((unsigned char) '#')
#define FCOM ((unsigned char) '~')

unsigned char * dameTrama(FILE *entrada, // stream de lectura
                  unsigned char *buffer, // deposito de trama
                const char *sensores) {  // nombres de sensores

     unsigned char *indice;    // recorre buffer cargando byte

     unsigned char c;          // para fgetc()
     unsigned char comprobacion; // para la suma de comprobacion
     unsigned char calculo;      // para sumar
     int i;

     printf("a "); fflush(stdout);
     // desechar primeros bytes hasta CCOM
Cabeza:
     do {
        c = (unsigned char) fgetc(entrada) {
        if (feof(entrada)) {
           return 0x00;
        }
     } while (c != CCOM) ;

     // Inicia la lectura de la trama
     indice = buffer;
    
     // lee el código de final de trama
     c = (unsigned char) fgetc(entrada) {
     *(indice++) = c;
     if (eof(entrada)) {
        return 0x00;
     }
     // condiciones de cumplimiento de trama
     if ( c == FCOM &&
          calculo == comprobacion &&
          (char *) 0x00 != strchr(sensores, buffer[5])) {
        return buffer;

     // la trama no está, y hay que tratar de compactar
     } else {
        // primero buscando el principio de otra trama
        for (indice = buffer+1; indice < buffer+20; indice ++) {
            if (*indice == CCOM)
               break;
        }
        // nos hemos salido del buffer,
        // eliminar la cabeza del archivo y volver a empezar.
     *indice = c;

      indice++;

     calculo = (unsigned char) 0x00;
     i = 1; // porque copiamos de 1 ... 17 calculando suma

     printf("b "); fflush(stdout);
     // Cargar bytes de los sensores
Carga:
     for ( ; i<18; i++) {
        c = (unsigned char) fgetc(entrada)
        if (feof(entrada)) {
           return 0x00;
        }
        calculo ^= (*(indice++) = c);
     }
     // lee el código de redundancia
     comprobacion = (unsigned char) fgetc(entrada) {
     *(indice++) = comprobacion;
     if (eof(entrada)) {
        return 0x00;
     }

     printf("c "); fflush(stdout);
     // lee el código de final de trama FCOM
Cola:
     c = (unsigned char) fgetc(entrada) {
     *(indice++) = c;
     if (eof(entrada)) {
        return 0x00;
     }
     if (c== FCOM){ printf("?");fflush(stdout);}
     if (calculo== comprobacion){ printf("x");fflush(stdout);}
     if (NULL != strchr(sensores, buffer[5])) {printf("x");fflush(stdout);}

     // condiciones de cumplimiento de trama
     if ( c == FCOM &&
          calculo == comprobacion &&
          (char *) 0x00 != strchr(sensores, buffer[5])) {
        return buffer;

     // la trama no está, y hay que tratar de compactar
     } else {
        // primero buscando el principio de otra trama
        for (indice = buffer+1; indice < buffer+20; indice ++) {
            if (*indice == CCOM)
               break;
        }
        // nos hemos salido del buffer,
        // eliminar la cabeza del archivo y volver a empezar.
        if (indice == buffer+20) {
           goto Cabeza;

        // hay que copiar desde donde hemos encontrado hasta
        // el último elemento que hemos transferido desde FILE
        } else {
           unsigned char* indice2 = indice;

           indice = buffer;
           calculo = (unsigned char) 0x00;
           i = 1; // porque copiamos de 1 ... 17 calculando suma
          *indice++ = CCOM; // primer elemento de buffer

           // si tenemos mala suerte y la CCOM ('#') es buffer[1],
           // no podemos entrar directamente en Carga:, tenemos
           // que ir a Cola: copiando y calculando la suma lógica.
           // de los 17 elementos que ya tenemos
           if (indice2 == buffer+1) {
              for ( ; i<18; i++, indice++)
                 calculo ^= (*(indice) = *(indice+1));
              goto Cola; // solo tenemos que leer un byte de FILE

           // pero si tenemos buena suerte, vamos copiando y
           // calculando la suma lógica hasta donde toque y
           // enganchamos con Carga: con cuidado de que indice e i
           // estén bien sincronizados.
           // en el peor de los casos el CCOM ('#') es buffer[2] con lo
           // que sólo nos resta leer la suma lógica de FILE.
           } else {
              for ( ; i<18 && indice < indice2; i++, indice++)
                 calculo ^= (*(indice) = *(indice+1));
              goto Carga;
           }
        }
     }
}
