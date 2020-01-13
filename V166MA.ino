// V165MA SE INCLUYE EL INICIO AUTOMATICO DE LA CARGA CUANDO ESTAMOS EN TIPO DE CARGA "INMEDIATA"
// V166MA REVISAR MENSAJE DE CONSIGNA INSUFICIENTE NO SE MUESTRA, PONE COCHE CONECTADO (SE AÑADE "CONSIGNA" Y "GENERACION_FV" A LOS TIPOS DE CARGA QUE INICIAN LA CARGA AL CONECTAR EL COCHE)
// V166MA REVISAR COMPORTAMIENTO EXTRAÑO DE LOS VALORES DE CARGA POR TIEMPO Y POR ENERGÍA (SE CAMBIAN LAS VARIABLES valorTipoCarga y tempValorInt a UNSIGNED LONG)
 


#include <Wire.h>
#include <RTClib.h>
#include <TimerOne.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd( 0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE ); //Configuramos la pantalla con la dirección 3f


//              DEFINICIÓN PINES ENTRADAS ANALOGICAS
const int pinGeneracionFV = 0;      // define el pin 0 como 'Generación FV'
const int pinConsumoGeneral = 1;    // define el pin 1 como 'Consumo General'
const int pinTensionCargador = 2;   // define el pin 2 como 'Tension'
const int pinConsumoCargador = 3;   // define el pin 3 como 'Consumo Cargador'

//              DEFINICIÓN PINES ENTRADAS DIGITALES
const int pinPulsadorInicio = 2;
const int pinPulsadorMenos = 3;
const int pinPulsadorMas = 4;
const int pinPulsadorProg = 5;
const int pinAlimentacionCargador = 6;    //Salida que activa el contactor que alimenta el cargador
const int pinRegulacionCargador = 9;      //Salida que activa la onda cuadrada

//              DEFINICION TIPOS CARGA
const int TARIFA_DH = 0;
const int FRANJA_HORARIA = 1;
const int INMEDIATA = 2;
const int ENERGIA = 3;
const int TIEMPO = 4;
const int CONSIGNA = 5;
const int GENERACION_FV = 6;
const int INTELIGENTE = 7;


const int BOTONINICIO = 0;
const int BOTONMAS = 1;
const int BOTONMENOS = 2;
const int BOTONPROG = 3;

//              DEFINICION VARIABLES GLOBALES
volatile int volatileTension;
byte horaInicioCarga = 1, minutoInicioCarga = 0, horaFinCarga = 9, minutoFinCarga = 0, intensidadProgramada = 16, consumoTotalMax = 22, generacionMinima = 5, tipoCarga = 2, tipoCargaInteligente = 0;
byte tiempoSinGeneracion = 10, tiempoConGeneracion = 5, lastCheckHour = 0, enPantallaNumero = 0, opcionNumero = 0, nuevaHora = 0, nuevoMinuto = 0, nuevoMes = 0, nuevoDia = 0, codigoDesbloqueo = 0, indPrSer = 0;
bool cargadorEnConsumoGeneral = true, conSensorGeneral = false, conFV = false, cargaPorExcedentes = false, apagarLCD = true, bloquearCargador = false, pantallaBloqueada = false, ControlPotencia = false;
bool bateriaCargada = false, permisoCarga = false, antesConectado = false, conectado = false, cargando = false, peticionCarga = false, cargaCompleta = false, ecualizando = false;
bool inicioCargaActivado = false, conTarifaValle = false, tempValorBool = false, errorCarga = false, luzLcd = true, horarioVerano = true, actualizarDatos = false, errorLimiteConsumo = false;
bool flancoBotonInicio = false, flancoBotonMas = false, flancoBotonMenos = false, flancoBotonProg = false, cambioCarga = false;
int duracionPulso = 0, tensionCargador = 0, numCiclos = 0, nuevoAnno = 0, ticksScreen = 0; //V166MA valorTipoCarga pasa de byte a int
int mediaIntensidadCargador, mediaIntensidadFV, mediaIntensidadGeneral;
int consumoCargadorAmperios = 0, generacionFVAmperios = 0, consumoGeneralAmperios = 0;
unsigned long kwTotales = 0, tempdecimasWatiosCargados = 0, CentesimasWatiosCargados = 0, acumTensionCargador = 0, acumIntensidadCargador = 0, acumIntensidadGeneral = 0, acumIntensidadFV = 0;
unsigned long tiempoInicioSesion = 0, tiempoCalculoEnergiaCargada = 0, tiempoGeneraSuficiente = 0, tiempoNoGeneraSuficiente = 0, tiempoUltimaPulsacionBoton = 0, tiempoOffBoton = 0;
unsigned long tiempoErrorCarga = 0, tiempoSinConsumoRestante = 0, tiempoConConsumoRestante = 0, tiempoEcualizando = 0, tempValorInt = 0, valorTipoCarga = 0;

DateTime timeNow;
const float factor = 0.244;
byte enheM[8] = { B01110, B00000, B10001, B11001, B10101, B10011, B10001, B00000}; //Definimos el nuevo carácter Ñ
const int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

// VARIABLES PARA EL RELOJ RTC ---------------
RTC_DS3231 rtc;

// Define various ADC prescaler--------------
const unsigned char PS_16 = (1 << ADPS2);
const unsigned char PS_32 = (1 << ADPS2) | (1 << ADPS0);
const unsigned char PS_64 = (1 << ADPS2) | (1 << ADPS1);
const unsigned char PS_128 = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

void setup(){
  //    ---------Se establece el valor del prescaler----------------
  ADCSRA &= ~PS_128;  // Eliminamos la configuración de la librería Arduino
  // Podemos elegir un prescaler entre PS_16, PS_32, PS_64 or PS_128
  ADCSRA |= PS_32;    // Configuramos el prescaler a 32

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
  inicioCargaActivado = EEPROM.read(13);
  horarioVerano = EEPROM.read(14);
  kwTotales = EEPROMReadlong(15); // Este dato ocupa 4 Bytes, direcciones 15, 16, 17 y 18.
  tiempoSinGeneracion = EEPROM.read(19);
  tiempoConGeneracion = EEPROM.read(20);
  apagarLCD = EEPROM.read(21);
  cargaPorExcedentes = EEPROM.read(22);
  bloquearCargador = EEPROM.read(23);
  ControlPotencia =  EEPROM.read(24);
  valorTipoCarga = EEPROMReadlong(25); // Este dato ocupa 4 Bytes, direcciones 25, 26, 27 y 28. V166MA
  
  lcd.begin(16, 2); //Inicialización de la Pantalla LCD
  lcd.createChar(1, enheM); //Creamos el nuevo carácter Ñ
  lcd.setBacklight(HIGH); //Encendemos la retroiluminación del display LCD
  luzLcd = true;
  pantallaBloqueada = false;
  
  Serial.begin(9600); // Iniciamos el puerto serie

  while (horaInicioCarga > 23) { // Comprobamos que el valor de la hora es válido
    lcd.setCursor(0, 0);
    lcd.print(F("     CARGAR     "));
    lcd.setCursor(0, 1);
    lcd.print(F("     PROGRAMA   "));
    delay(4000);
   }
  
  if (!rtc.begin()) { // Comprobamos la comunicación con el reloj RTC
    lcd.setCursor(0, 0);
    lcd.print(F("  SIN CONEXION  "));
    lcd.setCursor(0, 1);
    lcd.print(F("CON EL RELOJ RTC"));
    delay(4000);
    while (1);
  }
  
  Timer1.initialize(1000);         // Temporizador que activa un nuevo ciclo de onda (1000 hz)
  Timer1.attachInterrupt(GenPulsos); // Activa la interrupción y la asocia a la rutina GenPulsos
  
  lcd.setCursor(0, 0);
  lcd.print(F("    WALLBOX     "));
  lcd.setCursor(0, 1);
  lcd.print(F(" *** V 1.66MA ***"));
  delay(1500);
  
  //************** ACTIVAMOS EL MODO DE CARGA POR DEFECTO ***************
  if (!inicioCargaActivado){
    if (conFV && (conTarifaValle || (horaInicioCarga != horaFinCarga || minutoInicioCarga != minutoFinCarga))) {
      tipoCarga = INTELIGENTE;
      if (conTarifaValle) tipoCargaInteligente = TARIFA_DH;
      else tipoCargaInteligente = FRANJA_HORARIA;
    }else if (conTarifaValle)tipoCarga = TARIFA_DH;
    else if (horaInicioCarga != horaFinCarga || minutoInicioCarga != minutoFinCarga) tipoCarga = FRANJA_HORARIA;
  }
  digitalWrite(pinRegulacionCargador, HIGH);
  tiempoUltimaPulsacionBoton = millis();
}

 //********** RUTINA DE GENERACIÓN DE LA ONDA CUADRADA ***********
