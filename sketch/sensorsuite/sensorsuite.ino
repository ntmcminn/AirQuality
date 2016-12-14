/**
 * Air quality monitoring package for Arduino
 */
#include <ArduinoJson.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <string.h>
//#include <debug.h>
#include <avr/wdt.h>
#include <WiFi101.h>

/********************  WLAN settings ********************/
#define ADAFRUIT_WINC1500_IRQ     2           // MUST be an interrupt pin!
#define ADAFRUIT_WINC1500_RST     3
#define ADAFRUIT_WINC1500_CS      4
#define WLAN_SSID                 "AQnet"
#define WLAN_PASS                 "AQnet123"
#define WLAN_SECURITY             WLAN_SEC_WPA2
#define IDLE_TIMEOUT_MS           3000 

/******************** AM2302 temp and humidity sensor settings ********************/
#define DHT_PIN                   47          // digital input pin
#define DHT_TYPE                  DHT22       // DHT 22  (AM2302)

/******************** Sharp GP2Y10 dust sensor settings ********************/
#define DUST_SENSOR_ANALOG_IN     9           // analog pin to read dust sensor data
#define LED_POWER                 49          // digital pin used to provide power to dust sensor LED
#define SAMPLING_TIME             280 
#define DELTA_TIME                40
#define SLEEP_TIME                9680

/******************** MQ135 / MQ131 gas sensor settings ********************/
#define MQ135_ANALOG              8
#define MQ131_ANALOG              10
#define MQ7_ANALOG                11

/******************** Staus LED pins ********************/
#define LED_STARTING              24
#define LED_NETWORK_BAD           26
#define LED_NETWORK_OK            22
#define LED_SYSTEM_HALTED         26

/******************** General application settings ********************/
#define LOOP_DELAY                60000               // how frequently to take a reading from the sensor array (in ms)
#define DATA_VERSION              1                   // version of the API / JSON format that this package is using
#define INIT_PATH                 "/api/initdata"     // url path for saving unit initialization data
#define DATA_PATH                 "/api/aqdata"       // url path for saving air quality data readings
#define DATA_HOST                 "aqmonitor.aqmonitor1.net"
#define PORT                      8080                // port for contacting the API
#define IDLE_TIMEOUT_MS           10000               // timeout for http operations
#define DEVICE_ID                 1                   // id of this device in the AQ network
#define JSON_BUFFER_SIZE          512                 // size of buffer to use for JSON packages
#define CHUNK_SIZE                64                  // size of individual chunks to write out via client
#define READ_LENGTH               15                  // how much of the response to read.  We don't need the whole thing
#define MAX_CONNECT_ATTEMPTS      5                   // how many times to attemp wifi connect and DHCP lease

/******************** structs for sensor data ********************/
struct th
{
   float humidity;
   float temp;
};

struct ds
{
   float raw;
   float voltage;
   float density;
};

struct gs
{
   float MQ135;
   float MQ131;
   float MQ7;
};

/******************** initializations ********************/
DHT dht(DHT_PIN, DHT_TYPE);

// Initialize the Wifi client library
WiFiClient client;

const unsigned long
  dhcpTimeout     = 90L * 1000L, // Max time to wait for address from DHCP
  connectTimeout  = 15L * 1000L, // Max time to wait for server connection
  responseTimeout = 15L * 1000L; // Max time to wait for data from 
  
unsigned long
  currentTime = 0L;

uint32_t ip;
char hostip[16];

int status = WL_IDLE_STATUS;

void setup() {

  // first thing first, set pin mode for LED pins
  pinMode(LED_STARTING,OUTPUT);
  pinMode(LED_NETWORK_BAD, OUTPUT);
  pinMode(LED_NETWORK_OK, OUTPUT);
  
  // initialize serial comms
  Serial.begin(115200);

  // hello messages!
  Serial.println(F("\r\nInitializing air quality sensor array"));

  // set up the pins for the WINC1500 breakout
  WiFi.setPins(ADAFRUIT_WINC1500_CS, ADAFRUIT_WINC1500_IRQ, ADAFRUIT_WINC1500_RST);
  
  // set up the watchdog timer
  wdt_enable(WDTO_8S); 
   
  connectToNetwork();
  
  wdt_reset();
  
  // set up host IP char array for sendData
  hostStringFromIp(hostip);
  
  // send system startup notification to logging server
  // use sendData method to do this, passing path and payload
  char jsonbuffer[JSON_BUFFER_SIZE];
  int len = createInitPayload(jsonbuffer);
  
  //send to server
  sendData(INIT_PATH, jsonbuffer, len);
 
  // set output pin for dust sensor LED
  pinMode(LED_POWER,OUTPUT);

  // take initial sensor readings and discard.  This prevents weird spikes at startup.
  // dust sensor seems especially prone to super low value spikes when it is first read
  getGasSensorData();
  getTempHumidityData();
  getDustSensorData();
  
}

