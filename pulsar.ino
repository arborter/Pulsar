#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "M5Cardputer.h"
#include "M5GFX.h"
#include <M5Unified.h>
#include "Adafruit_NeoPixel.h"
#include "M5Cardputer.h"
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool ledOn = true;// change this to true to get cool led effect (only on fire)


WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;



// Menu code:
int currentIndex = 0, lastIndex = -1;
bool inMenu = true;
const char* menuItems[] = {
    "Scan WiFi",
    "Select Network",
    "Show WiFi Details",
    "Messenger",
    "Notes"
};

const int menuSize = sizeof(menuItems) / sizeof(menuItems[0]);

const uint8_t signalPin = 2;                   // Output pin for the signal

//led begin

#define PIN 21
#define NUMPIXELS 1

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB);
int delayval = 100;


void setColorRange(int startPixel, int endPixel, uint32_t color) {
  for (int i = startPixel; i <= endPixel; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
  delay(30);
}

//led end

const int maxMenuDisplay = 4;
int menuStartIndex = 0;

String ssidList[100];
int numSsid = 0;
bool isOperationInProgress = false;
int currentListIndex = 0;


//taskbar
M5Canvas taskBarCanvas(&M5.Display); // Framebuffer for the taskbar
//taskbar end






// initialization step

void setup() {
  M5.begin();
  Serial.begin(115200);
  M5.Lcd.setRotation(1);
  M5.Display.setTextSize(1.5);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.setTextFont(1);
  pinMode(signalPin, OUTPUT);
  digitalWrite(signalPin, LOW);
  taskBarCanvas.createSprite(M5.Display.width(), 12); // Create a framebuffer for the taskbar
  const char* startUpMessages[] = {
    "  0000000000 ",
    " 7777777777 "};

  const int numMessages = sizeof(startUpMessages) / sizeof(startUpMessages[0]);

  randomSeed(esp_random());
  int randomIndex = random(numMessages);
  const char* randomMessage = startUpMessages[randomIndex];

  int textY = 30;
  int lineOffset = 10;
  int lineY1 = textY - lineOffset;
  int lineY2 = textY + lineOffset + 30;

  M5.Display.clear();
  M5.Display.drawLine(0, lineY1, M5.Display.width(), lineY1, TFT_GREEN);
  M5.Display.drawLine(0, lineY2, M5.Display.width(), lineY2, TFT_GREEN);

  // Screen width
  int screenWidth = M5.Lcd.width();

  // Texts to display
  const char* text1 = "untiled";
  const char* text2 = "by cyberia";
  const char* text3 = "08292024";

  // Measurement of text width and calculation of cursor position
  int text1Width = M5.Lcd.textWidth(text1);
  int text2Width = M5.Lcd.textWidth(text2);
  int text3Width = M5.Lcd.textWidth(text3);

  int cursorX1 = (screenWidth - text1Width) / 2;
  int cursorX2 = (screenWidth - text2Width) / 2;
  int cursorX3 = (screenWidth - text3Width) / 2;

  // Y position for each line
  int textY1 = textY;
  int textY2 = textY + 20;
  int textY3 = textY + 45;

  // display on the screen
  M5.Lcd.setCursor(cursorX1, textY1);
  M5.Lcd.println(text1);

  M5.Lcd.setCursor(cursorX2, textY2);
  M5.Lcd.println(text2);

  M5.Lcd.setCursor(cursorX3, textY3);
  M5.Lcd.println(text3);

  // Serial display -- 
  Serial.println("-------------------");
  Serial.println("...");
  Serial.println("TEST MESSAGE");
  Serial.println("DELAY");
  Serial.println("-------------------");
  M5.Display.setCursor(0, textY + 80);
  M5.Display.println(randomMessage);
  Serial.println(" ");
  Serial.println(randomMessage);
  Serial.println("-------------------");

  if (ledOn) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(250);
  }
  pixels.begin(); // led init
  //cardgps.begin(9600, SERIAL_8N1, 1, -1); // Ensure that the RX/TX pins are correctly configured for your hardware
  pinMode(signalPin, OUTPUT);
  digitalWrite(signalPin, LOW);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  drawMenu();
}