void GenPulsos(){
  if (permisoCarga || bateriaCargada) {            // Si hay permiso de carga ......
    digitalWrite(pinRegulacionCargador, HIGH);    // activamos el pulso ....
    delayMicroseconds(duracionPulso); // durante el tiempo que marca "Duración_Pulsos" .... 
    volatileTension = analogRead(pinTensionCargador); //leemos la tensión en el pin CP
    digitalWrite(pinRegulacionCargador, LOW);     // desactivamos el pulso ....
  }else{                  // Si no hay permiso de carga ....
    digitalWrite(pinRegulacionCargador, HIGH);     // forzamos la señal de CP a 1 ....
    volatileTension = analogRead(pinTensionCargador); //leemos la tensión en el pin CP
  }
  actualizarDatos = true;
}

void loop(){
  
  unsigned long actualMillis = millis();
  
  if (actualizarDatos){
    
    int tension = volatileTension;                // Copiamos la tensión en CP a un auxiliar
    acumTensionCargador += tension;
    
    int lectura = analogRead(pinConsumoCargador); // leemos el valor de la analógica del trafo del cargador
    if (lectura < 510) lectura = 1020 - lectura;  // convertimos el semiciclo negativo a positivo
    acumIntensidadCargador += lectura; // sumamos el valor en el acumulador
    
    lectura = analogRead(pinConsumoGeneral);
    if (lectura < 510) lectura = 1020 - lectura;
    acumIntensidadGeneral += lectura;
    
    lectura = analogRead(pinGeneracionFV);            // lectura normal de la generación fotovoltaica
    if (lectura < 510) lectura = 1020 - lectura;
    acumIntensidadFV += lectura;
                                                     
    numCiclos++;
    if (numCiclos > 499){
      
      tensionCargador = acumTensionCargador / numCiclos;
      mediaIntensidadCargador = acumIntensidadCargador / numCiclos;
      mediaIntensidadGeneral = acumIntensidadGeneral / numCiclos;
      mediaIntensidadFV = acumIntensidadFV / numCiclos;
      
      consumoCargadorAmperios = 0;
      if (permisoCarga) consumoCargadorAmperios = (mediaIntensidadCargador - 510) * factor;    // Calcula el consumo del cargador en Amperios
      
      consumoGeneralAmperios = 0;
      // if (conSensorGeneral) consumoGeneralAmperios = (mediaIntensidadGeneral - 510) * factor;     // Calcula el consumo general en Amperios
      consumoGeneralAmperios = (mediaIntensidadGeneral - 510) * factor; // V165MA
      
      generacionFVAmperios = 0;
      // if (conFV) generacionFVAmperios = (mediaIntensidadFV - 510) * factor;     // Calcula el consumo general en Amperios
      generacionFVAmperios = (mediaIntensidadFV - 510) * factor; // V165MA
      
      numCiclos = 0; acumTensionCargador = 0; acumIntensidadFV = 0, acumIntensidadCargador = 0, acumIntensidadGeneral = 0;
      
      //*********************   CONTROL DEL HORARIO VERANO/INVIERNO   *******************
      timeNow = rtc.now();
      int horaNow = timeNow.hour();
      int minutoNow = timeNow.minute();
      if (horaNow > lastCheckHour || (horaNow == 0 && lastCheckHour == 23)){
        lastCheckHour = horaNow;
        bool tempHorarioVerano = EsHorarioVerano(timeNow);
        if (horarioVerano && !tempHorarioVerano && horaNow >= 3){
          horaNow--;
          horarioVerano = false;
          rtc.adjust(DateTime(timeNow.year(), timeNow.month(), timeNow.day(), horaNow, minutoNow, timeNow.second()));
          EEPROM.write(14, horarioVerano);
        }else if (!horarioVerano && tempHorarioVerano && horaNow >= 2){
          horaNow++;
          horarioVerano = true;
          rtc.adjust(DateTime(timeNow.year(), timeNow.month(), timeNow.day(), horaNow, minutoNow, timeNow.second()));
          EEPROM.write(14, horarioVerano);
        }
      }
      //********************** CONTROL DE LA EQUALIZACIÓN ********************** 
	   if (consumoCargadorAmperios < 6) {                  //Si el consumo del cargador está por debajo de 6A. (el mínimo de carga)
		   tiempoEcualizando = actualMillis;
		   if (actualMillis - tiempoEcualizando > 20000) ecualizando = true;  //y llevamos más de 20 segundos asumimos que estamos ecualizando la bateria y evitamos parar la carga
	     if (consumoCargadorAmperios < 1) ecualizando = false;              // y cuando la carga ya se ha detenido quitamos la memoria de ecualización
	   }
		  else ecualizando = false;  // si el consumo está por encima de 6A. la carga se detendrá cuando le toque
		   	   
      //********************** CONTROL DE LA INFORMACIÓN DEL VEHÍCULO **********************
      if (tensionCargador < 550){
        if (!errorCarga && permisoCarga){
          errorCarga = true;
          permisoCarga = false;
          digitalWrite(pinAlimentacionCargador, LOW);
        }
        tiempoErrorCarga = actualMillis;
      }else if (errorCarga && actualMillis - tiempoErrorCarga > 5000){
        errorCarga = false;
      }
      conectado = (tensionCargador < 660);
      peticionCarga = (tensionCargador < 600 && !errorCarga);
      if (peticionCarga && !cargando && permisoCarga) digitalWrite(pinAlimentacionCargador, HIGH);
      cargando = peticionCarga && permisoCarga;
      
      if (bateriaCargada && peticionCarga && !inicioCargaActivado){
        inicioCargaActivado = true;
        bateriaCargada = false;
      }
      
      //*********************   CONTROL DE LA CONEXIÓN DEL CONECTOR EN EL COCHE   *******************

      if (!conectado && tipoCarga == INMEDIATA) IniciarCarga(); // V165MA
      
      if (conectado && !antesConectado){
        if (!inicioCargaActivado && (tipoCarga == INTELIGENTE || tipoCarga == FRANJA_HORARIA || tipoCarga == TARIFA_DH || tipoCarga == CONSIGNA  || tipoCarga == GENERACION_FV)) IniciarCarga(); // V166MA
        antesConectado = true;
        if (!luzLcd){
          lcd.setBacklight(HIGH);
          luzLcd = true;
          tiempoUltimaPulsacionBoton = actualMillis;
        }
      }else if (!conectado && antesConectado){
        bateriaCargada = false;
        if (inicioCargaActivado) FinalizarCarga();
        if (conFV){
          if (conTarifaValle){
            tipoCarga = INTELIGENTE;
            tipoCargaInteligente = TARIFA_DH;
          }else if (horaInicioCarga != horaFinCarga || minutoInicioCarga != minutoFinCarga){
            tipoCarga = INTELIGENTE;
            tipoCargaInteligente = FRANJA_HORARIA;
          }
        }else if (conTarifaValle)tipoCarga = TARIFA_DH;
        else if (horaInicioCarga != horaFinCarga || minutoInicioCarga != minutoFinCarga) tipoCarga = FRANJA_HORARIA;
        antesConectado = false;
        if (!luzLcd){
          lcd.setBacklight(HIGH);
          luzLcd = true;
          tiempoUltimaPulsacionBoton = actualMillis;
        }
      }
      
      //*******************   GESTIÓN DE LOS TIPOS DE CARGA   *******************
      if (conectado && inicioCargaActivado && !errorCarga){
      //*********************   ANTES DE EMPEZAR A CARGAR   ******************  
        if (!cargando && (!cargaCompleta || peticionCarga)){
          bool puedeCargar = false;
          switch(tipoCarga){
            case TARIFA_DH:
              if (EnTarifaValle(horaNow)) puedeCargar = true;
              break;
            case FRANJA_HORARIA:
              if (EnFranjaHoraria(horaNow, minutoNow))puedeCargar = true;
              break;
            case ENERGIA:
            case TIEMPO:
            case INMEDIATA:
              puedeCargar = true;
              break;
            case CONSIGNA:
              if (generacionFVAmperios > 5) puedeCargar = true;
              break;
            case GENERACION_FV:
              if (AutorizaCargaExcedentesFV(actualMillis)){
                puedeCargar = true;
              }else if (generacionFVAmperios < 2){
                FinalizarCarga();
              }
              break;
            case INTELIGENTE:
              if (conTarifaValle){
                if (EnTarifaValle(horaNow)){
                  tipoCargaInteligente = TARIFA_DH;
                  puedeCargar = true;
                }else{
                  if (AutorizaCargaExcedentesFV(actualMillis)){
                    tipoCargaInteligente = GENERACION_FV;
                    puedeCargar = true;
                  }else tipoCargaInteligente = TARIFA_DH;
                }
              }else if (horaInicioCarga != horaFinCarga || minutoInicioCarga != minutoFinCarga){
                if (EnFranjaHoraria(horaNow, minutoNow)){
                  tipoCargaInteligente = FRANJA_HORARIA;
                  puedeCargar = true;
                }else{
                  if (AutorizaCargaExcedentesFV(actualMillis)){
                    tipoCargaInteligente = GENERACION_FV;
                    puedeCargar = true;
                  }else tipoCargaInteligente = FRANJA_HORARIA;
                }
              }
              break;
          }
          if (puedeCargar){
            if (permisoCarga && CentesimasWatiosCargados > tempdecimasWatiosCargados){ // si no esta cargando y nada se lo impide y ya ha cargado algo entendemos que acabo de cargar y paramos la carga
              bateriaCargada = true;
              FinalizarCarga();
            }else{
              cargaCompleta = false;
              tempdecimasWatiosCargados = CentesimasWatiosCargados;
              if (HayPotenciaParaCargar(actualMillis)) permisoCarga = true;
            }
          }else if (permisoCarga){
            permisoCarga = false;
            digitalWrite(pinAlimentacionCargador, LOW);
          }
        }
                //****************  DURANTE LA CARGA  ***************  
        else if (cargando){
          CalcularEnergias(actualMillis);
		  if (!ecualizando){     // Si no estamos ecualizando vamos comprobando las lógicas de carga o parada
			  switch(tipoCarga){
				case TARIFA_DH:
				  if (!EnTarifaValle(horaNow)) FinalizarCarga();
				  break;
				case FRANJA_HORARIA:
				  if (!EnFranjaHoraria(horaNow, minutoNow)) FinalizarCarga();
				  break;
				case ENERGIA:
				  if (CentesimasWatiosCargados / 100 >= valorTipoCarga) FinalizarCarga();
				  break;
				case TIEMPO:
				  if ((actualMillis - tiempoInicioSesion) >= (valorTipoCarga * 60000l)) FinalizarCarga();
				  break;
				case CONSIGNA:
				  if (generacionFVAmperios < 5) permisoCarga = false;
				  break;
				case GENERACION_FV:
				  if (!AutorizaCargaExcedentesFV(actualMillis)){
					if (generacionFVAmperios < 2) FinalizarCarga();
					else permisoCarga = false;
				  }
				  break;
				case INTELIGENTE:
				  switch (tipoCargaInteligente){
					case GENERACION_FV:
					  if (conTarifaValle){
						if (EnTarifaValle(horaNow)) tipoCargaInteligente = TARIFA_DH;
					  }else if (horaInicioCarga != horaFinCarga || minutoInicioCarga != minutoFinCarga){
						if (EnFranjaHoraria(horaNow, minutoNow)) tipoCargaInteligente = FRANJA_HORARIA;
					  }
					  if (tipoCargaInteligente == GENERACION_FV && !AutorizaCargaExcedentesFV(actualMillis)) permisoCarga = false;
					  break;
					case TARIFA_DH:
					  if (!EnTarifaValle(horaNow)){
						if (AutorizaCargaExcedentesFV(actualMillis))tipoCargaInteligente = GENERACION_FV;
						else permisoCarga = false;
					  }
					  break;
					case FRANJA_HORARIA:
					  if (!EnFranjaHoraria(horaNow, minutoNow) && !AutorizaCargaExcedentesFV(actualMillis)) permisoCarga = false;
					  if (!EnFranjaHoraria(horaNow, minutoNow)){
						if (AutorizaCargaExcedentesFV(actualMillis))tipoCargaInteligente = GENERACION_FV;
						else permisoCarga = false;
					  }
					  break;
				  }
				  break;
			  }
		  }
          if (permisoCarga){
            if (!HayPotenciaParaCargar(actualMillis)){
              permisoCarga = false;
            }
          }
          if (!permisoCarga) digitalWrite(pinAlimentacionCargador, LOW);
        }
      }
    }
    actualizarDatos = false;
  }
  
	// ********************* CONTROL DE LAS PULSACIONES EN EL TECLADO ************************
  if (digitalRead(pinPulsadorInicio) == HIGH){
    if (!flancoBotonInicio && (actualMillis - tiempoOffBoton > 100)){
      ProcesarBoton(BOTONINICIO);
      flancoBotonInicio = true;
    }
  }else if (flancoBotonInicio){
    flancoBotonInicio = false;
    tiempoOffBoton = actualMillis;
  }
  if (digitalRead(pinPulsadorProg) == HIGH){
    if (!flancoBotonProg && (actualMillis - tiempoOffBoton > 100)){
      ProcesarBoton(BOTONPROG);
      flancoBotonProg = true;
    }
  }else if (flancoBotonProg){
    flancoBotonProg = false;
    tiempoOffBoton = actualMillis;
  }
  if (digitalRead(pinPulsadorMas) == HIGH){
    if (!flancoBotonMas && (actualMillis - tiempoOffBoton > 100)){
      ProcesarBoton(BOTONMAS);
      flancoBotonMas = true;
    }
  }else if (flancoBotonMas){
    flancoBotonMas = false;
    tiempoOffBoton = actualMillis;
  }
  if (digitalRead(pinPulsadorMenos) == HIGH){
    if (!flancoBotonMenos && (actualMillis - tiempoOffBoton > 100)){
      ProcesarBoton(BOTONMENOS);
      flancoBotonMenos = true;
    }
  }else if (flancoBotonMenos){
    flancoBotonMenos = false;
    tiempoOffBoton = actualMillis;
  }
  
  if (luzLcd){
    if (tiempoUltimaPulsacionBoton > actualMillis) tiempoUltimaPulsacionBoton = actualMillis;
    if (actualMillis - tiempoUltimaPulsacionBoton >= 600000l){
      if (apagarLCD || bloquearCargador){
        luzLcd = false;
        enPantallaNumero = 0;
        lcd.clear();
        lcd.setBacklight(LOW);
        if (bloquearCargador) pantallaBloqueada = true;
      }
    }
  }
  ticksScreen++;
  if (ticksScreen >= 3000){
    if (luzLcd && (enPantallaNumero == 0 || enPantallaNumero == 10)) updateScreen();
    MonitorizarDatos();
    ticksScreen = 0;
  }
}

