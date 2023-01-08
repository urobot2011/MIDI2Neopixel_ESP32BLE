// Developed in Arduino IDE 1.8.19 (Windows)

// ESPAsyncWebServer https://github.com/me-no-dev/ESPAsyncWebServer
// + AsyncTCP https://github.com/me-no-dev/AsyncTCP
// esp32FOTA version 0.2.7 https://github.com/chrisjoyce911/esp32FOTA
// FastLED version 3.5 https://github.com/FastLED/FastLED
// BLEMidi version 0.3.0 https://github.com/max22-/ESP32-BLE-MIDI
// + NimBLE-Arduino version 1.4.0 https://github.com/h2zero/NimBLE-Arduino
// TFT_eSPI version 2.5.0 https://github.com/Bodmer/TFT_eSPI
// + Comment out '#inculde <User_Setup.h>' in C:\Users/(username)/Documents/Arduino/libraries/TFT_eSPI/User_Setup_Select.h file, and comment '#include <User_Setup/Setup25_TTGO_T_Display.h>' turn off processing
#define DEBUG
#define WIFI

#define WIFI_SSID ""
#define WIFI_PW   ""
#define WIFI_HOST "MIDI2Neopixel"

#define HTTP_USERNAME "admin"
#define HTTP_PASSWORD "admin"

#define NOTE_OFFSET         29     // Offset of first note from left (depends on Piano)
#define LEDS_PER_NOTE       2      // How many LED are on per key
#define LED_INT_STEPS       8      // LED Intensity button steps

//-------------------------

#define VAR "1.0.0"

#include <SPIFFS.h>
#include <FastLED.h>
#include <BLEMidi.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

#ifdef WIFI
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <esp32FOTA.hpp>
#endif

#define BUTTON_PIN1 0
#define BUTTON_PIN2 35
#define LED_ACT_PIN 32
#define LED_PIN     33
#define NUM_LEDS    144
#define LED_TYPE    WS2812B
#define COLOR_ORDER RGB
#define AMPERAGE    1500     // Power supply amperage in mA

CRGB leds[NUM_LEDS];
TFT_eSPI tft = TFT_eSPI();
esp32FOTA esp32FOTA("MIDI2Neopixel_ESP32BLE", VAR);

#ifdef WIFI
char* ssid = WIFI_SSID;
char* password = WIFI_PW;
char* host = WIFI_HOST;

char* http_username = HTTP_USERNAME;
char* http_password = HTTP_PASSWORD;

const char* ssidPath = "/data_ssid.txt";
const char* passwordPath = "/data_password.txt";
const char* hostPath = "/data_host.txt";
const char* wifierrorPath = "/error_wifi.txt";
const char* httpusernamePath = "/data_http_username.txt";
const char* httppasswordPath = "/data_http_password.txt";

const char* manifest_url = "http://urobot2011.github.io/MIDI2Neopixel_ESP32BLE/MIDI2Neopixel_ESP32BLE/MIDI2Neopixel_ESP32BLE.json";

bool wifimanagerbool = 0;

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>MIDI2Neopixel</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
      display: inline-block;
      text-align: center;
    }
    h2 {
      font-size: 1.8rem;
    }
    body {
      max-width: 600px;
      margin:0px auto;
      padding-bottom: 10px;
    }
  </style>
</head>
<body>
  <h2>MIDI2Neopixel</h2>
  <form action="/update" method="GET">
    <p>
      <label for = "ssid">SSID</label>
      <input type = "text" id = "ssid" name = "ssid" value = "%SSID%"><br>
      <label for = "pass">Password</label>
      <input type = "text" id = "pass" name = "pass" value = "%PASS%"><br>
      <label for = "host">Host</label>
      <input type = "text" id = "host" name = "host" value = "%HOST%"><br>
      <label for = "httpusername">Http username</label>
      <input type = "text" id = "httpusername" name = "httpusername" value = "%HTTPUSERNAME%"><br>
      <label for = "httppassword">Http password</label>
      <input type = "text" id = "httppassword" name = "httppassword" value = "%HTTPPASSWORD%"><br>
      <input type = "submit" value = "Submit">
    </p>
  </form>
  <button onclick="logoutButton()">Logout</button>
  %ESP32%
