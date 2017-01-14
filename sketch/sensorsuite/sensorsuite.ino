/**
 * Air quality monitoring package for Arduino.  This borrows HEAVILY from various sensor, wifi and other
 * examples found on the internet.  See the project github README.md for a list as best I can remember it.
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
#include <WiFiUdp.h>
#include <Adafruit_GPS.h>

/********************  WLAN settings ********************/
#define ADAFRUIT_WINC1500_IRQ     2  
#define ADAFRUIT_WINC1500_RST     3
#define ADAFRUIT_WINC1500_CS      4           // this pin can be pulled low to switch between SPI devices
#define WLAN_SSID                 "AQnet"
#define WLAN_PASS                 "AQnet123"
#define WLAN_SECURITY             WLAN_SEC_WPA2
#define IDLE_TIMEOUT_MS           3000 

/******************** AM2302 temp and humidity sensor settings ********************/
#define DHT_PIN                   5           // digital input pin
#define DHT_TYPE                  DHT22       // DHT 22  (AM2302)

/******************** Sharp GP2Y10 dust sensor settings ********************/
#define DUST_SENSOR_ANALOG_IN     8           // analog pin to read dust sensor data
#define LED_POWER                 7           // digital pin used to provide power to dust sensor LED
#define SAMPLING_TIME             280         // original value 280
#define DELTA_TIME                40
#define SLEEP_TIME                9680

/******************** MQ135 / MQ131 gas sensor settings ********************/
#define MQ135_ANALOG              14
#define MQ131_ANALOG              13
#define MQ7_ANALOG                15

/******************** Staus LED pins ********************/
#define LED_STARTING              24
#define LED_NETWORK_BAD           26
#define LED_NETWORK_OK            22
#define LED_SYSTEM_HALTED         26

/******************** General application settings ********************/
#define LOOP_DELAY                10000               // how frequently to take a reading from the sensor array (in ms)
#define DATA_VERSION              1                   // version of the API / JSON format that this package is using
#define INIT_PATH                 "/api/initdata"     // url path for saving unit initialization data
#define DATA_PATH                 "/api/aqdata"       // url path for saving air quality data readings
#define DATA_HOST                 "aqmonitor.aqmonitor1.net"
#define PORT                      8080                // port for contacting the API
#define IDLE_TIMEOUT_MS           10000               // timeout for http operations
#define DEVICE_ID                 1                   // id of this device in the AQ network
#define JSON_BUFFER_SIZE          512                 // size of buffer to use for JSON packages
#define CHUNK_SIZE                128                 // size of individual chunks to write out via client
#define READ_LENGTH               15                  // how much of the response to read.  We don't need the whole thing (15 to get the HTTP response line)
#define MAX_CONNECT_ATTEMPTS      5                   // how many times to attemp wifi connect and DHCP lease
#define NTP_PORT                  2390                // local port to listen for NTP packet responses
#define NTP_PACKET_SIZE           48                  // fixed size for an NTP packet
#define GPSECHO                   true                // turn local echo for GPS on / off
#define gpsSerial                 Serial3             // hardware serial port used by the GPS

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

/******************** other initializations ********************/
DHT dht(DHT_PIN, DHT_TYPE);                 // DHT22 temp + humidity sensor
Adafruit_GPS GPS(&gpsSerial);                // GPS instance
WiFiUDP Udp;                                // A UDP instance to let us send and receive packets over UDP
WiFiClient client;                          // A client instance to use to connect to our http server
unsigned long startTime = 0L;               // the startup time, retrieved from an NTP server          
byte packetBuffer[ NTP_PACKET_SIZE];        //buffer to hold incoming and outgoing packets
int status = WL_IDLE_STATUS;                // status of WINC1500 wireless
boolean usingInterrupt = false;             // tracks whether or not we are using interrupt for GPS