unsigned long lastTaskBarUpdateTime = 0;
const long taskBarUpdateInterval = 100; // Update every second

 String getBatteryLevel() {

  if (M5.Power.getBatteryLevel() < 0) {

    return String("error");
  } else {
    return String(M5.Power.getBatteryLevel());
  }
}

void drawTaskBar() {
  taskBarCanvas.fillRect(0, 0, taskBarCanvas.width(), 10, TFT_NAVY); // best color: TFT_navy rectangle at screen's top
  taskBarCanvas.fillRect(0, 10, taskBarCanvas.width(), 2, TFT_GREEN); 
  taskBarCanvas.setTextColor(TFT_GREEN);
  

  // Display the battery level on the righ
  String batteryLevel = getBatteryLevel();
  int batteryWidth = taskBarCanvas.textWidth(batteryLevel + "%");
  taskBarCanvas.setCursor(taskBarCanvas.width() - batteryWidth - 5, 2); // Position on the right
  taskBarCanvas.print(batteryLevel + "%");

  // Display the taskbar framebuffer
  taskBarCanvas.pushSprite(0, 0);
}

void handleDnsRequestSerial() {
  dnsServer.processNextRequest();
  server.handleClient();
  checkSerialCommands();
}

// execution step
void loop() {
  M5.update();
  M5Cardputer.update();
  handleDnsRequestSerial();
  unsigned long currentMillis = millis();

  // Update the taskbar independently of the menu
  if (currentMillis - lastTaskBarUpdateTime >= taskBarUpdateInterval && inMenu) {
    drawTaskBar();
    lastTaskBarUpdateTime = currentMillis;
  }

  if (inMenu) {
    if (lastIndex != currentIndex) {
      drawMenu();
      lastIndex = currentIndex;
    }
    handleMenuInput();
  }
}

void drawMenu() {
  M5.Display.fillRect(0, 13, M5.Display.width(), M5.Display.height() - 13, TFT_BLACK); // Clear the bottom part of the screen
  M5.Display.setTextSize(1.5); // Ensure the text size is correct
  M5.Display.setTextFont(1);

  int lineHeight = 13; // Increase line height if necessary
  int startX = 0;
  int startY = 10; // Adjust to leave room for the taskbar

  for (int i = 0; i < maxMenuDisplay; i++) {
    int menuIndex = menuStartIndex + i;
    if (menuIndex >= menuSize) break;

    if (menuIndex == currentIndex) {
      M5.Display.fillRect(0, 1 + startY + i * lineHeight, M5.Display.width(), lineHeight, TFT_NAVY);
      M5.Display.setTextColor(TFT_GREEN);
    } else {
      M5.Display.setTextColor(TFT_WHITE);
    }
    M5.Display.setCursor(startX, startY + i * lineHeight + (lineHeight / 2) - 4); // Adjust here
    M5.Display.println(menuItems[menuIndex]);
  }
  M5.Display.display();
}

// execution of menu operations
void executeMenuItem(int index) {
  inMenu = false;
  isOperationInProgress = true;
  switch (index) {
    case 0:
      scanWifiNetworks();
      break;
    case 1:
      showWifiList();
      break;
    case 2:
      showWifiDetails(currentListIndex);
      break;
      case 3:
      messenger(); /*  */
      break;
      case 4:
      notes(); /*  */
      break;
  }
  isOperationInProgress = false;
}

void enterDebounce() {
  while (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    M5.update();
    M5Cardputer.update();
    delay(10); // Small delay to reduce CPU load
  }
}