// ******************* RUTINA DE PROCESAMIENTO DE LOS MENÚS ******************
void ProcesarBoton(int button){
  unsigned long tempMillis = millis();
  if (luzLcd && !pantallaBloqueada){
    switch(enPantallaNumero){
      case 0:   // Pantalla principal
        switch (button){
          case BOTONINICIO:
            if (inicioCargaActivado){
              opcionNumero = 0;
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
      case 1:   //Pantalla selección configuración
        switch (button){
          case BOTONINICIO:
            if (opcionNumero == 0){
              enPantallaNumero = 11;
            }else if (opcionNumero == 1){
              enPantallaNumero = 10;
            }else{
              DateTime timeTemp  = rtc.now();
              nuevoAnno = timeTemp.year();
              if (nuevoAnno < 2017) nuevoAnno = 2017;
              nuevoMes = timeTemp.month();
              nuevoDia = timeTemp.day();
              nuevaHora = timeTemp.hour();
              nuevoMinuto = timeTemp.minute();
              enPantallaNumero = 130;
            }
            opcionNumero = 0;
            break;
          case BOTONMAS:
            (opcionNumero >= 2) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero <= 0) ? opcionNumero = 2 : opcionNumero--;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 2:    // Selección del tipo de carga
        switch (button){
          case BOTONINICIO:
            switch (opcionNumero){
              case TARIFA_DH:
                tipoCarga = TARIFA_DH;
                IniciarCarga();
                enPantallaNumero = 0;
                break;
              case FRANJA_HORARIA:
                tipoCarga = FRANJA_HORARIA;
                IniciarCarga();
                enPantallaNumero = 0;
                break;
              case ENERGIA:
                enPantallaNumero = 20;
                if (tipoCarga == ENERGIA) tempValorInt = valorTipoCarga;
                else tempValorInt = 500;
                break;
              case TIEMPO:
                enPantallaNumero = 21;
                if (tipoCarga == TIEMPO) tempValorInt = valorTipoCarga;
                else tempValorInt = 30;
                break;
              case INMEDIATA:
                tipoCarga = INMEDIATA;
                IniciarCarga();
                enPantallaNumero = 0;
                break;
              case CONSIGNA:
                tipoCarga = CONSIGNA;
                IniciarCarga();
                enPantallaNumero = 0;
                break;
              case GENERACION_FV:
                tipoCarga = GENERACION_FV;
                IniciarCarga();
                enPantallaNumero = 0;
                break;
              case INTELIGENTE:
                tipoCarga = INTELIGENTE;
                IniciarCarga();
                enPantallaNumero = 0;
                break;
            }
            break;
          case BOTONMAS:
            ((!conFV && opcionNumero >= 4) || opcionNumero >= 6) ? opcionNumero = 0 : opcionNumero++; // V166MA
            break;
          case BOTONMENOS:
            (opcionNumero <= 0) ? (!conFV) ? opcionNumero = 4 : opcionNumero = 6 : opcionNumero--; // V166MA
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            cambioCarga = false;
            break;
        }
        updateScreen();
        break;
      case 3:   //Pantalla ajuste intensidad programada
        switch (button){
          case BOTONINICIO:
            intensidadProgramada = tempValorInt;
            EEPROM.write(2, intensidadProgramada);
            enPantallaNumero = 0;
            break;
          case BOTONMAS:
            (tempValorInt >= 32) ? tempValorInt = 6 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 6) ? tempValorInt = 32 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 4:    //Pantalla opcion: fin carga ó cambiar carga
        switch (button){
          case BOTONINICIO:
            if (opcionNumero == 0){
              enPantallaNumero = 30;
              tempValorBool = false;
            }else{
              enPantallaNumero = 2;
              opcionNumero = tipoCarga;
              cambioCarga = true;
            }
            break;
          case BOTONMAS:
            (opcionNumero >= 1) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero <= 0) ? opcionNumero = 1 : opcionNumero--;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 10:    //Pantalla visualización datos
        switch (button){
          case BOTONINICIO:
            if (opcionNumero == 0){
              enPantallaNumero = 100;
              tempValorBool = false;
            }else enPantallaNumero = 1;
            opcionNumero = 0;
            break;
          case BOTONMAS:
            (opcionNumero >= 9) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero <= 0) ? opcionNumero = 9 : opcionNumero--;
            break;          
          case BOTONPROG:
            enPantallaNumero = 1;
            opcionNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 11:    //Pantalla ajuste configuración
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
                tempValorBool = cargaPorExcedentes;
                break;
              case 10:
                enPantallaNumero = 120;
                tempValorBool = conTarifaValle;
                break;
              case 11:
                enPantallaNumero = 121;
                tempValorInt = consumoTotalMax;
                break;
              case 12:
                enPantallaNumero = 122;
                tempValorInt = tiempoSinGeneracion;
                break;
              case 13:
                enPantallaNumero = 123;
                tempValorInt = tiempoConGeneracion;
                break;
              case 14:
                enPantallaNumero = 124;
                tempValorBool = ControlPotencia;
                break;
              case 15:
                enPantallaNumero = 125;
                tempValorBool = apagarLCD;
                break;
              case 16:
                enPantallaNumero = 126;
                tempValorBool = bloquearCargador;
                break;
            }
            break;
          case BOTONMAS:
            (opcionNumero >= 16) ? opcionNumero = 0 : opcionNumero++;
            break;
          case BOTONMENOS:
            (opcionNumero <= 0) ? opcionNumero = 16 : opcionNumero--;
            break;
          case BOTONPROG:
            enPantallaNumero = 1;
            opcionNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 20:    // Selección de Energía a cargar
        switch (button){
          case BOTONINICIO:
            tipoCarga = ENERGIA;
            valorTipoCarga = tempValorInt;
            IniciarCarga();
			EEPROMWritelong(25, valorTipoCarga); // V166MA
            enPantallaNumero = 0;
            break;
          case BOTONMAS:
            if (tempValorInt < 20000) tempValorInt += 500;
            break;
          case BOTONMENOS:
            if (tempValorInt > 500) tempValorInt -= 500;
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
            tipoCarga = TIEMPO;
            valorTipoCarga = tempValorInt;
            IniciarCarga();
            EEPROM.write(12, valorTipoCarga);
            enPantallaNumero = 0;
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
      case 30:   //Pantalla finalización carga
        switch (button){
          case BOTONINICIO:
            if (tempValorBool) FinalizarCarga();
            enPantallaNumero = 0;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorBool  = (tempValorBool) ? false : true;
            break;
          case BOTONPROG:
            enPantallaNumero = 0;
            break;
        }
        updateScreen();
        break;
      case 100:   //Pantalla reseteo Energía acumulada
        switch (button){
          case BOTONINICIO:
            if (tempValorBool){
              kwTotales = 0;
              EEPROM.write(15, 0);
              EEPROM.write(16, 0);
              EEPROM.write(17, 0);
              EEPROM.write(18, 0);
            }
            enPantallaNumero = 1;
            break;
          case BOTONMAS:
          case BOTONMENOS:
            tempValorBool = (tempValorBool) ? false : true;
            break;
          case BOTONPROG:
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 110:   //Pantalla ajuste hora inicio carga
        switch (button){
          case BOTONINICIO:
            horaInicioCarga = tempValorInt;
            EEPROM.write(0, horaInicioCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 23) ? tempValorInt = 0 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 0) ? tempValorInt = 23 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 111:   //Pantalla ajuste minuto inicio carga
        switch (button){
          case BOTONINICIO:
            minutoInicioCarga = tempValorInt;
            EEPROM.write(1, minutoInicioCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 59) ? tempValorInt = 0 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 0) ? tempValorInt = 59 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 112:   //Pantalla ajuste hora fin carga
        switch (button){
          case BOTONINICIO:
            horaFinCarga = tempValorInt;
            EEPROM.write(4, horaFinCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 23) ? tempValorInt = 0 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 0) ? tempValorInt = 23 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 113:   //Pantalla ajuste minuto fin carga
        switch (button){
          case BOTONINICIO:
            minutoFinCarga = tempValorInt;
            EEPROM.write(5, minutoFinCarga);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 59) ? tempValorInt = 0 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 0) ? tempValorInt = 59 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 114:   //Pantalla ajuste con sensor consumo general
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
      case 115:   //Pantalla ajuste intensidad programada
        switch (button){
          case BOTONINICIO:
            intensidadProgramada = tempValorInt;
            EEPROM.write(2, intensidadProgramada);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 36) ? tempValorInt = 6 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 6) ? tempValorInt = 36 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 116:   //Pantalla ajuste cargador en consumo general de la vivienda
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
      case 117:   //Pantalla ajuste con generación FV
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
      case 118:   //Pantalla ajuste generación mínima
        switch (button){
          case BOTONINICIO:
            generacionMinima = tempValorInt;
            EEPROM.write(8, generacionMinima);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 32) ? tempValorInt = 2 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 2) ? tempValorInt = 32 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 119:   //Pantalla ajuste Carga por Excedentes
        switch (button){
          case BOTONINICIO:
            cargaPorExcedentes = tempValorBool;
            EEPROM.write(22, cargaPorExcedentes);
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
      case 120:   //Pantalla ajuste con tarifa valle
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
      case 121:   //Pantalla ajuste consumo máximo total
        switch (button){
          case BOTONINICIO:
            consumoTotalMax = tempValorInt;
            EEPROM.write(3, consumoTotalMax);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 63) ? tempValorInt = 10 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 10) ? tempValorInt = 63 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 122:   //Pantalla ajuste tiempo sin generación
        switch (button){
          case BOTONINICIO:
            tiempoSinGeneracion = tempValorInt;
            EEPROM.write(19, tiempoSinGeneracion);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 60) ? tempValorInt = 0 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 0) ? tempValorInt = 60 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break;
      case 123:   //Pantalla ajuste tiempo con generación
        switch (button){
          case BOTONINICIO:
            tiempoConGeneracion = tempValorInt;
            EEPROM.write(20, tiempoConGeneracion);
            enPantallaNumero = 11;
            break;
          case BOTONMAS:
            (tempValorInt >= 60) ? tempValorInt = 0 : tempValorInt++;
            break;
          case BOTONMENOS:
            (tempValorInt <= 0) ? tempValorInt = 60 : tempValorInt--;
            break;
          case BOTONPROG:
            enPantallaNumero = 11;
            break;
        }
        updateScreen();
        break; 		
	    case 124:   //Pantalla Control de Potencia
        switch (button){
          case BOTONINICIO:
            ControlPotencia = tempValorBool;
            EEPROM.write(24, ControlPotencia);
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
	    case 125:   //Pantalla ajuste apagar LCD
        switch (button){
          case BOTONINICIO:
            apagarLCD = tempValorBool;
            EEPROM.write(21, apagarLCD);
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
	    case 126:   //Pantalla Bloquear Cargador
        switch (button){
          case BOTONINICIO:
            bloquearCargador = tempValorBool;
            EEPROM.write(23, bloquearCargador);
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
      case 130:    // Ajuste del año
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = 131;
            break;
          case BOTONMAS:
            if (nuevoAnno < 2050) nuevoAnno++;
            break;
          case BOTONMENOS:
            if (nuevoAnno > 2017) nuevoAnno--;
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
            (nuevoMes >= 12) ? nuevoMes = 1 : nuevoMes++;
            break;
          case BOTONMENOS:
            (nuevoMes <= 1) ? nuevoMes = 12 : nuevoMes--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 132:    //Ajuste del día
        switch (button){
          case BOTONINICIO:
            enPantallaNumero = 133;
            break;
          case BOTONMAS:
            {
              byte dias = (nuevoMes == 2) ? 28 + AnnoBisiesto(nuevoAnno) : daysInMonth[nuevoMes - 1];
              (nuevoDia >= dias) ? nuevoDia = 1 : nuevoDia++;
              break;
            }
          case BOTONMENOS:
            {
              byte dias = (nuevoMes == 2) ? 28 + AnnoBisiesto(nuevoAnno) : daysInMonth[nuevoMes - 1];
              (nuevoDia <= 1) ? nuevoDia = dias : nuevoDia--;
              break;
            }
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
            (nuevaHora >= 23) ? nuevaHora = 0 : nuevaHora++;
            break;
          case BOTONMENOS:
            (nuevaHora <= 0) ? nuevaHora = 23 : nuevaHora--;
            break;
          case BOTONPROG:
            opcionNumero = 2;
            enPantallaNumero = 1;
            break;
        }
        updateScreen();
        break;
      case 134:    //Ajuste de los minutos
        switch (button){
          case BOTONINICIO:
            rtc.adjust(DateTime(nuevoAnno, nuevoMes, nuevoDia, nuevaHora, nuevoMinuto, 0));
            horarioVerano = EsHorarioVerano(DateTime(nuevoAnno, nuevoMes, nuevoDia, nuevaHora, nuevoMinuto, 0));
            EEPROM.write(14, horarioVerano);
            enPantallaNumero = 135;
            break;
          case BOTONMAS:
            (nuevoMinuto >= 59) ? nuevoMinuto = 0 : nuevoMinuto++;
            break;
          case BOTONMENOS:
            (nuevoMinuto <= 0) ? nuevoMinuto = 59 : nuevoMinuto--;
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
    if (!luzLcd){
      updateScreen();
      luzLcd = true;
      lcd.setBacklight(HIGH);
    }else if (pantallaBloqueada){ // Código de desbloqueo " PROG - INICIO - MAS - MENOS "
      switch (button){
        case BOTONINICIO:
          (codigoDesbloqueo == 1 && (tempMillis - tiempoUltimaPulsacionBoton) < 2000) ? codigoDesbloqueo++ : codigoDesbloqueo = 0;
          break;
        case BOTONMAS:
          (codigoDesbloqueo == 2 && (tempMillis - tiempoUltimaPulsacionBoton) < 2000) ? codigoDesbloqueo++ : codigoDesbloqueo = 0;
          break;
        case BOTONMENOS:
          (codigoDesbloqueo == 3 && (tempMillis - tiempoUltimaPulsacionBoton) < 2000) ? codigoDesbloqueo++ : codigoDesbloqueo = 0;
          if (codigoDesbloqueo == 4){
            codigoDesbloqueo = 0;
            pantallaBloqueada = false;
            updateScreen();
          }
          break;
        case BOTONPROG:
          codigoDesbloqueo = 1;
          break;
      }
    }
  }
  tiempoUltimaPulsacionBoton = tempMillis;
}

void updateScreen(){
  switch(enPantallaNumero){
    case 0:   //Pantalla principal
      lcd.setCursor(0, 0);
      if (pantallaBloqueada){
        lcd.print(F("PANT. BLOQUEADA."));
        String hora = (timeNow.hour() < 10) ? "0" + (String)timeNow.hour() : (String)timeNow.hour();
        hora += ":";
        hora += (timeNow.minute() < 10) ? "0" + (String)timeNow.minute() : (String)timeNow.minute();
        lcd.setCursor(0, 1);
        lcd.print(F("      "));
        lcd.print(hora);
        lcd.print(F("     "));
      }else{
        MostrarTipoCarga();
        if (cargaCompleta){
          lcd.setCursor(0, 1);
          lcd.print(F("CARGADO "));
		if (CentesimasWatiosCargados < 100000l){
          lcd.print(CentesimasWatiosCargados / 100);
          lcd.print(F(" Wh    ")); // Hasta 1 KWh cargados mostramos los watios hora
        }else{
          float kWhCargados = (float (CentesimasWatiosCargados) / 100000l);
          lcd.print(kWhCargados,2);
          lcd.setCursor(12,1);
          lcd.print(F(" kWh   ")); // A partir de 1 KWh cargado mostramos los kilowatios hora
         }
		}else if (inicioCargaActivado){
          if (errorCarga){
            lcd.setCursor(0, 1);
            lcd.print(F("ERROR DE CARGA. "));
          }else if (errorLimiteConsumo){
            lcd.setCursor(0, 1);
            lcd.print(F("ESPERA POTENCIA "));
          }else if (permisoCarga){
            if (cargando){
              MostrarPantallaCarga();
            }else{
              lcd.setCursor(0, 1);
              lcd.print(F("ESP. COND. COCHE"));
            }
          }else if (conectado){
            switch (tipoCarga){
              case GENERACION_FV:
                lcd.setCursor(0, 1);
                lcd.print(F("GEN. FV INSUFIC."));
                break;
              case CONSIGNA:
                lcd.setCursor(0, 1);
                lcd.print(F("CONSIGNA INSUFIC."));
                break;
              case INTELIGENTE:
                lcd.setCursor(0, 1);
                lcd.print(F("INI.CARG:"));
                if (conTarifaValle)tempValorInt = (horarioVerano) ? 1380 : 1320;
                else tempValorInt = (horaInicioCarga * 60) + minutoInicioCarga;
                MostrarTiempoRestante(tempValorInt);
                break;
              case FRANJA_HORARIA:
                lcd.setCursor(0, 1);
                lcd.print(F("INI.CARG:"));
                tempValorInt = (horaInicioCarga * 60) + minutoInicioCarga;
                MostrarTiempoRestante(tempValorInt);
                break;
              case TARIFA_DH:
                lcd.setCursor(0, 1);
                lcd.print(F("INI.CARG:"));
                tempValorInt = (horarioVerano) ? 1380 : 1320;
                MostrarTiempoRestante(tempValorInt);
                break;
              case TIEMPO:
              case INMEDIATA:
              case ENERGIA:
                MostrarPantallaCarga();
                break;
            }
          }else{
            lcd.setCursor(0, 1);
            lcd.print(F("COCHE NO CONECT."));
          }
        }else if (conectado){
          lcd.setCursor(0, 1);
          lcd.print(F("COCHE CONECTADO "));
        }else{
          String hora = (timeNow.hour() < 10) ? "0" + (String)timeNow.hour() : (String)timeNow.hour();
          hora += ":";
          hora += (timeNow.minute() < 10) ? "0" + (String)timeNow.minute() : (String)timeNow.minute();
          lcd.setCursor(0, 1);
          lcd.print(F("      "));
          lcd.print(hora);
          lcd.print(F("     "));
        }
      }
      break;
    case 1:   //Pantalla selección configuración
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("OPCIONES:"));
      lcd.setCursor(0, 1);
      switch (opcionNumero){
        case 0:
          lcd.print(F("CONFIGURACION"));
          break;
        case 1:
          lcd.print(F("VISUALIZACION"));
          break;
        case 2:
          lcd.print(F("PUESTA EN HORA"));
          break;
      }
      break;
    case 2:    // pantalla tipo de carga
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("TIPO DE CARGA:"));
      lcd.setCursor(0, 1);
      switch (opcionNumero){
        case TARIFA_DH:
          lcd.print(F("TARIFA VALLE"));
          break;
        case FRANJA_HORARIA:
          lcd.print(F("POR HORARIO"));
          break;
        case ENERGIA:
          lcd.print(F("POR ENERGIA"));
          break;
        case TIEMPO:
          lcd.print(F("POR TIEMPO"));
          break;
        case INMEDIATA:
          lcd.print(F("INMEDIATA"));
          break;
        case CONSIGNA:
          lcd.print(F("CONSIGNA EXT."));
          break;
        case GENERACION_FV:
          lcd.print(F("EXCEDENTES FV"));
          break;
        case INTELIGENTE:
          lcd.print(F("INTELIGENTE"));
          break;
      }
      break;
    case 3:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("INTENSIDAD CARGA"));
      lcd.setCursor(7, 1);
      if (tempValorInt < 10)lcd.print(F(" "));
      lcd.print(tempValorInt);
      lcd.print(F(" A"));
      break;
    case 4:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("SELEC. OPCION:"));
      lcd.setCursor(0, 1);
      (opcionNumero == 0) ? lcd.print(F("FINALIZAR CARGA")) : lcd.print(F("CAMBIAR T. CARGA"));
      break;
    case 10:
      lcd.setCursor(0, 0);
      switch (opcionNumero){
        case 0:
          lcd.print(F("CARGA ACUMULADA:"));
          lcd.setCursor(0, 1);
          lcd.print(F("    "));
          lcd.print(kwTotales / 100000);
          lcd.print(F(" KWh          "));
          break;
        case 1:
          lcd.print(F("CARGA INICIADA: "));
          lcd.setCursor(0, 1);
          (inicioCargaActivado) ? lcd.print(F("       SI       ")) : lcd.print(F("       NO       "));
          break;
        case 2:
          lcd.print(F("COCHE CONECTADO:"));
          lcd.setCursor(0, 1);
          (conectado) ? lcd.print(F("       SI       ")) : lcd.print(F("       NO       "));
          break;
        case 3:
          lcd.print(F("COCHE CARGANDO: "));
          lcd.setCursor(0, 1);
          (cargando) ? lcd.print(F("       SI       ")) : lcd.print(F("       NO       "));
          break;
        case 4:
          lcd.print(F("CONSIGNA CARGA: "));
          lcd.setCursor(0, 1);
          lcd.print(F("       "));
          if (intensidadProgramada < 10)lcd.print(F(" "));
          lcd.print(intensidadProgramada);
          lcd.print(F(" A     "));
          break;
        case 5:
          lcd.print(F("VEH. CARGANDO A:"));
          lcd.setCursor(0, 1);
          lcd.print(F("   "));
          if (consumoCargadorAmperios < 10)lcd.print(F(" "));
          lcd.print(consumoCargadorAmperios);
          lcd.print(F(" A    ("));
          lcd.print(mediaIntensidadCargador); // se añade la lectura media del pin A3
          lcd.print(F(")    "));
          break;
        case 6:
          lcd.print(F("EXCEDENTES FV:  "));
          lcd.setCursor(0, 1);
          lcd.print(F("    "));
          if (generacionFVAmperios < 10)lcd.print(F(" "));
          lcd.print(generacionFVAmperios);
          lcd.print(F(" A ("));
          lcd.print(mediaIntensidadFV); // se añade la lectura media del pin A0
          lcd.print(F(")    "));
          (HayExcedentesFV()) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 7:
          lcd.print(F("CONSUMO GENERAL:"));
          lcd.setCursor(0, 1);
          lcd.print(F("   "));
          if (consumoGeneralAmperios < 10)lcd.print(F(" "));
          lcd.print(consumoGeneralAmperios);
          lcd.print(F(" A ("));
          lcd.print(mediaIntensidadGeneral); // se añade la lectura media del pin A1
          lcd.print(F(")   "));
          break;
        case 8:
          lcd.print(F("TENSION MEDIDA: "));
          lcd.setCursor(0, 1);
          lcd.print(F("      "));
          lcd.print(tensionCargador);
          lcd.print(F("       "));
          break;
        case 9:
          lcd.print(F("DURACION PULSO: "));
          lcd.setCursor(0, 1);
          lcd.print(F("      "));
          lcd.print(duracionPulso);
          if (duracionPulso < 100) lcd.print(F(" "));
          lcd.print(F("    "));
          if (permisoCarga || bateriaCargada) lcd.print(F("ON "));
          else lcd.print(F("OFF"));
          break;
      }
      break;
    case 11:
      lcd.clear();
      lcd.setCursor(0, 0);
      switch (opcionNumero){
        case 0:
          lcd.print(F("HORA INI CARGA:"));
          lcd.setCursor(7, 1);
          if (horaInicioCarga < 10)lcd.print(F("0"));
          lcd.print(horaInicioCarga);
          break;
        case 1:
          lcd.print(F("MINU. INI CARGA:"));
          lcd.setCursor(7, 1);
          if (minutoInicioCarga < 10)lcd.print(F("0"));
          lcd.print(minutoInicioCarga);
          break;
        case 2:
          lcd.print(F("HORA FIN CARGA:"));
          lcd.setCursor(7, 1);
          if (horaFinCarga < 10)lcd.print(F("0"));
          lcd.print(horaFinCarga);
          break;
        case 3:
          lcd.print(F("MINU. FIN CARGA:"));
          lcd.setCursor(7, 1);
          if (minutoFinCarga < 10)lcd.print(F("0"));
          lcd.print(minutoFinCarga);
          break;
        case 4:
          lcd.print(F("TRAFO GENERAL:  "));
          lcd.setCursor(7, 1);
          (conSensorGeneral) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 5:
          lcd.print(F("INTENSID. CARGA:"));
          lcd.setCursor(6, 1);
          if (intensidadProgramada < 10)lcd.print(F(" "));
          lcd.print(intensidadProgramada);
          lcd.print(F(" A"));
          break;
        case 6:
          lcd.print(F("CARGADOR EN T.G:"));
          lcd.setCursor(7, 1);
          (cargadorEnConsumoGeneral) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 7:
          lcd.print(F("TRAFO GENER. FV:"));
          lcd.setCursor(7, 1);
          (conFV) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 8:
          lcd.print(F("INTENS. MIN. FV:"));
          lcd.setCursor(6, 1);
          if (generacionMinima < 10)lcd.print(F(" "));
          lcd.print(generacionMinima);
          lcd.print(F(" A"));
          break;
        case 9:
          lcd.print(F("CARGA POR EXCED:"));
          lcd.setCursor(7, 1);
          (cargaPorExcedentes) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 10:
          lcd.print(F("TARIFA DISC HOR:"));
          lcd.setCursor(7, 1);
          (conTarifaValle) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 11:
          lcd.print(F("CONSU TOTAL MAX:"));
          lcd.setCursor(6, 1);
          if (consumoTotalMax < 10)lcd.print(F(" "));
          lcd.print(consumoTotalMax);
          lcd.print(F(" A"));
          break;
        case 12:
          lcd.print(F("TIEMPO SIN F.V.:"));
          lcd.setCursor(6, 1);
          if (tiempoSinGeneracion < 10)lcd.print(F(" "));
          lcd.print(tiempoSinGeneracion);
          lcd.print(F(" min"));
          break;
        case 13:
          lcd.print(F("TIEMPO CON F.V.:"));
          lcd.setCursor(6, 1);
          if (tiempoConGeneracion < 10)lcd.print(F(" "));
          lcd.print(tiempoConGeneracion);
          lcd.print(F(" min"));
          break;
        case 14:
          lcd.print(F("CTRL. POTENCIA: "));
          lcd.setCursor(7, 1);
          (ControlPotencia) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;   
        case 15:
          lcd.print(F("APAGAR LCD:     "));
          lcd.setCursor(7, 1);
          (apagarLCD) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
        case 16:
          lcd.print(F("BLOQ. CARGADOR: "));
          lcd.setCursor(7, 1);
          (bloquearCargador) ? lcd.print(F("SI")) : lcd.print(F("NO"));
          break;
      }
      break;
    case 20:  // Carga por Energía
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("ENERGIA:"));
      lcd.setCursor(5, 1);
      if (tempValorInt < 10)lcd.print(F(" "));
      lcd.print(tempValorInt);
      lcd.print(F(" W"));
      break;
    case 21:  // carga por franja de tiempo
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("TIEMPO:"));
      lcd.setCursor(5, 1);
      if (tempValorInt < 100)lcd.print(F(" "));
      lcd.print(tempValorInt);
      lcd.print(F(" MIN"));
      break;
    case 30:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("FINALIZAR CARGA:"));
      lcd.setCursor(7, 1);
      (tempValorBool) ? lcd.print(F("SI")) : lcd.print(F("NO"));
      break;
    case 100:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("RESET POT. ACUM."));
      lcd.setCursor(7, 1);
      (tempValorBool) ? lcd.print(F("SI")) : lcd.print(F("NO"));
      break;
    case 110:
    case 111:
    case 112:
    case 113:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE:"));
      lcd.setCursor(7, 1);
      if (tempValorInt < 10)lcd.print(F("0"));
      lcd.print(tempValorInt);
      break;
    case 114:
    case 116:
    case 117:
    case 119:
    case 120:
    case 124:
    case 125:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE:"));
      lcd.setCursor(7, 1);
      (tempValorBool) ? lcd.print(F("SI")) : lcd.print(F("NO"));
      break;
    case 115:
    case 118:
    case 121:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE:"));
      lcd.setCursor(6, 1);
      if (tempValorInt < 10)lcd.print(F(" "));
      lcd.print(tempValorInt);
      lcd.print(F(" A"));
      break;
    case 122:
    case 123:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE:"));
      lcd.setCursor(6, 1);
      if (tempValorInt < 10)lcd.print(F(" "));
      lcd.print(tempValorInt);
      lcd.print(F(" min."));
      break;
      case 130:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE A")); lcd.write(1); lcd.print(F("O:"));
      lcd.setCursor(5, 1);
      lcd.print(nuevoAnno);
      break;
    case 131:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE MES:"));
      lcd.setCursor(7, 1);
      if (nuevoMes < 10)lcd.print(F(" "));
      lcd.print(nuevoMes);
      break;
    case 132:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE DIA:"));
      lcd.setCursor(7, 1);
      if (nuevoDia < 10)lcd.print(F(" "));
      lcd.print(nuevoDia);
      break;
    case 133:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE HORA:"));
      lcd.setCursor(7, 1);
      if (nuevaHora < 10)lcd.print(F(" "));
      lcd.print(nuevaHora);
      break;
    case 134:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("AJUSTE MINUTO"));
      lcd.setCursor(7, 1);
      if (nuevoMinuto < 10)lcd.print(F(" "));
      lcd.print(nuevoMinuto);
      break;
    case 135:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("  AJUSTE RELOJ  "));
      lcd.setCursor(0, 1);
      lcd.print(F("    CORRECTO    "));
      break;
  }  
}

