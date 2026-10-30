#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Adafruit_SH110X.h>

String getBatteryJson();
void uploadToSupabase();

unsigned long lastUpload = 0;
unsigned long lastWeatherUpdate = 0;
unsigned long lastScreenChange = 0;
unsigned long lastPoll = 0;

const char* WIFI_SSID = ""; //input wifi name
const char* WIFI_PASSWORD = ""; //input password of wifi
const char* SUPABASE_URL =
""; //input supabase url


const char* SUPABASE_KEY =
""; //input supabase anon key here

//need to enabled rls in supabase

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,-1);
#define BUZZER_PIN 25



struct BatteryData {
  float packVoltage = 0;
  float current = 0;
  float remainingCapacity = 0;
  float ratedCapacity = 0;
  float cell1 = 0;
  float cell2 = 0;
  float cell3 = 0;
  float cell4 = 0;

  uint16_t cycles = 0;
  float soc = 0;

  String deviceName;
};

bool bluetoothConnected = false;
bool wifiConnected = false;
bool doConnect = false;
bool readyToUpload = false;



BatteryData battery;

DHT dht(26, DHT11);

float roomTemp = 0;
float roomHum = 0;

String weatherText = "--";
float weatherTemp = 0;
float weatherRain = 0;
int weatherCode = 0;



int currentScreen = 1;


TaskHandle_t commsTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;

static NimBLEUUID SERVICE_UUID("0000ff00-0000-1000-8000-00805f9b34fb"); //get the actual sevice uuid of the ble device
static NimBLEUUID RX_UUID("0000ff01-0000-1000-8000-00805f9b34fb"); //common receive data for decode
static NimBLEUUID TX_UUID("0000ff02-0000-1000-8000-00805f9b34fb"); //common send data to the bms module

NimBLEAdvertisedDevice* bmsDevice = nullptr;
NimBLERemoteCharacteristic* writeChar = nullptr;
NimBLEClient* client = nullptr;




uint8_t cmdBasicInfo[] = {
  0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77
};

uint8_t cmdCellVoltage[] = {
  0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77
};




String getBatteryJson() {

  String json = "{";

  json += "\"device\":\"" + battery.deviceName + "\",";

  json += "\"voltage\":" +
          String(battery.packVoltage, 2) + ",";

  json += "\"current\":" +
          String(battery.current, 2) + ",";

  json += "\"remainingah\":" +
          String(battery.remainingCapacity, 2) + ",";

  json += "\"ratedah\":" +
          String(battery.ratedCapacity, 2) + ",";

  json += "\"soc\":" +
          String(battery.soc, 1) + ",";

  json += "\"cycles\":" +
          String(battery.cycles) + ",";

  json += "\"cell1\":" +
          String(battery.cell1, 3) + ",";

  json += "\"cell2\":" +
          String(battery.cell2, 3) + ",";

  json += "\"cell3\":" +
          String(battery.cell3, 3) + ",";

  json += "\"cell4\":" +
          String(battery.cell4, 3);

  json += "}";

  return json;
}
void uploadToSupabase() {

  Serial.println("=== uploadToSupabase() ===");

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi NOT connected");
    return;
  }

  String json = getBatteryJson();

  Serial.println("Posting To:");
  Serial.println(SUPABASE_URL);

  Serial.println("Uploading JSON:");
  Serial.println(json);

  HTTPClient http;

  http.begin(SUPABASE_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization",
                 "Bearer " + String(SUPABASE_KEY));

  http.addHeader("Prefer",
                 "return=representation");

  int httpCode = http.POST(json);

  Serial.print("HTTP Response: ");
  Serial.println(httpCode);

  String response =
      http.getString();

  Serial.println("Response:");
  Serial.println(response);

  http.end();
}

