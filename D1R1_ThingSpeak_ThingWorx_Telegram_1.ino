#include "DHT.h"
#include <BH1750FVI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "LedControl.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266TelegramBOT.h>
#include <ArduinoJson.h>

// Wi-Fi point
const char* ssid     = "MGBot";
const char* password = "Terminator812";

// For ThingSpeak IoT
const String CHANNELID_0 = "135158";
const String WRITEAPIKEY_0 = "8RZEBEAICIOVNQ7D";
IPAddress thingspeak_server(184, 106, 153, 149);
const int httpPort = 80;

// For ThingWorx IoT
char iot_server[] = "cttit5402.cloud.thingworx.com";
IPAddress iot_address(52, 87, 101, 142);
char appKey[] = "ff9de221-e088-4726-97a6-c61aa61a15a6";
char thingName[] = "smile_flower_thing";
char serviceName[] = "apply_flower_data";

// ThingWorx parameters
#define sensorCount 5
char* sensorNames[] = {"air_temp", "air_hum", "soil_temp", "soil_hum", "light"};
float sensorValues[sensorCount];
// Номера датчиков
#define air_temp     0
#define air_hum      1
#define soil_temp    2
#define soil_hum     3
#define light        4

// Bot account
#define BOTtoken "262485007:AAH_yU78u3nDcJf0R8JW3_hbzGKEv2_t2AE"
#define BOTname "flower_bot"
#define BOTusername "flower_iot_bot"
TelegramBOT bot(BOTtoken, BOTname, BOTusername);

WiFiClient client;

#define THINGSPEAK_UPDATE_TIME 60000   // Update ThingSpeak data server
#define THINGWORX_UPDATE_TIME 5000     // Update ThingWorx data server
#define DS18B20_UPDATE_TIME 5000       // Update time for DS18B20 sensor
#define DHT22_UPDATE_TIME 5000         // Update time for DHT22 sensor
#define BH1750_UPDATE_TIME 5000        // Update time for BH1750 sensor
#define MOISTURE_UPDATE_TIME 5000      // Update time for moisture sensor
#define CONTROL_UPDATE_TIME 600000     // Update time for devices control
#define SMILE_UPDATE_TIME 1000         // Update time for smile
#define TELEGRAM_UPDATE_TIME 1000      // Update time for telegram bot
#define RESET_UPDATE_TIME 7200000      // Update time for reset watering timer

// DHT11 sensor
#define DHT22_PIN 0
DHT dht22(DHT22_PIN, DHT22, 15);

// DS18B20 sensor
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

// BH1750 sensor
BH1750FVI LightSensor_1;

// Moisture sensor
#define MOISTURE_PIN A0

// Relay
#define RELAY_PIN 15

// LED matrix 8x8
LedControl LED_MATRIX = LedControl(13, 14, 12, 1);

// Data from sensors
float h1 = 0;
float t1 = 0;
float t2 = 0;
float m1 = 0;
float l1 = 0;

// Control parameters
int pump1 = 0;

// Состояние помпы
int pump_state = 0;

// Timer counters
unsigned long timer_thingspeak = 0;
unsigned long timer_thingworx = 0;
unsigned long timer_ds18b20 = 0;
unsigned long timer_dht22 = 0;
unsigned long timer_bh1750 = 0;
unsigned long timer_moisture = 0;
unsigned long timer_control = 0;
unsigned long timer_smile = 0;
unsigned long timer_telegram = 0;
unsigned long timer_resetwater = 0;

// Watering counter
#define MAX_WATER_COUNT 10
int water_count = 0;

#define TIMEOUT 1000 // 1 second timout