// ******************* RUTINA DE INICIO DE CARGA ******************
void IniciarCarga(){
  if (!cambioCarga){
    CentesimasWatiosCargados = 0;
    cargaCompleta = false;
    inicioCargaActivado = true;
    bateriaCargada = false;
    EEPROM.write(13, inicioCargaActivado);
    cambioCarga = false;
  }else cambioCarga = false;
  EEPROM.write(11, tipoCarga);
}

// ******************* RUTINA DE FINALIZACIÓN DE CARGA ******************
void FinalizarCarga(){
  digitalWrite(pinAlimentacionCargador, LOW);
  cargaCompleta = false;
  if (CentesimasWatiosCargados > 0) cargaCompleta = true;
  permisoCarga = false;
  inicioCargaActivado = false;
  tiempoInicioSesion = 0;
  tiempoGeneraSuficiente = 0;
  tiempoNoGeneraSuficiente = 0;
  tiempoSinConsumoRestante = 0;
  tiempoConConsumoRestante = 0;
  errorLimiteConsumo = false;
  EEPROM.write(13, inicioCargaActivado);
  EEPROMWritelong(15, kwTotales);
}

void MostrarPantallaCarga(){
  if (tipoCarga == INTELIGENTE) MostrarTipoCarga();
  lcd.setCursor(0, 1);
  lcd.print(F("CARGA:"));
  if (consumoCargadorAmperios < 10)lcd.print(F(" "));
  lcd.print(consumoCargadorAmperios);
  lcd.print(F("A "));
  if (CentesimasWatiosCargados < 100000l){ // Hasta 1 KWh cargados mostramos los watios hora
  lcd.print(CentesimasWatiosCargados / 100);
  lcd.print(F("Wh   "));
  }else{ // A partir de 1 KWh cargado mostramos los kilowatios hora
   float kWhCargados = (float (CentesimasWatiosCargados) / 100000l);
   lcd.print(kWhCargados,1);
   if (CentesimasWatiosCargados < 100000l) { // Por debajo de 10kWh mostramos 1 decimal
   lcd.setCursor(13,1);	  
   lcd.print(F("kWh"));
   }else{ // Por encima de 10 kWh no mostramos decimales
    lcd.setCursor(12,1);	  
    lcd.print(F("kWh "));
   }
  }	
}

