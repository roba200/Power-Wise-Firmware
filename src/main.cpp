#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>
#include <Adafruit_ADS1X15.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <ZMPT101B.h>

String DEVICE_ID = "vwcBliq4fCGSPyn9uaPQ";

#define API_KEY "AIzaSyA31Jxretax4diOiDKizzVU-ck5Df5jf3g"
#define DATABASE_URL "powerwise00-default-rtdb.europe-west1.firebasedatabase.app"

#define USER_EMAIL "aryantfk5@gmail.com"
#define USER_PASSWORD "zxcvbnm123"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define ROOM1_RELAY 12
#define ROOM2_RELAY 13
#define ROOM3_RELAY 14
#define ROOM4_RELAY 15

#define WIFI_RESET_BTN 32
#define SENSOR_CALIBRATION_PIN 5

#define SENSITIVITY 500.0f

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_ADS1015 ads;
ZMPT101B voltageSensor(33, 50.0);
WiFiManager wifiManager;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData stream;

unsigned long sendDataPrevMillis = 0;
uint32_t idleTimeForStream = 1000;

float voltage = 0;

float room1_current = 0;
float room1_power = 0;
float room1_energy = 0;
float room1_threshold = 0;
int room1_relay = 0;

float room2_current = 0;
float room2_power = 0;
float room2_energy = 0;
float room2_threshold = 0;
int room2_relay = 0;

float room3_current = 0;
float room3_power = 0;
float room3_energy = 0;
float room3_threshold = 0;
int room3_relay = 0;

float room4_current = 0;
float room4_power = 0;
float room4_energy = 0;
float room4_threshold = 0;
int room4_relay = 0;

float room1_cal_volt = 0;
float room2_cal_volt = 0;
float room3_cal_volt = 0;
float room4_cal_volt = 0;

volatile bool dataChanged = false;

int offset1 = 0;
int offset2 = 0;
int offset3 = 0;
int offset4 = 0;

String parentPath = "/" + DEVICE_ID;
String childPath[8] = {"/room1_relay", "/room1_threshold", "/room2_relay", "/room2_threshold", "/room3_relay", "/room3_threshold", "/room4_relay", "/room4_threshold"};

String getADC(int n)
{
  return (String)ads.readADC_SingleEnded(n);
}

void sensorCalibrateDisplay()
{
  display.clearDisplay();
  display.setCursor(2, 28);
  display.println("Sensor Calibrating...");
  display.display();
}

void updateDisplay()
{

  // Serial.println(energy,10);
  display.setCursor(5, 4);
  display.println("Live Power Metrics");
  display.setCursor(19, 16);
  display.print("Voltage:");
  display.print(voltage);
  display.println("V");
  display.setCursor(25, 27);
  display.print("Current:");
  display.print(room1_current + room2_current + room3_current + room4_current);
  display.println("A");
  display.setCursor(25, 38);
  display.print("Power:");
  display.print(voltage * (room1_current + room2_current + room3_current + room4_current));
  display.println("Kw");
  display.display();
  display.clearDisplay();
}

int autoCalibrate(byte channel)
{ // required time, around 1 ms
  int _adc = 0, _sample = 0;
  int offset = 0;
  for (int i = 0; i < 20; i++)
  {
    _adc += ads.readADC_SingleEnded(channel);
    _sample++;
  }

  offset = _adc / 20.00; // average of 100 samples
  return offset;
}

float getAC(byte channel, float _freq, float _n_total_period, float results_adjuster, int offset)
{                                           // It can measure minimum 0.5 maximum
  float _signal_period = 1000000.0 / _freq; // Hz -> us
  _signal_period *= _n_total_period;

  //  Here, +8us confirms the last iteration. According to Arduino documentation,
  //  the micros() function has a resolution of 4us.
  unsigned long _total_time = (unsigned long)_signal_period + 8;
  unsigned long _start_time = micros();

  unsigned long _adc_sqr_sum = 0;
  unsigned int _adc_count = 0;
  long _adc;

  while (micros() - _start_time < _total_time)
  { // it capable for maximum 4096 samples.
    _adc_count++;
    _adc = ads.readADC_SingleEnded(channel) - offset;
    _adc_sqr_sum += _adc * _adc;
  }

  float _avg_adc_sqr_sum = _adc_sqr_sum / (float)_adc_count;
  float _rms_current = sqrt(_avg_adc_sqr_sum) * results_adjuster; // ADC x A/ADC -> A
  return _rms_current;
}