void notifyCallback(
    NimBLERemoteCharacteristic* pRemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {

  

  // CMD03
  if (length >= 20 &&
      pData[0] == 0xDD &&
      pData[1] == 0x03) {

    battery.packVoltage =
      ((pData[4] << 8) | pData[5]) / 100.0;
    battery.remainingCapacity =
  ((pData[8] << 8) | pData[9]) / 100.0;

battery.ratedCapacity =
  ((pData[10] << 8) | pData[11]) / 100.0;
if (battery.ratedCapacity > 0) {
  battery.soc =
  (battery.remainingCapacity /
   battery.ratedCapacity) * 100.0;
} else {
  battery.soc = 0;
}


    int16_t rawCurrent =
      (int16_t)((pData[6] << 8) | pData[7]);

    battery.current =
      rawCurrent / 100.0;

    battery.cycles =
      (pData[12] << 8) | pData[13];
  }

  // CMD04
  if (length >= 12 &&
      pData[0] == 0xDD &&
      pData[1] == 0x04) {

    battery.cell1 =
      ((pData[4] << 8) | pData[5]) / 1000.0;

    battery.cell2 =
      ((pData[6] << 8) | pData[7]) / 1000.0;

    battery.cell3 =
      ((pData[8] << 8) | pData[9]) / 1000.0;

    battery.cell4 =
      ((pData[10] << 8) | pData[11]) / 1000.0;
    
 
    readyToUpload = true;

  }

}

class ScanCallbacks : public NimBLEScanCallbacks {

public:
  void onResult(const NimBLEAdvertisedDevice* device) override {

    if (device->haveName() &&
        device->getName() == "DP04S007L4S100A") {

      Serial.println("BMS FOUND!");

      if (bmsDevice) {
        delete bmsDevice;
        bmsDevice = nullptr;
      }
      bmsDevice =
        new NimBLEAdvertisedDevice(*device);

      doConnect = true;

      NimBLEDevice::getScan()->stop();
    }
  }
};

void beep(int duration) {

  digitalWrite(BUZZER_PIN, LOW);
  delay(duration);
  digitalWrite(BUZZER_PIN, HIGH);
}
void beepPattern(
  int count,
  int duration
) {
  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, HIGH); //check if buzzer is active high or low
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    delay(duration);
  }
}

unsigned long lastAlarm = 0;

void checkAlarms() {
  if (battery.soc < 10 && millis() - lastAlarm > 30000) {
    beepPattern(3, 1000);
    lastAlarm = millis();
  }
  if (battery.soc >= 10 && battery.soc < 20 && millis() - lastAlarm > 30000) {
    beepPattern(1, 500);
    lastAlarm = millis();
  }
 
  if (!bluetoothConnected) {

    static unsigned long lastBTAlarm = 0;

    if (millis() - lastBTAlarm > 10000) {

    beepPattern(5, 100);

    lastBTAlarm = millis();

      }
    }
  if (!wifiConnected) {

    static unsigned long lastWiFiAlarm = 0;

    if (millis() - lastWiFiAlarm > 30000) {

    beepPattern(3, 200);

    lastWiFiAlarm = millis();

    }
  }

  float minCell =
  min(
      min(battery.cell1,battery.cell2),
      min(battery.cell3,battery.cell4)
  );

  float maxCell =
  max(
      max(battery.cell1,battery.cell2),
      max(battery.cell3,battery.cell4)
  );

  float delta =
  maxCell - minCell;

  if (delta > 0.05) {

    static unsigned long lastDeltaAlarm = 0;

    if (millis() - lastDeltaAlarm > 60000) {

        beepPattern(2,200);

        lastDeltaAlarm = millis();

    }
}
  if (roomTemp > 35 &&
    millis() - lastAlarm > 30000) {

    beepPattern(1,1000);

    lastAlarm = millis();
  }

  if (roomHum > 60 &&
    millis() - lastAlarm > 30000) {

  beepPattern(5,500);

  lastAlarm = millis();
  }
}

