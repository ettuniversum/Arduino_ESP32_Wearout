/*
>> Pulse Sensor Amped 1.2 <<
This code is for Pulse Sensor Amped by Joel Murphy and Yury Gitman
    www.pulsesensor.com 
    >>> Pulse Sensor purple wire goes to Analog Pin 0 <<<
Pulse Sensor sample aquisition and processing happens in the background via Timer 2 interrupt. 2mS sample rate.
PWM on pins 3 and 11 will not work when using this code, because we are using Timer 2!
The following variables are automatically updated:
Signal :    int that holds the analog signal data straight from the sensor. updated every 2mS.
IBI  :      int that holds the time interval between beats. 2mS resolution.
BPM  :      int that holds the heart rate value, derived every beat, from averaging previous 10 IBI values.
QS  :       boolean that is made true whenever Pulse is found and BPM is updated. User must reset.
Pulse :     boolean that is true when a heartbeat is sensed then false in time with pin13 LED going out.

This code is designed with output serial data to Processing sketch "PulseSensorAmped_Processing-xx"
The Processing sketch is a simple data visualizer. 
All the work to find the heartbeat and determine the heartrate happens in the code below.
Pin 13 LED will blink with heartbeat.
If you want to use pin 13 for something else, adjust the interrupt handler
It will also fade an LED on pin fadePin with every beat. Put an LED and series resistor from fadePin to GND.
Check here for detailed code walkthrough:
http://pulsesensor.myshopify.com/pages/pulse-sensor-amped-arduino-v1dot1

Code Version 1.2 by Joel Murphy & Yury Gitman  Spring 2013
This update fixes the firstBeat and secondBeat flag usage so that realistic BPM is reported.

*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

const int WIDTH=64;
const int HEIGHT=48;
const int LENGTH=WIDTH;

//  VARIABLES
int fadePin = 12;                 // pin to do fancy classy fading blink at each beat
int fadeRate = 0;                 // used to fade LED on with PWM on fadePin

// these variables are volatile because they are used during the interrupt service routine!
volatile int BPM;                   // used to hold the pulse rate
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // holds the time between beats, must be seeded! 
volatile boolean Pulse = false;     // true when pulse wave is high, false when it's low
volatile boolean QS = false;        // becomes true when Arduoino finds a beat.



//#define USE_ARDUINO_INTERRUPTS false
//#include <PulseSensorPlayground.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
//uint8_t Signal = 0;
//uint8_t BPM = 0;
//const int OUTPUT_TYPE = SERIAL_PLOTTER;

//  Variables
const int PulseWire = A0;       // PulseSensor PURPLE WIRE connected to ANALOG PIN 0
const int LED13 = 13;          // The on-board Arduino LED, close to PIN 13.
int Threshold = 472;           // Determine which Signal to "count as a beat" and which to ignore.

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "0000180D-0000-1000-8000-00805F9B34FB" // UART service UUID
#define CHARACTERISTIC_UUID_RX "00002A37-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID_TX "00002A37-0000-1000-8000-00805F9B34FB"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};


// The SetUp Function:
void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("WearoutBLE");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_READ  |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  // Start the service
  pService->start();
  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
  // Intialize analog read with lower bit
  //analogReadResolution(6);
  //analogSetAttenuation(ADC_6db);
  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS   
}




void loop(){
   if (deviceConnected) {
     if (QS == true){                       // Quantified Self flag is true when arduino finds a heartbeat
         fadeRate = 255;                  // Set 'fadeRate' Variable to 255 to fade LED with pulse
         QS = false;                      // reset the Quantified Self flag for next time    
         // Obtain pulse
         yield();
         delay(20); // bluetooth stack will go into congestion, if too many packets are sent
         uint8_t low_Byte = lowByte(BPM);
         //uint8_t high_Byte = highByte(BPM);
         //Serial.println(Signal);
         int bit_len = 1;
         pTxCharacteristic->setValue(&low_Byte, bit_len);
         pTxCharacteristic->notify();
      
          // disconnecting
          if (!deviceConnected && oldDeviceConnected) {
              delay(500); // give the bluetooth stack the chance to get things ready
              pServer->startAdvertising(); // restart advertising
              Serial.println("start advertising");
              oldDeviceConnected = deviceConnected;
          }
          // connecting
          if (deviceConnected && !oldDeviceConnected) {
          // do stuff here on connecting
              oldDeviceConnected = deviceConnected;
          }
      }// If PBM
   }// IF connected
  
}
