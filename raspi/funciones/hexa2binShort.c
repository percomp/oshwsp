/*************************************************************************
 * hexa2binShort() helper de aShort().
 */
unsigned int hexa2binShort(unsigned char c) {
    switch (c) {
       case '0': return 0x0000;
       case '1': return 0x0001;
       case '2': return 0x0002;
       case '3': return 0x0003;
       case '4': return 0x0004;
       case '5': return 0x0005;
       case '6': return 0x0006;
       case '7': return 0x0007;
       case '8': return 0x0008;
       case '9': return 0x0009;
       case 'A': return 0x000A;
       case 'B': return 0x000B;
       case 'C': return 0x000C;
       case 'D': return 0x000D;
       case 'E': return 0x000E;
       case 'F': /* se supone que solo debe haber estos valores */
       default : return 0x000F;
    }
}
