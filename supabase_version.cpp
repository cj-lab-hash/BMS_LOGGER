#include <WiFi.h>
#include <HTTPClient.h>
#include <NimBLEDevice.h>

const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

const char* SUPABASE_URL = "https://pnrbdohtrvbrmvabvkxc.supabase.co/rest/v1/bms_logs";
const char* SUPABASE_KEY = "sb_publishable_YAq1ZIeaJdjx4w0G4DwY3g_tXAZHuVk";

unsigned long lastSend = 0;


NimBLEAddress* targetAddress = nullptr;

void connectWifi()
{
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi Connected");
}

void discoverServices(NimBLEAddress address)
{
    Serial.println();
    Serial.println("Connecting...");

    NimBLEClient* client = NimBLEDevice::createClient();

    if (!client->connect(address))
    {
        Serial.println("Connection Failed");
        return;
    }

    Serial.println("Connected");
    Serial.println();

    //Depending on the NimBLE version
    // std::vector<NimBLERemoteService*>* services =
    //     client->getServices(true);
    auto* services = client->getServices(true);

    for (auto service : *services)
    {
        Serial.println("================================");
        Serial.print("SERVICE UUID: ");
        Serial.println(service->getUUID().toString().c_str());

        std::map<std::string, NimBLERemoteCharacteristic*>*
            chars = service->getCharacteristics(true);

        for (auto& c : *chars)
        {
            NimBLERemoteCharacteristic* chr =
                c.second;

            Serial.print("  CHARACTERISTIC: ");
            Serial.println(
                chr->getUUID().toString().c_str()
            );

            Serial.print("    Read: ");
            Serial.println(chr->canRead());

            Serial.print("    Write: ");
            Serial.println(chr->canWrite());

            Serial.print("    Notify: ");
            Serial.println(chr->canNotify());
        }
    }

    client->disconnect();
    Serial.println();
    Serial.println("Discovery Complete");
}

void scanForBMS()
{
    NimBLEScan* scan = NimBLEDevice::getScan();

    scan->setActiveScan(true);

    Serial.println("Scanning...");

    NimBLEScanResults results = scan->start(10);

    for (int i = 0; i < results.getCount(); i++)
    {
        NimBLEAdvertisedDevice device =
            results.getDevice(i);

        String name =
            device.getName().c_str();

        String mac =
            device.getAddress().toString().c_str();

        Serial.println("--------------------------------");
        Serial.print("Name: ");
        Serial.println(name);

        Serial.print("MAC: ");
        Serial.println(mac);

        if (
            name.indexOf("JBD") >= 0 ||
            name.indexOf("BMS") >= 0 ||
            name.indexOf("SP") >= 0
        )
        {
            Serial.println("*** POSSIBLE BMS FOUND ***");

            discoverServices(
                device.getAddress()
            );

            break;
        }
    }
}
void sendToSupabase()
{
    HTTPClient http;
    
    http.begin(SUPABASE_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization",
                    String("Bearer ") + SUPABASE_KEY);
    http.addHeader("Prefer", "return=minimal");

    String json = R"([
    {
    "device_name": "JBD_BMS",
    "voltage":14.2,
    "current":20.5,
    "soc":100,
    "temperature": 34,
    "cycles":346
    }
    ])";
    
    if (WiFi.status() != WL_CONNECTED)
    {
        connectWifi()
    }

    int httpCode = http.POST(json);

    Serial.print("HTTP Code: ");
    Serial.println(httpCode);

    if (httpCode > 0)
     {
    Serial.printf("HTTP %d\n", httpCode);
    String response = http.getString();
    if(response.length()){
        Serial.println(response);
    }
    
     } else {
        Serial.printf("Request failed: %s\n", http.errorToString(httpCode).c_str());
     }
     http.end();
}
void setup()
{
    Serial.begin(115200);

    

    NimBLEDevice::init("");

    scanForBMS();

    connectWifi();
}

void loop()
{
    if(millis() - lastSend > 15000)
    {
        sendToSupabase();
        lastSend = millis();
    }
}