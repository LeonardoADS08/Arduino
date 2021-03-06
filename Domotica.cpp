// Inicia el pin 2, 3 como medios de transmision para el modulo WIFI
#include <SoftwareSerial.h>
SoftwareSerial ESP8266(10, 11); 
/*
  Pin 2: RX
  Pin 3: TX
  Pin 53: Led alert
*/

// Variables para el "multi-hilos"
unsigned long anteriorDiagnostico = 0, actualDiagnostico = 0, intervaloDiagnostico = 900000; // 900000 - 15 minutos

// Variables global para almacenar las respuestas del ESP8266
String msg = "";
String mensajes[16];
int n;

// Constante modificada para aumentar el tamaño del buffer, algunas veces los datos pueden ser mayor a lo que soporte el valor por defecto
#define SERIAL_BUFFER_SIZE 64

// Utilidades
int alertPin = 53;

// Comandos AT para el ESP8266
String ordenes[] =
{
  "AT+RST",
  "AT+CWMODE=1",
  "AT+CWQAP",
  "AT+CIPSTA=\"192.168.5.70\",\"192.168.5.1\",\"255.255.255.0\"",
  "AT+CWJAP=\"ADSnet1\",\"HadBadLad2016\"",
  "AT+CIPMUX=1",
  "AT+CIPSERVER=1,80",
};

String diagnostico[] =
{
  "AT+CWMODE?", // Se debe comprobar que esta en modo 3
  "AT+CIPMUX?", // Se debe comprobar que esta en modo 1
  "AT+CIPSTA?",   // Verificar IP, 192.168.5.70
  "AT+CIPSERVER=1,80" // Debe salir que no se han hecho cambios, no change
};


// Prototipos de funciones
void iniciarSalidas();
void ledAlarma(int pin, int ticks);
void limpiarBuffer();
void leerMensaje();
void reiniciarServidor();
void iniciarWIFI();
bool analizarMensaje(String temp, bool acciones);
void reiniciarWIFI();
void cerrarConexiones();
bool realizarDiagnostico();
bool buscarAccion(String temp);

// Programa
void setup()
{
  // Inicio de la comunicación via Serial y los pines
  Serial.begin(19200);
  ESP8266.begin(19200);
  iniciarSalidas();
  
  // Inicio del modulo WIFI
  Serial.println("Comenzando servicios en 5 segundos");
  limpiarBuffer();
  delay(5000);
  iniciarWIFI();
}

void loop()
{
  // Proceso 1. Comunicacion con el modulo WIFI
  if (ESP8266.available())
  {
    if (ESP8266.peek() != '\n')
      msg += (char)ESP8266.peek();
    else 
    {
      analizarMensaje(msg, true);
      msg = "";
    }
    Serial.write(ESP8266.read());
    
    if (buscarAccion(msg))
    {
      msg = "";
      limpiarBuffer();
    }
  }

  if (Serial.available())
    ESP8266.write(Serial.read());

  // Proceso 2. Diagnosticos periodicos
  // Se debe verificar que no haya nada en el buffer, para no interrumpir acciones de lectura y escritura al modulo WIFI
  if (!Serial.available() && !ESP8266.available())
  {
    actualDiagnostico = millis();
    if (actualDiagnostico - anteriorDiagnostico > intervaloDiagnostico)
    {
      anteriorDiagnostico = millis();
      cerrarConexiones();
      if (!realizarDiagnostico())
      {
        Serial.println("Diagnostico finalizado, solucionando errores...");
        reiniciarServidor(); 
      }
      else Serial.println("Diagnostico finalizado, sin errores...");
    }
  }
}

// Declaracion de funciones

void ledAlarma(int pin, int ticks)
{
  for (int i = 0; i < ticks; ++i)
  {
    digitalWrite(pin, HIGH);
    delay(250);
    digitalWrite(pin, LOW);
    delay(250);
  }
}