// Smiles
const byte PROGMEM smile_sad[8] =
{
  B00111100,
  B01000010,
  B10100101,
  B10010001,
  B10010001,
  B10100101,
  B01000010,
  B00111100
};
const byte PROGMEM smile_neutral[8] =
{
  B00111100,
  B01000010,
  B10100101,
  B10100001,
  B10100001,
  B10100101,
  B01000010,
  B00111100
};
const byte PROGMEM smile_happy[8] =
{
  B00111100,
  B01000010,
  B10010101,
  B10100001,
  B10100001,
  B10010101,
  B01000010,
  B00111100
};
const byte PROGMEM smile_warning[8] =
{
  B00000000,
  B00001000,
  B00011110,
  B10111111,
  B10111111,
  B00011110,
  B00001000,
  B00000000
};
const byte PROGMEM smile_off[8] =
{
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000,
  B00000000
};
const byte PROGMEM smile_on[8] =
{
  B11111111,
  B11111111,
  B11111111,
  B11111111,
  B11111111,
  B11111111,
  B11111111,
  B11111111,
};

// Type of displayed smile
int smile_type = 0;

// Blink flag for smile
int smile_blink = 0;

// Moisture constants
#define MIN_MOISTURE 10
#define AVG_MOISTURE 30
#define MAX_MOISTURE 60

// Максимальное время ожидания ответа от сервера
#define IOT_TIMEOUT1 5000
#define IOT_TIMEOUT2 100

// Таймер ожидания прихода символов с сервера
long timer_iot_timeout = 0;

// Размер приемного буффера
#define BUFF_LENGTH 64

// Приемный буфер
char buff[BUFF_LENGTH] = "";

// Main setup
void setup()
{
  // Init serial port
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Init relay
  digitalWrite(RELAY_PIN, false);
  pinMode(RELAY_PIN, OUTPUT);

  // Init Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  // Start bot library
  bot.begin();

  // Init DHT11
  dht22.begin();

  // Init DS18B20
  ds18b20.begin();

  // Init BH1750
  LightSensor_1.begin();
  LightSensor_1.setMode(Continuously_High_Resolution_Mode);

  // Init moisture
  pinMode(MOISTURE_PIN, INPUT);

  // Init LED matrix
  LED_MATRIX.shutdown(0, false);
  LED_MATRIX.setIntensity(0, 8);
  LED_MATRIX.clearDisplay(0);

  // First measurement and print data from sensors
  readDHT22();
  readDS18B20();
  readBH1750();
  readMOISTURE();
  printAllSensors();

  // Control devices
  controlDEVICES();

  // Show smile
  showSmile();
}

// Main loop cycle
void loop()
{
  // Send data to ThingSpeak server
  if (millis() > timer_thingspeak + THINGSPEAK_UPDATE_TIME)
  {
    smile_type = 0;
    printAllSensors();
    sendThingSpeakStream();
    timer_thingspeak = millis();
  }

  // Send data to ThingWorx server
  if (millis() > timer_thingworx + THINGWORX_UPDATE_TIME)
  {
    sendThingWorxStream();
    timer_thingworx = millis();
  }

  // DHT22 sensor timeout
  if (millis() > timer_dht22 + DHT22_UPDATE_TIME)
  {
    readDHT22();
    timer_dht22 = millis();
  }

  // DS18B20 sensor timeout
  if (millis() > timer_ds18b20 + DS18B20_UPDATE_TIME)
  {
    readDS18B20();
    timer_ds18b20 = millis();
  }

  // BH1750 sensor timeout
  if (millis() > timer_bh1750 + BH1750_UPDATE_TIME)
  {
    readBH1750();
    timer_bh1750 = millis();
  }

  // Moisture sensor timeout
  if (millis() > timer_moisture + MOISTURE_UPDATE_TIME)
  {
    readMOISTURE();
    timer_moisture = millis();
  }

  // Control devices timeout
  if (millis() > timer_control + CONTROL_UPDATE_TIME)
  {
    controlDEVICES();
    timer_control = millis();
  }

  // Reset watering timers
  if (millis() > timer_resetwater + RESET_UPDATE_TIME)
  {
    water_count = 0;
    timer_resetwater = millis();
  }

  // Show smile
  if (millis() > timer_smile + SMILE_UPDATE_TIME)
  {
    showSmile();
    timer_smile = millis();
  }

  // Execute Telegram Bot
  if (millis() > timer_telegram + TELEGRAM_UPDATE_TIME)
  {
    bot.getUpdates(bot.message[0][1]);
    Telegram_ExecMessages();
    timer_telegram = millis();
  }
}