void setup() {

  // first thing first, set pin mode for LED pins
  pinMode(LED_STARTING,OUTPUT);
  pinMode(LED_NETWORK_BAD, OUTPUT);
  pinMode(LED_NETWORK_OK, OUTPUT);

  digitalWrite(LED_STARTING, LOW);
  digitalWrite(LED_NETWORK_OK, LOW);
  pinMode(LED_NETWORK_OK, LOW);
 
  // initialize serial comms
  Serial.begin(115200);

  // hello messages!
  Serial.println(F("\r\nInitializing air quality sensor array"));

  // set up the pins for the WINC1500 breakout
  WiFi.setPins(ADAFRUIT_WINC1500_CS, ADAFRUIT_WINC1500_IRQ, ADAFRUIT_WINC1500_RST);

  GPS.begin(9600);
  Serial.println(F("Started GPS serial connection"));
  // Turn on RMC and GGA including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  // set interrupt for gps
  useInterrupt(true);

  Serial.println(F("GPS init commands sent"));
  
  // set up the watchdog timer
  wdt_enable(WDTO_8S); 
   
  connectToNetwork();
  
  wdt_reset();
  
  // send system startup notification to logging server
  // use sendData method to do this, passing path and payload
  char jsonbuffer[JSON_BUFFER_SIZE];
  int len = createInitPayload(jsonbuffer);
  
  //send to server
  //sendData(INIT_PATH, jsonbuffer, len);

  setLocalTime();
  
  // set output pin for dust sensor LED
  pinMode(LED_POWER, OUTPUT);

  // take initial sensor readings and discard.  This prevents weird spikes at startup.
  // dust sensor seems especially prone to super low value spikes when it is first read
  getGasSensorData();
  getTempHumidityData();
  getDustSensorData();

  wdt_reset();
   
  digitalWrite(LED_STARTING,LOW);
  digitalWrite(LED_NETWORK_OK, HIGH);
}

void loop() {
  
  // sleep for the configured interval
  for(uint16_t i = 0; i < LOOP_DELAY; i = i + 5000) {
    delay(5000);
    wdt_reset();
  }

  // if the start time is zero, we haven't yet gotten it successfully. 
  // if this is the case, try at each loop
  if(startTime == 0) {
    setLocalTime();  
  }
  
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

/**
 * Sets the local time using data that comes from an NTP time server.  Without timestamps the data is useless,
 * so this method should set an error LED and halt the system if time cannot be determined.  At a later date this
 * should fall back to the GPS as a secondary time source so we can continue to monitor even if the internet
 * connection is interrupted.
 */
void setLocalTime() {

 // contact NTP server, use response as the current start time
  Udp.begin(NTP_PORT);
  sendNTPpacket(); 
  wdt_reset();
  delay(5000);
  Serial.println("Setting local time from NTP");

  unsigned long lastRead = millis();
  while(!Udp.parsePacket() && millis() - lastRead < IDLE_TIMEOUT_MS) {
    wdt_reset();
    delay(5000);
  }
  
  if (millis() - lastRead < IDLE_TIMEOUT_MS) {
    Serial.println(F("NTP packet received"));
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch); 
    startTime = epoch;    
  }else {
    // don't hang any longer if we can't get time
    //hang(F("Could not get NTP time, timeout exceeded"));
  }
}

/**
 * Creates a device initialization JSON package, used to tell the AQ network when this particular monitor started up.
 */