void MostrarTiempoRestante(int minutosRestantes){
  int minutosNow = (timeNow.hour() * 60) + timeNow.minute();
  if (minutosRestantes < minutosNow) minutosRestantes += 1440;
  minutosRestantes -= minutosNow;
  int horas = minutosRestantes / 60;
  int minutos = minutosRestantes % 60;
  if (horas < 10) lcd.print(F(" "));
  lcd.print(horas);
  lcd.print(F("h "));
  if (minutos < 10) lcd.print(F(" "));
  lcd.print(minutos);
  lcd.print(F("m"));
}

void MostrarTipoCarga(){
  switch (tipoCarga){
    case TARIFA_DH:
      lcd.print(F("TC:  TARIFA D.H."));
      break;
    case FRANJA_HORARIA:
      lcd.print(F("TC:   F. HORARIA"));
      break;
    case INMEDIATA:
      lcd.print(F("TC:    INMEDIATA"));
      break;
	case CONSIGNA:
      lcd.print(F("TC: CONSIG. EXT."));
      break;  
    case ENERGIA:
      lcd.print(F("TC:      ENERGIA"));
      break;
    case TIEMPO:
      lcd.print(F("TC:       TIEMPO"));
      break;
    case GENERACION_FV:
      lcd.print(F("TC: EXCEDENT. FV"));
      break;
    case INTELIGENTE:
      lcd.print(F("TC: INTELIG. "));
      if (tipoCargaInteligente == GENERACION_FV) lcd.print(F("EFV"));
  	  if (tipoCargaInteligente == TARIFA_DH) lcd.print(F("TDH"));
  	  if (tipoCargaInteligente == FRANJA_HORARIA) lcd.print(F("FHO"));
      break;
  }
}