void autoCuttOff()
{
  if (room1_relay == 1)
  {
    if (room1_current >= room1_threshold)
    {
      digitalWrite(ROOM1_RELAY, LOW);
      Firebase.RTDB.setIntAsync(&fbdo, DEVICE_ID + "/room1_relay", 0);
    }
    else if (room1_relay == 0)
    {
      digitalWrite(ROOM1_RELAY, HIGH);
    }
  }
  else
  {
    digitalWrite(ROOM1_RELAY, LOW);
  }

  if (room2_relay == 1)
  {
    if (room2_current >= room2_threshold)
    {
      digitalWrite(ROOM2_RELAY, LOW);
      Firebase.RTDB.setIntAsync(&fbdo, DEVICE_ID + "/room2_relay", 0);
    }
    else if (room2_relay == 0)
    {
      digitalWrite(ROOM2_RELAY, HIGH);
    }
  }
  else
  {
    digitalWrite(ROOM2_RELAY, LOW);
  }

  if (room3_relay == 1)
  {
    if (room3_current >= room3_threshold)
    {
      digitalWrite(ROOM1_RELAY, LOW);
      Firebase.RTDB.setIntAsync(&fbdo, DEVICE_ID + "/room3_relay", 0);
    }
    else if (room3_relay == 0)
    {
      digitalWrite(ROOM3_RELAY, HIGH);
    }
  }
  else
  {
    digitalWrite(ROOM3_RELAY, LOW);
  }

  if (room4_relay == 1)
  {
    if (room4_current >= room4_threshold)
    {
      digitalWrite(ROOM4_RELAY, LOW);
      Firebase.RTDB.setIntAsync(&fbdo, DEVICE_ID + "/room4_relay", 0);
    }
    else if (room4_relay == 0)
    {
      digitalWrite(ROOM4_RELAY, HIGH);
    }
  }
  else
  {
    digitalWrite(ROOM4_RELAY, LOW);
  }
}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}

void streamCallback(MultiPathStream stream)
{
  size_t numChild = sizeof(childPath) / sizeof(childPath[0]);

  for (size_t i = 0; i < numChild; i++)
  {
    if (stream.get(childPath[i]))
    {
      if (stream.dataPath == "/room1_threshold")
      {
        room1_threshold = stream.value.toFloat();
        Serial.println(stream.value.toFloat());
      }
      if (stream.dataPath == "/room1_relay")
      {
        room1_relay = stream.value.toInt();
        Serial.println(stream.value.toInt());
      }

      if (stream.dataPath == "/room2_threshold")
      {
        room2_threshold = stream.value.toFloat();
        Serial.println(stream.value.toFloat());
      }
      if (stream.dataPath == "/room2_relay")
      {
        room2_relay = stream.value.toInt();
        Serial.println(stream.value.toInt());
      }

      if (stream.dataPath == "/room3_threshold")
      {
        room3_threshold = stream.value.toFloat();
        Serial.println(stream.value.toFloat());
      }
      if (stream.dataPath == "/room3_relay")
      {
        room3_relay = stream.value.toInt();
        Serial.println(stream.value.toInt());
      }

      if (stream.dataPath == "/room4_threshold")
      {
        room4_threshold = stream.value.toFloat();
        Serial.println(stream.value.toFloat());
      }
      if (stream.dataPath == "/room4_relay")
      {
        room4_relay = stream.value.toInt();
        Serial.println(stream.value.toInt());
      }

      if (room1_relay == 1)
      {
        digitalWrite(ROOM1_RELAY, HIGH);
      }
      else if (room1_relay == 0)
      {
        digitalWrite(ROOM1_RELAY, LOW);
      }

      if (room2_relay == 1)
      {
        digitalWrite(ROOM2_RELAY, HIGH);
      }
      else if (room2_relay == 0)
      {
        digitalWrite(ROOM2_RELAY, LOW);
      }

      if (room3_relay == 1)
      {
        digitalWrite(ROOM3_RELAY, HIGH);
      }
      else if (room3_relay == 0)
      {
        digitalWrite(ROOM3_RELAY, LOW);
      }

      if (room4_relay == 1)
      {
        digitalWrite(ROOM4_RELAY, HIGH);
      }
      else if (room4_relay == 0)
      {
        digitalWrite(ROOM4_RELAY, LOW);
      }
    }
  }
  dataChanged = true;
}

void updateRoomData()
{
  FirebaseJson json;
  voltage = voltageSensor.getRmsVoltage();

  json.add("voltage", voltage);
  json.add("totalCurrent", room1_current + room2_current + room3_current + room4_current);
  json.add("totalPower", voltage * (room1_current + room2_current + room3_current + room4_current));

  json.add("room1_current", room1_current);
  json.add("room1_power", voltage * room1_current);
  json.add("room1_energy", room1_energy);

  json.add("room2_current", room2_current);
  json.add("room2_power", voltage * room2_current);
  json.add("room2_energy", room2_energy);

  json.add("room3_current", room3_current);
  json.add("room3_power", voltage * room3_current);
  json.add("room3_energy", room3_energy);

  json.add("room4_current", room4_current);
  json.add("room4_power", voltage * room4_current);
  json.add("room4_energy", room4_energy);

  Firebase.RTDB.updateNodeSilentAsync(&fbdo, DEVICE_ID, &json);
}