<script>
function logoutButton() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/logout", true);
  xhr.send();
  setTimeout(function(){ window.open("/logged-out","_self"); }, 1000);
}
</script>
</body>
</html>
)rawliteral";

const char logout_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <p>Logged out or <a href="/">return to homepage</a>.</p>
</body>
</html>
)rawliteral";

void serverstart(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
  });
    
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });

  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage;
    String inputParam;
    inputMessage = "No message sent";
    inputParam = "none";
    
    if (request->hasParam("ssid")) {
      inputMessage = request->getParam("ssid")->value();
      inputParam = "ssid";
      File data_ssid_write = SPIFFS.open(ssidPath, FILE_WRITE);
      data_ssid_write.println(inputMessage);
      data_ssid_write.close();
      ESP.restart();
    }
    if (request->hasParam("password")) {
      inputMessage = request->getParam("password")->value();
      inputParam = "password";
      File data_password_write = SPIFFS.open(passwordPath, FILE_WRITE);
      data_password_write.println(inputMessage);
      data_password_write.close();
      ESP.restart();
    }
    if (request->hasParam("host")) {
      inputMessage = request->getParam("host")->value();
      inputParam = "host";
      File data_host_write = SPIFFS.open(ssidPath, FILE_WRITE);
      data_host_write.println(inputMessage);
      data_host_write.close();
      ESP.restart();
    }
    if (request->hasParam("httpusername")) {
      inputMessage = request->getParam("httpusername")->value();
      inputParam = "httpusername";
      File data_httpusername_write = SPIFFS.open(httpusernamePath, FILE_WRITE);
      data_httpusername_write.println(inputMessage);
      data_httpusername_write.close();
      ESP.restart();
    }
    if (request->hasParam("httppassword")) {
      inputMessage = request->getParam("httppassword")->value();
      inputParam = "httppassword";
      File data_httppassword_write = SPIFFS.open(httppasswordPath, FILE_WRITE);
      data_httppassword_write.println(inputMessage);
      data_httppassword_write.close();
      ESP.restart();
    }
    request->send(200, "text/plain", "OK");
  });
}

String processor(const String& var){
  if(var == "SSID"){
    String txt = String(ssid);
    return txt;
  }
  if(var == "PASS"){
    String txt = String(password);
    return txt;
  }
  if(var == "HOST"){
    String txt = String(host);
    return txt;
  }
  if(var == "HTTPUSERNAME"){
    String txt = String(http_username);
    return txt;
  }
  if(var == "HTTPPASSWORD"){
    String txt = String(http_password);
    return txt;
  }
  return String();
}

void wifimanager(){
  wifimanagerbool = 1;
  WiFi.softAP(host, http_password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.println("Connect to wifi ssid: " + String(host) + ", password: " + String(http_password) + ",");
  Serial.println("Go to " + String(myIP) + ".");
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 2);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.println("MIDI2Neopixel");
  tft.println("Connect to wifi ssid: " + String(host) + ", password: " + String(http_password) + ",");
  tft.println("Go to " + String(myIP) + ".");
//  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//    if(!request->authenticate(http_username, http_password))
//      return request->requestAuthentication();
//    request->send_P(200, "text/html", index_html, processor);
//  });
//    
//  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send(401);
//  });
//
//  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send_P(200, "text/html", logout_html, processor);
//  });

  serverstart();
  
  server.begin();
  while (1) {
    
  }
}
void OTAStart(){
  String type;
  if (ArduinoOTA.getCommand() == U_FLASH)
    type = "sketch";
  else // U_SPIFFS
    type = "filesystem";
    
  // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
  Serial.println("Start updating " + type);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, 2);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.println("MIDI2Neopixel");
  tft.setTextColor(TFT_YELLOW);
  tft.println("Start updating");
}
void OTAEnd(){
  Serial.println("\nEnd");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.println("MIDI2Neopixel");
    tft.setTextColor(TFT_YELLOW);
    tft.println("End updating");
    CRGB color = 0xFF0000;
    for (int a = 1; a < 100; a++) {
      leds[a] = color;
    }
    FastLED.show();
}
void OTAProgress(unsigned int progress, unsigned int total){
  
}
void OTAError(ota_error_t error){
  
}
#endif