void limpiarBuffer()
{
  Serial.flush();
  ESP8266.flush();
}

void leerMensaje()
{
  // Se limpian los mensajes anteriores
  for (n = 0; n < 16; ++n)
    mensajes[n] = "";
  n = 0;

  // Se almacenan e imprimen los nuevos mensajes
  while (ESP8266.available())
  {
    mensajes[n] += (char)ESP8266.read();
    if (ESP8266.peek() == '\n')
    {
      ESP8266.read();
      Serial.println(mensajes[n]);
      ++n;
    }
  }

  limpiarBuffer();
  return;
}

void reiniciarServidor()
{
  Serial.println("\n\nError inesperado detectado, reiniciando servidor.");
  delay(5000);
  for (int i = 5; i < 7; ++i)
  {
    Serial.println("\nIniciando orden: " + ordenes[i]);
    ESP8266.println(ordenes[i]);
    delay(1000);

    leerMensaje();
  }

  ledAlarma(alertPin, 2);
  limpiarBuffer();
}

void iniciarWIFI()
{
  bool inicioCorrecto = true;
  for (int i = 1; i < 7; ++i)
  {
    Serial.println("\nIniciando orden: " + ordenes[i]);
    ESP8266.println(ordenes[i]);
    if (i == 4) delay(12000);
    else delay(3000);
    leerMensaje();

    for (int i = 0; i < n; ++i)
    {
      if ( !analizarMensaje(mensajes[i], false) )
        inicioCorrecto = false;
    }
    if (!inicioCorrecto) break;
  }

  if (inicioCorrecto)
  {
     Serial.println("\nSistema iniciado.");
     Serial.println("\nComandos utiles:\n Reiniciar: AT+RST\n IP: AT+CIFSR\n Puntos de acceso: AT+CWLAP\n");
     ledAlarma(alertPin, 1);
  }
  else
  {
    Serial.println("\nError encontrado, es necesario reiniciar el modulo");
    reiniciarWIFI();
  }
  limpiarBuffer();
}

bool analizarMensaje(String temp, bool acciones)
{
  // Usualmente se da cuando se reinicia el sistema por alguna razón, se debe reabrir los puertos
  if (temp.indexOf("ready") >= 0 || temp.indexOf("invalid") >= 0 || temp.indexOf("Ai-Thinker Technology Co.,Ltd.") >= 0 )
  {
    if (acciones)
      reiniciarServidor();
    return false;
  }

  /*
  // Formato para reiniciar el Modulo correctamente
  if (temp.indexOf("") >= 0)
  {
    if (acciones)
    {
      Serial.println("\nError encontrado, es necesario reiniciar el modulo");
      reiniciarWIFI();
    }
    return false;
  }
  */
  return true;
}

void reiniciarWIFI()
{
  Serial.println("Reiniciando WIFI");
  ESP8266.println(ordenes[0]);
  delay(10000);
  limpiarBuffer();
  iniciarWIFI();
}

void cerrarConexiones()
{
  Serial.println("Cerrando conexiones");
  ESP8266.println("AT+CIPSTATUS");
  leerMensaje();
  for (int i = 0; i < n; ++i)
  {
    ESP8266.println("AT+CIPCLOSE=" + String(n));
    limpiarBuffer();
  }
}

