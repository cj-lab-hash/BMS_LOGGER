// #include <WiFi.h>
// #include <WiFiClient.h>
// #include <WebServer.h>
// #include <NimBLEDevice.h>


// const char* ssid = "WIFI";
// const char* password = "PASSWORD";
// const char* DEVICE_KEY = "DEVICE_KEY";
// unsigned long lastSend=0;

// void setup() {
//     Serial.begin(115200);
//     NimBLEDevice::init("");
//     NimBLEScan* pScan = NimBLEDevice::getScan();
    
//     pScan->setActiveScan(true);
//     Serial.println("Scanning...");
//     NimBLEScanResults results = pScan->start(5);
//     connectWifi();
//     for(int i = 0; i < results.getCount(); i++) {
//         NimBLEAdvertisedDevice d = results.getDevice(i);

//     Serial.print("Name: ");
//     Serial.print(d.getName().c_str());

//     Serial.print(" MAC: ");
//     Serial.println(d.getAddress().toString().c_str());
//     }
// }


// void connectWifi() {
//     WIFI.begin(ssid, password);
//     while(WIFI.status()!=WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println("");
//     Serial.println("WIFI CONNECTED");
// }

// void sendToSupabase() {
//     HTTPClient http;
//     http.begin("https://bms-logger.onrender.com/api/bms");
//     http.addHeader(
//         "Content-Type",
//         "application/json"
//     );
//     http.addHeader(
//         "x-device-key",
//         DEVICE-KEY
//     );

//     String json = 
//     R"({
//         "voltage":14.2,
//         "current": 20.5,
//         "soc": 100,
//         "temperature":35,
//         "cycles":245
//     })";
//     int result = http.POST(json);
//     Serial.println(result);
//     http.end();
// }


// void loop() {

//   if(millis()-lastSend > 15000) {

//       sendToSupabase();

//       lastSend=millis();
//   }
// }
//for testing esp32 to render to supabase, remove if working//
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

const char* DEVICE_KEY =
"YOUR_DEVICE_KEY";

unsigned long lastSend = 0;

void connectWifi()
{
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
}

void sendToServer()
{
  HTTPClient http;

  http.begin(
  "https://bms-logger.onrender.com/api/bms"
  );

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "x-device-key",
    DEVICE_KEY
  );

  String json = R"({
    "device_name":"JBD_BMS",
    "voltage":14.2,
    "current":20.5,
    "soc":100,
    "temperature":35,
    "cycles":245
  })";

  int result = http.POST(json);

  Serial.print("HTTP Result: ");
  Serial.println(result);

  http.end();
}

void setup()
{
  Serial.begin(115200);
  connectWifi();
}

void loop()
{
  if(millis() - lastSend > 15000)
  {
    sendToServer();

    lastSend = millis();
  }
}