/**
 *
 * TrashTrack Pro
 * ---------------
 * Ali, Joseph, Miai, Sam, Xinghua
 * Prof. Tommy Chiu
 * Fall 2023
 *
 * Credits
 * --------------
 * HX711 Library - Bogdan Necula
 * LiquidCrystal_I2C Library - Frank de Brabander
 * NewPing Library - Tim Eckel
 * ESP32Servo - Kevin Harrington, John K. Bennett
 * IRremoteESP8266.h - David Conran // Only for testing
 * 
**/

#include "HX711.h"
#include "LiquidCrystal_I2C.h"
#include "NewPing.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Time.h>
#include <ESP32Servo.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

const char* ssid = "ESPwifi";
const char* password = "jordfrfr";

String GOOGLE_SCRIPT_ID = "AKfycbzMJUKkstgkIkBryAPBc-NRV1dcfEGpT2cwtOhNfoEsEQEo2FtWgTyaY9IEy2Ly7nSCRg";
String urlFinal = "";
int httpResponseCode = 0;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;         // Set GMT offset
const int daylightOffset_sec = 3600;  // Set daylight savings time offset
// bool data_sent_today = false;
// int currentHour = 0;
// int currentMinute = 0;
// int scheduledHour = 3;   // 03:00 (3:00 AM)
// int scheduledMinute = 0; // Change to time that you want data to be uploaded

int TRIG_PIN = 15;      // Pin 15
int ECHO_PIN = 4;       // Pin 4
int MAX_DISTANCE = 10;  // Maximum distance we want to ping for (in centimeters)
int distance = 0;
int motion_sensor_activation_timer = 0;     // Init
int motion_sensor_time_for_activation = 1;  // in 1/10 seconds (or whatever the delay is)

// Servo Attributes
int servo_open_timer = 0;  // Init
bool clear_to_open = false;
// ----
int servo_max_open_time = 20;  // in 1/10 seconds (or whatever the delay is)
int servo_open_pos = 40;       // in degrees
int servo_close_pos = 110;     // in degrees
// ---
int SERVO_PIN = 5;  // Red is power, brown is ground, orange is pin
int servo_pos = 0;  // Default position, in degrees
bool lid_toggle = false;

// Load Cell
int dt_pin = 14;
int sck_pin = 12;
float weight = 0;

bool debug = true;
bool send_data = false;  // send data every second
bool join_wifi = true;

int randNumber = 0;

int receiver_pin = 2;
IRrecv irrecv(receiver_pin);
decode_results results;
String remote_val = "";
String remote_last_run = "";

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);  // NewPing setup of pins and maximum distance
LiquidCrystal_I2C lcd(0x27, 16, 2);               // Yellow to SDA, orange to SCL
HTTPClient http;
Servo servo;
HX711 scale;

void setup() {
  servo.attach(SERVO_PIN);
  servo.write(servo_close_pos);
  servo.write(servo_close_pos);
  randomSeed(analogRead(0));
  WiFi.begin(ssid, password);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  irrecv.enableIRIn();  // Start the receiver
  lcd.init();           // initialize the lcd
  lcd.backlight();      // turn on the backlight
  lcd.print("Initializing!");
  Serial.begin(115200);  // Start serial communication at 9600 baud rate
  Serial.println(" ");

  scale.begin(dt_pin, sck_pin);
  scale.set_scale(0.888);  // Calibration factor
  scale.tare(5); // Tare weight using x samples

  lcd.setCursor(0, 0);

  if (join_wifi == true) {
    lcd.print("Joining WiFi...");

    // Connect to WiFi
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
    }
    Serial.println("WiFi connected.");
    // Initialize NTP

    lcd.setCursor(0, 0);
    lcd.print("Syncing time...");

    // Wait for time to be fetched
    while (time(nullptr) < 86400) {
      Serial.print(".");
      delay(100);
    }
    Serial.println("Time synchronized");
    lcd.clear();
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  }

  lcd.clear();
  lcd.print("Hello! \(^ u ^)/");
  lcd.setCursor(0, 1);
  lcd.print("Thx 4 using me!!");
  delay(100);
}

