/*

   Programa: Robochera
   Versión: 0.3.2
   Autor: FjRamírez
   Fecha: 05/09/2020
   Web: https://tuelectronica.es/


   Acceso web
   User: admin
   Password: AP password
*/

// ***** CONFIGURACIÓN DE PINES *****
#define DI_PIN D5 // Pin DI de la tira led WS2812b (GPIO5)
#define SERVOL_PIN D4 // Pin data servo izquierdo (GPIO4)
#define SERVOR_PIN D2 // Pin data servo derecho (GPIO2)
#define CONFIG_PIN D15 // Pin para resetear la contraseña del AP (GPIO15)
#define STATUS_PIN D2 // pin led de estado (GPIO18)

// ***** CONFIGURACIÓN DE PARÁMETROS *****
#define NUMPIXELS 16 // Número de leds
#define FIRMWARE_VERSION "0.3.2" // Versión del Firmware

#include "Servo.h"
#include <FastLED.h>
#include <ArduinoJson.h>
#include "EspMQTTClient.h"


EspMQTTClient mqttClient(
  "JAREPEATER24G",
  "v4dpfkEJHacmQC5%$",
  "192.168.1.116",  // MQTT Broker server ip
  "juan",   // Can be omitted if not needed
  "Jh0m34ss1st4nt01",   // Can be omitted if not needed
  "robochera"      // Client name that uniquely identify your device
);

unsigned long timer1 = 0; // Temporizador para luces
unsigned long timer2 = 0; // Temporizador para puerta
unsigned long timer3 = 0; // Temporizador para enviar el estado
unsigned long timer4 = 0; // Temporizador para reescribir el valor del servo

unsigned int frequency = 1000;
unsigned int beeps = 10;

String ledEffect = "none"; // Efecto de la tira led
boolean ledState = false; // Estado de la tira led
boolean deviceAdded = false; // Dispositivo añadido a HA

int i = 0;
int offset = -1; // Compensacion de servo izquierdo.
CRGB color = CRGB( 90, 210, 0); // GRB

CRGB strip[NUMPIXELS];
//ANGULO ABIERTO
int openParamValue = 180;
//VELOCIDAD APERTURA
int openSpeedParamValue = 15;
//ANGULO CERRADO
int closedParamValue = 0;
//VELOCIDAD DE CIERRE
int closedSpeedParamValue = 15;


boolean needMqttConnect = false;
boolean needReset = false; // Variable para determinar si es necesario reiniciar
int pinState = HIGH;
unsigned long lastMqttConnectionAttempt = 0;
Servo servoLeft;  // create servo object to control a servo
Servo servoRight;  // create servo object to control a servo

int value = 0; // Difine el ángulo de abertura de la puerta
int valuePre = 0; // Valor anterior

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Iniciando...");

  FastLED.addLeds<WS2812B, DI_PIN, RGB>(strip, NUMPIXELS);  // GRB ordering is typical
  FastLED.clear(); // Set all pixel colors to 'off'
  FastLED.show();

  servoLeft.attach(SERVOL_PIN, 500, 2400); // Objeto servo izquierdo
  servoRight.attach(SERVOR_PIN, 500, 2400); // Objeto servo derecho
  servoLeft.write(0); // NO SE SI FUNCIONA
  servoRight.write(0); // NO SE SI FUNCIONA

  // Optional functionalities of EspMQTTClient
  mqttClient.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  mqttClient.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  mqttClient.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
 
  Serial.println("Ready!");
}

void loop() {
  mqttClient.loop();
  
  if (needReset) { // Si es necesario reiniciar:
    Serial.println("Reiniciando...");
    ESP.restart(); // Reiniciamos el dispositivo
  }
  
  // Abrir/Cerrar puerta:
  if (valuePre < value) { // Si tenemos que abrir la puerta:
    openDoor(openSpeedParamValue);
  } else if ( valuePre > value) { // Si tenemos que cerrar la puerta
    closeDoor(closedSpeedParamValue);
  } 

  // Tira led:
  if (ledState) {
    if (ledEffect == "opendoor") {
      color = CRGB( 90, 210, 0);
      colorGoOut(100); // 80
    } else if (ledEffect == "closeddoor") {
      color = CRGB( 90, 210, 0);
      colorGoIn(100); // 80
    } else if (ledEffect == "opendoor2") {
      color = CRGB( 190, 0, 0); // Verde
      colorGoOut(100); // 80
    } else if (ledEffect == "colorblink") {
      color = CRGB( 90, 210, 0); // Naranja
      colorBlink(1000);
    }
  }

  // Enviar estado cada 10s:
  if (millis() >= (timer3 + 10000)) {
    timer3 = millis();
    sendStatus();
  }
}

