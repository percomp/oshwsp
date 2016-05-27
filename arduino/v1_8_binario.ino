 /*
 MPU 6000 - 6 axis IMU - Breakout from Drotek - v2
 
 Shows the output of Accelerometer and Gyroscope Sensors, using
 SPI library. For details on the sensor, see:

 MPU6000 de InvenSense
 breakout de DroTek
 
 This version writes binary data on Serial channel. This way we squeeze the throughput.
 
 Circuit:
 MPU000 sensor attached to pins:
 D13 : SCLK
 D12 : SDO
 D11 : SDI
 D10 : nada
 D07 : /CS para IMU_1 - tobillo - ankle
 D06 : /CS para IMU_2 - tobillo - ankle
 D05 : /CS para IMU_3 - mano    - wrist
 D04 : /CS para IMU_4 - cadera  - hip
 D03 : /CS para IMU_5 - rodilla - knee
 D02 : /CS para IMU_6 - rodilla - knee
 + 5V y GND
 
 Pin Boton : D8 + pulsador + 10KOhms
 Pin Led   : D9 (PWM) + led + 330 Ohms
 
 created october 2015
 by Cesar Llamas
 */

// the sensor communicates using SPI, so include the library:
#include <SPI.h>

#include "MPU6000.h"

// pins used for the connection with the sensor
// the other you need are controlled by the SPI library):
const byte R_ANKLE=7; // D07
const byte L_ANKLE=6; // D06
const byte R_KNEE =3; // D03
const byte L_KNEE =2; // D02
const byte HIP    =4; // D04
const byte HAND   =5; // D05
const int chipSelectPinMPU[7] = {
                                  R_ANKLE, L_ANKLE,
                                  R_KNEE,  L_KNEE,
                                  HIP, HAND,
                                }; // six sensors
                                  
const byte indiceSensor[7]    = { 'J', 'K',
                                  'L', 'M',
                                  'N', 'O'};
const byte NActiveSensors=6;

// Init values. Here the number of sensors is required.
// chipSelectPinMPU is related to writeRegister() and readRegister() that select
// the active chip through this variable
int actualMPU = 0; // MPU = 0 at init. It must be extern.

// constants won't change. They're used here to 
// set pin numbers:
const int buttonPin = 8;     // the number of the pushbutton pin
const int ledPin =  9;      // the number of the LED pin
char hexa[16]={'0', '1', '2', '3', '4', '5', '6', '7', '8',
               '9', 'A', 'B', 'C', 'D', 'E', 'F' };

// States the global module statues
int estado = LOW; /* LOW: stopped, HIGH: acquiring */

// variables will change:
int ledState = LOW;         // the current state of the output pin
int parpadeo = 0;

///// Struct holding binary output data
typedef struct {
     char          sensor;
     byte          accxh;
     byte          accxl;
     byte          accyh;
     byte          accyl;
     byte          acczh;
     byte          acczl;
     byte          gyrxh;
     byte          gyrxl;
     byte          gyryh;
     byte          gyryl;
     byte          gyrzh;
     byte          gyrzl;
} MiniTrama;

///// Binary output data with time.
struct MiniTiempo {
     unsigned long tiempo;
} ;

// Using an array of frames ensures that the Serial library doesnt
// interferes with writing new values coming from sensor readings
// and calls to micros().
// * el porque de mantener un array de tramas esta en
// * la conjuncion astral de los ISR del SPI, el Serial y micros().
// * sobre variables volatiles donde ademas hay una trama Serial
// *para el USB de un tamaño predefinido.
volatile struct MiniTiempo miniTiempo;

volatile byte miniTiempoOutput[2*sizeof(struct MiniTiempo)+2];
    // +2 por delimitadores ~0123$, *2 por maximo posible de escapes
int bytesTiempoOutput; // long final de la trama actual tiempo escapada

    // +2 por delimitadores #0123$, *2 por maximo posible de escapes
