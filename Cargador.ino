#include <RTClib.h>
#include <TimerOne.h>
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>//Se incluye la librería EEPROM

LiquidCrystal_I2C lcd( 0x3f, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE ); //Algunas pantallas llevan por defecto la dirección 27 y otras la 3F

//              DEFINICIÓN PINES ENTRADAS ANALOGICAS
int pinGeneracionFV = 0;      // define el pin 0 como 'Generacion FV'
int pinConsumoGeneral = 1;    // define el pin 1 como 'Consumo General'
int pinTensionCargador = 2;   // define el pin 2 como 'Tension'
int pinConsumoCargador = 3;   // define el pin 3 como 'Consumo Cargador'

//              DEFINICIÓN PINES ENTRADAS DIGITALES
int pinPulsadorInicio = 2;
int pinPulsadorMenos = 3;
int pinPulsadorMas = 4;
int pinPulsadorProg = 5;
int pinAlimentacionCargador = 6;    //Salida que activa el contactor que alimenta el cargador
int pinRegulacionCargador = 9;      //Salida que activa la onda cuadrada

//              DEFINICION TIPOS CARGA
const int TARIFAVALLE = 0;
const int FRANJAHORARIA = 1;
const int INMEDIATA = 2;
const int POTENCIA = 3;
const int FRANJATIEMPO = 4;
const int EXCEDENTESFV = 5;
const int INTELIGENTE = 6;

const int BOTONINICIO = 0;
const int BOTONMAS = 1;
const int BOTONMENOS = 2;
const int BOTONPROG = 3;

//              DEFINICION VARIABLES GLOBALES
int horaInicioCarga, minutoInicioCarga, intensidadProgramada, consumoTotalMax, horaFinCarga, minutoFinCarga, generacionMinima, tipoCarga, tipoCargaInteligente, valorTipoCarga, tempValorInt;
bool cargadorEnConsumoGeneral, conSensorGeneral, conFV, inicioCargaActivado, conTarifaValle, tempValorBool;
unsigned long kwTotales, watiosCargados;
int duracionPulso, tensionCargador, acumTensionCargador = 0, numTensionAcum = 0, numCiclos = 0, nuevoAnno;
bool permisoCarga, conectado, conectadoPrev, cargando, cargaCompleta, generacionSuficiente, luzLcd, horarioVerano;
int consumoCargador, generacionFV, consumoGeneral, picoConsumoCargador, picoGeneracionFV, picoConsumoGeneral;
int consumoCargadorAmperios, generacionFVAmperios, consumoGeneralAmperios;
unsigned long tiempoInicioSesion, tiempoCalculoPotenciaCargada, tiempoGeneraSuficiente, tiempoNoGeneraSuficiente, tiempoUltimaPulsacionBoton;
byte lastCheckHour = 0, enPantallaNumero, opcionNumero, nuevaHora, nuevoMinuto, nuevoMes, nuevoDia;
DateTime timeNow;


const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

// VARIABLES PARA EL RELOJ RTC ---------------
RTC_DS3231 rtc;

// Define various ADC prescaler--------------
const unsigned char PS_16 = (1 << ADPS2);
const unsigned char PS_32 = (1 << ADPS2) | (1 << ADPS0);
const unsigned char PS_64 = (1 << ADPS2) | (1 << ADPS1);
const unsigned char PS_128 = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