void handleMenuInput() {
  static unsigned long lastKeyPressTime = 0;
  const unsigned long keyRepeatDelay = 150; // Delay between key press repetitions
  static bool keyHandled = false;
  static int previousIndex = -1; // To track index changes
    enterDebounce();
  M5.update();
  M5Cardputer.update();

  // Variable to track menu state changes
  bool stateChanged = false;

  if (M5Cardputer.Keyboard.isKeyPressed(';')) {
    if (millis() - lastKeyPressTime > keyRepeatDelay) {
      currentIndex--;
      if (currentIndex < 0) {
        currentIndex = menuSize - 1;  // Loop at the end of the menu
      }
      lastKeyPressTime = millis();
      stateChanged = true;
    }
    keyHandled = true;
  } else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
    if (millis() - lastKeyPressTime > keyRepeatDelay) {
      currentIndex++;
      if (currentIndex >= menuSize) {
        currentIndex = 0;  // Loop at the beginning of the menu
      }
      lastKeyPressTime = millis();
      stateChanged = true;
    }
    keyHandled = true;
  } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
    executeMenuItem(currentIndex);
    stateChanged = true;
    keyHandled = true;
  } else {
    // No relevant key is pressed, reset the flag
    keyHandled = false;
  }

  // Reset the last press time if no key is pressed
  if (!keyHandled) {
    lastKeyPressTime = 0;
  }

  // Update the display only if the menu state has changed
  if (stateChanged || currentIndex != previousIndex) {
    menuStartIndex = max(0, min(currentIndex, menuSize - maxMenuDisplay));
    drawMenu();
    previousIndex = currentIndex; // Update the previous index
  }
}

String currentlySelectedSSID = "";

void selectNetwork(int index) {
  if (index >= 0 && index < numSsid) {
    currentlySelectedSSID = ssidList[index];
    Serial.println("SSID select: " + currentlySelectedSSID);
  } else {
    Serial.println("Index SSID invalid");
  }
}

void scanWifiNetworks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  unsigned long startTime = millis();
  int n;
  while (millis() - startTime < 5000) {
    M5.Display.clear();
    M5.Display.fillRect(0, M5.Display.height() - 20, M5.Display.width(), 20, TFT_BLACK);
    M5.Display.setCursor(12 , M5.Display.height() / 2 );
    M5.Display.print("Scan in progress... ");
    Serial.println("-------------------");
    Serial.println("WiFi Scan in progress... ");
    M5.Display.display();
    n = WiFi.scanNetworks();
    if (n != WIFI_SCAN_RUNNING) break;
  }
  Serial.println("-------------------");
  Serial.println("Near Wifi Network : ");
  numSsid = min(n, 100);
  for (int i = 0; i < numSsid; i++) {
    ssidList[i] = WiFi.SSID(i);
    Serial.print(i);
    Serial.print(": ");
    Serial.println(ssidList[i]);
  }
  Serial.println("-------------------");
  Serial.println("WiFi Scan Completed ");
  Serial.println("-------------------");
  waitAndReturnToMenu("Scan Completed");

}

void showWifiList() {
  bool needsDisplayUpdate = true;  // lag to determine if the display needs to be updated
  enterDebounce();
  static bool keyHandled = false;
  while (!inMenu) {
    if (needsDisplayUpdate) {
      M5.Display.clear();
      const int listDisplayLimit = M5.Display.height() / 13; // Adjust according to the new text size
      int listStartIndex = max(0, min(currentListIndex, numSsid - listDisplayLimit));

      M5.Display.setTextSize(1.5);
      for (int i = listStartIndex; i < min(numSsid, listStartIndex + listDisplayLimit + 1); i++) {
        if (i == currentListIndex) {
          M5.Display.fillRect(0, (i - listStartIndex) * 13, M5.Display.width(), 13, TFT_NAVY); // Adjust the height
          M5.Display.setTextColor(TFT_GREEN);
        } else {
          M5.Display.setTextColor(TFT_WHITE);
        }
        M5.Display.setCursor(2, (i - listStartIndex) * 13); // Adjust the height
        M5.Display.println(ssidList[i]);
      }
      M5.Display.display();
      needsDisplayUpdate = false;  // Reset the flag after updating the display
    }

    M5.update();
    M5Cardputer.update();
    handleDnsRequestSerial();
    delay(10); // Small delay to reduce the processor load

    // Check the keyboard inputs here
    if ((M5Cardputer.Keyboard.isKeyPressed(';') && !keyHandled) || (M5Cardputer.Keyboard.isKeyPressed('.') && !keyHandled)) {
      if (M5Cardputer.Keyboard.isKeyPressed(';')) {
        currentListIndex--;
        if (currentListIndex < 0) currentListIndex = numSsid - 1;
      } else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
        currentListIndex++;
        if (currentListIndex >= numSsid) currentListIndex = 0;
      }
      needsDisplayUpdate = true;  // Mark for display update
      keyHandled = true;
    } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) && !keyHandled) {
      inMenu = true;
      Serial.println("-------------------");
      Serial.println("SSID " + ssidList[currentListIndex] + " selected");
      Serial.println("-------------------");
      waitAndReturnToMenu(ssidList[currentListIndex] + " selected");
      needsDisplayUpdate = true;  // Flag for display update.
      keyHandled = true;
    } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
      inMenu = true;
      drawMenu();
      break;
    } else if (!M5Cardputer.Keyboard.isKeyPressed(';') &&
                !M5Cardputer.Keyboard.isKeyPressed('.') &&
               !M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
      keyHandled = false;
    }
  }
}


