// Prototype for a Sensor Hub in a Home 'IoT' appliance
// Written by Koos van Strien, Public Domain

#include "DHT.h"            // Adafruit's DHT Library
#include <SoftwareSerial.h>
#include <ArduinoJson.h>    // ArduinoJson written by Benoit Blanchon. I used 5.0.7.
#include <Timer.h>          // Timer 1.3 by Jack Christensen / Damian Philipp / Simon Monk

#define DHTPIN0 8     // what digital pin we're connected to
#undef DEBUGGING

#ifdef DEBUGGING
#define READ_INTERVAL 5000 // ms
#else
#define READ_INTERVAL 500
#endif

SoftwareSerial btSerial(10, 11); // RX (connect to BT TX), TX  (connect to BT RX)
String json;
Timer t;

// Uncomment whatever type you're using!
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor

// Initialize DHT sensor.
// Note that older versions of this library took an optional third parameter to
// tweak the timings for faster processors.  This parameter is no longer needed
// as the current DHT reading algorithm adjusts itself to work on faster procs.
DHT dht(DHTPIN0, DHTTYPE);

struct sensor_value {
  const int pin;
  float currentHumidity;
  float minimumHumidity;
  float maximumHumidity;
  float currentTemp;
  float minimumTemp;
  float maximumTemp;
};

sensor_value sensors[] = {
  { DHTPIN0, 0, 0, 0, 0, 0, 0 }
};

int n_sensors = 0;

void setup() {
  Serial.begin(9600);

  dht.begin();
  btSerial.begin(38400);
  
  delay(2000);

  n_sensors = sizeof(sensors) / sizeof(sensor_value);
  // Be sure to read sensors before starting to handle bluetooth requests!
  update_sensors();
  
  t.every(READ_INTERVAL, update_sensors);

  pinMode(2, OUTPUT);
  
#ifdef DEBUGGING
  Serial.println("Debug mode is ON!");
#else
  Serial.println("Debug mode is OFF!");
#endif
}


void update_sensors() {
  for(int i = 0; i < n_sensors; i++)
  {
    readHumidity(i);
    readTemperature(i);
#ifdef DEBUGGING
    Serial.print("[SENS] Humidity C:");
    Serial.print(sensors[i].currentHumidity);
    Serial.print(" Min: ");
    Serial.print(sensors[i].minimumHumidity);
    Serial.print(" Max: ");
    Serial.println(sensors[i].maximumHumidity);

    Serial.print("[SENS] Temperature C:");
    Serial.print(sensors[i].currentTemp);
    Serial.print(" Min: ");
    Serial.print(sensors[i].minimumTemp);
    Serial.print(" Max: ");
    Serial.println(sensors[i].maximumTemp);
#endif
  }
}

void readHumidity(int sensornr) {
  float h = dht.readHumidity();
#ifdef DEBUGGING
  Serial.print("[RS] Hum: ");
  Serial.println(h);
#endif
  if(!isnan(h)) {
    digitalWrite(2, LOW);
    sensors[sensornr].currentHumidity = h;
    if(h > sensors[sensornr].maximumHumidity)
      sensors[sensornr].maximumHumidity = h;
    if(h < sensors[sensornr].minimumHumidity or sensors[sensornr].minimumHumidity == 0)
      sensors[sensornr].minimumHumidity = h;
  } else {
    digitalWrite(2, HIGH);
  }
}

void readTemperature(int sensornr) {
  float f = dht.readTemperature();
#ifdef DEBUGGING
  Serial.print("[RS] Temp: ");
  Serial.println(f);
#endif
  if(!isnan(f)) {
    digitalWrite(2, LOW);
    sensors[sensornr].currentTemp = f;
    if(f > sensors[sensornr].maximumTemp)
      sensors[sensornr].maximumTemp = f;
    if(f < sensors[sensornr].minimumTemp)
      sensors[sensornr].minimumTemp = f;
  } else {
    digitalWrite(2, HIGH);
  }
}