void setup() {
  //    ---------Se establece el valor del prescaler----------------
  ADCSRA &= ~PS_128;  // remove bits set by Arduino library
  // you can choose a prescaler from above. PS_16, PS_32, PS_64 or PS_128
  ADCSRA |= PS_32;    // set our own prescaler to 32

// DEFINICIÓN DE LOS PINES COMO ENTRADA O SALIDA
  pinMode(pinRegulacionCargador, OUTPUT);
  pinMode(pinAlimentacionCargador, OUTPUT);
  pinMode(pinPulsadorProg, INPUT);
  pinMode(pinPulsadorMas, INPUT);
  pinMode(pinPulsadorMenos, INPUT);
  pinMode(pinPulsadorInicio, INPUT);

  //    ---------   Se recuperan los ultimos valores de las variables guardadas en la eeprom----------------
  horaInicioCarga = EEPROM.read(0);
  minutoInicioCarga = EEPROM.read(1);
  intensidadProgramada = EEPROM.read(2);
  consumoTotalMax = EEPROM.read(3);
  horaFinCarga = EEPROM.read(4);
  minutoFinCarga = EEPROM.read(5);
  cargadorEnConsumoGeneral = EEPROM.read(6);
  conSensorGeneral = EEPROM.read(7);
  generacionMinima = EEPROM.read(8);
  conFV = EEPROM.read(9);
  conTarifaValle = EEPROM.read(10);
  tipoCarga = EEPROM.read(11);
  valorTipoCarga = EEPROM.read(12);
  inicioCargaActivado = EEPROM.read(13);
  kwTotales = EEPROMReadlong(14); // Este dato ocuparia 4 Bytes, por lo que no se pueden usar las direcciones 15, 16 y 17.

  lcd.begin(16, 2);
  lcd.backlight();
  luzLcd = true;
  
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println(F("SIN CONEX. RELOJ"));
    while (1);
  }
  
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  if (horaInicioCarga > 24) {    //Si es la primera vez que se ejecuta el programa, la lectura de la Eeprom da un valor alto, así que se asignan valores normales
    horaInicioCarga = 0;
    EEPROM.write(0, horaInicioCarga);
    cargadorEnConsumoGeneral = false;
    EEPROM.write(6, cargadorEnConsumoGeneral); 
    conSensorGeneral = true;
    EEPROM.write(7, conSensorGeneral);
    conFV = false;
    EEPROM.write(9, conFV);
    conTarifaValle = false;
    EEPROM.write(10, conTarifaValle);
    valorTipoCarga = 0;
    EEPROM.write(12, valorTipoCarga);
    inicioCargaActivado = false;
    EEPROM.write(13, inicioCargaActivado);
  }
  if (generacionMinima > 100) {
    generacionMinima = 0;
    EEPROM.write(8, generacionMinima);
  }
  if (minutoInicioCarga > 59) {
    minutoInicioCarga = 0;
    EEPROM.write(1, minutoInicioCarga);
  }
  if (intensidadProgramada < 6 ){ // Corregimos el valor de la Intensidad Programada si fuese necesario .... 
    intensidadProgramada = 6;    // no puede ser menor de 6A
    EEPROM.write(2, intensidadProgramada);
  }else if (intensidadProgramada > 32 ){
    intensidadProgramada = 32;   // tampoco puede ser mayor de 32A, ya que es la intensidad máxima que soporta el cargador.
    EEPROM.write(2, intensidadProgramada);
  }
  if (consumoTotalMax > 60) {
    consumoTotalMax = 32;
    EEPROM.write(3, consumoTotalMax); //Si el valor es erroneo lo ponemos a 32
  }
  if (horaFinCarga > 24) {
    horaFinCarga = 0;
    EEPROM.write(4, horaFinCarga);//Si el valor es erroneo lo ponemos a 0
  }
  if (minutoFinCarga > 59) {
    minutoFinCarga = 0;
    EEPROM.write(5, minutoFinCarga); //Si el valor es erroneo lo ponemos a 0
  }
  if (tipoCarga > 6){
    tipoCarga = 0;
    EEPROM.write(11, tipoCarga); //Si el valor es erroneo lo ponemos a 0
  }
  
  if (kwTotales > 4000000000) {
    kwTotales = 0;
    EEPROM.write(14, 0);//Si el valor es erroneo  reseteamos el valor de los KW acumulados
    EEPROM.write(15, 0);
    EEPROM.write(16, 0);
    EEPROM.write(17, 0);
  }
  
  Timer1.initialize(1000);         // Temporizador que activa un nuevo ciclo de onda
  Timer1.attachInterrupt(RetPulsos); // Activa la interrupcion y la asocia a RetPulsos

  duracionPulso = ((intensidadProgramada * 105 / 7) - 60);
  permisoCarga = false;
  conectado = false;
  enPantallaNumero = 0;
  
  lcd.setCursor(0, 0);
  lcd.print(" WALLBOX FEBOAB ");
  lcd.setCursor(0, 1);
  lcd.print("**** V 1.02 ****");
  delay(1500);
  digitalWrite(pinRegulacionCargador, HIGH);
}

 //----RUTINA DE GENERACIÓN DE LA ONDA CUADRADA----
void RetPulsos() {                         
  if (permisoCarga) {            // Si hay permiso de carga ......
    digitalWrite(pinRegulacionCargador, HIGH);    // activamos el pulso ....
    delayMicroseconds(duracionPulso); // durante el tiempo que marca "Duración_Pulsos" .... 
    digitalWrite(pinRegulacionCargador, LOW);     // desactivamos el pulso ....
  }else{                  // Si no hay permiso de carga ....
    digitalWrite(pinRegulacionCargador, HIGH);     // activamos el pulso ....
  }
}