void loop() {
  time_t now;
  time(&now);
  randNumber = random(1, 6);
  lcd.clear();
  lcd.setCursor(0, 0);

  if (scale.is_ready()) {
    weight = (scale.get_units(3) * 0.00195);  // Average of 3 readings for stability
    if (weight < 5){
      weight = 0;
    } else {
      double rounded_number = round(weight / 5.0) * 5; // Round weight
      weight = rounded_number;
    }
    int weight = abs(weight)
    lcd.print("Weight: ");
    lcd.print(weight);
    lcd.print(" g ");
  }

  // Get IR receiver
  if (irrecv.decode(&results)) {
    remote_val = getButtonName(results.value);
    irrecv.resume();
  } 

  // Remote button 1 to open lid
  if (remote_val == "1") {
    motion_sensor_activation_timer = 0;
    servo_open_timer = 0;
    clear_to_open = true;
    remote_val = "X";
    remote_last_run = "1, OPEN";
  }

  if (remote_val == "3") {
    lcd.setCursor(0, 1);
    lcd.print("Taring...");
    Serial.println("Taring...");
    scale.tare(3);
    remote_last_run = "3, TARE";
  }

  distance = sonar.ping_cm();  // Send ping, get distance in cm
  lcd.setCursor(0, 1);

  // Faces
  if (distance == 0 && clear_to_open == false) {  // Just chilling
    lcd.print("   . (^ u ^).   ");
  } else if (clear_to_open == true) {
    lcd.print("   . (> u <).   ");                      // Open
  } else if (distance > 0 && clear_to_open == false) {  // When it detects you
    lcd.print("   . (O u O)/   ");
  } else if (servo_open_timer >= servo_max_open_time) {
    lcd.print("   . (- u -)/   ");
  } else if (weight > 2500) {  // Change to actual weight
    lcd.print("   . (; - ;).   ");
  }

  // Lid opening logic
  if (distance > 0) {
    if (motion_sensor_activation_timer >= motion_sensor_time_for_activation) {
      clear_to_open = true;
      servo_open_timer = 0;
    } else {
      motion_sensor_activation_timer += 1;
    }
  } else {
    motion_sensor_activation_timer = 0;
  }

  if (clear_to_open == true) {
    if (servo_open_timer >= servo_max_open_time && distance == 0) {
      clear_to_open = false;
      motion_sensor_activation_timer = 0;
      servo_open_timer = 0;
      servo_pos = servo_close_pos;
      servo.write(servo_pos);
    } else {
      if (distance == 0) {
        servo_open_timer += 1;
      }
      servo_pos = servo_open_pos;
      servo.write(servo_pos);
    }
  }

  // Send data to Google Sheets
  if (send_data == true || remote_val == "2") {
    lcd.setCursor(0, 1);
    lcd.print("Sending data...");
    urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + "date=" + String(now) + "&weight=" + String(weight);
    http.begin(urlFinal.c_str());
    httpResponseCode = http.GET();
    http.end();
    lcd.setCursor(0, 1);
    if (httpResponseCode >= 200) {
      lcd.print("Data sent!");
    }
    remote_val = "x";
  } else if (remote_val == "2" && join_wifi == false) {
    lcd.setCursor(0, 1);
    lcd.print("No wifi connected!");
    delay(2000);
  }

  // Just debug stuff
  if (debug == true) {
    Serial.print("Grams: ");
    Serial.print(weight);
    Serial.print(" ");
    Serial.print("Distance: ");
    Serial.print(distance);

    Serial.print(" | MS activ. timer: ");
    Serial.print(motion_sensor_activation_timer);

    Serial.print(" | Servo open timer: ");
    if (clear_to_open == false) {
      Serial.print(" false");
    } else {
      Serial.print(" true");
    }
    Serial.print(" ");
    Serial.print(servo_open_timer);

    Serial.print(" | Servo pos: ");
    Serial.print(servo_pos);
    Serial.print(" HTTP Response: ");
    Serial.print(httpResponseCode);
    Serial.print(" | Remote code: ");
    Serial.println(remote_last_run);
  }

  delay(100);
}

// IR logic
String getButtonName(unsigned long code) {
  switch (code) {
    case 0xFFA25D: return "POWER";
    case 0xFFE21D: return "FUNC/STOP";
    case 0xFF629D: return "VOL+";
    case 0xFF22DD: return "FAST BACK";
    case 0xFF02FD: return "PAUSE";
    case 0xFFC23D: return "FAST FORWARD";
    case 0xFFE01F: return "DOWN";
    case 0xFFA857: return "VOL-";
    case 0xFF906F: return "UP";
    case 0xFF9867: return "EQ";
    case 0xFFB04F: return "ST/REPT";
    case 0xFF6897: return "0";
    case 0xFF30CF: return "1";
    case 0xFF18E7: return "2";
    case 0xFF7A85: return "3";
    case 0xFF10EF: return "4";
    case 0xFF38C7: return "5";
    case 0xFF5AA5: return "6";
    case 0xFF42BD: return "7";
    case 0xFF4AB5: return "8";
    case 0xFF52AD: return "9";
    case 0xFFFFFFFF: return "REPEAT";
    default: return "UNKNOWN";
  }
}
