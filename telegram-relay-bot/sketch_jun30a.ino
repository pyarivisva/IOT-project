#include <Wire.h>
#include <RTClib.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AsyncTelegram2.h>
#include <time.h>
#include <BearSSLHelpers.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Setup bot Telegram
#define USE_CLIENTSSL true
BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);
AsyncTelegram2 myBot(client);

const char* ssid = "mau makan";  // SSID  
const char* passwd = "yourPassword"; // Password
const char* token = "yourTokenBot";  // Token bot

// Definisikan pin-pin yang terhubung
#define RELAY_PIN D3  // Relay terhubung ke pin D3
#define BUTTON_PIN D0 // Push button terhubung ke pin D0

// Variabel
int relayStatus = HIGH;
int buttonStatus = LOW;
int lastButtonStatus = LOW;
int read;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, 60000); // 8 * 3600 untuk GMT+8, 60000 untuk update setiap 60 detik 

// Waktu penjadwalan
int onHour = 18;    // Jam untuk menyalakan relay (24 jam format)
int onMinute = 0;   // Menit untuk menyalakan relay
int offHour = 6;    // Jam untuk mematikan relay (24 jam format)
int offMinute = 0;  // Menit untuk mematikan relay

// Inisialisasi RTC
RTC_DS3231 rtc;

// Web server pada port 80
ESP8266WebServer server(80);

// Nama dan password untuk access point
const char* ap_ssid = "ESP8266_AP";
const char* ap_password = "12345678";

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.println("Starting Telegram Bot...");

  // Mulai dalam mode Access Point
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("Access Point IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Sambungkan ke WiFi dalam mode Station
  WiFi.begin(ssid, passwd);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi Connected!");
  Serial.print("Local IP Address: ");
  Serial.println(WiFi.localIP());

  configTime(8 * 3600, 0, "time.google.com", "time.windows.com", "pool.ntp.org");
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
  myBot.setUpdateTime(2000);
  myBot.setTelegramToken(token);
  Serial.print("\nConnecting to Telegram server ... ");
  if (myBot.begin()) {
    Serial.println("Connected");
  } else {
    Serial.println("Not Connected");
  }

  // Inisialisasi pin relay sebagai output
  pinMode(RELAY_PIN, OUTPUT);
  // Inisialisasi pin tombol sebagai input dengan resistor pull-up internal
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Setup RTC
  if (!rtc.begin()) {
    Serial.println("RTC tidak ditemukan!");
    while (1);
  }

  // Sinkronisasi waktu dengan NTP dan atur RTC
  setRTCTimeFromNTP();

  // Setup web server
  server.on("/", handleRoot);
  server.on("/setwifi", handleSetWiFi);
  server.on("/time", handleTime);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  // Baca kondisi tombol dan kontrol relay
  goButton();

  // Proses pesan dari bot Telegram
  message();

  // Penjadwalan relay
  DateTime now = rtc.now();

  // Tampilkan waktu sekarang di serial monitor
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  // Penjadwalan ON relay
  if (now.hour() == onHour && now.minute() == onMinute && now.second() == 0) {
    digitalWrite(RELAY_PIN, LOW); // Nyalakan relay
    relayStatus = LOW;
    Serial.println("Relay ON (Scheduled)");
    myBot.sendMessage(TBMessage(), "Relay ON (Scheduled)");
  }

  // Penjadwalan OFF relay
  if (now.hour() == offHour && now.minute() == offMinute && now.second() == 0) {
    digitalWrite(RELAY_PIN, HIGH); // Matikan relay
    relayStatus = HIGH;
    Serial.println("Relay OFF (Scheduled)");
    myBot.sendMessage(TBMessage(), "Relay OFF (Scheduled)");
  }

  delay(1000);  // Delay 1 detik

  // Handle web server
  server.handleClient();
}