bool realizarDiagnostico()
{
  bool resultadoDiagnostico, resultadoFinal = true;
  Serial.println("Ejecutando diagnostico periodico");
  
  Serial.println("\n Comprobando CWMODE");
  ESP8266.println(diagnostico[0]);
  delay(1000);
  leerMensaje();
  resultadoDiagnostico = false;
  for (int i = 0; i < n; ++i)
  {
    if (mensajes[i].indexOf("+CWMODE:1") >= 0)
    {
      resultadoDiagnostico = true;
      break;
    }
  }
  if (!resultadoDiagnostico) resultadoFinal = false;
  limpiarBuffer();

  Serial.println("\n Comprobando CIPMUX");
  ESP8266.println(diagnostico[1]);
  delay(1000);
  leerMensaje();
  resultadoDiagnostico = false;
  for (int i = 0; i < n; ++i)
  {
    if (mensajes[i].indexOf("+CIPMUX:1") >= 0)
    {
      resultadoDiagnostico = true;
      break;
    }
  }
  if (!resultadoDiagnostico) resultadoFinal = false;
  limpiarBuffer();
  
  Serial.println("\n Comprobando CIPSTA");
  ESP8266.println(diagnostico[2]);
  delay(1000);
  leerMensaje();
  resultadoDiagnostico = false;
  for (int i = 0; i < n; ++i)
  {
    if (mensajes[i].indexOf("192.168.5.70") >= 0)
    {
      resultadoDiagnostico = true;
      break;
    }
  }
  if (!resultadoDiagnostico) resultadoFinal = false;
  limpiarBuffer();
  
  Serial.println("\n Comprobando CIPSERVER");
  ESP8266.println(diagnostico[3]);
  delay(1000);
  leerMensaje();
  resultadoDiagnostico = false;
  for (int i = 0; i < n; ++i)
  {
     if (mensajes[i].indexOf("no change") >= 0)
     {
      resultadoDiagnostico = true;
      break;
     }
  }
  if (!resultadoDiagnostico) resultadoFinal = false;
  
  limpiarBuffer();
  ledAlarma(alertPin, 5);
  return resultadoFinal;
}

void iniciarSalidas()
{
// Pines de los relés
  for (int i = 22; i <= 29; ++i)
    {
      pinMode(i, OUTPUT);
      digitalWrite(i, LOW);
      delay(50);
      digitalWrite(i, HIGH);
    }
  
  // Pin para alertas
  pinMode(53, OUTPUT);
  digitalWrite(53, HIGH);
  delay(250);
  digitalWrite(53, LOW);
  
  // Solo dios sabe para sirve este pin; Esta sin usar en el arduino.
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);
}

bool buscarAccion(String temp)
{
  if (temp.indexOf("P13") >= 0)
    {
      digitalWrite(13, !digitalRead(13));
      Serial.println("\nInvirtiendo pin 13");
      limpiarBuffer();
      return true;
    }
    else if (temp.indexOf("22") >= 0)
   {
      digitalWrite(22, !digitalRead(22));
      Serial.println("\nInvirtiendo pin 22");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("23") >= 0)
   {
      digitalWrite(23, !digitalRead(23));
      Serial.println("\nInvirtiendo pin 23");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("P24") >= 0)
   {
      digitalWrite(24, !digitalRead(24));
      Serial.println("\nInvirtiendo pin 24");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("P25") >= 0)
   {
      digitalWrite(25, !digitalRead(25));
      Serial.println("\nInvirtiendo pin 25");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("P26") >= 0)
   {
      digitalWrite(26, !digitalRead(26));
      Serial.println("\nInvirtiendo pin 26");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("P27") >= 0)
   {
      digitalWrite(27, !digitalRead(27));
      Serial.println("\nInvirtiendo pin 27");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("P28") >= 0)
   {
      digitalWrite(28, !digitalRead(28));
      Serial.println("\nInvirtiendo pin 28");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("P29") >= 0)
   {
      digitalWrite(29, !digitalRead(29));
      Serial.println("\nInvirtiendo pin 29");
      limpiarBuffer();
      return true;
   }
   else if (temp.indexOf("Encender") >= 0)
   {
    Serial.println("\nEncendiendo todo");
    for (int i = 22; i <= 29; ++i)
      digitalWrite(i, LOW);
    return true;
   }
   else if (temp.indexOf("Apagar") >= 0)
   {
    Serial.println("\nApagando todo");
    for (int i = 22; i <= 29; ++i)
      digitalWrite(i, HIGH);
    return true;
   }
   return false;
}