byte color = 0xFFFFFF;
int ledProgram = 1;
int ledIntensity = 2;  // default LED brightness
int ledBrightness = ceil(255 / LED_INT_STEPS * ledIntensity); // default LED brightness
int buttonState1;
int buttonState2;
int lastButtonState1 = LOW;
int lastButtonState2 = LOW;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay = 50;
unsigned long lastDebounceTime3 = 0;
unsigned long lastDebounceTime4 = 0;
unsigned long debounceDelay2 = 3000;
bool button1_bool = 0;
bool button2_bool = 0;
unsigned int Progress = 0;

//------------------------------------------- FUNCTIONS ----------------------------------------------//

const char* pitch_name(byte pitch) {
  static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return names[pitch % 12];
}

int pitch_octave(byte pitch) {
  return (pitch / 12) - 1;
}

void SetNote(byte pitch, CRGB color) {
  int b = 0;
  int c = 0;
  for (int a = 0; a < LEDS_PER_NOTE; a++) {
    if (int(pitch) > 44) { // led stripe solder joints compensation
      b = 1;//64
    } else {
      b = 0;
    }
    leds[(((pitch - NOTE_OFFSET)*LEDS_PER_NOTE) + a - b - c)] = color;
  }
}

void SetNote2(byte pitch, int hue) {
  int b = 0;
  int c = 0;
  for (int a = 0; a < LEDS_PER_NOTE; a++) {
    if (int(pitch) > 44) { // led stripe solder joints compensation
      b = 1;//64
    } else {
      b = 0;
    }
    leds[(((pitch - NOTE_OFFSET)*LEDS_PER_NOTE) + a - b - c)].setHSV(hue, 255, 255);
  }
}

void noteOn(byte channel, byte pitch, byte velocity) {
  if (velocity > 0) {
    switch (channel) {

      // Left Hand
      case 144: // Default Channel is 144
      case 155:
      case 145 ... 149:
        if (ledProgram == 1) {
          SetNote(pitch, 0x0000FF);
        } else if (ledProgram == 2) {
          SetNote(pitch, 0x00FF00);
        } else if (ledProgram == 3) {
          SetNote2(pitch, pitch * velocity);
        } else if (ledProgram == 4) {
          SetNote2(pitch, velocity * 2);
        } else if (ledProgram == 5) {
          SetNote(pitch, 0xFFFFFF);
        } else if (ledProgram == 6) {
          SetNote2(pitch, ceil((pitch - NOTE_OFFSET) * (255 / (NUM_LEDS / LEDS_PER_NOTE))));
        } else {
        }
        break;

      // Right Hand
      case 156:
      //case 150 ... 154:
      //SetNote(pitch, 0x00FF00);
      //break;
      case 150 ... 154:
        if (ledProgram == 1) {
          SetNote(pitch, 0x0000FF);
        } else if (ledProgram == 2) {
          SetNote(pitch, 0x00FF00);
        } else if (ledProgram == 3) {
          SetNote2(pitch, pitch * velocity);
        } else if (ledProgram == 4) {
          SetNote2(pitch, velocity * 2);
        } else if (ledProgram == 5) {
          SetNote(pitch, 0xFFFFFF);
        } else if (ledProgram == 6) {
          SetNote2(pitch, ceil((pitch - NOTE_OFFSET) * (255 / (NUM_LEDS / LEDS_PER_NOTE))));
        } else {
        }
        break;

      default:
        //SetNote(pitch, 0xFFFFFF);
        //break;
        if (ledProgram == 1) {
          SetNote(pitch, 0x0000FF);
        } else if (ledProgram == 2) {
          SetNote(pitch, 0x00FF00);
        } else if (ledProgram == 3) {
          SetNote2(pitch, pitch * velocity);
        } else if (ledProgram == 4) {
          SetNote2(pitch, velocity * 2);
        } else if (ledProgram == 5) {
          SetNote(pitch, 0xFFFFFF);
        } else if (ledProgram == 6) {
          SetNote2(pitch, ceil((pitch - NOTE_OFFSET) * (255 / (NUM_LEDS / LEDS_PER_NOTE))));
        } else {
        }
        break;
    }

#ifdef DEBUG
    Serial.println("Note ON   - Channel:" + String(channel) + " Pitch:" + String(pitch) + " Note:" + pitch_name(pitch) + String(pitch_octave(pitch)) + " Velocity:" + String(velocity));
#endif
  } else {
    SetNote(pitch, 0x000000); // black
#ifdef DEBUG
    Serial.println("Note OFF2 - Channel:" + String(channel) + " Pitch:" + String(pitch) + " Note:" + pitch_name(pitch) + String(pitch_octave(pitch)) + " Velocity:" + String(velocity));
#endif
  }
  FastLED.show();
}