// Send IoT packet to ThingSpeak
void sendThingSpeakStream()
{
  Serial.print("Connecting to ");
  Serial.print(thingspeak_server);
  Serial.println("...");
  if (client.connect(thingspeak_server, httpPort))
  {
    if (client.connected())
    {
      Serial.println("Sending data to ThingSpeak server...\n");
      String post_data = "field1=";
      post_data = post_data + String(t1, 1);
      post_data = post_data + "&field2=";
      post_data = post_data + String(h1, 1);
      post_data = post_data + "&field3=";
      post_data = post_data + String(t2, 1);
      post_data = post_data + "&field4=";
      post_data = post_data + String(m1, 1);
      post_data = post_data + "&field5=";
      post_data = post_data + String(l1, 1);
      post_data = post_data + "&field6=";
      post_data = post_data + String(pump1);
      Serial.println("Data to be send:");
      Serial.println(post_data);
      client.println("POST /update HTTP/1.1");
      client.println("Host: api.thingspeak.com");
      client.println("Connection: close");
      client.println("X-THINGSPEAKAPIKEY: " + WRITEAPIKEY_0);
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      int thisLength = post_data.length();
      client.println(thisLength);
      client.println();
      client.println(post_data);
      client.println();
      delay(1000);
      timer_thingspeak = millis();
      while ((client.available() == 0) && (millis() < timer_thingspeak + TIMEOUT));
      while (client.available() > 0)
      {
        char inData = client.read();
        Serial.print(inData);
      }
      Serial.println();
      client.stop();
      Serial.println("Data sent OK!");
      Serial.println();
    }
  }
}

// Подключение к серверу IoT ThingWorx
void sendThingWorxStream()
{
  // Подключение к серверу
  Serial.println("Connecting to IoT server...");
  if (client.connect(iot_address, 80))
  {
    // Проверка установления соединения
    if (client.connected())
    {
      // Отправка заголовка сетевого пакета
      Serial.println("Sending data to IoT server...\n");
      Serial.print("POST /Thingworx/Things/");
      client.print("POST /Thingworx/Things/");
      Serial.print(thingName);
      client.print(thingName);
      Serial.print("/Services/");
      client.print("/Services/");
      Serial.print(serviceName);
      client.print(serviceName);
      Serial.print("?appKey=");
      client.print("?appKey=");
      Serial.print(appKey);
      client.print(appKey);
      Serial.print("&method=post&x-thingworx-session=true");
      client.print("&method=post&x-thingworx-session=true");
      // Отправка данных с датчиков
      for (int idx = 0; idx < sensorCount; idx ++)
      {
        Serial.print("&");
        client.print("&");
        Serial.print(sensorNames[idx]);
        client.print(sensorNames[idx]);
        Serial.print("=");
        client.print("=");
        Serial.print(sensorValues[idx]);
        client.print(sensorValues[idx]);
      }
      // Закрываем пакет
      Serial.println(" HTTP/1.1");
      client.println(" HTTP/1.1");
      Serial.println("Accept: application/json");
      client.println("Accept: application/json");
      Serial.print("Host: ");
      client.print("Host: ");
      Serial.println(iot_server);
      client.println(iot_server);
      Serial.println("Content-Type: application/json");
      client.println("Content-Type: application/json");
      Serial.println();
      client.println();

      // Ждем ответа от сервера
      timer_iot_timeout = millis();
      while ((client.available() == 0) && (millis() < timer_iot_timeout + IOT_TIMEOUT1));

      // Выводим ответ о сервера, и, если медленное соединение, ждем выход по таймауту
      int iii = 0;
      bool currentLineIsBlank = true;
      bool flagJSON = false;
      timer_iot_timeout = millis();
      while ((millis() < timer_iot_timeout + IOT_TIMEOUT2) && (client.connected()))
      {
        while (client.available() > 0)
        {
          char symb = client.read();
          Serial.print(symb);
          if (symb == '{')
          {
            flagJSON = true;
          }
          else if (symb == '}')
          {
            flagJSON = false;
          }
          if (flagJSON == true)
          {
            buff[iii] = symb;
            iii ++;
          }
          timer_iot_timeout = millis();
        }
      }
      buff[iii] = '}';
      buff[iii + 1] = '\0';
      Serial.println(buff);
      // Закрываем соединение
      client.stop();

      // Расшифровываем параметры
      StaticJsonBuffer<BUFF_LENGTH> jsonBuffer;
      JsonObject& json_array = jsonBuffer.parseObject(buff);
      pump_state = json_array["pump_state"];
      Serial.println("Pump state:   " + String(pump_state));
      Serial.println();
      // Делаем управление устройствами
      if (pump_state)
      {
        digitalWrite(RELAY_PIN, true);
      }
      else
      {
        digitalWrite(RELAY_PIN, false);
      }
      Serial.println("Packet successfully sent!");
      Serial.println();
    }
  }
}

