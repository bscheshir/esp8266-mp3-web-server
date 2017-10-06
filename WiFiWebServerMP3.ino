/*
    This sketch demonstrates how to set up a simple HTTP-like mp3 server.
      http://server_ip/mp3/next will set the mp3 play next,
      http://server_ip/mp3/stop will set the mp3 stop
    server_ip is the IP address of the ESP8266 module, will be
    printed to Serial when the module is connected.
*/

/*
  #include "filename.h" will look in the sketch folder first and next in the library directories
  #include <filename.h> will only look in the library directories
*/
#include "VolumeControl.h"

#include "FS.h"
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <DFPlayer_Mini_Mp3.h> //http://iarduino.ru/file/140.html

//   RX is digital pin 7 (D7 - GIPO13)(connect to TX of other device)
//   TX is digital pin 8 (D8 - GIPO15)(connect to RX of other device)
#define MP3TX 13
#define MP3RX 15
#define VOL A0

const char* ssid = "your-ssid";
const char* password = "your-password";

// softwere serial for mp3 player
SoftwareSerial mp3serial(MP3TX, MP3RX); // RX, TX

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

// VolumeControl callback
void setVolume(int volume) {
  Serial.println((String)"Volume set to " + volume);
  mp3_set_volume(volume);
}

//non-block volume control class
VolumeControl vc(VOL, 500, &setVolume);

bool mp3state() {
  bool result;
  int i = 0;
  while (mp3serial.available()) {
    delay(3);  //пауза для того, чтобы буфер наполнился
    if (mp3serial.available() > 0) {
      char c = mp3serial.read();  //получить один байт из порта
      if (i == 6) {
        result = (bool) c;
      }
      i++;
      if (i > 9) { // Ответ 10 байт
        delay(30);
        break;
      }
      Serial.print(c, HEX);
      Serial.print(' ');
    }
  }
  Serial.println();
  return result;
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // Init mp3
  mp3serial.begin(9600);
  mp3_set_serial(mp3serial);
  delay(100);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Start the server
  server.begin();
  Serial.println("Server started");

  // Print the IP address
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long startMills = millis();
  // Set volume
  vc.update(startMills);

  if(mp3serial.available()){
    Serial.println("State from mp3");
    mp3state();
  }

  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Wait until the client sends some data
  Serial.println("new client");
  while (!client.available()) {
    delay(1);
    if (millis() - startMills > 5000) {
      Serial.println("Client Timeout!");
      client.stop();
      return;
    }
  }
  Serial.println("Client available");

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  // Prepare the response
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\nmp3 is now ";

  // Match the request
  int val;
  if (req.indexOf("/mp3/next") != -1) {
    Serial.println("mp3_next");
    mp3_next();
    delay(100);
    s += "playing next";
  }
  else if (req.indexOf("/mp3/stop") != -1) {
    Serial.println("mp3_stop");
    mp3_stop();
    delay(100);
    s += "stop";
  }
  else if (req.indexOf("/volume/up") != -1) {
    Serial.println("mp3_set_volume +");
    vc.softwareSet(vc.current() + 1);
    delay(100);
    s += "volume up";
  }
  else if (req.indexOf("/volume/down") != -1) {
    Serial.println("mp3_set_volume -");
    vc.softwareSet(vc.current() - 1);
    delay(100);
    s += "volume down";
  }
  else if (req.indexOf("/mp3/status") != -1) {
    Serial.println("mp3_get_state");
    mp3_get_state();
    delay(100);
    while (!mp3serial.available()) {
      delay(1);
      if (millis() - startMills > 5000) {
        Serial.println("Get state Timeout!");
        client.stop();
        return;
      }
    }

    Serial.println(mp3state(), HEX);
    s += "get state";
  }
  else if (req.indexOf("/favicon.ico") != -1) {

    delay(1000);
    if (!SPIFFS.begin()) {
      Serial.println("Failed to mount file system");
      client.stop();
      return;
    }
    // Load favicon file
    File f = SPIFFS.open("/favicon.ico", "r");
    if (!f) {
      Serial.println("Failed to open favicon file");
    }
    else
      Serial.println("Success to open favicon file");

    client.print((String)"HTTP/1.1 200 OK\nContent-Length: " + f.size() + (String)"Content-Type: image/x-icon\n\n");
    Serial.print(f.name());
    Serial.print(": ");
    long s = millis();
    client.write(f, 1460);
    s = millis() - s;
    Serial.print(s);
    Serial.print(" ms  @");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    f.close();

    Serial.println("Favicon has been sended");
    client.stop();
    delay(1);
    return;
  }
  else {
    Serial.println("invalid request");
    client.stop();
    return;
  }

  s += "</html>\n";

  client.flush();


  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected
  // when the function returns and 'client' object is detroyed
}