volatile byte bloquecito0[2*sizeof(MiniTrama)+2];
volatile byte bloquecito1[2*sizeof(MiniTrama)+2];
volatile byte bloquecito2[2*sizeof(MiniTrama)+2];
volatile byte bloquecito3[2*sizeof(MiniTrama)+2];
volatile byte bloquecito4[2*sizeof(MiniTrama)+2];
volatile byte bloquecito5[2*sizeof(MiniTrama)+2];
volatile byte *bloqueOutput[6] = {bloquecito0, bloquecito1, bloquecito2,
                                 bloquecito3, bloquecito4, bloquecito5};
MiniTrama  bloque[6];
int bytesTramaOutput;     // long final de la última trama escapada

/*******
 * Escape protocol to: 
 * - insert frame marks (start, end & time start)
 * - nullify the effects of xon-xoff byte sequence
 * in the internal handling of the serial connection.
 */
/*  mecanismo de escape para los delimitadores de tramas
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
*  Setup
********************************************************/
void setup() {
  int i;

  ledState = HIGH;         // the current state of the output pin

  // initialize the LED pin as an output:
  pinMode(ledPin, OUTPUT);
  pinMode(10, OUTPUT);      // SS para funcionar como esclavo SPI: desactivado
  analogWrite(ledPin, 150); // es una salida PWM y 150 de brillo esta bien
  digitalWrite(ledPin, ledState);  // encender el led al menos al principio.
  
  /* estado de la adquisicion en el arranque */
  estado = LOW;   

  
  //********************************** Interfaz Serie
  //Serial.begin(230400); /* Velocidad del canal serie USB del arduino (115200, 230400,...)*/
  Serial.begin(115200); /* Velocidad del canal serie USB del arduino (115200, 230400,...)*/
  
  int clockDivider = 100;  /* en Hz */
  // DEF_SMPRD_DIV = (byte) (0x00FF && clockDivider);
  int sampRate = (8000 / (clockDivider + 1));
  
  int accMode  = 1;  /* 0: +-2g, 1: +-4g, 2: +-8g, 3: +-16g */
  int gyrMode  = 2;  /* 0: 250º/s, 1: 500º/s, 2: 1000º/s, 3: 2000º/s */

  //********************************** Interfaz SPI

  // start the SPI library:
  SPI.begin();

  // Configuracion de la interfaz SP del Breakout de Drotek.
  //SPI.setDataMode(SPI_MODE0); // valor base de reloj H, transmision por flanco ascendente.
  //SPI.setClockDivider(SPI_CLOCK_DIV16);
  //SPI.setBitOrder(MSBFIRST);
  delay(100);

  for (i=0; i<NActiveSensors; i++) {
     pinMode(chipSelectPinMPU[i], OUTPUT);
  }
  
  // duermeMPUs();
  
  digitalWrite(ledPin, HIGH);  
  //*********************************** MPU
  // Miniinicializacion.
  for (actualMPU=0; actualMPU<NActiveSensors; actualMPU++) {
//    initMPU(chipSelectPinMPU[actualMPU]);
    initMPU(chipSelectPinMPU[actualMPU]);
  }
  
  delay(1000);
  
  Serial.begin(115200); /* Velocidad del canal serie USB del arduino (115200, 230400,...)*/

}

