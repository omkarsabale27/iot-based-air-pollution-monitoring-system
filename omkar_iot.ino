// IoT Air Pollution Monitor (ESP8266 / NodeMCU + MQ135 + I2C LCD)
// Serves a web page you can access by IP address (printed to Serial).
// Libraries required: ESP8266WiFi, ESP8266WebServer, LiquidCrystal_I2C

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// -------- USER CONFIG --------
const char* ssid = "YOUR_WIFI_SSID";        // <<-- set your WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD"; // <<-- set your WiFi password

// Pins (adjust if your wiring differs)
const int mqPin = A0;      // MQ-135 analog out
const int ledPin = D5;     // LED indicator (with resistor)
const int buzzerPin = D6;  // Buzzer (active)

// LCD config (common address 0x27; change if your module uses 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Web server running on port 80
ESP8266WebServer server(80);

// Alert threshold (ADC) - tune based on calibration
const int ALERT_ADC_THRESHOLD = 300; // raw ADC threshold (0-1023). tune as needed
const int ALERT_DURATION_MS = 800;   // buzzer duration when alert

// Helper to convert ADC to an approximate "ppm" for demonstration only.
// MQ sensors require calibration. Replace with calibration equation for real results.
float adcToApproxPPM(int adc) {
  // Map 0-1023 to 0-500 ppm roughly; this is a demo mapping only.
  return (float)adc * (500.0f / 1023.0f);
}

String buildHTML() {
  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<title>Air Pollution Monitor</title>"
                "<style>body{font-family:Arial,Helvetica,sans-serif;text-align:center;padding:10px;} "
                ".card{display:inline-block;padding:12px;border-radius:8px;box-shadow:0 0 8px rgba(0,0,0,0.1);} "
                ".val{font-size:2.4em;margin:6px 0;} .stat{color:#666;margin-bottom:8px;} </style>"
                "</head><body>"
                "<h2>IoT Air Pollution Monitor</h2>"
                "<div class='card'>"
                "<div class='stat'>Raw ADC</div><div id='adc' class='val'>--</div>"
                "<div class='stat'>Approx. PPM</div><div id='ppm' class='val'>--</div>"
                "<div id='status' class='stat'>--</div>"
                "</div>"
                "<p>Auto-updates every 2s. IP: ";
  html += WiFi.localIP().toString();
  html += "</p>"
          "<script>"
          "function update(){fetch('/data').then(r=>r.json()).then(d=>{"
          "document.getElementById('adc').innerText=d.adc;"
          "document.getElementById('ppm').innerText=d.ppm.toFixed(1)+' ppm';"
          "document.getElementById('status').innerText=d.status;"
          "});}"
          "setInterval(update,2000); update();"
          "</script>"
          "</body></html>";
  return html;
}

// Serve main page
void handleRoot() {
  server.send(200, "text/html", buildHTML());
}

// Serve JSON data for AJAX
void handleData() {
  int adc = analogRead(mqPin);
  float ppm = adcToApproxPPM(adc);
  const char* status = (adc >= ALERT_ADC_THRESHOLD) ? "ALERT: High pollution" : "Normal";
  String payload = "{";
  payload += "\"adc\":" + String(adc) + ",";
  payload += "\"ppm\":" + String(ppm, 2) + ",";
  payload += "\"status\":\"" + String(status) + "\"";
  payload += "}";
  server.send(200, "application/json", payload);
}

void setup() {
  Serial.begin(115200);
  delay(10);

  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  // Start LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Air Monitor");
  lcd.setCursor(0,1);
  lcd.print("Starting...");
  delay(1000);

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
    lcd.setCursor(0,1);
    lcd.print("Try: ");
    lcd.print(attempts);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0,1);
    lcd.print(WiFi.localIP().toString());
  } else {
    Serial.println();
    Serial.println("WiFi FAILED");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("WiFi Failed");
    lcd.setCursor(0,1);
    lcd.print("Check creds");
    // Still continue as AP not implemented; update code if you want fallback
  }

  // Setup web server endpoints
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");

  delay(500);
}

unsigned long lastAlertTime = 0;

void loop() {
  server.handleClient();

  // Read sensor and update LCD
  int adc = analogRead(mqPin);
  float ppm = adcToApproxPPM(adc);

  // Update LCD every ~500ms
  static unsigned long lastLCD = 0;
  if (millis() - lastLCD > 500) {
    lastLCD = millis();
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ADC:");
    lcd.print(adc);
    lcd.setCursor(0,1);
    lcd.print("PPM:");
    lcd.print(ppm,1);
  }

  // Alert logic
  if (adc >= ALERT_ADC_THRESHOLD) {
    digitalWrite(ledPin, HIGH);
    // beep the buzzer briefly but not too often
    if (millis() - lastAlertTime > ALERT_DURATION_MS + 500) {
      digitalWrite(buzzerPin, HIGH);
      delay(ALERT_DURATION_MS);
      digitalWrite(buzzerPin, LOW);
      lastAlertTime = millis();
    }
  } else {
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
  }

  // tiny delay to free CPU
  delay(10);
}