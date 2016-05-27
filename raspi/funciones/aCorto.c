/*************************************************************************
 * aCorto() Pasa de hexadecimal codificado en ASCII a short.
 */
short aCorto(unsigned char *s) {
    short int i = 0x0000;
         i =            hexa2binShort(s[0]);
         i = (i << 4) | hexa2binShort(s[1]);
         i = (i << 4) | hexa2binShort(s[2]);
         i = (i << 4) | hexa2binShort(s[3]);
    return i;
}