void displayConfigMode()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(30, 4);
  display.println(F("CONFIG MODE"));
  display.setCursor(18, 17);
  display.println(F("Connect to WIFI"));
  display.setCursor(13, 30);
  display.println(F("PowerWise & Go to"));
  display.setCursor(11, 44);
  display.print(F("http://"));
  display.println(WiFi.softAPIP());
  display.display();
}

void displayfailedConnect()
{
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Failed to connect"));
  display.display();
  Serial.println("failed to connect and hit timeout");
  ESP.restart();
  delay(1000);
}

void displayConnected()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(38, 4);
  display.println("WOO HAA!");
  display.setCursor(25, 24);
  display.println("Connected to:");
  String line = WiFi.SSID();
  int x = (display.width() - (line.length() * 6)) / 2;
  display.setCursor(x, 35);
  display.println(line);
  display.display();
}

void displayNotConnected()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Not connected");
  display.display();
}

void configModeCallback(WiFiManager *myWiFiManager)
{
  displayConfigMode();
}

void loop2(void *pvParameters)
{
  sensorCalibrateDisplay();
  offset1 = autoCalibrate(0);
  offset2 = autoCalibrate(1);
  offset3 = autoCalibrate(2);
  offset4 = autoCalibrate(3);

  while (1)
  {
    float current1 = getAC(0, 50, 20, 0.09, offset1);
    float current2 = getAC(1, 50, 20, 0.09, offset2);
    float current3 = getAC(2, 50, 20, 0.09, offset3);
    float current4 = getAC(3, 50, 20, 0.09, offset4);

    if (current1 < 0.1)
    {
      room1_current = 0;
    }
    else
    {
      room1_current = current1;
    }
    if (current2 < 0.1)
    {
      room2_current = 0;
    }
    else
    {
      room2_current = current2;
    }
    if (current3 < 0.1)
    {
      room3_current = 0;
    }
    else
    {
      room3_current = current3;
    }
    if (current4 < 0.1)
    {
      room4_current = 0;
    }
    else
    {
      room4_current = current4;
    }

    Serial.println(room1_current);

    // Serial.println(getADC(1));
    // delay(100);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(WIFI_RESET_BTN, INPUT_PULLUP);
  pinMode(SENSOR_CALIBRATION_PIN, INPUT_PULLUP);
  pinMode(ROOM1_RELAY, OUTPUT);
  pinMode(ROOM2_RELAY, OUTPUT);
  pinMode(ROOM3_RELAY, OUTPUT);
  pinMode(ROOM4_RELAY, OUTPUT);

  ads.setGain(GAIN_ONE);
  ads.begin();

  voltageSensor.setSensitivity(SENSITIVITY);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  wifiManager.setAPCallback(configModeCallback);

  if (!wifiManager.autoConnect("PowerWise"))
  {
    displayfailedConnect();
  }

  if (WiFi.status() == WL_CONNECTED)
  {

    displayConnected();
    config.api_key = API_KEY;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectNetwork(true);
    fbdo.keepAlive(5, 5, 1);

    if (!Firebase.RTDB.beginMultiPathStream(&stream, parentPath))
    {
      Serial.printf("sream begin error, %s\n\n", stream.errorReason().c_str());
    }
    else
    {
      Serial.println("stream begin Success");
    }

    Firebase.RTDB.setMultiPathStreamCallback(&stream, streamCallback, streamTimeoutCallback);
  }
  else
  {
    displayNotConnected();
  }

  xTaskCreatePinnedToCore(
      loop2,   // Function to implement the task
      "loop2", // Name of the task
      20000,   // Stack size in bytes
      NULL,    // Task input parameter
      1,       // Priority of the task
      NULL,    // Task handle.
      0        // Core where the task should run
  );
}

void loop()
{
  // room1_current = getAC(0, 50, 20, 0.09, offset1) - 0.15;

  // room3_current = getAC(2, 50, 20, 0.09, offset3) - 0.15;
  // room4_current = getAC(3, 50, 20, 0.09, offset4) - 0.15;

  if (dataChanged)
  {
    dataChanged = false;
  }

  if (Firebase.ready() && (millis() - sendDataPrevMillis > idleTimeForStream || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();
    updateRoomData();
  }

  if (digitalRead(WIFI_RESET_BTN) == LOW)
  {
    wifiManager.resetSettings();
    ESP.restart();
  }

  if (digitalRead(SENSOR_CALIBRATION_PIN) == LOW)
  {
    sensorCalibrateDisplay();
    offset1 = autoCalibrate(0);
    offset2 = autoCalibrate(1);
    offset3 = autoCalibrate(2);
    offset4 = autoCalibrate(3);
  }

  autoCuttOff();
  updateDisplay();

  if (!fbdo.httpConnected())
  {
    // Server was disconnected!
  }
}