bool EsHorarioVerano(DateTime fecha){
  if (fecha.month() > 3 && fecha.month() < 10){
    return true;
  }else{
    //el último domingo de Marzo
    int dhv = daysInMonth[2] - DateTime(fecha.year(), 3, daysInMonth[2]).dayOfTheWeek();
    //el último domingo de Octubre
    int dhi = daysInMonth[9] - DateTime(fecha.year(), 10, daysInMonth[9]).dayOfTheWeek();
    if ((fecha.month() == 3 && (fecha.day() > dhv || (fecha.day() == dhv && fecha.hour() >= 2))) || (fecha.month() == 10 && (fecha.day() < dhi || (fecha.day() == dhi && fecha.hour() < 3)))){
      return true;
    }
  }
  return false;
}

//************** CONTROL DE SI ESTAMOS DENTRO DE LA TARIFA REDUCIDA *******************
bool EnTarifaValle(int horaNow){
  return (horarioVerano && (horaNow >= 23 || horaNow < 13)) || (!horarioVerano && (horaNow >= 22 || horaNow < 12));
}

//************** CONTROL DE SI ESTAMOS DENTRO DEL HORARIO PROGRAMADO ******************
bool EnFranjaHoraria(int horaNow, int minutoNow){
  if (horaInicioCarga == horaFinCarga && minutoInicioCarga == minutoFinCarga) return false;
  int minutosInicio = horaInicioCarga * 60 + minutoInicioCarga;
  int minutosFin = horaFinCarga * 60 + minutoFinCarga;
  int minutosNow = horaNow * 60 + minutoNow;
  if (minutosInicio < minutosFin && minutosNow >= minutosInicio && minutosNow < minutosFin) return true;
  if (minutosInicio > minutosFin && (minutosNow >= minutosInicio || minutosNow < minutosFin)) return true;
  return false;
}


