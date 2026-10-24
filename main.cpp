#include <NimBLEDevice.h>
#include <WiFi.h>
#include <HTTPClient.h>
const char* WIFI_SSID = "Bonaks!";
const char* WIFI_PASSWORD = "C@rleigh_Call1xta";
const char* SUPABASE_URL =
"https://YOURPROJECT.supabase.co/rest/v1/battery_logs";

const char* SUPABASE_KEY =
"YOUR_SUPABASE_ANON_KEY";


struct BatteryData {
  float packVoltage = 0;
  float current = 0;
  float remainingCapacity = 0;
  float ratedCapacity = 0;
  float cell1 = 0;
  float cell2 = 0;
  float cell3 = 0;
  float cell4 = 0;
  float remainingCapacity = 0;
float ratedCapacity = 0;
  uint16_t cycles = 0;
  float soc = 0;

  String deviceName;
};

BatteryData battery;

static NimBLEUUID SERVICE_UUID("0000ff00-0000-1000-8000-00805f9b34fb");
static NimBLEUUID RX_UUID("0000ff01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID TX_UUID("0000ff02-0000-1000-8000-00805f9b34fb");

NimBLEAdvertisedDevice* bmsDevice = nullptr;
NimBLERemoteCharacteristic* writeChar = nullptr;
NimBLEClient* client = nullptr;

bool doConnect = false;
unsigned long lastPoll = 0;

uint8_t cmdBasicInfo[] = {
  0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77
};

uint8_t cmdCellVoltage[] = {
  0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77
};

uint8_t cmdDeviceInfo[] = {
  0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77
};

void printBatteryData() {

  Serial.println();
  Serial.println("========== BMS ==========");

  Serial.print("Device: ");
  Serial.println(battery.deviceName);
  Serial.print("Remaining Ah: ");
Serial.println(battery.remainingCapacity);

Serial.print("Rated Ah: ");
Serial.println(battery.ratedCapacity);

Serial.print("SOC: ");
Serial.print(battery.soc, 1);
Serial.println("%");
  Serial.print("Voltage: ");
  Serial.print(battery.packVoltage, 2);
  Serial.println(" V");

  Serial.print("Current: ");
  Serial.print(battery.current, 2);
  Serial.println(" A");

  Serial.print("Cycles: ");
  Serial.println(battery.cycles);

  Serial.println();

  Serial.print("Cell1: ");
  Serial.print(battery.cell1, 3);
  Serial.println(" V");

  Serial.print("Cell2: ");
  Serial.print(battery.cell2, 3);
  Serial.println(" V");

  Serial.print("Cell3: ");
  Serial.print(battery.cell3, 3);
  Serial.println(" V");

  Serial.print("Cell4: ");
  Serial.print(battery.cell4, 3);
  Serial.println(" V");

  Serial.println("=========================");
  Serial.println();
  Serial.println("JSON:");
Serial.println(getBatteryJson());

}
String getBatteryJson() {

  String json = "{";

  json += "\"device\":\"" + battery.deviceName + "\",";
  json += "\"voltage\":" + String(battery.packVoltage, 2) + ",";
  json += "\"current\":" + String(battery.current, 2) + ",";
  json += "\"cycles\":" + String(battery.cycles) + ",";
  json += "\"soc\":" + String(battery.soc, 1) + ",";
  json += "\"remainingAh\":" + String(battery.remainingCapacity, 2) + ",";
  json += "\"ratedAh\":" + String(battery.ratedCapacity, 2) + ",";
  json += "\"cell1\":" + String(battery.cell1, 3) + ",";
  json += "\"cell2\":" + String(battery.cell2, 3) + ",";
  json += "\"cell3\":" + String(battery.cell3, 3) + ",";
  json += "\"cell4\":" + String(battery.cell4, 3);

  json += "}";

  return json;
}
void uploadToSupabase() {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Not Connected");
    return;
  }

  HTTPClient http;

  http.begin(SUPABASE_URL);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization",
                 "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Prefer", "return=minimal");

  String json = getBatteryJson();

  Serial.println("Uploading JSON:");
  Serial.println(json);

  int httpCode = http.POST(json);

  Serial.print("HTTP Response: ");
  Serial.println(httpCode);

  http.end();
}
void notifyCallback(
    NimBLERemoteCharacteristic* pRemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {

  Serial.print("RX: ");

  for (size_t i = 0; i < length; i++) {

    if (pData[i] < 16)
      Serial.print("0");

    Serial.print(pData[i], HEX);
    Serial.print(" ");
  }

  Serial.println();

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

battery.soc =
  (battery.remainingCapacity /
   battery.ratedCapacity) * 100.0;

    int16_t rawCurrent =
      (int16_t)((pData[6] << 8) | pData[7]);

    battery.current =
      rawCurrent / 100.0;

    battery.cycles =
      (pData[12] << 8) | pData[13];
      Serial.println("===== BYTE MAP =====");

for(int i = 0; i < length; i++) {
  Serial.print("[");
  Serial.print(i);
  Serial.print("] = 0x");

  if(pData[i] < 16)
    Serial.print("0");

  Serial.println(pData[i], HEX);
}

Serial.println("====================");
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

    printBatteryData();
    uploadToSupabase();
  }

  // CMD05
  if (length >= 10 &&
      pData[0] == 0xDD &&
      pData[1] == 0x05) {

    battery.deviceName = "";

    for (size_t i = 4; i < length; i++) {
      char c = (char)pData[i];
      if (c >= 32 && c <= 126) {
    battery.deviceName += c;
    }
    }
    Serial.print("Device Name: ");
    Serial.println(battery.deviceName);
  }
}