void updateOLED() {
  
  float power =
    battery.packVoltage *
    battery.current;

  float abspower = abs(power);

  float remainingWH =
  battery.remainingCapacity *
  battery.packVoltage;
  

  float fullWh =
  battery.ratedCapacity *
  battery.packVoltage;
  float missingWh =
  fullWh - remainingWH;

  float hours = 0;

  if (abspower > 0.1) {

      if (battery.current > 0) {
        hours = missingWh / abspower;

  }  else if (battery.current < 0) {
    hours = remainingWH /abspower;
  } 
  }

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0,0);
  display.print("BMS");

display.setCursor(35,0);

display.print(
  bluetoothConnected
  ? "BT"
  : "BX"
);

display.setCursor(55,0);

display.print(
  wifiConnected
  ? "WF"
  : "WX"
);

  display.setCursor(0,12);
  display.print("SOC: ");
  display.print(battery.soc,1);
  display.println("%");
  display.setCursor(70,12);
  display.print((int)remainingWH);
  display.print("Wh");

  display.setCursor(0,22);
  display.print("V: ");
  display.print(battery.packVoltage,2);
  display.println("V");

  display.setCursor(64,22);
  display.print("I: ");
  display.print(battery.current,2);
  display.println("A");

  int hrs = (int)hours;
  int mins = (int)((hours -hrs) * 60);
  if (mins >= 60) {
    hrs++;
    mins = 0;
  }

  display.setCursor(0,32);
  display.print("P: ");
  display.print(power,1);
  display.println("W");

  display.setCursor(0,42);
  display.drawRect(0, 40, 90, 14, WHITE);
  display.fillRect(90, 44, 4, 6, WHITE);
  
  int fillWidth = constrain((battery.soc / 100.0) * 88,
    0,
    88
  );

  if (battery.soc < 10) {
    if ((millis() / 500) % 2) {
      display.fillRect(
        1,
        41,
        fillWidth,
        12,
        WHITE
      );

    }

  }
  else {
    display.fillRect(
      1,
      41,
      fillWidth,
      12,
      WHITE
    );
  }
  display.setCursor(100,43);
  display.print((int)battery.soc);
  display.print("%");
  display.setCursor(0,55);

  if (battery.current > 0) {
    display.print("CHG:");
    display.print(hrs);
    display.print("h ");
    display.print(mins);
    display.print("m");
  } else if (battery.current < 0) {
    display.print("DIS:");
    display.print(hrs);
    display.print("h ");
    display.print(mins);
    display.print("m");
  } else {
    display.print("IDL");
  }
  display.display();

}
//weather update
void weather() {

  if (WiFi.status() != WL_CONNECTED)
    return;

  HTTPClient http;

  http.begin(
    "https://api.open-meteo.com/v1/forecast?latitude=14.311823&longitude=120.917901&hourly=precipitation_probability&current=temperature_2m,rain,weather_code"
  );

  int httpCode = http.GET();

  if (httpCode == 200) {

    String payload = http.getString();

    // DynamicJsonDocument doc(2048);
    DynamicJsonDocument doc(512);

    deserializeJson(doc, payload);

    weatherTemp =
      doc["current"]["temperature_2m"];

    weatherRain =
      doc["current"]["rain"];

    weatherCode =
      doc["current"]["weather_code"];

    switch(weatherCode) {

      case 0:
        weatherText = "Sunny";
        break;

      case 1:
      case 2:
      case 3:
        weatherText = "Cloudy";
        break;

      case 45:
      case 48:
        weatherText = "Fog";
        break;

      case 61:
      case 63:
      case 65:
        weatherText = "Rain";
        break;

      case 80:
      case 81:
      case 82:
        weatherText = "Showers";
        break;

      case 51:
      case 53:
      case 55:
        weatherText = "Drizzle";
        break;

      case 66:
      case 67:
        weatherText = "Freezing Rain";
      break;

      case 95:
        weatherText = "Storm";
        break;

      case 96:
      case 99:
        weatherText = "Thunder";
        break;

      default:
        weatherText = "Unknown";
    }
  }

  http.end();
}

