

/********************************************************
*  Leer un registro de un MPU
********************************************************/

byte leeRegistro(int iMPU, byte thisRegister) {
  return readRegister(chipSelectPinMPU[iMPU], thisRegister);
}

/********************************************************
*  Enviar un mandato a un MPU
********************************************************/
void escribeRegistro(int iMPU, byte thisRegister, byte thisValue) {
  writeRegister(chipSelectPinMPU[iMPU], thisRegister, thisValue);
}


/********************************************************
*  Resetear un MPU
********************************************************/
// Resetea un MPU6000 segun p. 41 del Manual de Registros, para el caso SPI
// demora 200ms
void reseteaMPU(int iMPU) {
  resetMPU(chipSelectPinMPU[iMPU]);
}

/******************************************************
*  Duerme un sensor
******************************************************/
void duermeMPU(int iMPU) {
  sleepMPU(chipSelectPinMPU[iMPU]);
}

/******************************************************
*  Duerme todos los sensores
******************************************************/
void duermeMPUs() {
  for (int i=0; i<NActiveSensors; i++) {
    sleepMPU(chipSelectPinMPU[i]);
  }
}

/******************************************************
*  Despierta sensor
******************************************************/
void despiertaMPU(int iMPU) {
  // con temp.disabled y GyroZ como reloj.
  wakeupMPU(chipSelectPinMPU[iMPU]);
}

/******************************************************
*  Despierta sensores
******************************************************/
void despiertaMPUs() {
  // con temp.disabled y GyroZ como reloj.
  for (int i=0; i<NActiveSensors; i++) {
    wakeupMPU(chipSelectPinMPU[i]);
  }
}



/*******************************************************
* Arranca un sensor.
*******************************************************/
int startSensor(int i) {

   // Tercero, saca del modo Sleep en que est el sistema despues de un reseteo.
   // Pide la velocidad de uno de los giroscopos segun recomienda el manual
      
   byte intentos = 5;
   byte res;
   Serial.print(i);
   while (--intentos != 0) {
      writeRegister(i, PWR_MGMT_1, PWR_MGMT_INIT);
      delay(100);
      
      writeRegister(i, SIGNAL_PATH_RESET, DEF_SIGNAL_PATH_RESET);
      delay(100);
      

      // Wake up device and select GyroZ clock. Note that the
      // MPU6000 starts up in sleep mode, and it can take some time
      // for it to come out of sleep
      writeRegister(i, PWR_MGMT_1, PWR_MGMT_CLK_SEL_PPLGYROZ);
      delay(100);
      
      writeRegister(i, USER_CTRL, DEF_USER_CTRL_I2C_IF_DIS);
      delay(100);

      if ((res=readRegister(i, PWR_MGMT_1)) == PWR_MGMT_CLK_SEL_PPLGYROZ) {
         Serial.print(res, HEX);Serial.print("-");
         Serial.println("bien");
         return(1);
         //break;
      }
      Serial.print(res, HEX);Serial.print("--");
      delay(2);
    }
    Serial.println("MAL");
    return(0);
}


