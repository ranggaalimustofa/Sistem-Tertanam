#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// internet port
AsyncWebServer server(80);

// default hostserver
String serverHost = "192.168.0.100";

// variable declaration
const int infrared1 = 13;
const int infrared2 = 14;
const int resetButton = 12;
const int buzz = 25;
boolean Object1 = false;
boolean Object2 = false;
int hitung = 0;

// I2C LCD configuration
LiquidCrystal_I2C lcd(0x27, 16, 2);

// declaration task
TaskHandle_t task1, task2, task3, task4, task5, taskResetButton;

void readInfrared1(void *parameter);
void readInfrared2(void *parameter);
void checkWiFiConnection(void *parameter);
void displayWiFiInfo(void *parameter);
void sendDataToServer(void *parameter);
void resetButtonTask(void *parameter);

void buzzer();

void setup()
{
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.print("Initializing...");

  // Create an instance of WiFiManager
  WiFiManager wifiManager;

  // Try to connect to the stored WiFi network
  if (!wifiManager.autoConnect("Cuman Pemula IoT"))
  {
    Serial.println("Failed to connect, we should reset and see if it connects");
    lcd.clear();
    lcd.print("Please Connet");
    lcd.setCursor(0, 1);
    lcd.print("to WIFI");
    delay(3000);
    ESP.restart();
    delay(5000);
  }

  // Connected to Wi-Fi
  Serial.println("Connected to Wi-Fi!");
  buzzer();
  buzzer();

  // Initilize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error occurred while mounting SPIFFS");
    return;
  }

  // Setup the web server
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // read HTML response
    File file = SPIFFS.open("/index.html", "r");
    if (file) {
      request->send(200, "text/html", file.readString());
      file.close();
    } else {
      request->send(404, "text/plain", "File not found");
    } });

  server.serveStatic("/set", SPIFFS, "/");

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    // for saved configuration
    if (request->hasParam("host", true)) {
      serverHost = request->getParam("host", true)->value();
      request->send(200, "text/plain", "Saved");
      delay(3000);
    } else {
      request->send(400, "text/plain", "Bad Request");
    } });

  server.serveStatic("/", SPIFFS, "/");

  server.begin();

  pinMode(infrared1, INPUT);
  pinMode(infrared2, INPUT);
  pinMode(buzz, OUTPUT);
  pinMode(resetButton, INPUT_PULLUP);

  xTaskCreatePinnedToCore(readInfrared1, "ReadInfrared1", 10000, NULL, 1, &task1, 1);
  xTaskCreatePinnedToCore(readInfrared2, "ReadInfrared2", 10000, NULL, 1, &task2, 1);
  xTaskCreatePinnedToCore(checkWiFiConnection, "CheckWiFiConnection", 10000, NULL, 1, &task3, 1);
  xTaskCreatePinnedToCore(displayWiFiInfo, "DisplayWiFiInfo", 5000, NULL, 1, &task4, 1);
  xTaskCreatePinnedToCore(sendDataToServer, "SendDataToServer", 20000, NULL, 1, &task5, 1);
  xTaskCreatePinnedToCore(resetButtonTask, "ResetButtonTask", 10000, NULL, 1, &taskResetButton, 1);
}

void loop()
{

  vTaskDelay(1000 / portTICK_PERIOD_MS);
}

// task button reset
void resetButtonTask(void *parameter)
{
  while (1)
  {
    if (digitalRead(resetButton) == LOW && hitung > 0)
    {
      hitung = 0;
      ESP_LOGI("RESET", "Reset Button Pressed!");
      buzzer();
      buzzer();
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// data ascending
void readInfrared1(void *parameter)
{
  while (1 && WiFi.status() == WL_CONNECTED)
  {
    int readSensor1 = digitalRead(infrared1);

    if (readSensor1 == 0 && Object1 == false)
    {
      hitung++;
      Object1 = true;
      Serial.print("hitung = ");
      Serial.println(hitung);
      buzzer();
    }
    else if (readSensor1 == 1 && Object1 == true)
    {
      Object1 = false;
    }

    delay(10);
  }
}

// data descending
void readInfrared2(void *parameter)
{
  while (1 && WiFi.status() == WL_CONNECTED)
  {
    int readSensor2 = digitalRead(infrared2);

    if (readSensor2 == 0 && Object2 == false)
    {
      if (hitung > 0)
      {
        hitung--;
        Object2 = true;
        Serial.print("hitung = ");
        Serial.println(hitung);
        buzzer();
      }
    }
    else if (readSensor2 == 1 && Object2 == true)
    {
      Object2 = false;
    }

    delay(10);
  }
}

// task chacking connection
void checkWiFiConnection(void *parameter)
{
  while (1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      lcd.println("WiFi connection lost!");
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// task display
void displayWiFiInfo(void *parameter)
{
  while (1)
  {
    lcd.clear();
    if (String(serverHost) == "192.168.0.100")
    {
      lcd.print("WIFI: Connected");
    }
    else
    {
      lcd.print("Hs:" + String(serverHost));
    }
    lcd.setCursor(0, 1);
    lcd.print("IP:" + WiFi.localIP().toString());

    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}

// task sending to server
void sendDataToServer(void *parameter)
{
  while (1)
  {
    HTTPClient http;

    if (WiFi.status() == WL_CONNECTED)
    {
      http.begin("http://" + String(serverHost) + "/ManagementStockBarang/proses.php?hitung=" + String(hitung));
      int httpCode = http.GET();

      if (httpCode > 0)
      {
        char json[100];
        String payload = http.getString();
        payload.toCharArray(json, 100);

        DynamicJsonDocument doc(JSON_OBJECT_SIZE(2));
        deserializeJson(doc, json);

        // Process the received data if needed
      }
      http.end();
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// bip function
void buzzer()
{
  digitalWrite(buzz, HIGH);
  delay(100);
  digitalWrite(buzz, LOW);
  delay(100);
}