void loop() {
  int tension = analogRead(pinTensionCargador);

  if (tension > 100){
    acumTensionCargador += tension;
    numTensionAcum++;
  }

  if (permisoCarga){
    int consumoCargadorTemp = analogRead(pinConsumoCargador);  // Leemos el consumo del cargador
    int consumoGeneralTemp = analogRead(pinConsumoGeneral);    // Leemos el consumo general de la vivienda
    int generacionFVTemp = analogRead(pinGeneracionFV);      // Leemos la generación de la instalación fotovoltaica
    
    if (consumoCargadorTemp > picoConsumoCargador)           // toma el valor más alto de consumo del cargador de entre 300 lecturas
      picoConsumoCargador = consumoCargadorTemp;
    if (consumoGeneralTemp > picoConsumoGeneral)       // toma el valor más alto de consumo general de entre 300 lecturas
      picoConsumoGeneral = consumoGeneralTemp;
    if (generacionFVTemp > picoGeneracionFV)         // toma el valor más alto de generación fotovoltaica de entre 300 lecturas
      picoGeneracionFV = generacionFVTemp;
  }
  
  numCiclos++;
  if (numCiclos > 299){
    numCiclos = 0;
    if (acumTensionCargador > 0 && numTensionAcum > 0){
      tensionCargador = acumTensionCargador / numTensionAcum;
    }
    if (permisoCarga){
      consumoCargador = picoConsumoCargador;
      consumoGeneral = picoConsumoGeneral;
      generacionFV = picoGeneracionFV;
      acumTensionCargador = 0;
      numTensionAcum = 0;
      picoConsumoCargador = 0;
      picoConsumoGeneral = 0;
      picoGeneracionFV = 0;
  
      consumoCargadorAmperios = ((consumoCargador - 520) * 4) / 24;    // Calcula el consumo del cargador en Amperios
      if (consumoCargadorAmperios < 0){                   // Se restan 520 porque la lectura se hace a través de un divisor de tensión
        consumoCargadorAmperios = 0;
      }
      consumoGeneralAmperios = ((consumoGeneral - 520) * 4) / 24;     // Calcula el consumo general en Amperios
      if ((consumoGeneralAmperios < 0) || !conSensorGeneral){
        consumoGeneralAmperios = 0;
      }
      generacionFVAmperios = ((generacionFV - 520) * 4) / 24;       // Calcula la generación fotovoltaica en Amperios
      if ((generacionFVAmperios < 0) || !conFV)    {
        generacionFVAmperios = 0;
      }
    }
    
    conectado = (tensionCargador < 660);
    cargando = (tensionCargador < 600);
    timeNow = rtc.now();
    int horaNow = timeNow.hour();
    int minutoNow = timeNow.minute();
    if (horaNow >= 3 && horaNow > lastCheckHour){
      lastCheckHour = horaNow;
      bool tempHorarioVerano = EsHorarioVerano(timeNow);
      if (horarioVerano && !tempHorarioVerano){
        horaNow--;
        horarioVerano = false;
        rtc.adjust(DateTime(timeNow.year(), timeNow.month(), timeNow.day(), horaNow, minutoNow, timeNow.second()));
      }else if (!horarioVerano && tempHorarioVerano){
        horaNow++;
        horarioVerano = true;
        rtc.adjust(DateTime(timeNow.year(), timeNow.month(), timeNow.day(), horaNow, minutoNow, timeNow.second()));
      }
    }
    if (conectado && inicioCargaActivado){
      if (!cargando && !cargaCompleta){
        bool puedeCargar = false;
        switch(tipoCarga){
          case TARIFAVALLE:
            if (horarioVerano){
              if (horaNow >= 23 || horaNow < 13){
                puedeCargar = true;
              }
            }else{
              if (horaNow >= 22 || horaNow < 12){
                puedeCargar = true;
              }
            }
            break;
          case FRANJAHORARIA:
            if (EnFranjaHoraria(horaNow, minutoNow))puedeCargar = true;
            break;
          case POTENCIA:
          case FRANJATIEMPO:
          case INMEDIATA:
            puedeCargar = true;
            break;
          case EXCEDENTESFV:
            if (HayExcedentesFV())puedeCargar = true;
            break;
          case INTELIGENTE:
              if (HayExcedentesFV()){
                tipoCargaInteligente = EXCEDENTESFV;
                puedeCargar = true;
              }else if (conTarifaValle){
                if (horarioVerano){
                  if (horaNow >= 23 || horaNow < 13){
                    tipoCargaInteligente = TARIFAVALLE;
                    puedeCargar = true;
                  }
                }else{
                  if (horaNow >= 22 || horaNow < 12){
                    tipoCargaInteligente = TARIFAVALLE;
                    puedeCargar = true;
                  }
                }
              }else if (EnFranjaHoraria(horaNow, minutoNow)){
                tipoCargaInteligente = FRANJAHORARIA;
                puedeCargar = true;
              }
            break;
        }
        if (puedeCargar && permisoCarga){
          FinalizarCarga();
        }
        if (puedeCargar){
          digitalWrite(pinAlimentacionCargador, HIGH);
          duracionPulso = CalcularDuracionPulso();
          permisoCarga = puedeCargar;
        }
      }else if (cargando){
        CalcularPotencias();
        duracionPulso = CalcularDuracionPulso();
        switch(tipoCarga){
          case TARIFAVALLE:
            if ((!horarioVerano && horaNow >= 12) || (horarioVerano && horaNow >= 13)){
              FinalizarCarga();
            }
            break;
          case FRANJAHORARIA:
            if (horaFinCarga < horaNow || (horaFinCarga == horaNow &&  minutoFinCarga <= minutoNow)){
              FinalizarCarga();
            }
            break;
          case POTENCIA:
            if (watiosCargados >= (valorTipoCarga * 100)){
              FinalizarCarga();
            }
            break;
          case FRANJATIEMPO:
            {
              long tiempoCarga = millis() - tiempoInicioSesion;
              long valor = valorTipoCarga * 60000;
              if (tiempoCarga >= valor){
                FinalizarCarga();
              }
            }
            break;
          case EXCEDENTESFV:
            permisoCarga = CheckExcedendesFV();
            break;
          case INTELIGENTE:
            switch (tipoCargaInteligente){
              case EXCEDENTESFV:
                permisoCarga = CheckExcedendesFV();
                break;
              case TARIFAVALLE:
                if ((!horarioVerano && horaNow >= 12) || (horarioVerano && horaNow >= 13)){
                  FinalizarCarga();
                }
                break;
              case FRANJAHORARIA:
              if (horaFinCarga < horaNow || (horaFinCarga == horaNow &&  minutoFinCarga <= minutoNow)){
                FinalizarCarga();
              }
              break;
            }
            break;
        }
      }
    }else if (!conectado && inicioCargaActivado){
      FinalizarCarga();
    }
    if (enPantallaNumero == 0 && luzLcd) updateScreen();
  }

  if (digitalRead(pinPulsadorInicio) == HIGH) ProcesarBoton(BOTONINICIO);
  if (digitalRead(pinPulsadorProg) == HIGH) ProcesarBoton(BOTONPROG);
  if (digitalRead(pinPulsadorMas) == HIGH) ProcesarBoton(BOTONMAS);
  if (digitalRead(pinPulsadorMenos) == HIGH) ProcesarBoton(BOTONMENOS);
  
  if (luzLcd){
    if (millis() - tiempoUltimaPulsacionBoton >= 600000){
      luzLcd = false;
      enPantallaNumero = 0;
      lcd.noBacklight();
      lcd.clear();
    }
  }
}

