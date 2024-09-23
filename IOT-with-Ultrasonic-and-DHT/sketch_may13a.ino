#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// OLED PIN
#define OLED_SDA D7
#define OLED_SCL D6
#define OLED_ADDR 0x3c
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// DHT PIN
#define DHTPIN D3
#define DHTTYPE DHT22

//ULTRASONIC PIN
#define trigPin D1
#define echoPin D2

//Variable declaration
unsigned long DHTdelay;
unsigned long OLEDdelay;
unsigned long ULTRAdelay;
float humidity;
float temperatureC;
long duration;
float distance;
bool isOled = true;
bool isDHT = true;
bool isSOUND = true;

void OledSetup (){
  Wire.begin(OLED_SDA, OLED_SCL);
  OLED.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  OLED.clearDisplay();
  OLED.setTextColor(WHITE);
  OLED.setTextSize(1);
}

void OledDisplay(String msg, int rowOffset=0, int colOffset=0){
  int offsetPadding = 12;
  for (int y = rowOffset*offsetPadding; y <= rowOffset*offsetPadding+7; y++){
    for(int x = 0; x<127; x++){
      OLED.drawPixel(x,y,BLACK);
    }
  }
  
  OLED.setTextColor(WHITE);
  OLED.setCursor(colOffset, rowOffset * offsetPadding);
  OLED.println(msg.substring(0,21));
  OLED.display();
}

void goUltraSound(){
  if(isSOUND && (millis() - ULTRAdelay>500)) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    distance = duration * 0.034 / 2;
    ULTRAdelay = millis();
  }
}

void goDHT (){
  if(isDHT && (millis() - DHTdelay>2000)){
    humidity = dht.readHumidity();
    temperatureC = dht.readTemperature();
    DHTdelay = millis();
  }
}

void goOLED(){
  if(isOled && (millis() - OLEDdelay>100)) {
    OledDisplay("Suhu & Kelembaban", 0);
    OledDisplay(" ",1);
    OledDisplay (String("Suhu : ") + String(temperatureC) + "'C", 2);
    OledDisplay(String("Kelembaban : ") + String(humidity) + "%", 3);
    OledDisplay(String("Jarak : ") + String(distance) + " cm", 4);
    OLEDdelay = millis();
  }
}
void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  OledSetup();

}

void loop() {
  goUltraSound();
  goDHT();
  goOLED();
}