class ScanCallbacks : public NimBLEScanCallbacks {

public:
  void onResult(const NimBLEAdvertisedDevice* device) override {

    if (device->haveName() &&
        device->getName() == "DP04S007L4S100A") {

      Serial.println("BMS FOUND!");

      bmsDevice =
        new NimBLEAdvertisedDevice(*device);

      doConnect = true;

      NimBLEDevice::getScan()->stop();
    }
  }
};

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println("INIT");

  NimBLEDevice::init("");

  NimBLEScan* scan =
      NimBLEDevice::getScan();

  scan->setActiveScan(true);

  scan->setScanCallbacks(
      new ScanCallbacks());

  Serial.println("SCANNING...");

  scan->start(0, false, true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

Serial.print("Connecting WiFi");

while (WiFi.status() != WL_CONNECTED) {

  delay(500);
  Serial.print(".");
}

Serial.println();
Serial.println("WiFi Connected");
Serial.println(WiFi.localIP());
}

String getBatteryJson() {

  String json = "{";

  json += "\"device\":\"" + battery.deviceName + "\",";
  json += "\"voltage\":" + String(battery.packVoltage, 2) + ",";
  json += "\"current\":" + String(battery.current, 2) + ",";
  json += "\"remainingAh\":" +
        String(battery.remainingCapacity,2) + ",";

json += "\"ratedAh\":" +
        String(battery.ratedCapacity,2) + ",";

json += "\"soc\":" +
        String(battery.soc,1) + ",";
  json += "\"cycles\":" + String(battery.cycles) + ",";

  json += "\"cell1\":" + String(battery.cell1, 3) + ",";
  json += "\"cell2\":" + String(battery.cell2, 3) + ",";
  json += "\"cell3\":" + String(battery.cell3, 3) + ",";
  json += "\"cell4\":" + String(battery.cell4, 3);

  json += "}";

  return json;
}
void loop() {

  if (doConnect && bmsDevice) {

    doConnect = false;

    client = NimBLEDevice::createClient();

    Serial.println("CONNECTING...");

    if (!client->connect(bmsDevice)) {

      Serial.println("CONNECT FAILED");
      return;
    }

    Serial.println("CONNECTED!");

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

    delay(1000);

    writeChar->writeValue(
      cmdDeviceInfo,
      sizeof(cmdDeviceInfo),
      false);

    delay(500);

    writeChar->writeValue(
      cmdBasicInfo,
      sizeof(cmdBasicInfo),
      false);

    delay(500);

    writeChar->writeValue(
      cmdCellVoltage,
      sizeof(cmdCellVoltage),
      false);

    lastPoll = millis();
  }

  if (client &&
      client->isConnected() &&
      writeChar &&
      millis() - lastPoll > 10000) {

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

  delay(50);
}
