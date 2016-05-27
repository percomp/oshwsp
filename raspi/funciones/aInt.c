/*************************************************************************
 * aInt() Pasa de hexadecimal codificado en ASCII a unsigned int.
 */
unsigned int aInt(unsigned char *s) {
    unsigned int t=0x00000000;
         t =            hexa2binInt(s[0]);
         t = (t << 4) | hexa2binInt(s[1]);
         t = (t << 4) | hexa2binInt(s[2]);
         t = (t << 4) | hexa2binInt(s[3]);
         t = (t << 4) | hexa2binInt(s[4]);
         t = (t << 4) | hexa2binInt(s[5]);
         t = (t << 4) | hexa2binInt(s[6]);
         t = (t << 4) | hexa2binInt(s[7]);
    return t;
}
