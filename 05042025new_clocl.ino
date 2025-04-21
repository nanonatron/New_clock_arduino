#include <NTPClient.h>
// change next line to use with another board/shield
#include <ESP8266WiFi.h>
//#include <WiFi.h> // for WiFi shield
//#include <WiFi101.h> // for WiFi 101 shield or MKR1000
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include "Button2.h"
#include <EEPROM.h>
#include <FastLED.h>
#include <BH1750.h>
#include <Wire.h>


// FIRST STARTUP BYTE ASSIGNMENT DEFINES  ///
#define EEPROM_SIZE 512  // Define EEPROM size (bytes)
#define SETUP_FLAG_ADDR 0  // First address to store setup flag
#define SSID_ADDR 1         // Address for storing WiFi SSID
#define PASS_ADDR 33        // Address for storing WiFi Password
#define Color_NUM 60

// SEGMENT DIAPLAY //
#define LED_PIN 2         // Data pin for WS2812
#define NUM_LEDS 30       // Total LEDs (7*4 for segments + 2 for colons)
#define BRIGHTNESS 50     // Adjust brightness (0-255)
CRGB leds[NUM_LEDS];

// NTP TIMER //

// COLORS AND TIME SWAPER//
#define btnLeftPin 9
#define btnRightPin 10
Button2 btnLeft; // left button pin
Button2 btnRight; // Right button pin

#define TIMEOUT_MS 5000 //save color after 5 seconds of inactivity
bool lastButtonLeftState = HIGH;
bool lastButtonRightState = HIGH;
CRGB colors[19] = {  // 20 predefined FastLED colors
  CRGB::Red, CRGB::DarkRed, CRGB::FireBrick, CRGB::DarkOrange, CRGB::Gold,
  CRGB::Yellow, CRGB::LawnGreen, CRGB::Green, CRGB::ForestGreen, CRGB::Turquoise,
  CRGB::Teal, CRGB::Cyan, CRGB::SkyBlue, CRGB::Blue, CRGB::Navy,
  CRGB::Purple, CRGB::DeepPink, CRGB::Magenta, CRGB::White
};
int colorIndex = 0;
unsigned long lastPressTime = 0;
bool colorSelected = false;  // Tracks if user has interacted


BH1750 lightMeter; //lightsensor
const int lightPin = A0; // pin for color potentiometer

const long interval = 120000;  // interval at which to request time again

WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 60000);
bool timeSwap = HIGH;

///
//SEGMENT DISPLAY STUFF//
///
// Segment mapping (ABCDEFG order)
const byte numbers[10][7] = {
  {1, 1, 1, 1, 1, 1, 0},  // 0
  {0, 1, 1, 0, 0, 0, 0},  // 1
  {1, 1, 0, 1, 1, 0, 1},  // 2
  {1, 1, 1, 1, 0, 0, 1},  // 3
  {0, 1, 1, 0, 0, 1, 1},  // 4
  {1, 0, 1, 1, 0, 1, 1},  // 5
  {1, 0, 1, 1, 1, 1, 1},  // 6
  {1, 1, 1, 0, 0, 0, 0},  // 7
  {1, 1, 1, 1, 1, 1, 1},  // 8
  {1, 1, 1, 1, 0, 1, 1}   // 9
};
// LED mapping per segment
const byte segmentMap[4][7] = {
  {23, 24, 25, 26, 27, 28, 29}, // Segment 4 (LEDs 24-30)
  {16, 17, 18, 19, 20, 21, 22},// Segment 3 (LEDs 17-23)
  {7, 8, 9, 10, 11, 12, 13},   // Segment 2 (LEDs 8-14)
  {0, 1, 2, 3, 4, 5, 6}       // Segment 1 (LEDs 1-7)
};

// Colon (Delimiters)
const byte colonLeds[2] = {14, 15};


//////////
/// VOIDS BEFORE TIME ///
//////////
// COLOR SWAPER AND SAVER //
void click(Button2& btn){  
  if (btn == btnLeft) {  // Correct way: compare pointers
    colorIndex = (colorIndex + 1) % 19;  // Wrap around using modulo
  } else if (btn == btnRight) {
    colorIndex = (colorIndex - 1 + 19) % 19;  // Ensure wrap-around for negatives
  }

  leds[0] = colors[colorIndex];  
  FastLED.show();
  lastPressTime = millis();  
  colorSelected = true;

}

void doubleClick(Button2& btn) {
  if (timeSwap == HIGH) {
    timeClient.setTimeOffset(3600);
    timeSwap = LOW;  // Use '=' for assignment
  } else {  // Use 'else' instead of another 'if' to avoid unnecessary checks
    timeClient.setTimeOffset(7200);
    timeSwap = HIGH;
  }
}

// --- SoftAP Code ---
const char *ssid = "ESP_Config";  // Name of the Wi-Fi hotspot
const char *password = "12345678"; // Password (must be at least 8 characters)

ESP8266WebServer server(80); // Create a web server on port 80

const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Wi-Fi Setup</title>
</head>
<body>
    <h2>Enter Wi-Fi Details</h2>
    <form action="/submit" method="GET">
        SSID: <input type="text" name="ssid"><br><br>
        Password: <input type="text" name="password"><br><br>
        <input type="submit" value="Submit">
    </form>
</body>
</html>
)rawliteral";

// Handles root page
void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

