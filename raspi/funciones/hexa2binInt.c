/************************************************************************
 * hexa2binInt() helper de aInt().
 */
unsigned int hexa2binInt(unsigned char c) {
    switch (c) {
       case '0': return 0x00000000;
       case '1': return 0x00000001;
       case '2': return 0x00000002;
       case '3': return 0x00000003;
       case '4': return 0x00000004;
       case '5': return 0x00000005;
       case '6': return 0x00000006;
       case '7': return 0x00000007;
       case '8': return 0x00000008;
       case '9': return 0x00000009;
       case 'A': return 0x0000000A;
       case 'B': return 0x0000000B;
       case 'C': return 0x0000000C;
       case 'D': return 0x0000000D;
       case 'E': return 0x0000000E;
       case 'F': /* se supone que solo debe haber estos valores */
       default : return 0x0000000F;
    }
}