//temperature screen
void tempScreen() {

  roomTemp = dht.readTemperature();
  roomHum = dht.readHumidity();

  display.clearDisplay();

  display.setCursor(20,0);
  display.print("ROOM DATA");

  display.setCursor(0,20);
  display.print("Temp: ");
  display.print(roomTemp,1);
  display.print(" C");

  display.setCursor(0,40);
  display.print("Hum : ");
  display.print(roomHum,1);
  display.print(" %");

  display.display();
}

//weatherscreen
void weatherScreen() {

  display.clearDisplay();

  display.setCursor(45,0);
  display.print("WEATHER");

  display.setCursor(0,16);
  display.print("Temp:");
  display.print(weatherTemp,1);
  display.print("C");

  display.setCursor(0,32);
  display.print("Rain:");
  display.print(weatherRain,1);
  display.print("mm");

  display.setCursor(0,48);
  display.print(weatherText);

  display.display();
}
//helper for the time screen
String getTimeString() {

    struct tm timeinfo;

    if (!getLocalTime(&timeinfo)) {
        return "--:--:--";
    }

    char buffer[20];

    strftime(
        buffer,
        sizeof(buffer),
        "%H:%M:%S",
        &timeinfo
    );

    return String(buffer);
}

void timeScreen() {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(30,0);
    display.print("CURRENT TIME");
    display.setTextSize(2);
    display.setCursor(10,25);
    display.print(getTimeString());
    display.display();
}
void systemScreen() {

     unsigned long uptime = millis() / 1000;
    unsigned long hours = uptime / 3600;
    unsigned long minutes = (uptime % 3600) / 60;
    unsigned long seconds = uptime % 60;

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(50,0);
    display.print("SYSTEM");

    display.setCursor(0,15);
    display.print("Heap:");
    display.print(ESP.getFreeHeap());

    display.setCursor(0,30);
    display.print("RSSI:");
    display.print(WiFi.RSSI());

    display.setCursor(0,45);
    display.print("Uptime:");
    display.print(hours);
    display.print(":");
    if (minutes < 10)
        display.print("0");
    display.print(minutes);
    display.print(":");
    if (seconds < 10)
        display.print("0");
    display.print(seconds);
    display.display();
}
void drawCurrentScreen() {

  if (millis() - lastScreenChange > 5000) {

    currentScreen++;

    if (currentScreen > 5)
      currentScreen = 1;

    lastScreenChange = millis();
  }

  switch(currentScreen) {

    case 1:
      updateOLED();
      break;

    case 2:
      tempScreen();
      break;

    case 3:
      weatherScreen();
      break;

    case 4:
      timeScreen();
      break;

    case 5:
      systemScreen();
      break;
  }
}

//core 0 task
void commsTask(void *pvParameters) {

  while(true) {

    bluetoothConnected =
      client &&
      client->isConnected();

    wifiConnected =
      WiFi.status() == WL_CONNECTED;

    if (readyToUpload &&
        millis() - lastUpload > 3000) {

      readyToUpload = false;

      lastUpload = millis();

      uploadToSupabase();
    }
    

    if (millis() - lastWeatherUpdate >
        1800000UL) {

      weather();

      lastWeatherUpdate = millis();
    }

    vTaskDelay(
      100 / portTICK_PERIOD_MS
    );
  }
}

//core 1 task
void uiTask(void *pvParameters) {

  while(true) {

    drawCurrentScreen();

    checkAlarms();

    vTaskDelay(
      200 / portTICK_PERIOD_MS
    );
  }
}

class ClientCallbacks : public NimBLEClientCallbacks {

  void onConnect(NimBLEClient* pClient) override {

    Serial.println("BLE Connected");
    bluetoothConnected = true;
  }