// Handles form submission
void handleSubmit() {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("password");

    
    Serial.println("Received Wi-Fi Credentials:"); 
    Serial.print("SSID: ");                        
    Serial.println(newSSID);                            
    Serial.print("Password: ");                       
    Serial.println(newPass);                          

    // Save Wi-Fi credentials to EEPROM
    saveToEEPROM(SSID_ADDR, newSSID);
    saveToEEPROM(PASS_ADDR, newPass);
    delay(500);
    Serial.println("SSID and Password have been saved to EEPROM!"); 
    EEPROM.write(SETUP_FLAG_ADDR, 1);  // Reset setup flag for next boot
    EEPROM.commit();

    server.send(200, "text/html", "<h3>Credentials Received! Check Serial Monitor.</h3>");

}

void saveToEEPROM(int startAddr, String data) {
    for (int i = 0; i < data.length(); i++) {
        EEPROM.write(startAddr + i, data[i]);
    }
    EEPROM.write(startAddr + data.length(), '\0'); // Null-terminate string
    EEPROM.commit();
}

void setup(){
  Serial.begin(115200);
  timeClient.begin();
  Wire.begin();
  lightMeter.begin();
  EEPROM.begin(EEPROM_SIZE);  // Initialize EEPROM
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  FastLED.show();
  btnLeft.begin(btnLeftPin);
  btnRight.begin(btnRightPin);
  btnRight.setClickHandler(click);
  btnLeft.setClickHandler(click);
  btnRight.setDoubleClickHandler(doubleClick);
  btnLeft.setDoubleClickHandler(doubleClick);

  colorIndex = EEPROM.read(Color_NUM);

  // WIFI CONNECTION AND FIRST TIME ROUTINE //
  
  if (EEPROM.read(SETUP_FLAG_ADDR) != 1) {
    WiFi.softAP(ssid, password);  // Start as an Access Point
    Serial.println("."); Serial.println("ah, new here?");       
    delay(5000);                                                 
    Serial.println("Wi-Fi Hotspot Started");                      
    Serial.print("Connect to: ");                            
    Serial.println(ssid);                                    
    Serial.println("Go to http://192.168.4.1 in your browser"); 

    server.on("/", handleRoot);        // Handle root page
    server.on("/submit", handleSubmit); // Handle form submission
    server.begin();

    // Wait for user to submit Wi-Fi credentials through the SoftAP
    while (EEPROM.read(SETUP_FLAG_ADDR) != 1) {
      server.handleClient();  // Handle web requests
    }

    Serial.println("WiFi credentials saved!");
    WiFi.softAPdisconnect(); // Disconnect SoftAP after setup

    // CONNECTING TO WIFI //
    String storedSSID = readFromEEPROM(SSID_ADDR, 32);
    String storedPass = readFromEEPROM(PASS_ADDR, 32);
    
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    
 
    Serial.print("Connecting to WiFi: ");                    
    Serial.println(storedSSID);                                 

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      Serial.print(".");  
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");       
    } else {
      Serial.println("\nWiFi Connection Failed. Restarting Setup...");    
      EEPROM.write(SETUP_FLAG_ADDR, 0);  // Reset setup flag for next boot
      EEPROM.commit();
      ESP.restart();
    }

  } else {
    // --- Normal Startup ---

    String storedSSID = readFromEEPROM(SSID_ADDR, 32);
    String storedPass = readFromEEPROM(PASS_ADDR, 32);
    
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    
  
    Serial.print("Connecting to WiFi: ");       
    Serial.println(storedSSID);               

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      Serial.print(".");                       
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected!");         
    } else {
      Serial.println("\nWiFi Connection Failed. Restarting Setup...");   
      EEPROM.write(SETUP_FLAG_ADDR, 0);  // Reset setup flag for next boot
      EEPROM.commit();
      ESP.restart();
    }
  }
}  //  **Added missing closing bracket for `setup()`**




void loop() {
  btnLeft.loop();
  btnRight.loop();
  //////////
  // light sensor readout and mapping
  //////////
  int mappedHue;
  float lux = lightMeter.readLightLevel();
  mappedHue = map(analogRead(lightPin), 10, 1023, 5, 255);
  int light = map(lux, 0, 360, 5, mappedHue);
  FastLED.setBrightness(light);
  timeClient.update();
  

// PART FOR saving color after switch void //

  if (colorSelected && millis() - lastPressTime > 5000) {  // 5-second timeout
        EEPROM.write(Color_NUM, colorIndex);
        EEPROM.commit();
        Serial.println("New color saved");

        colorSelected = false;  // Reset flag so it doesn't keep saving
    }
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes(); 
    displayTime(hours, minutes);     
}


// INTO THE VOIDS !!! //

// Function to read strings from EEPROM
String readFromEEPROM(int startAddr, int maxLength) {
  String data = "";
  for (int i = 0; i < maxLength; i++) {
    char c = EEPROM.read(startAddr + i);
    if (c == '\0') break;
    data += c;
  }
  return data;
}


// CONFIGS HOW TO DISPLAY HOURS AND MINUTES //
void displayTime(int hours, int minutes) {
    FastLED.clear();
    
    int d1 = (hours / 10) % 10;
    int d2 = hours % 10;
    int d3 = (minutes / 10) % 10;
    int d4 = minutes % 10;

    setSegment(0, d1);
    setSegment(1, d2);
    setSegment(2, d3);
    setSegment(3, d4);

      leds[colonLeds[0]] = colors[colorIndex];
      leds[colonLeds[1]] = colors[colorIndex];
    
    
    FastLED.show();
}

// CONFIGURES THE SEGEMNTS FOR DISPLAY //
void setSegment(int seg, int num) {
    for (int i = 0; i < 7; i++) {
        if (numbers[num][i] == 1) {
            leds[segmentMap[seg][i]] = colors[colorIndex];
        }
    }
}