void onConnectionEstablished() {
 mqttClient.setMaxPacketSize(1024);  

  mqttClient.subscribe("robochera/value", [] (const String &payload)  { 
    Serial.println("Incoming message of topic robochera/value: " + payload);
    value = payload.toInt();
  });

  mqttClient.subscribe("homeassistant/switch/robochera/set", [] (const String &payload)  {
     Serial.println("Incoming message of topic homeassistant/switch/robochera/set: " + payload);
   
    if (payload == "ON") {
      value = openParamValue;
      mqttClient.publish("homeassistant/switch/robochera/state", "ON");
    } else {
      value = closedParamValue;
      mqttClient.publish("homeassistant/switch/robochera/state", "OFF");
    }
  });

  mqttClient.subscribe("homeassistant/light/robochera/set", [] (const String &payload)  {
     Serial.println("Incoming message of topic homeassistant/light/robochera/set: " + payload);
   
    if (payload == "ON") {
      ledState = true;
      mqttClient.publish("homeassistant/light/robochera/state", "ON");
    } else {
      ledState = false;
      FastLED.clear();
      FastLED.show();
      mqttClient.publish("homeassistant/light/robochera/state", "OFF");
    }
  });

   mqttClient.subscribe("homeassistant/light/robochera/fx", [] (const String &payload)  {
     Serial.println("Incoming message of topic homeassistant/light/robochera/fx: " + payload);
   
    ledEffect = payload;
    FastLED.clear();
    FastLED.show();
    mqttClient.publish("homeassistant/light/robochera/fx_stat", ledEffect);
  });

   mqttClient.subscribe("homeassistant/light/robochera/cmnd/dimmer", [] (const String &payload)  {
     Serial.println("Incoming message of topic homeassistant/light/robochera/cmnd/dimmer: " + payload);
   
    FastLED.setBrightness(payload.toInt());
  });

  if (!deviceAdded) { // Si no se ha registrado el dispositivo en HA:
    Serial.print("Registering device in Home Assistant...");
    register_homeassistant(); // Registramos dispositivo en Home Assistant
  }
}

// Efecto de salida en leds:
void colorGoOut(uint16_t wait) {
  unsigned long now = millis();
  if ( (wait < now - timer1) ) {
    timer1 = now;
    if (i < 0) {
      FastLED.clear(); // Set all pixel colors to 'off'
      FastLED.show();
      i = NUMPIXELS / 2 - 1;
    } else {
      strip[i] = color;
      strip[NUMPIXELS - i - 1] = color;
      FastLED.show();   // Send the updated pixel colors to the hardware.
      i = i - 1;
    }
  }
}

// Efecto de entrada en leds:
void colorGoIn(uint16_t wait) {
  unsigned long now = millis();
  if ( (wait < now - timer1) ) {
    timer1 = now;
    if (i < NUMPIXELS / 2) {
      strip[i] = color;
      strip[NUMPIXELS - i - 1] = color;
      FastLED.show();   // Send the updated pixel colors to the hardware.
      i = i + 1;
    } else {
      FastLED.clear(); // Set all pixel colors to 'off'
      FastLED.show();
      i = 0;
    }
  }
}

// Efecto leds "intermitentes naranja":
void colorBlink(uint16_t wait) {
  unsigned long now = millis();
  if ( (wait < now - timer1) ) {
    timer1 = now;
    if (i == 0) {
      //strip = color;
      fill_solid( strip, NUMPIXELS, color);
      //strip[NUMPIXELS-i-1] = color;
      FastLED.show();   // Send the updated pixel colors to the hardware.
      i = i + 1;
    } else {
      FastLED.clear(); // Set all pixel colors to 'off'
      FastLED.show();
      i = 0;
    }
  }
}

