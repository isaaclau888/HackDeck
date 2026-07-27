#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <TAMC_GT911.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- Wi-Fi Configuration (FILL THESE IN!) ---
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* SLACK_WEBHOOK_URL = "https://hooks.slack.com/services/YOUR/SLACK/WEBHOOK_URL";

// --- Hardware Pins ---
#define I2C_SDA 19
#define I2C_SCL 20
#define TOUCH_INT -1
#define TOUCH_RST -1
#define TOUCH_WIDTH  800
#define TOUCH_HEIGHT 480

// --- Global Data Variables ---
String i2cResults[16]; 
int i2cDeviceCount = -1;

String githubUser = "isaaclau888"; 
int publicRepos = 0;
int followersCount = 0;
String lastCommitMsg = "Fetching...";
unsigned long lastDataFetch = 0;

bool wifiConnected = false;
unsigned long lastWifiCheck = 0;

// --- Display Sleep & Backlight Management ---
unsigned long lastActivityTime = 0;
const unsigned long DIM_TIMEOUT   = 30000;  // 30 sec
const unsigned long SLEEP_TIMEOUT = 120000; // 2 min

enum ScreenPowerState { BRIGHT, DIMMED, OFF };
ScreenPowerState powerState = BRIGHT;

// --- Display Driver Setup ---
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_RGB _panel_instance;
  lgfx::Bus_RGB   _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX() {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      
      cfg.pin_d0  = 8;   cfg.pin_d1  = 3;   cfg.pin_d2  = 46;  cfg.pin_d3  = 9;
      cfg.pin_d4  = 1;   cfg.pin_d5  = 5;   cfg.pin_d6  = 6;   cfg.pin_d7  = 7;
      cfg.pin_d8  = 15;  cfg.pin_d9  = 16;  cfg.pin_d10 = 4;   cfg.pin_d11 = 45;
      cfg.pin_d12 = 48;  cfg.pin_d13 = 47;  cfg.pin_d14 = 21;  cfg.pin_d15 = 14;
      
      cfg.pin_henable = 40; 
      cfg.pin_vsync   = 41;
      cfg.pin_hsync   = 39;
      cfg.pin_pclk    = 42;
      cfg.freq_write  = 14000000;
      
      cfg.hsync_polarity    = 0; cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4; cfg.hsync_back_porch  = 16;
      cfg.vsync_polarity    = 0; cfg.vsync_front_porch = 4;
      cfg.vsync_pulse_width = 4; cfg.vsync_back_porch  = 4;
      cfg.pclk_idle_high    = 1;
      
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 800; cfg.memory_height = 480;
      cfg.panel_width   = 800; cfg.panel_height  = 480;
      cfg.offset_x      = 0;   cfg.offset_y      = 0;
      _panel_instance.config(cfg);
    }
    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1;
      _panel_instance.config_detail(cfg);
    }
    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = 2;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX lcd;
TAMC_GT911 ts = TAMC_GT911(I2C_SDA, I2C_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);

enum AppScreen { DASHBOARD, PINOUT, CONTROLLER };
AppScreen currentScreen = DASHBOARD;
unsigned long lastTouchTime = 0;

void setScreenPower(ScreenPowerState state) {
  if (powerState == state) return;
  powerState = state;
  switch (state) {
    case BRIGHT: lcd.setBrightness(180); break;
    case DIMMED: lcd.setBrightness(30);  break;
    case OFF:    lcd.setBrightness(0);   break;
  }
}

void drawNavBar() {
  lcd.fillRect(0, 0, 800, 50, lcd.color565(30, 30, 30));
  
  // Dashboard
  lcd.fillRect(10, 5, 210, 40, currentScreen == DASHBOARD ? lcd.color565(0, 150, 255) : lcd.color565(60, 60, 60));
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("DASHBOARD", 50, 15);

  // Pinout
  lcd.fillRect(225, 5, 210, 40, currentScreen == PINOUT ? lcd.color565(0, 200, 100) : lcd.color565(60, 60, 60));
  lcd.drawString("PINOUT", 290, 15);

  // Controller
  lcd.fillRect(445, 5, 210, 40, currentScreen == CONTROLLER ? lcd.color565(255, 120, 0) : lcd.color565(60, 60, 60));
  lcd.drawString("CONTROLLER", 490, 15);

  // Wi-Fi Status Box
  if (WiFi.status() == WL_CONNECTED) {
    lcd.fillRoundRect(665, 8, 125, 34, 6, lcd.color565(0, 180, 80));
    lcd.setTextSize(1);
    lcd.drawString("Wi-Fi OK", 700, 20);
  } else {
    lcd.fillRoundRect(665, 8, 125, 34, 6, lcd.color565(200, 50, 50));
    lcd.setTextSize(1);
    lcd.drawString("Connecting...", 680, 20);
  }
}