void loop() {
  t.update();
  
  if(btSerial.available() >= 8)
  {
    json = "";
    Serial.println("Start receiving!");

    char c = 'a';
    char bitCounter = 0;
    while(int(c) != 10) {
      if (btSerial.available()) {
        c = btSerial.read();  //gets one byte from serial buffer
        bitCounter++;
#ifdef DEBUGGING
        Serial.print(c);
#endif
        if(int(c) != 13 and int(c) != 10 and int(c) != 32) {
          json += c; //makes the string readString
        }
      }
      // TODO: Make buffersize dynamic
      if(bitCounter >= 16 or int(c) == 10)
      {
#ifdef DEBUGGING
        Serial.print("BC: ");
        Serial.print((int)bitCounter);
        Serial.print(", c: ");
        Serial.print(c);
        Serial.print(" (");
        Serial.print((int)c);
        Serial.println(")");

        Serial.println("[S] ACK");
#endif
        // 8 bytes read. Need to 're-sync' using an ACK
        btSerial.println("ACK");
        bitCounter = 0;
      }
    }

#ifdef DEBUGGING
    Serial.println("");
#endif

    char jsonCharArray[200];
    StaticJsonBuffer<200> jsonBuffer;
    json.toCharArray(jsonCharArray, 200);

#ifdef DEBUGGING
    Serial.print("In buffer: '");
    Serial.print(jsonCharArray);
    Serial.println("'");
#endif
    
    JsonObject& root = jsonBuffer.parseObject(jsonCharArray);
  
    if(!root.success())
    {
#ifdef DEBUGGING
      Serial.println("parseObject() failed");
#endif
      btSerial.println("{\"msgtype\":\"error\",\"message\":\"JSON probably invalid. parseObject() failed.\"}");
      return;
    }

    String cmd = root["cmd"].asString();

#ifdef DEBUGGING
    Serial.print("Received cmd: '");
    Serial.print(cmd);
    Serial.println("'");
#endif
  
    if(cmd == "help")
    {
      // TODO: Make buffersize dynamic
      String msg = "{\"sensorprotocol\":\"KSN\",\"buffersize\":16,\"protocolversion\":0.1,\"cmds\":[{\"cmd\":\"query\",\"params\":[{\"key\":\"SensorType\",\"values\":[\"TempC\",\"Humidity\"]},{\"key\":\"ValueType\",\"values\":[\"max\",\"min\",\"current\"]}]}]}";

#ifdef DEBUGGING
      Serial.println("Parsing 'help', sending: ");
      Serial.println(msg);
#endif
      btSerial.println(msg);
    }
    else if (cmd == "query")
    {
      String SensorType = root["params"]["SensorType"].asString();
      String ValueType = root["params"]["ValueType"].asString();

      // Example: {"cmd":"query","params":{"SensorType": "TempC", "ValueType": "current"}}
      
      btSerial.print("{\"SensorType\":\"");
      btSerial.print(SensorType);
      btSerial.print("\",\"ValueType\":\"");
      btSerial.print(ValueType);
      btSerial.print("\",\"Value\":\"");
      
      String result;
      if (SensorType == "TempC" and ValueType == "current")
        result = sensors[0].currentTemp;
      else if(SensorType == "TempC" and ValueType == "min")
        result = sensors[0].minimumTemp;
      else if(SensorType == "TempC" and ValueType == "max")
        result = sensors[0].maximumTemp;
      else if (SensorType == "Humidity" and ValueType == "current")
        result = sensors[0].currentHumidity;
      else if(SensorType == "Humidity" and ValueType == "min")
        result = sensors[0].minimumHumidity;
      else if(SensorType == "Humidity" and ValueType == "max")
        result = sensors[0].maximumHumidity;
      else
        result = "Unknown";
      btSerial.print(result);
      btSerial.println("\"}");
    }
  }
}