/********************************************************
*  Bucle principal
********************************************************/
void loop() {

  int luzon = 500;
  int actualMPU;
  
  short int xacc=0x0, yacc=0x0, zacc=0x0;
  byte xaccl, xacch;
  byte yaccl, yacch;
  byte zaccl, zacch;
  
  short int temp;
  byte templ, temph;

  short int xgir=0x0, ygir=0x0, zgir=0x0;
  byte xgirl, xgirh;
  byte ygirl, ygirh;
  byte zgirl, zgirh;

  if (luzon <= parpadeo) {
    ledState = !ledState;
    if (HIGH == estado) {
       luzon = 10;
       digitalWrite(ledPin, ledState);
    }else{
       luzon = 200;
       digitalWrite(ledPin, LOW);
    }
    parpadeo = 0;
  } else {
    parpadeo++;
  }
  
  miniTiempo.tiempo = micros();
  escaparMiniTiempo();
  Serial.write((byte *) miniTiempoOutput, bytesTiempoOutput);
  for (actualMPU=0; actualMPU<6; actualMPU++) {
    bloque[actualMPU].sensor = indiceSensor[actualMPU];
  
    SPI.beginTransaction(settingsHIGH);
     digitalWrite(chipSelectPinMPU[actualMPU], LOW);
      SPI.transfer(ACCEL_XOUTH | 0x80); // see "readRegister()"
        bloque[actualMPU].accxh = SPI.transfer(0x00);
        bloque[actualMPU].accxl = SPI.transfer(0x00);
        bloque[actualMPU].accyh = SPI.transfer(0x00);
        bloque[actualMPU].accyl = SPI.transfer(0x00);
        bloque[actualMPU].acczh = SPI.transfer(0x00);
        bloque[actualMPU].acczl = SPI.transfer(0x00);

        temph = SPI.transfer(0x00);
        templ = SPI.transfer(0x00);

        bloque[actualMPU].gyrxh = SPI.transfer(0x00);
        bloque[actualMPU].gyrxl = SPI.transfer(0x00);
        bloque[actualMPU].gyryh = SPI.transfer(0x00);
        bloque[actualMPU].gyryl = SPI.transfer(0x00);
        bloque[actualMPU].gyrzh = SPI.transfer(0x00);
        bloque[actualMPU].gyrzl = SPI.transfer(0x00);
     digitalWrite(chipSelectPinMPU[actualMPU], HIGH); // cortar SPI
    SPI.endTransaction();  // take the chip select high to de-select:


    escaparMiniTrama(actualMPU);
    Serial.write((byte *) bloqueOutput[actualMPU], bytesTramaOutput);
  }
}
/********************************************************
*  Copia Bucle principal
********************************************************/
void bloop() {

  int luzon = 500;
  int actualMPU;
  
  short int xacc=0x0, yacc=0x0, zacc=0x0;
  byte xaccl, xacch;
  byte yaccl, yacch;
  byte zaccl, zacch;
  
  short int temp;
  byte templ, temph;

  short int xgir=0x0, ygir=0x0, zgir=0x0;
  byte xgirl, xgirh;
  byte ygirl, ygirh;
  byte zgirl, zgirh;

  if (luzon <= parpadeo) {
    ledState = !ledState;
    if (HIGH == estado) {
       luzon = 10;
       digitalWrite(ledPin, ledState);
    }else{
       luzon = 200;
       digitalWrite(ledPin, LOW);
    }
    parpadeo = 0;
  } else {
    parpadeo++;
  }
  
  miniTiempo.tiempo = micros();
  escaparMiniTiempo();
  Serial.write((byte *) miniTiempoOutput, bytesTiempoOutput);
  for (actualMPU=0; actualMPU<6; actualMPU++) {
    bloque[actualMPU].sensor = indiceSensor[actualMPU];
  
        bloque[actualMPU].accxh = leeRegistro(actualMPU, ACCEL_XOUTH);
        bloque[actualMPU].accxl = leeRegistro(actualMPU, ACCEL_XOUTL);
        bloque[actualMPU].accyh = leeRegistro(actualMPU, ACCEL_YOUTH);
        bloque[actualMPU].accyl = leeRegistro(actualMPU, ACCEL_YOUTL);
        bloque[actualMPU].acczh = leeRegistro(actualMPU, ACCEL_ZOUTH);
        bloque[actualMPU].acczl = leeRegistro(actualMPU, ACCEL_ZOUTL);

        bloque[actualMPU].gyrxh = leeRegistro(actualMPU, GYRO_XOUTH);
        bloque[actualMPU].gyrxl = leeRegistro(actualMPU, GYRO_XOUTL);
        bloque[actualMPU].gyryh = leeRegistro(actualMPU, GYRO_YOUTH);
        bloque[actualMPU].gyryl = leeRegistro(actualMPU, GYRO_YOUTL);
        bloque[actualMPU].gyrzh = leeRegistro(actualMPU, GYRO_ZOUTH);
        bloque[actualMPU].gyrzl = leeRegistro(actualMPU, GYRO_ZOUTL);

    escaparMiniTrama(actualMPU);
    Serial.write((byte *) bloqueOutput[actualMPU], bytesTramaOutput);
  }
}