//************** CALCULO DE ENERGÍA CARGADA EN EL COCHE ******************
void CalcularEnergias(unsigned long currentMillis){
  if (tiempoInicioSesion == 0) {
    tiempoInicioSesion = currentMillis;    // Al comienzo de la sesión de carga comenzamos a contar el tiempo para saber cuanto tiempo llevamos ....
    tiempoCalculoEnergiaCargada = currentMillis;
  }

  unsigned long tiempoCalculoWatios = currentMillis - tiempoCalculoEnergiaCargada;  // Evaluamos el tiempo para el cálculo de watios cargados ....
  if (tiempoCalculoWatios > 12000) {                    // si llevamos más de 12 seg ....
    tiempoCalculoEnergiaCargada = currentMillis;        // lo reseteamos
    tiempoCalculoWatios = 0;
  }
  
  if (tiempoCalculoWatios > 3000) {                   // Si llevamos más de 3 seg vamos sumando ...
    CentesimasWatiosCargados = CentesimasWatiosCargados + ((consumoCargadorAmperios * 23000l) / (3600000l / tiempoCalculoWatios));
    kwTotales = kwTotales + ((consumoCargadorAmperios * 23000l) / (3600000l / tiempoCalculoWatios));
    tiempoCalculoEnergiaCargada = currentMillis;  // Si no estamos cargando reseteamos el tiempo de cálculo de la energía cargada
  }
}

  //******************* CONTROL DE SI HAY EXCEDENTES FOTOVOLTAICOS ******************
bool HayExcedentesFV(){
  if (cargaPorExcedentes){
    if (cargadorEnConsumoGeneral){
      return (generacionFVAmperios - consumoGeneralAmperios - consumoCargadorAmperios >= generacionMinima);
    }else{
      return (generacionFVAmperios - consumoGeneralAmperios >= generacionMinima);
    }
  }
  return (generacionFVAmperios >= generacionMinima);  // Verificamos si hay suficientes excedentes fotovoltaicos....
}
 
  //******************* CONTROL DE AUTORIZACIÓN CARGA FOTOVOLTAICA ******************