// Print sensors data to terminal
void printAllSensors()
{
  Serial.print("Air temperature: ");
  Serial.print(t1);
  Serial.println(" *C");
  Serial.print("Air humidity: ");
  Serial.print(h1);
  Serial.println(" %");
  Serial.print("Soil temperature: ");
  Serial.print(t2);
  Serial.println(" *C");
  Serial.print("Soil moisture: ");
  Serial.print(m1);
  Serial.println(" %");
  Serial.print("Ambient light intensity: ");
  Serial.print(l1);
  Serial.println(" lx");
  Serial.print("Water pump state: ");
  Serial.println(pump1);
  Serial.println("");
}

// Read DHT22 sensor
void readDHT22()
{
  h1 = dht22.readHumidity();
  t1 = dht22.readTemperature();
  sensorValues[air_hum] = h1;
  sensorValues[air_temp] = t1;
  if (isnan(h1) || isnan(t1))
  {
    Serial.println("Failed to read from DHT22 sensor!");
  }
}

// Read DS18B20 sensor
void readDS18B20()
{
  ds18b20.requestTemperatures();
  t2 = ds18b20.getTempCByIndex(0);
  sensorValues[soil_temp] = t2;
  if (isnan(t2))
  {
    Serial.println("Failed to read from DS18B20 sensor!");
  }
}

// Read BH1750 sensor
void readBH1750()
{
  l1 = LightSensor_1.getAmbientLight();
  sensorValues[light] = l1;
}

// Read MOISTURE sensor
void readMOISTURE()
{
  m1 = analogRead(MOISTURE_PIN) / 1023.0 * 100.0;
  sensorValues[soil_hum] = m1;
}

// Control devices
void controlDEVICES()
{
  // Pump
  if (m1 < MIN_MOISTURE)
  {
    water_count = water_count + 1;
    if (water_count < MAX_WATER_COUNT)
    {
      digitalWrite(RELAY_PIN, true);
      delay(5000);
      digitalWrite(RELAY_PIN, false);
      pump1 = 1;
    }
  } else if ((m1 >= MIN_MOISTURE) && (m1 < AVG_MOISTURE))
  {
    water_count = water_count + 1;
    if (water_count < MAX_WATER_COUNT)
    {
      digitalWrite(RELAY_PIN, true);
      delay(2000);
      digitalWrite(RELAY_PIN, false);
      pump1 = 1;
    }
  } else if (m1 >= AVG_MOISTURE)
  {
    pump1 = 0;
  }
  if (pump_state)
  {
    digitalWrite(RELAY_PIN, true);
    pump1 = 1;
  }
}