  void onDisconnect(
      NimBLEClient* pClient,
      int reason) override {

    Serial.println("BLE Disconnected");

    bluetoothConnected = false;

    writeChar = nullptr;

    doConnect =false;
    battery.deviceName = "DISCONNECTED";


    Serial.println("Starting Reconnect Scan...");

        if (!NimBLEDevice::getScan()->isScanning()) {
          NimBLEDevice::getScan()->start(
          0,
          false,
          true
        );
    }
  }
};

void setup() {

  Serial.begin(115200);
  delay(3000);
  Wire.begin(21,22); //21-sda, 22-sck
  if (!display.begin(SSD1306_SWITCHCAPVCC,0X3C)) {
  //if (!display.begin(0x3C, true )) //for SH1106X display
    Serial.println("OLED FAILED");
    while (true);
  }
  display.clearDisplay();
  // display.setTextColor(SH1106X_WHITE); // if the display is SH1106X, set all colors like this
  display.setTextColor(WHITE); 
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("BMS MONITOR");
  display.display();
  pinMode(BUZZER_PIN,OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println("BOOTING");

  Serial.println("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 30000) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("WiFi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

  } else {
    wifiConnected = false;
    Serial.println("WiFi FAILED");
  }

  Serial.println("Starting BLE");

  NimBLEDevice::init("");

  NimBLEScan* scan =
      NimBLEDevice::getScan();

  scan->setActiveScan(true);

  scan->setScanCallbacks(
      new ScanCallbacks());

  Serial.println("SCANNING...");

  scan->start(0, false, true);


  dht.begin();

xTaskCreatePinnedToCore(
    commsTask,
    "CommsTask",
    12000,
    NULL,
    1,
    &commsTaskHandle,
    0
);

xTaskCreatePinnedToCore(
    uiTask,
    "UiTask",
    12000,
    NULL,
    1,
    &uiTaskHandle,
    1
);
configTime(
  8 * 3600,
  0,
  "pool.ntp.org",
  "time.nist.gov"
);
weather();

}
void loop() {
  if (doConnect && bmsDevice) {

    doConnect = false;
    
    if (client) {
      NimBLEDevice::deleteClient(client);
      client = nullptr;
      }
    client = NimBLEDevice::createClient();
    client->setClientCallbacks(
    new ClientCallbacks(),
    false
    );

    Serial.println("CONNECTING...");

    if (!client->connect(bmsDevice)) {

      Serial.println("CONNECT FAILED");
      return;
    }
    
    NimBLERemoteService* service =
        client->getService(SERVICE_UUID);

    if (!service) {

      Serial.println("FF00 NOT FOUND");
      return;
    }

    auto* notifyChar =
      service->getCharacteristic(RX_UUID);

    writeChar =
      service->getCharacteristic(TX_UUID);

    if (!notifyChar || !writeChar) {

      Serial.println("CHARACTERISTICS NOT FOUND");
      return;
    }

    notifyChar->subscribe(
      true,
      notifyCallback
    );

    Serial.println("NOTIFICATION ENABLED");
    Serial.println("CONNECTED!");
    battery.deviceName = "DP04S007L4S100A";
    beepPattern(2, 100);
   
    delay(300);
    

    writeChar->writeValue(
      cmdBasicInfo,
      sizeof(cmdBasicInfo),
      false);

    delay(100);

    writeChar->writeValue(
      cmdCellVoltage,
      sizeof(cmdCellVoltage),
      false);

    lastPoll = millis();
  }

  if (client &&
      client->isConnected() &&
      writeChar &&
      millis() - lastPoll > 2000) {

    lastPoll = millis();

    Serial.println("Refreshing...");

    writeChar->writeValue(
      cmdBasicInfo,
      sizeof(cmdBasicInfo),
      false);

    delay(300);

    writeChar->writeValue(
      cmdCellVoltage,
      sizeof(cmdCellVoltage),
      false);
  }
  delay(1);
}