void noteOff(byte channel, byte pitch, byte velocity) {
  if (ledProgram == 5) {
    fill_rainbow( leds, NUM_LEDS, 0, 5);
  } else {
    SetNote(pitch, 0x000000); // black
  }

#ifdef DEBUG
  Serial.print("Note OFF  - Channel:");
  Serial.print(String(channel));
  Serial.print(" Pitch:");
  Serial.print(String(pitch));
  Serial.print(" Note:");
  Serial.print(pitch_name(pitch) + String(pitch_octave(pitch)));
  Serial.print(" Velocity:");
  Serial.println(String(velocity));
#endif
  FastLED.show();
}

void controlChange(byte channel, byte control, byte value) {
#ifdef DEBUG
  Serial.print("Control  - Channel:");
  Serial.print(String(channel));
  Serial.print(" Control:");
  Serial.print(String(control));
  Serial.print(" Value:");
  Serial.println(String(value));
#endif
}

void connected();

void onNoteOn(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
  noteOn(byte(channel), byte(note), byte(velocity));
}

void onNoteOff(uint8_t channel, uint8_t note, uint8_t velocity, uint16_t timestamp) {
  noteOff(byte(channel), byte(note), byte(velocity));
}

void onControlChange(uint8_t channel, uint8_t controller, uint8_t value, uint16_t timestamp) {
  controlChange(byte(channel), byte(controller), byte(value));
}

void connected() {
  Serial.println("Connected");
}

//-------------------------------------------- SETUP ----------------------------------------------//

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println("MIDI2Neopixel Booting...");
  Serial.println("Initializing bluetooth");
  BLEMidiClient.enableDebugging();
#endif
  tft.init();
  tft.setRotation(1);