void loop() {
  
  // sleep for the configured interval
  for(uint16_t i = 0; i < LOOP_DELAY; i = i + 5000) {
    delay(5000);
    wdt_reset();
  }
  
  Serial.print(F("Free RAM: ")); 
  //Serial.println(getFreeRam(), DEC);
  
  // read values from gas sensors
  struct gs gsdata = getGasSensorData();
  
  // read humidity and temp from AM2302 
  struct th thdata = getTempHumidityData();
  
  // read output from dust sensor
  struct ds dsdata = getDustSensorData();
  
  // create payload to send to aggregation server
  char jsonbuffer[JSON_BUFFER_SIZE];
  int len = createAqPayload(gsdata, thdata, dsdata, jsonbuffer);

  wdt_reset();
  
  //send to server
  sendData(DATA_PATH, jsonbuffer, len);

  wdt_reset();

}

int createInitPayload(char *jbuf) {
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  // add in the time and version data for this package
  root["time"] = currentTime + (int)(millis() / 1000);
  root["dataversion"] = DATA_VERSION;
  //root["freemem"] = getFreeRam();
  root["deviceid"] = DEVICE_ID;
  root["action"] = "init";
  
  int len = root.measureLength();
  root.printTo(jbuf, len + 1);

  // append null terminator after buffered data
  jbuf[len + 2] = "\0";
  
  // return total size of char array
  return len + 2;
}

int createAqPayload(gs gsdata, th thdata, ds dsdata, char *jbuf){
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  // add in the time and version data for this package
  root["time"] = currentTime + (int)(millis() / 1000);
  root["dataversion"] = DATA_VERSION;
  //root["freemem"] = getFreeRam();
  root["deviceid"] = DEVICE_ID;
  
  // create and add nested object for temp / humidity
  JsonObject& thdataobj = root.createNestedObject("thdata");
  thdataobj["temp"] = thdata.temp;
  thdataobj["humidity"] = thdata.humidity;
  
  // create and add nested object for dust sensor measurements
  JsonObject& dsdataobj = root.createNestedObject("dsdata");
  dsdataobj["raw"] = dsdata.raw;
  dsdataobj["voltage"] = dsdata.voltage;
  dsdataobj["density"] = dsdata.density;
  
  // create and add nested object for gas sensor measurements
  JsonObject& gsdataobj = root.createNestedObject("gsdata");
  gsdataobj["MQ135"] = gsdata.MQ135;
  gsdataobj["MQ131"] = gsdata.MQ131;
  gsdataobj["MQ7"] = gsdata.MQ7;
  
  int len = root.measureLength();
  root.printTo(jbuf, len + 1);

  // append null terminator after buffered data
  jbuf[len + 2] = "\0";
  
  // return total size of char array
  return len + 2;
}

void hostStringFromIp(char* ipbuffer) {
  IPAddress ipobj = IPAddress(ip);
  sprintf(ipbuffer, "%d.%d.%d.%d\0", ipobj[3], ipobj[2], ipobj[1], ipobj[0]);
}

void sendData(char path[], char *json, int len) {

  
  Serial.print(F("Sending this data: "));
  Serial.println(json);

  char jsondata[len];
  memcpy(jsondata, json, len);
  
  Serial.print(F("Connecting to server: "));
  Serial.print(hostip);
  Serial.print(F(":"));
  Serial.print(PORT);

  wdt_reset();

  client.connect(ip, PORT);
  
  wdt_reset();

  
  if(client.connected()) {

    Serial.println(F("...OK"));

    char clen[3];
    sprintf(clen, "%d", len);
    
    Serial.print(F("Sending headers..."));
    client.print(F("POST "));
    client.print(path);
    client.println(F(" HTTP/1.1")); 
    client.print(F("Host: "));
    client.println(hostip);
    client.print(F("Content-Length: "));
    client.println(clen);
    client.println(F("Connection: close"));
    client.print(F("Content-Type: application/json"));
    client.println("\r\n\r\n");
    Serial.println(F("OK"));

    // send json payload to server
    Serial.print(F("Sending content..."));
    sendChunkedData(client, jsondata, CHUNK_SIZE);
    Serial.println(F("OK"));
  
    // read response
    Serial.println(F("Reading server response"));
    Serial.print(F("Client connected: "));
    Serial.println(client.connected());
    
    unsigned long lastRead = millis();
    int readlen = 0;
    while (readlen < READ_LENGTH && client.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
      Serial.print(F("connected, available data bytes: "));
      Serial.println(client.available());
      delay(10);
      while (client.available() && readlen < READ_LENGTH) {
        delay(10);
        char c = client.read();
        Serial.print(c);
        lastRead = millis();
        readlen++;
      }
    }
    Serial.print(F("\r\n\r\n"));
    Serial.println(F("Done"));
    
    client.stop();
  }else{
    Serial.println(F("\r\nConnection failed"));
  }
}