int createInitPayload(char *jbuf) {
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  // add in the time and version data for this package
  root["time"] = startTime + (int)(millis() / 1000);
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

/**
 * Generates the JSON used to store and transmit the air quality data.  This package of data includes sensor 
 * readings, some system info (device id, data format version), and the current system time in GMT
 */
int createAqPayload(gs gsdata, th thdata, ds dsdata, char *jbuf){
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  // add in the time and version data for this package
  if(startTime != 0) {
    root["time"] = startTime + (int)(millis() / 1000);
  }

  // do we have a GPS fix?  If so, add gps data to the package
  if (GPS.newNMEAreceived()) {
    GPS.parse(GPS.lastNMEA());   
  }
  
  if(GPS.fix) {
    Serial.println(F("GPS fix available"));
    JsonObject& gpsdataobj = root.createNestedObject("gps");
    gpsdataobj["fixquality"] = (int)GPS.fixquality;
    gpsdataobj["latdegrees"] = GPS.latitudeDegrees;
    gpsdataobj["londegrees"] = GPS.longitudeDegrees;
    gpsdataobj["speed"] = GPS.speed;
    gpsdataobj["altitude"] = GPS.altitude;
    gpsdataobj["sats"] = GPS.satellites;
  }else {
    Serial.println(F("No GPS fix available"));
  }
  
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

/**
 * Handles sending generated JSON data to the server configured in the settings
 */
void sendData(char path[], char *json, int len) {

  Serial.print(F("Sending this data: "));
  Serial.println(json);

  char jsondata[len];
  memcpy(jsondata, json, len);
  
  Serial.print(F("Connecting to server: "));
  Serial.print(DATA_HOST);
  Serial.print(F(":"));
  Serial.print(PORT);

  wdt_reset();
   
  client.connect(DATA_HOST, PORT);
  
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
    client.println(DATA_HOST);
    client.print(F("Content-Length: "));
    client.println(clen);
    client.println(F("Connection: close"));
    client.println(F("Content-Type: application/json"));
    client.println("\r\n");
    Serial.println(F("OK"));

    // send json payload to server
    Serial.print(F("Sending content..."));
    //sendChunkedData(jsondata);
    //client.println("{\"test\":\"test\"}");
    client.println(jsondata);
    Serial.println(F("OK"));
  
    // read response
    Serial.println(F("Reading server response"));
    Serial.print(F("Client connected: "));
    Serial.println(client.connected());

    wdt_reset();
    
    unsigned long lastRead = millis();
    int readlen = 0;
    while (readlen < READ_LENGTH && client.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
      Serial.print(F("connected, available data bytes: "));
      Serial.println(client.available());
      delay(100);
      while (client.available() > 0) {
        char c = client.read();
        Serial.print(c);
        lastRead = millis();
        readlen++;
      }
    }
    Serial.print(F("\r\n\r\n"));
    Serial.println(F("Done"));

  }else{
    Serial.println(F("\r\nConnection failed"));
  }
}

/**
 * Sends data to server in chunks of a size set by CHUNK_SIZE
 */
void sendChunkedData(String input) {
  
  // Get String length
  int length = input.length();
  int max_iteration = (int)(length/CHUNK_SIZE);
  if(length % CHUNK_SIZE != 0) {max_iteration++;}

  int st = 0;
  int en = CHUNK_SIZE;
  
  for (int i = 0; i < max_iteration; i++) {
    //Serial.print("chunk: ");
    String chunk = input.substring(st, en);
    char chunkarray[CHUNK_SIZE];
    chunk.toCharArray(chunkarray, CHUNK_SIZE);
    client.print(chunkarray);
    Serial.print(chunkarray);
    Serial.print("|");
    st = en - 1;
    en = st + CHUNK_SIZE;
    if(en > length) {en = length;}
  }  
}

/**
 * Reads raw, voltage and calcualtes particle density from dust sensor,
 * returns data as a struct
 */
struct ds getDustSensorData() {
  struct ds ds_instance;

  digitalWrite(LED_POWER, LOW); // power on the LED
  delayMicroseconds(SAMPLING_TIME);
 
  ds_instance.raw = analogRead(DUST_SENSOR_ANALOG_IN); // read the dust value
 
  delayMicroseconds(DELTA_TIME);
  digitalWrite(LED_POWER, HIGH); // turn the LED off
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

/**
 * Reads the current temp and relative humidity from the DHT sensor,
 * returns data as a struct
 */
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

/**
 * Gets the analog readings from all three MQ series gas sensors,
 * returns the values as a struct
 */
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

/**
 * send an NTP request to the time server running on the data host, do not wait for a response
 */
unsigned long sendNTPpacket()
{
  Serial.println("Sending NTP packet to server");
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;               // LI, Version, Mode
  packetBuffer[1] = 0;                        // Stratum, or type of clock
  packetBuffer[2] = 6;                        // Polling Interval
  packetBuffer[3] = 0xEC;                     // Peer Clock Precision
  
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(DATA_HOST, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

/**
 * On error, print PROGMEM string to serial monitor and stop
 */
void hang(const __FlashStringHelper *str) {
  digitalWrite(LED_STARTING,LOW);
  digitalWrite(LED_NETWORK_OK, LOW);
  digitalWrite(LED_SYSTEM_HALTED, HIGH);
  
  Serial.println(str);
  for(;;);
}

/**
 * Connect to the Wifi network that has been configured, set status LEDs according to connection state
 */
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
  
  wdt_reset();
  
  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(WLAN_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(WLAN_SSID, WLAN_PASS);
    
    wdt_reset();
    
    // wait 5 seconds for connection:
    delay(5000);

    wdt_reset();
  }
  // you're connected now, so print out the status:
  printWifiStatus();
 
}

/**
 * Prints the Wifi status to the serial port, for debugging and testing purposes
 */
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

// *************************** code from Adafruit GPS example ************************

/** 
 *  Interrupt is called once a millisecond, looks for any new GPS data, and stores it
 */
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

/**
 * Sets up interrupt to read GPS data
 */
void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}