void message() {
  TBMessage msg;
  if (myBot.getNewMessage(msg)) {
    String msgText = msg.text;

    if (msgText.equalsIgnoreCase("/start")) {
      myBot.sendMessage(msg, "Hi " + String(msg.sender.id) + ", Selamat datang di Our IOT Bot! Ketik /on untuk menghidupkan relay, ketik /off untuk mematikan relay, ketik /seton HH:MM untuk mengatur waktu ON, dan ketik /setoff HH:MM untuk mengatur waktu OFF, serta /status untuk mengetahui status relay.");
    } else if (msgText.equalsIgnoreCase("/on")) {
      relayStatus = LOW;
      digitalWrite(RELAY_PIN, LOW);
      myBot.sendMessage(msg, "Relay ON");
    } else if (msgText.equalsIgnoreCase("/off")) {
      relayStatus = HIGH;
      digitalWrite(RELAY_PIN, HIGH);
      myBot.sendMessage(msg, "Relay OFF");
    } else if (msgText.startsWith("/seton ")) {
      String time = msgText.substring(7);
      int colonIndex = time.indexOf(':');
      if (colonIndex != -1) {
        onHour = time.substring(0, colonIndex).toInt();
        onMinute = time.substring(colonIndex + 1).toInt();
        myBot.sendMessage(msg, "Waktu ON relay diatur ke " + time);
      } else {
        myBot.sendMessage(msg, "Format waktu tidak valid. Gunakan HH:MM.");
      }
    } else if (msgText.startsWith("/setoff ")) {
      String time = msgText.substring(8);
      int colonIndex = time.indexOf(':');
      if (colonIndex != -1) {
        offHour = time.substring(0, colonIndex).toInt();
        offMinute = time.substring(colonIndex + 1).toInt();
        myBot.sendMessage(msg, "Waktu OFF relay diatur ke " + time);
      }else if (msgText.startsWith("/status")) {
      myBot.sendMessage(msg, getStatus());
      } else {
        myBot.sendMessage(msg, "Format waktu tidak valid. Gunakan HH:MM.");
      }
    }
  }
}

String getStatus() {
  String status = "Status Relay: ";
  status += (relayStatus == LOW) ? "ON" : "OFF";
  status += "\nWaktu ON: " + String(onHour) + ":" + String(onMinute);
  status += "\nWaktu OFF: " + String(offHour) + ":" + String(offMinute);
  return status;
}

void setRTCTimeFromNTP() {
  timeClient.begin();
  timeClient.update();
  time_t now = timeClient.getEpochTime();
  rtc.adjust(DateTime(now));
}

void goButton() {
  // Membaca input push button
  read = digitalRead(BUTTON_PIN);
  if (read == HIGH && relayStatus == LOW) {
    buttonStatus = 1 - buttonStatus;
    delay(500);
  }

  relayStatus = read;

  if (buttonStatus != lastButtonStatus) {
    if (buttonStatus == HIGH) {
      Serial.println("Relay ON");
      digitalWrite(RELAY_PIN, LOW);
    } else {
      Serial.println("Relay OFF");
      digitalWrite(RELAY_PIN, HIGH);
    }
    lastButtonStatus = buttonStatus;
  }
}

void handleRoot() {
  String html = "<html>\
    <body>\
      <h1>WiFi Configuration</h1>\
      <form action=\"/setwifi\" method=\"post\">\
        <label for=\"ssid\">SSID:</label>\
        <input type=\"text\" id=\"ssid\" name=\"ssid\"><br><br>\
        <label for=\"password\">Password:</label>\
        <input type=\"text\" id=\"password\" name=\"password\"><br><br>\
        <input type=\"submit\" value=\"Submit\">\
      </form>\
      <h2>Current Time</h2>\
      <div id=\"time\"></div>\
      <script>\
        function updateTime() {\
          var xhr = new XMLHttpRequest();\
          xhr.open('GET', '/time', true);\
          xhr.onload = function() {\
            if (xhr.status === 200) {\
              document.getElementById('time').innerText = xhr.responseText;\
            }\
          };\
          xhr.send();\
        }\
        setInterval(updateTime, 1000);\
      </script>\
    </body>\
  </html>";
  server.send(200, "text/html", html);
}

void handleSetWiFi() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");

  // Konfigurasi ulang WiFi
  WiFi.begin(ssid.c_str(), password.c_str());

  String response = "Trying to connect to " + ssid;
  server.send(200, "text/plain", response);

  delay(5000);

  if (WiFi.status() == WL_CONNECTED) {
    server.send(200, "text/plain", "Connected to " + ssid);
  } else {
    server.send(200, "text/plain", "Failed to connect to " + ssid);
  }
}

void handleTime() {
  DateTime now = rtc.now();
  String currentTime = String(now.year()) + "-" +
                       String(now.month()) + "-" +
                       String(now.day()) + " " +
                       String(now.hour()) + ":" +
                       String(now.minute()) + ":" +
                       String(now.second());
  server.send(200, "text/plain", currentTime);
}