void showWifiDetails(int networkIndex) {
  inMenu = false;
  bool keyHandled = false;  // to handle the key press response once

  auto updateDisplay = [&]() {
    if (networkIndex >= 0 && networkIndex < numSsid) {
      M5.Display.clear();
      M5.Display.setTextSize(1.5);
      int y = 2;
      int x = 0;

      // SSID
      M5.Display.setCursor(x, y);
      M5.Display.println("SSID:" + (ssidList[networkIndex].length() > 0 ? ssidList[networkIndex] : "N/A"));
      y += 20;

      // Channel
      int channel = WiFi.channel(networkIndex);
      M5.Display.setCursor(x, y);
      M5.Display.println("Channel:" + (channel > 0 ? String(channel) : "N/A"));
      y += 16;

      // Security
      String security = getWifiSecurity(networkIndex);
      M5.Display.setCursor(x, y);
      M5.Display.println("Security:" + (security.length() > 0 ? security : "N/A"));
      y += 16;

      // Signal Strength
      int32_t rssi = WiFi.RSSI(networkIndex);
      M5.Display.setCursor(x, y);
      M5.Display.println("Signal:" + (rssi != 0 ? String(rssi) + " dBm" : "N/A"));
      y += 16;

      // MAC Address
      uint8_t* bssid = WiFi.BSSID(networkIndex);
      String macAddress = bssidToString(bssid);
      M5.Display.setCursor(x, y);
      M5.Display.println("MAC:" + (macAddress.length() > 0 ? macAddress : "N/A"));
      y += 16;

      M5.Display.setCursor(80, 110);
      M5.Display.println("ENTER:Clone");
      M5.Display.setCursor(20, 110);
      M5.Display.println("<");
      M5.Display.setCursor(M5.Display.width() - 20 , 110);
      M5.Display.println(">");

      M5.Display.display();
    }
  };

  updateDisplay();

  enterDebounce();
  unsigned long lastKeyPressTime = 0;
  const unsigned long debounceDelay = 200; // Time in milliseconds to ignore additional presses

  while (!inMenu) {
    M5.update();
    M5Cardputer.update();
    handleDnsRequestSerial();

    if (millis() - lastKeyPressTime > debounceDelay) {
      if (M5Cardputer.Keyboard.isKeyPressed('/') && !keyHandled) {
        networkIndex = (networkIndex + 1) % numSsid;
        updateDisplay();
        lastKeyPressTime = millis();
      } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) && !keyHandled) {
        inMenu = true;
        drawMenu();
        break;
      } else if (M5Cardputer.Keyboard.isKeyPressed(',') && !keyHandled) {
        networkIndex = (networkIndex - 1 + numSsid) % numSsid;
        updateDisplay();
        lastKeyPressTime = millis();
      }

      if (!M5Cardputer.Keyboard.isKeyPressed('/') &&
          !M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) &&
          !M5Cardputer.Keyboard.isKeyPressed(',')) {
        keyHandled = false;
      }
    }
  }
}