void fetchGitHubData() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.github.com/users/" + githubUser;
  
  http.begin(url);
  http.setUserAgent("ESP32-HackDeck");
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      publicRepos = doc["public_repos"] | 0;
      followersCount = doc["followers"] | 0;
      lastCommitMsg = "Data Sync OK!";
    }
  } else {
    lastCommitMsg = "HTTP Error: " + String(httpCode);
  }
  http.end();
}

void sendWebhook(String actionName) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send webhook: Wi-Fi disconnected");
    return;
  }

  HTTPClient http;
  http.begin(SLACK_WEBHOOK_URL);
  http.addHeader("Content-Type", "application/json");
  
  String payload = "{\"text\":\"*HackDeck Alert:* Triggered `" + actionName + "` from workbench!\"}";
  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    Serial.println("Slack notification sent successfully!");
  } else {
    Serial.printf("Slack HTTP Error: %d\n", httpCode);
  }
  http.end();
}

void scanI2CBus() {
  i2cDeviceCount = 0;
  for (int i = 0; i < 16; i++) i2cResults[i] = "";

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      if (i2cDeviceCount < 16) {
        char hexBuffer[10];
        sprintf(hexBuffer, "0x%02X", address);
        i2cResults[i2cDeviceCount] = String(hexBuffer);
        i2cDeviceCount++;
      }
    }
  }
}

void drawScreenContent() {
  lcd.fillRect(0, 51, 800, 429, TFT_BLACK);

  if (currentScreen == DASHBOARD) {
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.drawString("HackDeck Live Feed", 30, 75);

    // Card 1: GitHub
    lcd.fillRoundRect(30, 110, 350, 180, 10, lcd.color565(20, 25, 35));
    lcd.drawRoundRect(30, 110, 350, 180, 10, lcd.color565(0, 150, 255));
    lcd.setTextColor(lcd.color565(0, 150, 255));
    lcd.drawString("GitHub: @" + githubUser, 50, 130);

    lcd.setTextColor(TFT_WHITE);
    lcd.drawString("Public Repos: " + String(publicRepos), 50, 170);
    lcd.drawString("Followers:    " + String(followersCount), 50, 205);
    
    lcd.setTextSize(1);
    lcd.setTextColor(lcd.color565(180, 180, 180));
    lcd.drawString("Status: " + lastCommitMsg, 50, 250);

    // Card 2: MakerTeam
    lcd.setTextSize(2);
    lcd.fillRoundRect(420, 110, 350, 180, 10, lcd.color565(20, 25, 35));
    lcd.drawRoundRect(420, 110, 350, 180, 10, lcd.color565(0, 200, 100));
    lcd.setTextColor(lcd.color565(0, 200, 100));
    lcd.drawString("MakerTeam Feed", 440, 130);

    lcd.setTextColor(TFT_WHITE);
    lcd.drawString("Status: Active", 440, 170);
    lcd.drawString("Mode: Connected", 440, 205);

    // Telemetry Footer
    lcd.fillRoundRect(30, 310, 740, 110, 10, lcd.color565(25, 25, 25));
    lcd.drawRoundRect(30, 310, 740, 110, 10, lcd.color565(100, 100, 100));
    lcd.drawString("System Telemetry", 50, 330);
    lcd.drawString("IP: " + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("Offline")), 50, 370);

  } else if (currentScreen == PINOUT) {
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.drawString("Workbench Hardware Reference", 30, 75);

    // Pinout Card
    lcd.fillRoundRect(30, 110, 350, 310, 10, lcd.color565(20, 25, 35));
    lcd.drawRoundRect(30, 110, 350, 310, 10, lcd.color565(0, 200, 100));
    lcd.setTextColor(lcd.color565(0, 200, 100));
    lcd.drawString("On-Board Pinout", 50, 130);

    lcd.setTextSize(1);
    lcd.setTextColor(TFT_WHITE);
    lcd.drawString("I2C SDA:     GPIO 19", 50, 170);
    lcd.drawString("I2C SCL:     GPIO 20", 50, 195);
    lcd.drawString("PWM Dimmer:  GPIO 2",  50, 220);
    lcd.drawString("RGB HENABLE: GPIO 40", 50, 245);
    lcd.drawString("RGB VSYNC:   GPIO 41", 50, 270);
    lcd.drawString("RGB HSYNC:   GPIO 39", 50, 295);
    lcd.drawString("RGB PCLK:    GPIO 42", 50, 320);

    // I2C Scanner Card
    lcd.setTextSize(2);
    lcd.fillRoundRect(420, 110, 350, 310, 10, lcd.color565(20, 25, 35));
    lcd.drawRoundRect(420, 110, 350, 310, 10, lcd.color565(255, 180, 0));
    lcd.setTextColor(lcd.color565(255, 180, 0));
    lcd.drawString("Live I2C Bus Scan", 440, 130);

    // SCAN Button
    lcd.fillRoundRect(440, 165, 140, 40, 8, lcd.color565(0, 150, 255));
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.drawString("SCAN", 480, 177);

    lcd.setTextSize(1);
    lcd.setTextColor(TFT_WHITE);
    if (i2cDeviceCount == -1) {
      lcd.drawString("Tap SCAN to check I2C bus...", 440, 225);
    } else if (i2cDeviceCount == 0) {
      lcd.setTextColor(lcd.color565(255, 100, 100));
      lcd.drawString("No external devices found!", 440, 225);
    } else {
      lcd.setTextColor(lcd.color565(0, 255, 150));
      lcd.drawString("Found " + String(i2cDeviceCount) + " device(s):", 440, 220);
      
      int startY = 250;
      for (int i = 0; i < i2cDeviceCount; i++) {
        int col = i % 3;
        int row = i / 3;
        lcd.drawString(i2cResults[i], 440 + (col * 90), startY + (row * 30));
      }
    }

    lcd.setTextColor(lcd.color565(180, 180, 180));
    lcd.drawString("---------------------------------", 440, 360);
    lcd.drawString("Logic Level: 3.3V ONLY | GPIO 19/20", 440, 385);

  } else if (currentScreen == CONTROLLER) {
    lcd.setTextColor(TFT_WHITE);
    lcd.setTextSize(2);
    lcd.drawString("Touch Macro Control Surface", 30, 80);

    lcd.fillRoundRect(50, 150, 200, 120, 12, lcd.color565(255, 120, 0));
    lcd.drawString("Action A", 90, 200);

    lcd.fillRoundRect(300, 150, 200, 120, 12, lcd.color565(255, 120, 0));
    lcd.drawString("Action B", 340, 200);

    lcd.fillRoundRect(550, 150, 200, 120, 12, lcd.color565(255, 120, 0));
    lcd.drawString("Action C", 590, 200);
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize display first
  lcd.init();
  setScreenPower(BRIGHT);
  lcd.fillScreen(TFT_BLACK);

  // Correct order for GT911 touch bus init
  Wire.begin(I2C_SDA, I2C_SCL);
  ts.begin();
  ts.setRotation(ROTATION_NORMAL);

  lastActivityTime = millis();

  // Start background Wi-Fi connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  drawNavBar();
  drawScreenContent();
}