void IniciarCarga(){
  watiosCargados = 0;
  inicioCargaActivado = true;
  enPantallaNumero = 0;
  EEPROM.write(11, tipoCarga);
  EEPROM.write(13, inicioCargaActivado);
}

void FinalizarCarga(){
  digitalWrite(pinAlimentacionCargador, LOW);
  cargaCompleta = true;
  permisoCarga = false;
  inicioCargaActivado = false;
  tiempoInicioSesion = 0;                  // Se restan 520 porque la lectura se hace a través de un divisor de tensión
  consumoCargadorAmperios = 0;
  consumoGeneralAmperios = 0;
  generacionFVAmperios = 0;
  EEPROM.write(13, inicioCargaActivado);
  EEPROMWritelong(14, kwTotales);
}

void ProcesarBoton(int button){
  tiempoUltimaPulsacionBoton = millis();
  if (luzLcd){
    switch(enPantallaNumero){
      case 0:   // pantalla principal
        switch (button){
          case BOTONINICIO:
            if (inicioCargaActivado){
              enPantallaNumero = 4;
            }else{
              enPantallaNumero = 2;
              opcionNumero = tipoCarga;
            }
            break;
          case BOTONPROG:
            enPantallaNumero = 1;
            opcionNumero = 0;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorInt = intensidadProgramada;
            enPantallaNumero = 3;
            break;
        }
        updateScreen();
        break;
      case 1:   //Pantalla selecion configuracion
        switch (button){
          case BOTONINICIO:
            if (opcionNumero == 0){
              enPantallaNumero = 11;
            }else if (opcionNumero == 1){
              enPantallaNumero = 10;
            }else{
              DateTime timeTemp  = rtc.now();
              nuevoAnno = timeTemp.year();
              if (nuevoAnno < 2015) nuevoAnno = 2015;
              nuevoMes = timeTemp.month() + 1;
              nuevoDia = timeTemp.day();
              nuevaHora = timeTemp.hour();
              nuevoMinuto = timeTemp.minute();
              enPantallaNumero = 130;
            }
            opcionNumero = 0;
            break;
          case BOTONMAS:
            (opcionNumero == 2) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero == 0) ? opcionNumero = 2 : opcionNumero--;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 2:    // seleccion del tipo de carga
        switch (button){
          case BOTONINICIO:
            switch (opcionNumero){
              case TARIFAVALLE:
                tipoCarga = TARIFAVALLE;
                IniciarCarga();
                break;
              case FRANJAHORARIA:
                tipoCarga = FRANJAHORARIA;
                IniciarCarga();
                break;
              case POTENCIA:
                enPantallaNumero = 20;
                if (tipoCarga == POTENCIA) tempValorInt = valorTipoCarga;
                else tempValorInt = 5;
                break;
              case FRANJATIEMPO:
                enPantallaNumero = 21;
                if (tipoCarga == FRANJATIEMPO) tempValorInt = valorTipoCarga;
                else tempValorInt = 30;
                break;
              case INMEDIATA:
                tipoCarga = INMEDIATA;
                IniciarCarga();
                break;
              case EXCEDENTESFV:
                tipoCarga = EXCEDENTESFV;
                IniciarCarga();
                break;
              case INTELIGENTE:
                tipoCarga = INTELIGENTE;
                IniciarCarga();
                break;
            }
            break;
          case BOTONMAS:
            if (!conSensorGeneral && opcionNumero == 4)opcionNumero = 0;
            (opcionNumero == 6) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            if (!conSensorGeneral && opcionNumero == 0)opcionNumero = 4;
            (opcionNumero == 0) ? opcionNumero = 6 : opcionNumero--;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 3:
        switch (button){
          case BOTONINICIO:
            intensidadProgramada = tempValorInt;
            EEPROM.write(2, intensidadProgramada);
            enPantallaNumero = 0;
            break;
          case BOTONMAS:
            if (tempValorInt == 36) tempValorInt = 6;
            break;
          case BOTONMENOS:
            if (tempValorInt == 6) tempValorInt = 36;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 4:
        switch (button){
          case BOTONINICIO:
            FinalizarCarga();
            if (watiosCargados == 0){
              cargaCompleta = false;
            }
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 10:
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = (opcionNumero == 0) ? 100 : 1;
            opcionNumero = 0;
          case BOTONMAS:
            (opcionNumero == 7) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero == 0) ? opcionNumero = 7 : opcionNumero--;
            break;          
          case BOTONPROG:
            enPantallaNumero = 1;
            opcionNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 11:
        switch (button){
          case BOTONINICIO:
            switch(opcionNumero){
              case 0:
                enPantallaNumero = 110;
                tempValorInt = horaInicioCarga;
                break;
              case 1:
                enPantallaNumero = 111;
                tempValorInt = minutoInicioCarga;
                break;
              case 2:
                enPantallaNumero = 112;
                tempValorInt = horaFinCarga;
                break;
              case 3:
                enPantallaNumero = 113;
                tempValorInt = minutoFinCarga;
                break;
              case 4:
                enPantallaNumero = 114;
                tempValorBool = conSensorGeneral;
                break;
              case 5:
                enPantallaNumero = 115;
                tempValorInt = intensidadProgramada;
                break;
              case 6:
                enPantallaNumero = 116;
                tempValorBool = cargadorEnConsumoGeneral;
                break;
              case 7:
                enPantallaNumero = 117;
                tempValorBool = conFV;
                break;
              case 8:
                enPantallaNumero = 118;
                tempValorInt = generacionMinima;
                break;
              case 9:
                enPantallaNumero = 119;
                tempValorBool = conTarifaValle;
                break;
              case 10:
                enPantallaNumero = 120;
                tempValorInt = consumoTotalMax;
                break;
            }
            break;
          case BOTONMAS:
            (opcionNumero == 10) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero == 0) ? opcionNumero = 10 : opcionNumero--;
            break;
          case BOTONPROG:
            enPantallaNumero = 1;
            opcionNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 20:    // seleccion de potencia de carga
        switch (button){
          case BOTONINICIO:
            tipoCarga = POTENCIA;
            valorTipoCarga = tempValorInt;
            IniciarCarga();
            EEPROM.write(12, valorTipoCarga);
            break;
          case BOTONMAS:
            if (tempValorInt < 50) tempValorInt += 5;
            break;
          case BOTONMENOS:
            if (tempValorInt > 5) tempValorInt -= 5;
            break;
          case BOTONPROG:
            enPantallaNumero = 2;
            break;
        }
        updateScreen();
        break;
      case 21:    // seleccion de tiempo de carga
        switch (button){
          case BOTONINICIO:
            tipoCarga = FRANJATIEMPO;
            valorTipoCarga = tempValorInt;
            IniciarCarga();
            EEPROM.write(12, valorTipoCarga);
            break;
          case BOTONMAS:
            if (tempValorInt < 600) tempValorInt += 30;
            break;
          case BOTONMENOS:
            if (tempValorInt > 5) tempValorInt -= 30;
            break;
          case BOTONPROG:
            enPantallaNumero = 2;
            break;
        }
        updateScreen();
        break;
      case 100:
        switch (button){
          case BOTONINICIO:
            kwTotales = 0;
            EEPROM.write(14, 0);
            EEPROM.write(15, 0);
            EEPROM.write(16, 0);
            EEPROM.write(17, 0);
            enPantallaNumero = 1;
            break;
          case BOTONPROG:
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 110:
        switch (button){
          case BOTONINICIO:
            horaInicioCarga = tempValorInt;
            EEPROM.write(0, horaInicioCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 23) tempValorInt = 0;
            break;
          case BOTONMENOS:
            if (tempValorInt == 0) tempValorInt = 23;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 111:
        switch (button){
          case BOTONINICIO:
            minutoInicioCarga = tempValorInt;
            EEPROM.write(1, minutoInicioCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 59) tempValorInt = 0;
            break;
          case BOTONMENOS:
            if (tempValorInt == 0) tempValorInt = 59;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 112:
        switch (button){
          case BOTONINICIO:
            horaFinCarga = tempValorInt;
            EEPROM.write(4, horaFinCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 23) tempValorInt = 0;
            break;
          case BOTONMENOS:
            if (tempValorInt == 0) tempValorInt = 23;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 113:
        switch (button){
          case BOTONINICIO:
            minutoFinCarga = tempValorInt;
            EEPROM.write(5, minutoFinCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 59) tempValorInt = 0;
            break;
          case BOTONMENOS:
            if (tempValorInt == 0) tempValorInt = 59;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 114:
        switch (button){
          case BOTONINICIO:
            conSensorGeneral = tempValorBool;
            EEPROM.write(7, conSensorGeneral);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorBool = (tempValorBool) ? false : true;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 115:
        switch (button){
          case BOTONINICIO:
            intensidadProgramada = tempValorInt;
            EEPROM.write(2, intensidadProgramada);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 36) tempValorInt = 6;
            break;
          case BOTONMENOS:
            if (tempValorInt == 6) tempValorInt = 36;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 116:
        switch (button){
          case BOTONINICIO:
            cargadorEnConsumoGeneral = tempValorBool;
            EEPROM.write(6, cargadorEnConsumoGeneral); 
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorBool = (tempValorBool) ? false : true;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 117:
        switch (button){
          case BOTONINICIO:
            conFV = tempValorBool;
            EEPROM.write(9, conFV);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorBool = (tempValorBool) ? false : true;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 118:
        switch (button){
          case BOTONINICIO:
            generacionMinima = tempValorInt;
            EEPROM.write(8, generacionMinima);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 36) tempValorInt = 6;
            break;
          case BOTONMENOS:
            if (tempValorInt == 6) tempValorInt = 36;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 119:
        switch (button){
          case BOTONINICIO:
            conTarifaValle = tempValorBool;
            EEPROM.write(10, conTarifaValle);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorBool = (tempValorBool) ? false : true;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 120:
        switch (button){
          case BOTONINICIO:
            consumoTotalMax = tempValorBool;
            EEPROM.write(3, consumoTotalMax);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            if (tempValorInt == 60) tempValorInt = 10;
            break;
          case BOTONMENOS:
            if (tempValorInt == 10) tempValorInt = 60;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 130:    // Ajuste del año
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = 131;
            break;
          case BOTONMAS:
            if (nuevoAnno < 2050) nuevoAnno++;
            break;
          case BOTONMENOS:
            if (nuevoAnno > 2015) nuevoAnno--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 131:    //Ajuste del mes
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = 132;
            break;
          case BOTONMAS:
            if (nuevoMes == 12) nuevoMes = 1;
            else nuevoMes++;
            break;
          case BOTONMENOS:
            if (nuevoMes == 1) nuevoMes = 12;
            else nuevoMes--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 132:    //Ajuste del dia
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = 133;
            break;
          case BOTONMAS:
            (nuevoDia == daysInMonth[nuevoMes - 1]) ? nuevoDia = 1 : nuevoDia++;
            break;
          case BOTONMENOS:
            (nuevoDia == 1) ? nuevoDia = daysInMonth[nuevoMes - 1] : nuevoDia--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 133:    //Ajuste de la hora
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = 134;
            break;
          case BOTONMAS:
            (nuevaHora == 23) ? nuevaHora = 0 : nuevaHora++;
            break;
          case BOTONMENOS:
            (nuevaHora == 0) ? nuevaHora = 23 : nuevaHora--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 134:    //Ajuste del los minutos
        switch (button){
          case BOTONINICIO:
            rtc.adjust(DateTime(nuevoAnno, nuevoMes, nuevoDia, nuevaHora, nuevoMinuto, 0));
            enPantallaNumero = 135;
            break;
          case BOTONMAS:
            (nuevoMinuto == 59) ? nuevoMinuto = 0 : nuevoMinuto++;
            break;
          case BOTONMENOS:
            (nuevoMinuto == 0) ? nuevoMinuto = 59 : nuevoMinuto--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 135:    //Ajuste hora ok
        switch (button){
          case BOTONINICIO:
          case BOTONMAS:
          case BOTONMENOS:
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
    }
  }else{    
    luzLcd = true;
    updateScreen();
    lcd.backlight();
  }
}

void updateScreen(){
  lcd.clear();
  lcd.setCursor(0, 0);
  switch(enPantallaNumero){
    case 0:
      if (cargando){
        lcd.print("Cargando a:");
        lcd.setCursor(6, 1);
        lcd.print(consumoCargadorAmperios + " A");
      }else{
        if (conectado && inicioCargaActivado){
          switch (tipoCarga){
            case EXCEDENTESFV:
            case INTELIGENTE:
              lcd.print("Generacion FV");
              lcd.setCursor(0, 1);
              lcd.print("Insuficiente");
              break;
            case FRANJATIEMPO:
              lcd.print("Esperando Hora");
              lcd.setCursor(0, 1);
              lcd.print("Programada.");
              break;
            case TARIFAVALLE:
              lcd.print("Esperando");
              lcd.setCursor(0, 1);
              lcd.print("Tarifa Valle.");
              break;
          }
        }else if (inicioCargaActivado){
          lcd.print("Coche No");
          lcd.setCursor(0, 1);
          lcd.print("Conectado.");
        }else if (conectado){
          lcd.print("Coche Conectado.");
          lcd.setCursor(0, 1);
          lcd.print("A la espera...");
        }else if (cargaCompleta){
          lcd.print("Cargado ");
          lcd.setCursor(0, 1);
          lcd.print(watiosCargados / 100 + "wh");
        }else{
            lcd.setCursor(0, 0);
            lcd.print(" WALLBOX FEBOAB ");
            String hora = (timeNow.hour() < 10) ? "0" + (String)timeNow.hour() : (String)timeNow.hour();
            hora += ":";
            hora += (timeNow.minute() < 10) ? "0" + (String)timeNow.minute() : (String)timeNow.minute();
            lcd.setCursor(6, 1);
            lcd.print(hora);
        }
      }
      break;
    case 1:
      lcd.print("Opciones:");
      lcd.setCursor(0, 1);
      switch (opcionNumero){
        case 0:
          lcd.print("Configuracion.");
          break;
        case 1:
          lcd.print("Visualizacion.");
          break;
        case 2:
          lcd.print("PTA En Hora.");
          break;
      }
      break;
    case 2:    // pantalla tipo de carga
      lcd.print("Tipo de Carga:");
      lcd.setCursor(0, 1);
      switch (opcionNumero){
        case TARIFAVALLE:
          lcd.print("Tarifa Valle.");
          break;
        case FRANJAHORARIA:
          lcd.print("Por Horario.");
          break;
        case POTENCIA:
          lcd.print("Por Energia.");
          break;
        case FRANJATIEMPO:
          lcd.print("Por Tiempo.");
          break;
        case INMEDIATA:
          lcd.print("Inmediata.");
          break;
        case EXCEDENTESFV:
          lcd.print("Excedentes FV.");
          break;
        case INTELIGENTE:
          lcd.print("Inteligente.");
          break;
      }
      break;
    case 3:
      lcd.print("Intensidad Carga");
      lcd.setCursor(6, 1);
      lcd.print(tempValorInt + " A");
      break;
    case 10:
      switch (opcionNumero){
        case 0:
          lcd.print("Carga Acumulada:");
          lcd.setCursor(2, 1);
          lcd.print(kwTotales + " KWH");
          break;
        case 1:
          lcd.print("Autoriz. Carga:");
          lcd.setCursor(7, 1);
          (inicioCargaActivado) ? lcd.print("Si") : lcd.print("No");
          break;
        case 2:
          lcd.print("Coche Conectado:");
          lcd.setCursor(7, 1);
          (conectado) ? lcd.print("Si") : lcd.print("No");
          break;
        case 3:
          lcd.print("Coche Cargando:");
          lcd.setCursor(7, 1);
          (cargando) ? lcd.print("Si") : lcd.print("No");
          break;
        case 4:
          lcd.print("Consigna Carga:");
          lcd.setCursor(6, 1);
          lcd.print(intensidadProgramada + " A");
          break;
        case 5:
          lcd.print("Intensidad Carga:");
          lcd.setCursor(6, 1);
          lcd.print(consumoCargadorAmperios + " A");
          break;
        case 6:
          lcd.print("Excedentes FV:");
          lcd.setCursor(6, 1);
          lcd.print(generacionFVAmperios + " A");
          break;
        case 7:
          lcd.print("Consumo General:");
          lcd.setCursor(6, 1);
          lcd.print(consumoGeneralAmperios + " A");
          break;
      }
      break;
    case 4:
      lcd.print("FINALIZAR CON");
      lcd.setCursor(0, 1);
      lcd.print("LA CARGA.");
      break;
    case 11:
      switch (opcionNumero){
        case 0:
          lcd.print("Hora Ini Carga:");
          lcd.setCursor(7, 1);
          (horaInicioCarga < 10) ? lcd.print("0" + horaInicioCarga) : lcd.print(horaInicioCarga);
          break;
        case 1:
          lcd.print("Minu. Ini Carga:");
          lcd.setCursor(7, 1);
          (minutoInicioCarga < 10) ? lcd.print("0" + minutoInicioCarga) : lcd.print(minutoInicioCarga);
          break;
        case 2:
          lcd.print("Hora Fin Carga:");
          lcd.setCursor(7, 1);
          (horaFinCarga < 10) ? lcd.print("0" + horaFinCarga) : lcd.print(horaFinCarga);
          break;
        case 3:
          lcd.print("Minu. Fin Carga:");
          lcd.setCursor(7, 1);
          (minutoFinCarga < 10) ? lcd.print("0" + minutoFinCarga) : lcd.print(minutoFinCarga);
          break;
        case 4:
          lcd.print("Trafo General:");
          lcd.setCursor(7, 1);
          (conSensorGeneral) ? lcd.print("Si") : lcd.print("No");
          break;
        case 5:
          lcd.print("Intens. Maxima:");
          lcd.setCursor(6, 1);
          lcd.print(intensidadProgramada + " A");
          break;
        case 6:
          lcd.print("Carg T General:");
          lcd.setCursor(7, 1);
          (cargadorEnConsumoGeneral) ? lcd.print("Si") : lcd.print("No");
          break;
        case 7:
          lcd.print("Traf Generacion:");
          lcd.setCursor(7, 1);
          (conFV) ? lcd.print("Si") : lcd.print("No");
          break;
        case 8:
          lcd.print("Intens Min Gen:");
          lcd.setCursor(6, 1);
          lcd.print(generacionMinima + " A");
          break;
        case 9:
          lcd.print("Tarifa Valle:");
          lcd.setCursor(7, 1);
          (conTarifaValle) ? lcd.print("Si") : lcd.print("No");
          break;
        case 10:
          lcd.print("Consu Total Max:");
          lcd.setCursor(6, 1);
          lcd.print(consumoTotalMax + " A");
          break;
      }
      break;
    case 20:  // Carga por potencia
      lcd.print("Potencia:");
      lcd.setCursor(0, 1);
      lcd.print(tempValorInt * 100 + " w");
      break;
    case 21:  // carga por franja de tiempo
      lcd.print("Tiempo:");
      lcd.setCursor(0, 1);
      lcd.print(tempValorInt + " min");
      break;
    case 100:
      lcd.print(" RESET POTENCIA ");
      lcd.setCursor(3, 1);
      lcd.print("ACUMULADA");
      break;
    case 110:
    case 111:
    case 112:
    case 113:
      lcd.print("Ajuste:");
      lcd.setCursor(7, 1);
      (tempValorInt < 10) ? lcd.print("0" + tempValorInt) : lcd.print(tempValorInt);
      break;
    case 114:
    case 116:
    case 117:
    case 119:
      lcd.print("Ajuste:");
      lcd.setCursor(7, 1);
      (tempValorBool) ? lcd.print("Si") : lcd.print("No");
      break;
    case 115:
    case 118:
    case 120:
      lcd.print("Ajuste:");
      lcd.setCursor(6, 1);
      lcd.print(tempValorInt + " A");
      break;
    case 130:
      lcd.print("Ajuste año:");
      lcd.setCursor(0, 1);
      lcd.print(nuevoAnno);
      break;
    case 131:
      lcd.print("Ajuste mes:");
      lcd.setCursor(0, 1);
      lcd.print(nuevoMes);
      break;
    case 132:
      lcd.print("Ajuste dia:");
      lcd.setCursor(0, 1);
      lcd.print(nuevoDia);
      break;
    case 133:
      lcd.print("Ajuste hora:");
      lcd.setCursor(0, 1);
      lcd.print(nuevaHora);
      break;
    case 134:
      {
        lcd.print("Ajuste min:");
        String str = (String)nuevoMinuto; // es una prueba a ver si asi lo centra en la pantalla
        int lenght = str.length();
        if (lenght == 1){
          str = "0" + str;
          lenght = 2;
        }
        lenght = 8 - (lenght / 2);
        lcd.setCursor(lenght, 1);
        lcd.print(str);
      }
      break;
    case 135:
      lcd.print("Ajuste");
      lcd.setCursor(0, 1);
      lcd.print("Correcto.");
      break;
  }  
}

bool EsHorarioVerano(DateTime fecha){
  if (fecha.month() > 2 && fecha.month() < 9){
    return true;
  }else{
    //el último domingo de Marzo
    int dhv = daysInMonth[2] - DateTime(fecha.year(), 2, daysInMonth[2], 0, 0, 0).dayOfTheWeek();
    //el último domingo de Octubre
    int dhi = daysInMonth[9] - DateTime(fecha.year(), 9, daysInMonth[9], 0, 0, 0).dayOfTheWeek();
    if ((fecha.month() == 2  && fecha.day() >= dhv) || (fecha.month() == 9 && fecha.day() < dhi)){
      return true;
    }
  }
  return false;
}

bool HayExcedentesFV(){
  unsigned long currentMillis = millis();
  if (tiempoGeneraSuficiente > currentMillis) tiempoGeneraSuficiente = currentMillis;
  if (tiempoNoGeneraSuficiente > currentMillis) tiempoNoGeneraSuficiente = currentMillis;
  
  generacionSuficiente = (generacionFVAmperios >= generacionMinima);  // Verificamos si hay suficientes excedentes fotovoltaicos....
  
  if (generacionSuficiente){                  // Si hay excedentes sufucientes ....
    tiempoNoGeneraSuficiente = currentMillis;        // comenzamos a controlar el tiempo durante el que no hay excedentes ....
    if (currentMillis - tiempoGeneraSuficiente > 120000) return true;   // Si hay excedentes durante más de 2 minutos activamos la carga
  }
  return false;
}

bool CheckExcedendesFV(){
  generacionSuficiente = (generacionFVAmperios >= generacionMinima);  // Verificamos si hay suficientes excedentes fotovoltaicos....
  
  if (!generacionSuficiente){                // Si NO hay excedentes sufucientes ....
    unsigned long currentMillis = millis();
    tiempoGeneraSuficiente = currentMillis;         // comenzamos a controlar el tiempo durante el que hay excedentes ....
    if (currentMillis - tiempoNoGeneraSuficiente > 120000) return false; // Si no hay excedentes durante más de 2 minutos desactivamos la carga
  }
  return true;
}

bool EnFranjaHoraria(int horaNow, int minutoNow){
  if (horaInicioCarga > 0 || minutoInicioCarga > 0){
    if (horaInicioCarga > horaNow){
      if (horaNow < horaFinCarga || (horaNow == horaFinCarga && minutoNow < minutoFinCarga)) return true;
    }else if (horaInicioCarga == horaNow && minutoInicioCarga >= minutoNow){
        return true;
    }else if(horaFinCarga < horaInicioCarga){
      if (horaFinCarga > horaNow || (horaFinCarga == horaNow && minutoInicioCarga > minutoNow)) return true;
    }
  }
  return false;
}

void CalcularPotencias(){
  unsigned long currentMillis = millis();
  if (tiempoInicioSesion == 0) {
    tiempoInicioSesion = currentMillis;    // Al comienzo de la sesión de carga comenzamos a contar el tiempo para saber cuanto tiempo llevamos ....
    watiosCargados = 0;            // y reseteamos el contador de watios cargados
  }

  unsigned long tiempoCalculoWatios = currentMillis - tiempoCalculoPotenciaCargada;  // Evaluamos el tiempo para el cálculo de watios cargados ....
  if (tiempoCalculoWatios > 12000) {                    // si llevamos más de 12 seg ....
    tiempoCalculoWatios = currentMillis;              // lo reseteamos
    tiempoCalculoWatios = 0;
  }
  
  if (tiempoCalculoWatios > 3000) {                   // Si llevamos más de 3 seg vamos sumando ...
    watiosCargados = watiosCargados + ((consumoCargadorAmperios * 24500l) / (3600000l / tiempoCalculoWatios));  // Lo normal seria 230, pero en mi caso tengo la tensión muy alta ....
    kwTotales = kwTotales + ((consumoCargadorAmperios * 24500l) / (3600000l / tiempoCalculoWatios));
    tiempoCalculoPotenciaCargada = currentMillis;  // Si no estamos cargando reseteamos el tiempo de cálculo de la energía cargada
  }
}

int CalcularDuracionPulso(){
  int pulso;
  int consumo = ObtenerConsumoRestante();
  if ((consumo < intensidadProgramada) && conSensorGeneral){  // Si el consumo restante es menor que la potencia programada y tenemos el sensor general conectado..
    pulso = ((consumo * 100 / 6) - 28);          // calculamos la duración del pulso en función de la intensidad restante
  }else{
    pulso = ((intensidadProgramada * 100 / 6) - 28);
  }
  if (pulso < 72) pulso = 72;   // Si la duración del pulso resultante es menor de 72(6A) lo ponemos a 6 A.
  return pulso;
}

int ObtenerConsumoRestante(){
  if (!conSensorGeneral){
    return 32;                    // Asignamos 32 Amperios como consumo restante
  }else {
    if (cargadorEnConsumoGeneral){ // Aquí se controla si el cargador está incluido en el trafo de consumo general de la vivienda,
                      // para tenerlo en cuenta al calcular la intensidad disponible.
                      // Esto es debido a que la normativa permite sacar la alimentación del cargador directamente del contador
                      // Se hace en edificios para no tener que tirar la línea desde el CGP de la vivienda hasta el garage          
      return (generacionFVAmperios + consumoTotalMax) - consumoGeneralAmperios; // Si el trafo que mide consumo general incluye el cargador //no se resta el consumo del cargador
    }else{
      return (generacionFVAmperios + consumoTotalMax) - (consumoGeneralAmperios - consumoCargadorAmperios); // Si el trafo que mide consumo general no incluye el cargador se resta el consumo del cargador
    }
  }
}

void EEPROMWritelong(int address, long value)   //    Función que permite escribir un dato de tipo Long en la eeprom partiendolo en 4 Bytes
{
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address)       //    Función que permite leer un dato de tipo Long de la eeprom partiendo de 4 Bytes
{
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}
