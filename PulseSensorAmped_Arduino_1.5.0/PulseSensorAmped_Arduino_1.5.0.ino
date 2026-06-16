#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Adafruit_NeoPixel.h>
#include <pb_encode.h>
#include "battery_status.pb.h"


NimBLECharacteristic* pCharacteristic    = nullptr;
bool                  deviceConnected    = false;
bool                  oldDeviceConnected = false;
uint32_t              value              = 0;

// --- BLE SETTINGS ---
// UUIDs must match your Android App's expectations
#define SERVICE_UUID           "0000180D-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID_TX "00002A37-0000-1000-8000-00805F9B34FB" 
#define PIN        0  // Define the pin for the NeoPixel
#define NUMPIXELS  1  // Number of pixels in the NeoPixel

// Initialize the NeoPixel library.
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// --- HARDWARE SETTINGS ---
const int PulseWire = A1;       // Pulse Sensor Purple Wire (Use A1 for 3.3V safety!)
const int BATTERY_PIN = A2;     // Battery Analog Pin for voltage measurement
int Threshold = 2000;            // Threshold for "Beat" detection (Adjust if needed)


class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override { deviceConnected = true; };

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        // Peer disconnected, add them to the whitelist
        // This allows us to use the whitelist to filter connection attempts
        // which will minimize reconnection time.
        NimBLEDevice::whiteListAdd(connInfo.getAddress());
        deviceConnected = false;
    }
} serverCallbacks;

void onAdvComplete(NimBLEAdvertising* pAdvertising) {
    Serial.println("Advertising stopped");
    if (deviceConnected) {
        return;
    }
    // If advertising timed out without connection start advertising without whitelist filter
    pAdvertising->setScanFilter(false, false);
    pAdvertising->start();
}


// --- SETUP ---
void setup() {
    Serial.begin(115200);

    // 1. Initialize NimBLE
    NimBLEDevice::init("Wearout"); 

    // 2. Create Server & Set Callbacks
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);
    pServer->advertiseOnDisconnect(false);

    // 3. Create Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // 4. Create Characteristic
    // Note: We use NOTIFY so the phone gets data automatically
    pCharacteristic =
        pService->createCharacteristic(CHARACTERISTIC_UUID_TX,
                                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    pService->start();

    // 5. Start Service
    pService->start();

    // 6. Start Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("Wearout");
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->enableScanResponse(true);
    pAdvertising->setAdvertisingCompleteCallback(onAdvComplete);
    pAdvertising->start();
    Serial.println("Waiting a client connection to notify...");
    // Initialize the NeoPixel LED
    pixels.begin(); 

}

// --- MAIN LOOP ---
void loop() {
    // 2. Sensor Reading & LED Logic (Always run this!)
    int sensorValue = analogRead(PulseWire);
    pixels.setPixelColor(0, pixels.Color(150, 0, 0)); // Set the color to red

    // Flash LED if heartbeat detected
    if (sensorValue > Threshold) {
      pixels.show();   // Send the updated pixel colors to the hardware.
    } else {
      pixels.clear();  // Turn off the NeoPixel
      pixels.show();
    }

    if (deviceConnected) {
        // Read battery voltage (assuming 1/2 voltage divider and 3.3V reference)
        uint32_t raw_voltage = analogReadMilliVolts(BATTERY_PIN);
        float actual_voltage = (raw_voltage / 1000.0) * 2.0;
        int current_percentage = map((int)(actual_voltage * 100), 320, 420, 0, 100);

        DeviceData message = DeviceData_init_zero;
        
        // Populate heart measurement sub-message
        message.heart_measurement.ppg_value = sensorValue;

        // Populate battery status sub-message
        message.battery_status.voltage = raw_voltage;
        message.battery_status.percentage = constrain(current_percentage, 0, 100);
        message.battery_status.is_charging = false; // Set based on your hardware logic if available

        // Prepare output buffer and stream
        uint8_t buffer[DeviceData_size];
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        // Encode the message
        bool status = pb_encode(&stream, DeviceData_fields, &message);
        if (status) {
            size_t message_length = stream.bytes_written;
            pCharacteristic->setValue(buffer, message_length);
            pCharacteristic->notify();
        } else {
            Serial.print("Encoding failed: ");
            Serial.println(PB_GET_ERROR(&stream));
        }
    }

    if (!deviceConnected && oldDeviceConnected) {
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        if (NimBLEDevice::getWhiteListCount() > 0) {
            // Allow anyone to scan but only whitelisted can connect.
            pAdvertising->setScanFilter(false, true);
        }
        // advertise with whitelist for 30 seconds
        pAdvertising->start(30 * 1000);
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

    delay(10);
}