// Show soil moisture smile
void showSmile()
{
  // Smiles
  LED_MATRIX.shutdown(0, false);
  LED_MATRIX.setIntensity(0, 8);
  LED_MATRIX.clearDisplay(0);
  if (smile_type == 0)
  {
    if (m1 < MIN_MOISTURE)
    {
      LED_MATRIX.setRow(0, 0, smile_sad[0]);
      LED_MATRIX.setRow(0, 1, smile_sad[1]);
      LED_MATRIX.setRow(0, 2, smile_sad[2]);
      LED_MATRIX.setRow(0, 3, smile_sad[3]);
      LED_MATRIX.setRow(0, 4, smile_sad[4]);
      LED_MATRIX.setRow(0, 5, smile_sad[5]);
      LED_MATRIX.setRow(0, 6, smile_sad[6]);
      LED_MATRIX.setRow(0, 7, smile_sad[7]);
    } else if ((m1 >= MIN_MOISTURE) && (m1 < AVG_MOISTURE))
    {
      LED_MATRIX.setRow(0, 0, smile_neutral[0]);
      LED_MATRIX.setRow(0, 1, smile_neutral[1]);
      LED_MATRIX.setRow(0, 2, smile_neutral[2]);
      LED_MATRIX.setRow(0, 3, smile_neutral[3]);
      LED_MATRIX.setRow(0, 4, smile_neutral[4]);
      LED_MATRIX.setRow(0, 5, smile_neutral[5]);
      LED_MATRIX.setRow(0, 6, smile_neutral[6]);
      LED_MATRIX.setRow(0, 7, smile_neutral[7]);
    } else if ((m1 >= AVG_MOISTURE) && (m1 < MAX_MOISTURE))
    {
      LED_MATRIX.setRow(0, 0, smile_happy[0]);
      LED_MATRIX.setRow(0, 1, smile_happy[1]);
      LED_MATRIX.setRow(0, 2, smile_happy[2]);
      LED_MATRIX.setRow(0, 3, smile_happy[3]);
      LED_MATRIX.setRow(0, 4, smile_happy[4]);
      LED_MATRIX.setRow(0, 5, smile_happy[5]);
      LED_MATRIX.setRow(0, 6, smile_happy[6]);
      LED_MATRIX.setRow(0, 7, smile_happy[7]);
    } else if (m1 >= MAX_MOISTURE)
    {
      LED_MATRIX.setRow(0, 0, smile_warning[0]);
      LED_MATRIX.setRow(0, 1, smile_warning[1]);
      LED_MATRIX.setRow(0, 2, smile_warning[2]);
      LED_MATRIX.setRow(0, 3, smile_warning[3]);
      LED_MATRIX.setRow(0, 4, smile_warning[4]);
      LED_MATRIX.setRow(0, 5, smile_warning[5]);
      LED_MATRIX.setRow(0, 6, smile_warning[6]);
      LED_MATRIX.setRow(0, 7, smile_warning[7]);
    }
  } else if (smile_type == 1)
  {
    LED_MATRIX.setRow(0, 0, smile_sad[0]);
    LED_MATRIX.setRow(0, 1, smile_sad[1]);
    LED_MATRIX.setRow(0, 2, smile_sad[2]);
    LED_MATRIX.setRow(0, 3, smile_sad[3]);
    LED_MATRIX.setRow(0, 4, smile_sad[4]);
    LED_MATRIX.setRow(0, 5, smile_sad[5]);
    LED_MATRIX.setRow(0, 6, smile_sad[6]);
    LED_MATRIX.setRow(0, 7, smile_sad[7]);
  }
  else if (smile_type == 2)
  {
    LED_MATRIX.setRow(0, 0, smile_happy[0]);
    LED_MATRIX.setRow(0, 1, smile_happy[1]);
    LED_MATRIX.setRow(0, 2, smile_happy[2]);
    LED_MATRIX.setRow(0, 3, smile_happy[3]);
    LED_MATRIX.setRow(0, 4, smile_happy[4]);
    LED_MATRIX.setRow(0, 5, smile_happy[5]);
    LED_MATRIX.setRow(0, 6, smile_happy[6]);
    LED_MATRIX.setRow(0, 7, smile_happy[7]);
  }
  else if (smile_type == 3)
  {
    smile_blink = 1 - smile_blink;
    if (smile_blink == 0)
    {
      LED_MATRIX.setRow(0, 0, smile_off[0]);
      LED_MATRIX.setRow(0, 1, smile_off[1]);
      LED_MATRIX.setRow(0, 2, smile_off[2]);
      LED_MATRIX.setRow(0, 3, smile_off[3]);
      LED_MATRIX.setRow(0, 4, smile_off[4]);
      LED_MATRIX.setRow(0, 5, smile_off[5]);
      LED_MATRIX.setRow(0, 6, smile_off[6]);
      LED_MATRIX.setRow(0, 7, smile_off[7]);
    }
    else
    {
      LED_MATRIX.setRow(0, 0, smile_on[0]);
      LED_MATRIX.setRow(0, 1, smile_on[1]);
      LED_MATRIX.setRow(0, 2, smile_on[2]);
      LED_MATRIX.setRow(0, 3, smile_on[3]);
      LED_MATRIX.setRow(0, 4, smile_on[4]);
      LED_MATRIX.setRow(0, 5, smile_on[5]);
      LED_MATRIX.setRow(0, 6, smile_on[6]);
      LED_MATRIX.setRow(0, 7, smile_on[7]);
    }
  }
}

