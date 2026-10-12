#include <WIFI.h>
#include <WIFIClient.h>
#include <WebServer.h>
#include <NimBLEDevice.h>


const char* ssid = "WIFI";
const char* password = "PASSWORD";
const DEVICE_KEY = "$2a$12$Mb6zJjWSQASdh6IAWEqbleiTetQaXAy2QdT.U7z5bMxsxPym0jK.W";

void setup() {
    Serial.begin(115200);
    NimBLEDevice::init("");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    
    pScan->setActiveScan(true);
    Serial.println("Scanning...");
    NimBLEScanResults results = pScan->start(5);

    for(int i=0. i<results.getCount(); i++) {
        NimBLEAdvertisedDevice d = results.getDevice(i);

    Serial.print("Name: ");
    Serial.print(d.getName().c_str());

    Serial.print(" MAC: ");
    Serial.print(d.getAddress().toString().c_str());
    }
}
void loop() {}

void connectWifi() {
    WIFI.begin(ssid, password);
    while(WIFI.status()!=WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WIFI CONNECTED");
}

void sendToSupabase() {
    HTTPClient http;
    http.begin("https://pnrbdohtrvbrmvabvkxc.supabase.co");
    http.addHeader(
        "Content-Type",
        "application/json"
    );
    http.addHeader(
        "x-device-key",
        "DEVICE-KEY"
    );

    String json = 
    R"({
        "voltage":14.2,
        "current": 20.5,
        "soc": 100,
        "temperature":35,
        "cycles":245
    })";
    int result = http.POST(json);
    serial.println(result);
    http.end();
}
unsigned long lastSend=0;

void loop() {

  if(millis()-lastSend > 15000) {

      sendToSupabase();

      lastSend=millis();
  }
}