void sendChunkedData(WiFiClient client, String input, int chunksize) {
  
  // Get String length
  int length = input.length();
  int max_iteration = (int)(length/chunksize);
  if(length % chunksize != 0) {max_iteration++;}

  int st = 0;
  int en = chunksize;
  
  for (int i = 0; i < max_iteration; i++) {
    //Serial.print("chunk: ");
    String chunk = input.substring(st, en);
    char chunkarray[chunksize];
    chunk.toCharArray(chunkarray, chunksize);
    client.print(chunkarray);
    //Serial.println(chunkarray);
    st = en - 1;
    en = st + chunksize;
    if(en > length) {en = length;}
  }  
}


struct ds getDustSensorData() {
  struct ds ds_instance;

  digitalWrite(LED_POWER, LOW); // power on the LED
  delayMicroseconds(SAMPLING_TIME);
 
  ds_instance.raw = analogRead(DUST_SENSOR_ANALOG_IN); // read the dust value
 
  delayMicroseconds(DELTA_TIME);
  digitalWrite(LED_POWER,HIGH); // turn the LED off
  delayMicroseconds(SLEEP_TIME);

  ds_instance.voltage = ds_instance.raw * (5.0 / 1024.0);
  ds_instance.density = (0.17 * ds_instance.voltage - 0.1) * 1000;

  Serial.print(F("DUST SENSOR: "));
  Serial.print(F("Raw Signal Value (0-1023): "));
  Serial.print(ds_instance.raw);
  Serial.print(F(" - Voltage: "));
  Serial.print(ds_instance.voltage);
  Serial.print(F(" - Dust Density: "));
  Serial.println(ds_instance.density);
  
  return ds_instance;
}

struct th getTempHumidityData() {
  struct th th_instance;

  th_instance.temp = dht.readTemperature();
  th_instance.humidity = dht.readHumidity();
  Serial.print(F("TEMP + HUMIDITY SENSOR: "));
  Serial.print(F("Humidity: ")); 
  Serial.print(th_instance.humidity);
  Serial.print(F(" - Temperature: ")); 
  Serial.print(th_instance.temp);
  Serial.println(F(" *C "));
  
  return th_instance;
}

struct gs getGasSensorData() {
  struct gs gs_instance;
  gs_instance.MQ135 = analogRead(MQ135_ANALOG);
  gs_instance.MQ131 = analogRead(MQ131_ANALOG);
  gs_instance.MQ7 = analogRead(MQ7_ANALOG);
  
  Serial.print(F("MQ135 Sensor Raw Signal Value (0-1023): "));
  Serial.print(gs_instance.MQ135);
  Serial.print(F(" MQ131 Sensor Raw Signal Value (0-1023): "));
  Serial.print(gs_instance.MQ131);
  Serial.print(F(" MQ7 Sensor Raw Signal Value (0-1023): "));
  Serial.println(gs_instance.MQ7);
  
  return gs_instance;
}

bool displayConnectionDetails(void)
{
 
}


// On error, print PROGMEM string to serial monitor and stop
void hang(const __FlashStringHelper *str) {
  digitalWrite(LED_STARTING,LOW);
  digitalWrite(LED_NETWORK_OK, LOW);
  digitalWrite(LED_SYSTEM_HALTED, HIGH);
  
  Serial.println(str);
  for(;;);
}

void connectToNetwork() {

  int attempts = 0;
  boolean wifi = false;
  boolean dhcp = false;

  digitalWrite(LED_STARTING,HIGH);
  digitalWrite(LED_NETWORK_OK, LOW);
  
  Serial.print(F("Initializing WINC1500..."));
  
  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("WiFi shield not present"));
    hang(F("Wifi shield not present"));
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(WLAN_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WLAN_SSID, WLAN_PASS);

    // wait 5 seconds for connection:
    delay(5000);
  }
  // you're connected now, so print out the status:
  printWifiStatus();
  
  digitalWrite(LED_STARTING,LOW);
  digitalWrite(LED_NETWORK_OK, HIGH);
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print(F("SSID: "));
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print(F("IP Address: "));
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print(F("Signal strength (RSSI):"));
  Serial.print(rssi);
  Serial.println(F(" dBm"));
}