String getWifiSecurity(int networkIndex) {
  switch (WiFi.encryptionType(networkIndex)) {
    case WIFI_AUTH_OPEN:
      return "Open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2_ENTERPRISE";
    default:
      return "Unknown";
  }
}

String bssidToString(uint8_t* bssid) {
  char mac[18];
  snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
  return String(mac);
}

void messenger(){
}

void notes(){
}

void toggleLED() {
    inMenu = false;
    ledOn = !ledOn;  // Toggle the LED state

   // Save the new state in the configuration file
   // saveConfigParameter("ledOn", ledOn); this came in original source code, the saveVonfigParameter led0n, ledOn thing beow 
    M5.Display.fillScreen(TFT_BLACK); // TFT_BLACK
    M5.Display.setCursor(M5.Display.width() / 2 - 72, M5.Display.height() / 2);
    M5.Display.print("LED " + String(ledOn ? "On" : "Off"));
    delay(1000);
}

void waitAndReturnToMenu(String message) {
  M5.Display.clear();
  M5.Display.setTextSize(1.5);

  int messageWidth = message.length() * 9;  // Each character is 6 pixels wide
  int startX = (M5.Display.width() - messageWidth) / 2;  // Calculate starting X position

  // Set the cursor to the calculated position
  M5.Display.setCursor(startX, M5.Display.height() / 2);
  M5.Display.println(message);
  M5.Display.fillRect(0, M5.Display.height() - 30, M5.Display.width(), 30, TFT_BLACK);

  M5.Display.display();
  delay(1500);
  inMenu = true;
  drawMenu();
}

void checkSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "scan_wifi") {
      isOperationInProgress = true;
      inMenu = false;
      scanWifiNetworks();
      //sendBLE("-------------------");
      //sendBLE("Near Wifi Network : ");
      for (int i = 0; i < numSsid; i++) {
        ssidList[i] = WiFi.SSID(i);
        //sendBLE(String(i) + ": " + ssidList[i]);
    }} else if (command.startsWith("select_network")) {
      int ssidIndex = command.substring(String("select_network ").length()).toInt();
      selectNetwork(ssidIndex);
      //sendBLE("SSID select: " + currentlySelectedSSID);
    } else if (command.startsWith("detail_ssid")) {
      int ssidIndex = command.substring(String("detail_ssid ").length()).toInt();
      String security = getWifiSecurity(ssidIndex);
      int32_t rssi = WiFi.RSSI(ssidIndex);
      uint8_t* bssid = WiFi.BSSID(ssidIndex);
      String macAddress = bssidToString(bssid);
      M5.Display.display();
      Serial.println("------Wifi-Info----");
      //sendBLE("------Wifi-Info----");
      Serial.println("SSID: " + (ssidList[ssidIndex].length() > 0 ? ssidList[ssidIndex] : "N/A"));
      //sendBLE("SSID: " + (ssidList[ssidIndex].length() > 0 ? ssidList[ssidIndex] : "N/A"));
      Serial.println("Channel: " + String(WiFi.channel(ssidIndex)));
      //sendBLE("Channel: " + String(WiFi.channel(ssidIndex)));
      Serial.println("Security: " + security);
      //sendBLE("Security: " + security);
      Serial.println("Signal: " + String(rssi) + " dBm");
      //sendBLE("Signal: " + String(rssi) + " dBm");
      Serial.println("MAC: " + macAddress);
      //sendBLE("MAC: " + macAddress);
      Serial.println("-------------------");
      //sendBLE("-------------------");
    } else if (command == "help") {
      Serial.println("-------------------");
      //sendBLE("-------------------");
      Serial.println("Available Commands:");
      //sendBLE("Available Commands:");
      Serial.println("scan_wifi - Scan WiFi Networks");
      //sendBLE("scan_wifi - Scan WiFi Networks");
      Serial.println("select_network <index> - Select WiFi <index>");
      //sendBLE("select_network <index> - Select WiFi <index>"); 
      Serial.println("detail_ssid <index> - Details of WiFi <index>");
      //sendBLE("detail_ssid <index> - Details of WiFi <index>");
    }
  }
}