// Abrir puerta:
void openDoor(uint16_t wait) {
  unsigned long now = millis();
  if ( (wait < now - timer2) ) { // Repetimos cada x tiempo:
    timer2 = now;
    if (valuePre < value) { // Si el valor previo es menor que el valor actual:
      valuePre = valuePre + 1;
      servoLeft.write(valuePre + offset);
      servoRight.write(180 - valuePre);
      Serial.print("Apertura puerta ");
      Serial.println(valuePre);
    }
  }
}

// Cerrar puerta:
void closeDoor(uint16_t wait) {
  unsigned long now = millis();
  if ( (wait < now - timer2) ) { // Repetimos cada x tiempo:
    timer2 = now;
    if (valuePre > value) { // Si el valor previo es mayor que el valor actual:
      valuePre = valuePre - 1;
      servoLeft.write(valuePre + offset);
      servoRight.write(180 - valuePre);
      Serial.print("Cierre puerta ");
      Serial.println(valuePre);
    }
  }
}

// Envia el estdo del dispositivo
void sendStatus() {
  // Puerta:
  if ( value >= openParamValue) {
    mqttClient.publish("homeassistant/switch/robochera/state", "ON");
  } else {
    mqttClient.publish("homeassistant/switch/robochera/state", "OFF");
  }

  // LED:
  if (ledState) {
    mqttClient.publish("homeassistant/light/robochera/state", "ON");
  } else {
    mqttClient.publish("homeassistant/light/robochera/state", "OFF");
  }

  // Led efecto
  mqttClient.publish("homeassistant/light/robochera/fx_stat", ledEffect);
}


// Registra el dispositivo en Home Assistant:
void register_homeassistant() {
  DynamicJsonDocument doc(1024);
  DynamicJsonDocument device(1024);
  DynamicJsonDocument effect_list(1024);

  device["ids"] = "Robochera"; // identifiers
  device["name"] = "Robochera";
  device["sw"] = FIRMWARE_VERSION; // sw_version
  device["mdl"] = "nodemuc v0.0.1"; // modelo
  device["mf"] = "FjRamirez - Tuelectronica.es"; // manufacturer

  // Interruptor puerta
  doc["~"] = "homeassistant/switch/robochera",
             doc["name"] = "Robochera Door";
  doc["cmd_t"] = "~/set"; // command_topic
  doc["stat_t"] = "~/state";
  doc["uniq_id"] = "robochera_door"; // unique_id
  doc["ic"] = "mdi:garage-variant"; // icon
  doc["device"] = device;

  serializeJson(doc, Serial); // Damos formato JSON
  char playload[measureJson(doc) + 1]; // Creamos un array con el número de carácteres de doc mas fin de linea
  serializeJson(doc, playload, measureJson(doc) + 1); // Pasamos de doc a playload con formato JSON y fin de linea

  mqttClient.publish("homeassistant/switch/robochera/config", playload, true); // Publicamos la configuración
  doc.clear(); // Limpiamos el documento dinamico

  // Luz led
  doc["~"] = "homeassistant/light/robochera",
             doc["name"] = "Robochera Light";
  doc["cmd_t"] = "~/set"; // command_topic
  doc["stat_t"] = "~/state";
  doc["uniq_id"] = "robochera_light"; // unique_id
  doc["bri_cmd_t"] = "~/cmnd/dimmer"; // brightness_command_topic

  doc["device"] = device;
  doc["fx_cmd_t"] = "~/fx";
  doc["fx_stat_t"] = "~/fx_stat";
  effect_list.add("none");
  effect_list.add("opendoor");
  effect_list.add("closeddoor");
  effect_list.add("opendoor2");
  effect_list.add("colorblink");
  doc["effect_list"] = effect_list;

  serializeJson(doc, Serial); // Damos formato JSON
  char playload2[measureJson(doc) + 1]; // Creamos un array con el número de carácteres de doc mas fin de linea
  serializeJson(doc, playload2, measureJson(doc) + 1); // Pasamos de doc a playload con formato JSON y fin de linea

  mqttClient.publish("homeassistant/light/robochera/config", playload2,  true); // Publicamos la configuración

  deviceAdded = true;
}