bool AutorizaCargaExcedentesFV(unsigned long currentMillis){
  if (HayExcedentesFV()){                  // Si hay excedentes suficientes ....
    if (tiempoGeneraSuficiente == 0){
      tiempoNoGeneraSuficiente = 1;
      return true;
    }else if (tiempoGeneraSuficiente == 1){
      tiempoGeneraSuficiente = currentMillis;
    }else if (currentMillis - tiempoGeneraSuficiente > (unsigned long)tiempoConGeneracion * 60000l){
      tiempoGeneraSuficiente = 0;
      tiempoNoGeneraSuficiente = 1;
      return true;
    }
  }else{    // Si NO hay excedentes suficientes ....
    if (tiempoNoGeneraSuficiente == 0){
      tiempoGeneraSuficiente = 1;
    }else if (tiempoNoGeneraSuficiente == 1){
      tiempoNoGeneraSuficiente = currentMillis;
      return true;
    }else{
      if (currentMillis - tiempoNoGeneraSuficiente > (unsigned long)tiempoSinGeneracion * 60000l){
        tiempoNoGeneraSuficiente = 0;
        tiempoGeneraSuficiente = 1;
      }else return true;
    }
  }
  return false;
}

//******************* CONTROL DE LA POTENCIA DISPONIBLE EN LA VIVIENDA PARA CARGAR ************************
bool HayPotenciaParaCargar(unsigned long currentMillis){
  
  int IntensidadCalculadaParaCargar = IntensidadDisponible();
  bool puedeCargarPot = false;
  if (tipoCarga == CONSIGNA || tipoCarga == GENERACION_FV || (tipoCarga == INTELIGENTE && tipoCargaInteligente == GENERACION_FV)){  //En los tipos de carga FV..
    if (conSensorGeneral && cargaPorExcedentes){     //Si tenemos sensor general y la carga FV es por excedentes..
      duracionPulso = ((generacionFVAmperios - consumoGeneralAmperios) * 100 / 6) - 28;  // Calculamos restanto el consumo a la generación.. 
      puedeCargarPot = true;
    }else if (conSensorGeneral){  // Si no cargamos por excedentes ..
      if (IntensidadCalculadaParaCargar > 5){ // y la intensidad calculada para cargar fuese 6 o más amperios ..
        if (IntensidadCalculadaParaCargar > generacionFVAmperios){  // y además es mayor que la generación FV ..
          duracionPulso = (generacionFVAmperios * 100 / 6) - 28; // calculamos el pulso según la generación.
          puedeCargarPot = true;
        }else{                                               // si la intensidad calculada fuese menor que la generación FV..
          duracionPulso = (IntensidadCalculadaParaCargar * 100 / 6) - 28; // calculamos el pulso según la intensidad calculada.
          puedeCargarPot = true;
        }
      }
    }else{                       // si no tenemos sensor general..
      duracionPulso = (generacionFVAmperios * 100 / 6) - 28;  // calculamos el pulso según la generación.
      puedeCargarPot = true;
    }
  }else{
    if (IntensidadCalculadaParaCargar > 5){  // Si la Intensidad Calculada para Cargar es 6 o más amperios ..
      if ((IntensidadCalculadaParaCargar < intensidadProgramada) && conSensorGeneral){  // si la Intensidad Calculada de Carga es menor que la intensidad programada y tenemos el sensor general conectado..
        duracionPulso = (IntensidadCalculadaParaCargar * 100 / 6) - 28;          // calculamos la duración del pulso en función de la intensidad restante
        puedeCargarPot =  true;
      }else{  // si la Intensidad Calculada para Cargar es mayor que la intensidad programada ..
        duracionPulso = (intensidadProgramada * 100 / 6) - 28; // calculamos el pulso según la intensidad programada.
        puedeCargarPot =  true;
      }
    }
  }
  if (duracionPulso < 72) duracionPulso = 72;   // Si la duración del pulso resultante es menor de 72(6A) lo ponemos a 6 A.
  
  if (!ControlPotencia){
    errorLimiteConsumo = false;	  
    return true;
	  
  }
	  
  if (puedeCargarPot){  // Control de los tiempos de disparo y reset del límite de consumo
    if (tiempoConConsumoRestante == 0){
      tiempoSinConsumoRestante = 1;
      errorLimiteConsumo = false;
      return true;
    }else if (tiempoConConsumoRestante == 1){
      tiempoConConsumoRestante = currentMillis;
    }else if (currentMillis - tiempoConConsumoRestante > (unsigned long)tiempoConGeneracion * 60000l){
      tiempoSinConsumoRestante = 1;
      tiempoConConsumoRestante = 0;
      errorLimiteConsumo = false;
      return true;
    }
  }else{
    if (tiempoSinConsumoRestante == 0){
      tiempoConConsumoRestante = 1;
      errorLimiteConsumo = true;
    }else if (tiempoSinConsumoRestante == 1){
      tiempoSinConsumoRestante = currentMillis;
      return true;
    }else if (currentMillis - tiempoSinConsumoRestante > 30000){
      tiempoSinConsumoRestante = 0;
      tiempoConConsumoRestante = 1;
      errorLimiteConsumo = true;
    }else return true;
  }
  return false;
}

//************** CALCULO DE LA INTENSIDAD DISPONIBLE ***********************
int IntensidadDisponible(){
  if (!conSensorGeneral){          // Si no hay sensor general....
    return 32;                     // Asignamos 32 Amperios como consumo restante
  }else{
    if (cargadorEnConsumoGeneral){ // Aquí se controla si el cargador está incluido en el sensor de consumo general de la vivienda,         
      return (consumoTotalMax - (consumoGeneralAmperios - consumoCargadorAmperios)); // Si el sensor que mide consumo general incluye el cargador se resta el consumo del cargador
    }else{
      return (consumoTotalMax - consumoGeneralAmperios); // Si el sensor que mide consumo general no incluye el cargador no se resta el consumo del cargador
    }
  }
}

// ************** FUNCIÓN PARA ESCRIBIR UN DOBLE ENTERO EN MEMORIA ************
void EEPROMWritelong(int address, long value){
  //Descomponemos un doble entero (long) en 4 bytes usando la función bitshift.
  //One = Byte más significativo -> Four = Byte menos significativo.
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Escribe los 4 bytes en la memoria eeprom.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address){       // Función que permite leer un dato de tipo doble entero (long) de la eeprom partiendo de 4 Bytes
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Devuelve el long adecuando (desplazando) los datos antes de sumarlos.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

void MonitorizarDatos(){
  indPrSer ++;
  if (indPrSer > 20) indPrSer = 0;
  switch (indPrSer){
    case 1:
	Serial.println("Version de Programa -------> 1.66MA");
    break;
    case 2:
	Serial.println("Tension Cargador(Media) ---> " + (String)tensionCargador);
	break;
    case 3:	
	Serial.println("Consumo General Amperios --> " + (String)consumoGeneralAmperios);
	break;
    case 4:	
	Serial.println("Consumo Cargador Amperios -> " + (String)consumoCargadorAmperios);
	break;
    case 5:	
	Serial.println("Media Intensidad Cargador -> " + (String)mediaIntensidadCargador);
	break;
    case 6:	
	Serial.println("Generacion FV Amperios ----> " + (String)generacionFVAmperios);
	break;
    case 7:	
	Serial.print("Conectado -----------------> "); if (conectado) Serial.println("SI"); else Serial.println("NO");
	break;
    case 8:	
	Serial.print("Cargando ------------------> "); if (cargando) Serial.println("SI"); else Serial.println("NO");
	break;
    case 9:	
	Serial.print("Carga Completa--- ---------> "); if (cargaCompleta) Serial.println("SI"); else Serial.println("NO");
	break;
    case 10:	
	Serial.print("Inicio Carga Activado -----> "); if (inicioCargaActivado) Serial.println("SI"); else Serial.println("NO");
	break;
    case 11:	
	Serial.print("Peticion Carga ------------> "); if (peticionCarga) Serial.println("SI"); else Serial.println("NO");
	break;
    case 12:	
	Serial.print("Bateria Cargada -----------> "); if (bateriaCargada) Serial.println("SI"); else Serial.println("NO");
	break;
    case 13:	
	Serial.print("Permiso de Carga ----------> "); if (permisoCarga) Serial.println("SI"); else Serial.println("NO");
	break;
    case 14:	
	Serial.print("Hay Excedentes FV ---------> "); if (HayExcedentesFV()) Serial.println("SI"); else Serial.println("NO");
	break;
    case 15:	
	Serial.print("Error Limite Consumo ------> "); if (errorLimiteConsumo) Serial.println("SI"); else Serial.println("NO");
	break;
    case 16:	
	Serial.print("Error Carga ---------------> "); if (errorCarga) Serial.println("SI"); else Serial.println("NO");
	break;
    case 17:	
	Serial.println("Duracion del pulso --------> " + (String)duracionPulso);
	break;
  }
  }

bool AnnoBisiesto(unsigned int ano){
  return ano % 4 == 0 && (ano % 100 !=0 || ano % 400 == 0);
}