// Execute Telegram events
void Telegram_ExecMessages()
{
  for (int i = 1; i < bot.message[0][0].toInt() + 1; i++)
  {
    bot.message[i][5] = bot.message[i][5].substring(0, bot.message[i][5].length());
    String str1 = bot.message[i][5];
    str1.toUpperCase();
    Serial.println("Message: " + str1);
    if ((str1 == "START") || (str1 == "\/START"))
    {
      bot.sendMessage(bot.message[i][4], "Привет! Я цветочек!", "");
    }
    else if ((str1 == "STOP") || (str1 == "\/STOP"))
    {
      bot.sendMessage(bot.message[i][4], "Пока!", "");
    }
    else if ((str1 == "AIR") || (str1 == "WEATHER"))
    {
      bot.sendMessage(bot.message[i][4], "Температура воздуха: " + String(t1, 1) + " *C", "");
      bot.sendMessage(bot.message[i][4], "Влажность воздуха: " + String(h1, 1) + " %", "");
    }
    else if ((str1 == "AIR TEMP") || (str1 == "AIR TEMPERATURE"))
    {
      bot.sendMessage(bot.message[i][4], "Температура воздуха: " + String(t1, 1) + " *C", "");
    }
    else if ((str1 == "AIR HUM") || (str1 == "AIR HUMIDITY"))
    {
      bot.sendMessage(bot.message[i][4], "Влажность воздуха: " + String(h1, 1) + " %", "");
    }
    else if ((str1 == "SOIL") || (str1 == "GROUND"))
    {
      bot.sendMessage(bot.message[i][4], "Температура почвы: " + String(t2, 1) + " *C", "");
      bot.sendMessage(bot.message[i][4], "Влажность почвы: " + String(m1, 1) + " %", "");
    }
    else if ((str1 == "SOIL TEMP") || (str1 == "SOIL TEMPERATURE"))
    {
      bot.sendMessage(bot.message[i][4], "Температура почвы: " + String(t2, 1) + " *C", "");
    }
    else if ((str1 == "SOIL HUM") || (str1 == "SOIL HUMIDITY") || (str1 == "SOIL MOISTURE"))
    {
      bot.sendMessage(bot.message[i][4], "Влажность почвы: " + String(m1, 1) + " %", "");
    }
    else if ((str1 == "LIGHT") || (str1 == "LUMINOSITY") || (str1 == "BRIGHT") || (str1 == "BRIGHTNESS"))
    {
      bot.sendMessage(bot.message[i][4], "Освещенность: " + String(l1, 1) + " lx", "");
    }
    else if ((str1 == "HI") || (str1 == "HI!") || (str1 == "HELLO") || (str1 == "HELLO!"))
    {
      bot.sendMessage(bot.message[i][4], "Привет! Я цветочек!", "");
    }
    else if ((str1 == "BYE") || (str1 == "BYE!") || (str1 == "BYE-BYE") || (str1 == "BYE-BYE!"))
    {
      bot.sendMessage(bot.message[i][4], "Пока!", "");
    }
    else if ((str1 == "HOW DO YOU DO") || (str1 == "HOW DO YOU DO?") || (str1 == "HOW ARE YOU") || (str1 == "HOW ARE YOU?")
             || (str1 == "HRU") || (str1 == "HRU?"))
    {
      if (m1 < MIN_MOISTURE)
      {
        bot.sendMessage(bot.message[i][4], "Я уже засох, поливай быстрей!", "");
      } else if ((m1 >= MIN_MOISTURE) && (m1 < AVG_MOISTURE))
      {
        bot.sendMessage(bot.message[i][4], "Ну желательно бы чуть-чуть полить", "");
      } else if ((m1 >= AVG_MOISTURE) && (m1 < MAX_MOISTURE))
      {
        bot.sendMessage(bot.message[i][4], "У меня всё нормально, а у тебя?", "");
      } else if (m1 >= MAX_MOISTURE)
      {
        bot.sendMessage(bot.message[i][4], "Меня залили, давай вытирай подоконник!", "");
      }
    }
    else if ((str1 == "WHERE ARE YOU") || (str1 == "WHERE R U") || (str1 == "WHERE ARE YOU?") || (str1 == "WHERE R U?"))
    {
      bot.sendMessage(bot.message[i][4], "Привет, я в Ташкенте на M2M КОНФЕРЕНЦИЯ UCELL \"Технологии будущего\"", "");
    }

    else if ((str1 == "YOU LIKE IT") || (str1 == "U LIKE IT") || (str1 == "YOU LIKE IT?") || (str1 == "U LIKE IT?"))
    {
      bot.sendMessage(bot.message[i][4], "Мне нравится Ташкент :)", "");
    }

    else if ((str1 == "WHAT IS IOT") || (str1 == "IOT") || (str1 == "WHAT IS IOT?") || (str1 == "IOT?"))
    {
      bot.sendMessage(bot.message[i][4], "Интернет вещей — концепция  вычислительной сети физических предметов («вещей»), оснащённых встроенными технологиями для взаимодействия друг с другом или с внешней средой, рассматривающая организацию таких сетей как явление, способное перестроить экономические и общественные процессы, исключающее из части действий и операций необходимость участия человека", "");
    }
    else if ((str1 == ":(") || (str1 == ":-("))
    {
      smile_type = 1;
      bot.sendMessage(bot.message[i][4], "Чё такой грустный?", "");
    }
    else if ((str1 == ":)") || (str1 == ":-)"))
    {
      smile_type = 2;
      bot.sendMessage(bot.message[i][4], "С чего радуемся?", "");
    }
    else if (str1 == "BLINK")
    {
      smile_type = 3;
      bot.sendMessage(bot.message[i][4], "Ну ладно, помигаем", "");
    }
    else if ((str1 == "H2O") || (str1 == "WATER") || (str1 == "PUMP"))
    {
      bot.sendMessage(bot.message[i][4], "Поливаем 5 секунд...", "");
      digitalWrite(RELAY_PIN, true);
      delay(5000);
      digitalWrite(RELAY_PIN, false);
      pump1 = 1;
      bot.sendMessage(bot.message[i][4], "Всё, цветок полит!", "");
    }
    else
    {
      if (random(3) == 0)
      {
        bot.sendMessage(bot.message[i][4], "Чё?", "");
      }
      else if (random(3) == 1)
      {
        bot.sendMessage(bot.message[i][4], "Не понял", "");
      }
      else
      {
        bot.sendMessage(bot.message[i][4], "Не знаю такого", "");
      }
    }
  }
  bot.message[0][0] = "";
}