void loop() {
  ts.read();
  
  if (ts.isTouched) {
    lastActivityTime = millis();
    
    if (powerState != BRIGHT) {
      setScreenPower(BRIGHT);
      delay(200);
      return;
    }

    if (millis() - lastTouchTime > 250) {
      int touchX = ts.points[0].x;
      int touchY = ts.points[0].y;
      lastTouchTime = millis();

      // Top Nav Bar
      if (touchY < 50) {
        if (touchX < 225) {
          currentScreen = DASHBOARD;
        } else if (touchX >= 225 && touchX < 445) {
          currentScreen = PINOUT;
        } else if (touchX >= 445 && touchX < 665) {
          currentScreen = CONTROLLER;
        }
        drawNavBar();
        drawScreenContent();
      }
      // Scan Button
      else if (currentScreen == PINOUT) {
        if (touchX >= 440 && touchX <= 580 && touchY >= 165 && touchY <= 205) {
          lcd.fillRoundRect(440, 165, 140, 40, 8, lcd.color565(0, 200, 100));
          lcd.setTextColor(TFT_WHITE);
          lcd.setTextSize(2);
          lcd.drawString("SCANNING", 455, 177);
          
          scanI2CBus();
          delay(150);
          drawScreenContent();
        }
      }
      // Controller Macros
      else if (currentScreen == CONTROLLER) {
        if (touchY >= 150 && touchY <= 270) {
          if (touchX >= 50 && touchX <= 250) {
            sendWebhook("Action A");
          } else if (touchX >= 300 && touchX <= 500) {
            sendWebhook("Action B");
          } else if (touchX >= 550 && touchX <= 750) {
            sendWebhook("Action C");
          }
        }
      }
    }
  }

  // Idle Screen Dimming & Sleep
  unsigned long idleTime = millis() - lastActivityTime;
  if (idleTime > SLEEP_TIMEOUT) {
    setScreenPower(OFF);
  } else if (idleTime > DIM_TIMEOUT) {
    setScreenPower(DIMMED);
  }

  // Auto-fetch GitHub telemetry every 30s
  if (powerState != OFF && currentScreen == DASHBOARD && (millis() - lastDataFetch > 30000 || lastDataFetch == 0)) {
    lastDataFetch = millis();
    fetchGitHubData();
    drawScreenContent();
  }

  // Non-blocking Wi-Fi reconnect monitor
  if (millis() - lastWifiCheck > 3000) {
    lastWifiCheck = millis();
    bool currentStatus = (WiFi.status() == WL_CONNECTED);
    if (currentStatus != wifiConnected) {
      wifiConnected = currentStatus;
      drawNavBar();
    }
  }

  delay(20);
}