#ifdef WIFI
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  File data_ssid = SPIFFS.open(ssidPath);
  File data_password = SPIFFS.open(passwordPath);
  File data_host = SPIFFS.open(hostPath);
  File data_httpusername = SPIFFS.open(httpusernamePath);
  File data_httppassword = SPIFFS.open(httppasswordPath);

  if (!data_ssid || !data_password || !data_host || !data_httpusername || !data_httppassword) {
    File data_ssid_write = SPIFFS.open(ssidPath, FILE_WRITE);
    File data_password_write = SPIFFS.open(passwordPath, FILE_WRITE);
    File data_host_write = SPIFFS.open(httpusernamePath, FILE_WRITE);
    File data_httpusername_write = SPIFFS.open(passwordPath, FILE_WRITE);
    File data_httppassword_write = SPIFFS.open(httppasswordPath, FILE_WRITE);
    data_ssid_write.println(WIFI_SSID);
    data_password_write.println(WIFI_PW);
    data_host_write.println(WIFI_HOST);
    data_httpusername_write.println(HTTP_USERNAME);
    data_httppassword_write.println(HTTP_PASSWORD);
    
    data_ssid_write.close();
    data_password_write.close();
    data_host_write.close();
    data_httpusername_write.close();
    data_httppassword_write.close();
  }

  String txt;

  txt = "";
  while (data_ssid.available()) {
    //Serial.write(data_ssid.read());
    txt += data_ssid.read();
  }
  txt.toCharArray(ssid, txt.length());
  txt = "";
  while (data_password.available()) {
    //Serial.write(data_ssid.read());
    txt += data_password.read();
  }
  txt.toCharArray(password, txt.length());
  txt = "";
  while (data_host.available()) {
    //Serial.write(data_ssid.read());
    txt += data_host.read();
  }
  txt.toCharArray(host, txt.length());
  txt = "";
  while (data_httpusername.available()) {
    //Serial.write(data_ssid.read());
    txt += data_httpusername.read();
  }
  txt.toCharArray(http_username, txt.length());
  txt = "";
  while (data_httppassword.available()) {
    //Serial.write(data_ssid.read());
    txt += data_httppassword.read();
  }
  txt.toCharArray(http_password, txt.length());
  txt = "";
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed!");
    File wifierror = SPIFFS.open(wifierrorPath);
    if(!wifierror){
      if(ssid == "" && password == "") {
        wifimanager();
        break;
      }
      Serial.println("Rebooting...");
      File wifierror_write = SPIFFS.open(wifierrorPath, FILE_WRITE);
      wifierror_write.println("1");
      wifierror_write.close();
      delay(1000);
      ESP.restart();
    } else {
      SPIFFS.remove(wifierrorPath);
      wifimanager();
      break;
    }
  }
  if(!SPIFFS.remove(wifierrorPath)){
    
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(host);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    OTAStart();
  })
  .onEnd([]() {
    OTAEnd();
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    //OTAProgress(progress, total);
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.println("MIDI2Neopixel");
    tft.setTextColor(TFT_YELLOW);
    tft.print("Updating progress: ");
    tft.println((progress / (total / 100)));
    fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
    CRGB color = 0x0000FF;
    leds[(progress / (total / 100))] = color;
    for (int a = 1; a < (progress / (total / 100)); a++) {
      leds[a] = color;
    }
    color = 0xFF0000;
    leds[(100)] = color;
    FastLED.show();
    Progress = progress;
  })
  .onError([](ota_error_t error) {
    //OTAError(error);
    Serial.printf("Error[%u]: ", error);
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.println("MIDI2Neopixel");
    tft.setTextColor(TFT_YELLOW);
    tft.print("Updating Error[");
    tft.print(error);
    tft.print("]: ");
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
      tft.println("Auth Failed!");
    }
    else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
      tft.println("Begin Failed!");
    }
    else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
      tft.println("Connect Failed!");
    }
    else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
      tft.println("Receive Failed!");
    }
    else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
      tft.println("End Failed!");
    }
    CRGB color = 0xFF0000;
    for (int a = 1; a < Progress; a++) {
      leds[a] = color;
    }
    FastLED.show();
  });

  ArduinoOTA.begin();

  esp32FOTA.setManifestURL( manifest_url );
  esp32FOTA.setProgressCb( [](size_t progress, size_t size) {
    Serial.printf("Progress: %u%%\r", (progress / (size / 100)));
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.println("MIDI2Neopixel");
    tft.setTextColor(TFT_YELLOW);
    tft.print("Updating progress: ");
    tft.println((progress / (size / 100)));
    fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
    CRGB color = 0x0000FF;
    leds[(progress / (size / 100))] = color;
    for (int a = 1; a < (progress / (size / 100)); a++) {
      leds[a] = color;
    }
    color = 0xFF0000;
    leds[(100)] = color;
    FastLED.show();
    Progress = progress;
  });
  esp32FOTA.setUpdateFinishedCb( [](int partition, bool restart_after) {
    Serial.printf("Update could not begin with %s partition\n", partition==U_SPIFFS ? "spiffs" : "firmware" );
    // do some stuff e.g. notify a MQTT server the update completed successfully
    Serial.println("\nEnd");
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_WHITE);
    tft.println("MIDI2Neopixel");
    tft.setTextColor(TFT_YELLOW);
    tft.println("End updating");
    CRGB color = 0xFF0000;
    for (int a = 1; a < 100; a++) {
      leds[a] = color;
    }
    FastLED.show();
    if( restart_after ) {
      ESP.restart();
    }
  });

  serverstart();
  server.begin();

  Serial.println("Wifi ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  //BLEMidiClient.begin("MIDI2Neopixel");
  BLEMidiServer.begin("MIDI2Neopixel");
  BLEMidiServer.setOnConnectCallback(connected);
  BLEMidiServer.setOnDisconnectCallback([]() {    // To show how to make a callback with a lambda function
    Serial.println("Disconnected");
  });
  BLEMidiServer.setNoteOnCallback(onNoteOn);
  BLEMidiServer.setNoteOffCallback(onNoteOff);
  BLEMidiServer.setControlChangeCallback(onControlChange);
  delay(3000); // power-up safety delay
  FastLED.setMaxPowerInVoltsAndMilliamps(5, AMPERAGE);
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(LED_ACT_PIN, OUTPUT);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(ledBrightness);
  fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
  SetNote(NOTE_OFFSET + ledProgram - 1, 0x0000FF);
  FastLED.show();
  tft.fillScreen(TFT_BLACK);
#ifdef DEBUG
  Serial.println("MIDI2Neopixel is ready!");
#endif
  tft.setTextSize(1);
  tft.setCursor(0, 0, 2);
  tft.setTextColor(TFT_BLUE, TFT_WHITE);
  tft.println("MIDI2Neopixel is ready!");
  tft.print("IP address: ");
  tft.println(WiFi.localIP());
  fill_rainbow( leds, NUM_LEDS, 0, 5);
  FastLED.show();
  delay(3000);
  fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
  FastLED.show();
}

//--------------------------------------------- LOOP ----------------------------------------------//
void loop() {
  ////////////////////////////////////////////// Button 1 /////////////////////////////////////////////

  int reading1 = digitalRead(BUTTON_PIN1);

  if (reading1 != lastButtonState1) {
    lastDebounceTime1 = millis();
  }

  if ((millis() - lastDebounceTime1) > debounceDelay) {
    if (reading1 != buttonState1) {
      buttonState1 = reading1;

      // only toggle the LED if the new button state is HIGH
      if (buttonState1 == LOW) {
        ledProgram++;
        if (ledProgram > 6) {
          ledProgram = 1;
        }
#ifdef DEBUG
        Serial.println("Button 1 pressed! Program is " + String(ledProgram));
#endif
        // Set LED indication for program
        fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
        SetNote(NOTE_OFFSET + ledProgram - 1, 0xFF0000);
        FastLED.show();
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0, 2);
        tft.setTextColor(TFT_BLUE, TFT_WHITE);
        tft.println("MIDI2Neopixel");
        tft.setTextColor(TFT_YELLOW);
        tft.print("ledIntensity: ");
        tft.println(ledIntensity);
        tft.print("ledProgram: ");
        tft.println(ledProgram);
        lastDebounceTime3 = millis();
        button1_bool = 1;
      }
    }
    if (((millis() - lastDebounceTime3) > debounceDelay2) && button1_bool) {
      fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
      FastLED.show();
      button1_bool = 0;
    }
  }

  ////////////////////////////////////////////// Button 2 /////////////////////////////////////////////

  int reading2 = digitalRead(BUTTON_PIN2);

  if (reading2 != lastButtonState2) {
    lastDebounceTime2 = millis();
  }

  if ((millis() - lastDebounceTime2) > debounceDelay) {
    if (reading2 != buttonState2) {
      buttonState2 = reading2;

      // only toggle the LED if the new button state is HIGH
      if (buttonState2 == LOW) {
        ledIntensity++;
        SetNote(NOTE_OFFSET + ledIntensity - 2, 0x000000);
        if (ledIntensity > LED_INT_STEPS) {
          ledIntensity = 1;
        }
        // Map ledIntensity to ledBrightness
        ledBrightness = map(ledIntensity, 1, LED_INT_STEPS, 3, 255);
        FastLED.setBrightness(ledBrightness);
#ifdef DEBUG
        Serial.print("Button 2 pressed! Intensity is ");
        Serial.print(String(ledIntensity));
        Serial.print(" and brightness to ");
        Serial.print(String(ledBrightness));
#endif
        // Set LED indication for program
        SetNote(NOTE_OFFSET + ledIntensity - 1, 0x00FF00);
        FastLED.show();
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 0, 2);
        tft.setTextColor(TFT_BLUE, TFT_WHITE);
        tft.println("MIDI2Neopixel");
        tft.setTextColor(TFT_YELLOW);
        tft.print("ledIntensity: ");
        tft.println(ledIntensity);
        tft.print("ledProgram: ");
        tft.println(ledProgram);
        lastDebounceTime4 = millis();
        button2_bool = 1;
      }
    }
    if (((millis() - lastDebounceTime4) > debounceDelay2) && button2_bool) {
      fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
      FastLED.show();
      button2_bool = 0;
    }
  }

  if (BLEMidiServer.isConnected()) {
  }

  lastButtonState1 = reading1;
  lastButtonState2 = reading2;

#ifdef WIFI
  ArduinoOTA.handle();
  //server.handleClient();
  esp32FOTA.handle();
#endif
}
//------------------------------------------ END OF LOOP -------------------------------------------//
