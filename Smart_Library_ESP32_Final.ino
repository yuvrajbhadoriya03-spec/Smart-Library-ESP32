/************************************************************
              SMART LIBRARY - ESP32
              GitHub Safe Final Version

  Features:
  - Student RFID attendance / entry / exit
  - Book issue / return using RFID
  - Optional book issue
  - IR based entry gate
  - RFID based exit gate
  - Servo gate
  - OLED live clock
  - Blynk IoT
  - Security alert for unknown RFID

  IMPORTANT:
  Replace WiFi and Blynk placeholders locally.
  DO NOT upload real credentials to GitHub.
************************************************************/

#define BLYNK_TEMPLATE_ID "TMPL3TraVkq7D"
#define BLYNK_TEMPLATE_NAME "Smart Library"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

// ========================================================
// WIFI
// ========================================================

char ssid[] = "YOUR_WIFI_NAME";
char pass[] = "YOUR_WIFI_PASSWORD";

// ========================================================
// LIBRARIES
// ========================================================

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <time.h>

// ========================================================
// RFID
// ========================================================

#define RFID_SS_PIN 5
#define RFID_RST_PIN 27

MFRC522 rfid(
  RFID_SS_PIN,
  RFID_RST_PIN
);

// Student RFID UID
// Replace with your actual student UID if different.
byte STUDENT_UID[4] = {
  0x17,
  0xF1,
  0x74,
  0x06
};

// Book RFID UID
// Replace with your actual book UID if different.
byte BOOK_UID[4] = {
  0xE1,
  0x41,
  0xF1,
  0x05
};

// ========================================================
// OLED
// ========================================================

#define OLED_SDA 21
#define OLED_SCL 22

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// ========================================================
// GATE
// ========================================================

#define IR_PIN 34
#define SERVO_PIN 25

Servo gateServo;

const int GATE_CLOSED_ANGLE = 0;
const int GATE_OPEN_ANGLE = 90;

// ========================================================
// LED + BUZZER
// ========================================================

#define BUZZER_PIN 26
#define RED_LED_PIN 32
#define GREEN_LED_PIN 33

// ========================================================
// BUTTONS
// ========================================================

#define ISSUE_BUTTON_PIN 14
#define RETURN_BUTTON_PIN 12

// ========================================================
// LIBRARY STATE
// ========================================================

String studentName = "Yuvraj";
String bookName = "Library Book";

bool insideLibrary = false;
bool bookIssued = false;

// ========================================================
// GATE STATE
// ========================================================

bool gateOpen = false;

// Entry sequence
bool entrySequence = false;
unsigned long irClearTime = 0;

// Exit sequence
bool exitSequence = false;
unsigned long exitOpenTime = 0;

// ========================================================
// RFID / BUTTON TIMERS
// ========================================================

unsigned long lastRFIDTime = 0;
unsigned long lastButtonTime = 0;

// ========================================================
// OLED MESSAGE TIMER
// ========================================================

unsigned long oledMessageUntil = 0;

// ========================================================
// MODES
// ========================================================

enum SystemMode {
  NORMAL_MODE,
  ISSUE_MODE,
  RETURN_MODE
};

SystemMode mode = NORMAL_MODE;

// ========================================================
// OLED MESSAGE
// ========================================================

void showOLED(
  String line1,
  String line2 = "",
  String line3 = ""
) {

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(0, 5);
  display.println(line1);

  display.setCursor(0, 27);
  display.println(line2);

  display.setCursor(0, 49);
  display.println(line3);

  display.display();

  oledMessageUntil =
    millis() + 2500;
}

// ========================================================
// NORMAL OLED CLOCK
// ========================================================

void showNormalScreen() {

  if (
    millis() < oledMessageUntil
  ) {
    return;
  }

  struct tm timeinfo;

  if (
    !getLocalTime(&timeinfo)
  ) {

    display.clearDisplay();

    display.setTextColor(
      SSD1306_WHITE
    );

    display.setTextSize(1);

    display.setCursor(0, 5);
    display.println("SMART LIBRARY");

    display.setCursor(0, 30);
    display.println("TIME ERROR");

    display.display();

    return;
  }

  char timeString[12];
  char dateString[15];

  strftime(
    timeString,
    sizeof(timeString),
    "%H:%M:%S",
    &timeinfo
  );

  strftime(
    dateString,
    sizeof(dateString),
    "%d-%m-%Y",
    &timeinfo
  );

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("SMART LIBRARY");

  display.setTextSize(2);

  display.setCursor(10, 20);
  display.println(timeString);

  display.setTextSize(1);

  display.setCursor(28, 48);
  display.println(dateString);

  display.display();
}

// ========================================================
// UID COMPARISON
// ========================================================

bool checkUID(
  byte *readUID,
  byte *knownUID
) {

  for (
    byte i = 0;
    i < 4;
    i++
  ) {

    if (
      readUID[i] != knownUID[i]
    ) {
      return false;
    }
  }

  return true;
}

// ========================================================
// GET CURRENT TIME
// ========================================================

String getTimeNow() {

  struct tm timeinfo;

  if (
    !getLocalTime(&timeinfo)
  ) {
    return "Time N/A";
  }

  char buffer[25];

  strftime(
    buffer,
    sizeof(buffer),
    "%d-%m-%Y %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}

// ========================================================
// BLYNK UPDATE
// ========================================================

void updateBlynk() {

  Blynk.virtualWrite(
    V0,
    insideLibrary ? "Present" : "Absent"
  );

  Blynk.virtualWrite(
    V1,
    bookIssued ? "Issued" : "Available"
  );

  Blynk.virtualWrite(
    V2,
    gateOpen ? "OPEN" : "LOCKED"
  );

  Blynk.virtualWrite(
    V3,
    "SAFE"
  );

  Blynk.virtualWrite(
    V4,
    insideLibrary ? studentName : "No User"
  );
}

// ========================================================
// SECURITY ALERT
// ========================================================

void securityAlert(
  String message
) {

  digitalWrite(
    RED_LED_PIN,
    HIGH
  );

  digitalWrite(
    GREEN_LED_PIN,
    LOW
  );

  digitalWrite(
    BUZZER_PIN,
    HIGH
  );

  Blynk.virtualWrite(
    V3,
    "SECURITY ALERT"
  );

  Blynk.logEvent(
    "security_alert",
    message
  );

  showOLED(
    "SECURITY ALERT",
    message,
    "UNAUTHORIZED"
  );

  Serial.println(
    "SECURITY ALERT: " + message
  );

  delay(1200);

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  digitalWrite(
    RED_LED_PIN,
    LOW
  );

  Blynk.virtualWrite(
    V3,
    "SAFE"
  );
}

// ========================================================
// OPEN GATE
// ========================================================

void openGate() {

  if (gateOpen) {
    return;
  }

  gateServo.write(
    GATE_OPEN_ANGLE
  );

  gateOpen = true;

  digitalWrite(
    GREEN_LED_PIN,
    HIGH
  );

  Blynk.virtualWrite(
    V2,
    "OPEN"
  );

  Serial.println(
    "GATE OPEN"
  );
}

// ========================================================
// CLOSE GATE
// ========================================================

void closeGate() {

  if (!gateOpen) {
    return;
  }

  gateServo.write(
    GATE_CLOSED_ANGLE
  );

  gateOpen = false;

  digitalWrite(
    GREEN_LED_PIN,
    LOW
  );

  Blynk.virtualWrite(
    V2,
    "LOCKED"
  );

  Serial.println(
    "GATE CLOSED"
  );
}

// ========================================================
// ENTRY GATE USING IR
// ========================================================

void entryGateControl() {

  // Exit sequence has priority
  if (exitSequence) {
    return;
  }

  bool detected =
    digitalRead(IR_PIN) == LOW;

  // ------------------------------------------------------
  // Person approaches from outside
  // ------------------------------------------------------

  if (
    !gateOpen &&
    !detected == false
  ) {

    entrySequence = true;

    openGate();

    showOLED(
      "GATE OPEN",
      "ENTRY DETECTED",
      "SCAN RFID INSIDE"
    );

    Serial.println(
      "IR: ENTRY DETECTED"
    );
  }

  // ------------------------------------------------------
  // Person has passed IR
  // ------------------------------------------------------

  if (
    gateOpen &&
    entrySequence &&
    !detected
  ) {

    if (
      irClearTime == 0
    ) {

      irClearTime =
        millis();
    }

    // Wait 1.5 seconds after IR becomes clear
    if (
      millis() - irClearTime >= 1500
    ) {

      closeGate();

      entrySequence = false;

      irClearTime = 0;
    }
  }

  // Person still in front of sensor
  if (detected) {

    irClearTime = 0;
  }
}

// ========================================================
// EXIT GATE
// ========================================================

void exitGateControl() {

  if (!exitSequence) {
    return;
  }

  // Wait 2 seconds after exit RFID
  if (
    millis() - exitOpenTime >= 2000
  ) {

    openGate();

    exitSequence = false;

    entrySequence = true;

    irClearTime = 0;

    showOLED(
      "EXIT GATE OPEN",
      "PLEASE PASS",
      "IR MONITORING"
    );

    Serial.println(
      "EXIT: GATE OPEN"
    );
  }
}

// ========================================================
// NORMAL STUDENT RFID
// ========================================================

void processStudentNormal() {

  String currentTime =
    getTimeNow();

  // ------------------------------------------------------
  // ENTRY
  // ------------------------------------------------------

  if (!insideLibrary) {

    insideLibrary = true;

    Blynk.virtualWrite(
      V0,
      "Present | IN: " + currentTime
    );

    Blynk.virtualWrite(
      V4,
      studentName
    );

    showOLED(
      "WELCOME",
      studentName,
      "ATTENDANCE MARKED"
    );

    Serial.println(
      "ENTRY TIME: " + currentTime
    );

    return;
  }

  // ------------------------------------------------------
  // EXIT
  // ------------------------------------------------------

  String exitTime =
    getTimeNow();

  insideLibrary = false;

  Blynk.virtualWrite(
    V0,
    "Absent | OUT: " + exitTime
  );

  Blynk.virtualWrite(
    V4,
    "No User"
  );

  showOLED(
    "GOODBYE",
    studentName,
    "EXIT RECORDED"
  );

  Serial.println(
    "EXIT TIME: " + exitTime
  );

  // Start exit gate sequence
  exitSequence = true;

  exitOpenTime =
    millis();
}

// ========================================================
// ISSUE BOOK MODE
// ========================================================

void startIssueMode() {

  if (!insideLibrary) {

    showOLED(
      "ACCESS DENIED",
      "STUDENT NOT INSIDE"
    );

    return;
  }

  if (bookIssued) {

    showOLED(
      "BOOK ALREADY",
      "ISSUED"
    );

    return;
  }

  mode =
    ISSUE_MODE;

  showOLED(
    "ISSUE BOOK",
    "SCAN STUDENT ID",
    "THEN BOOK"
  );

  Serial.println(
    "ISSUE MODE"
  );
}

// ========================================================
// RETURN BOOK MODE
// ========================================================

void startReturnMode() {

  if (!insideLibrary) {

    showOLED(
      "ACCESS DENIED",
      "STUDENT NOT INSIDE"
    );

    return;
  }

  if (!bookIssued) {

    showOLED(
      "NO BOOK",
      "TO RETURN"
    );

    return;
  }

  mode =
    RETURN_MODE;

  showOLED(
    "RETURN BOOK",
    "SCAN STUDENT ID",
    "THEN BOOK"
  );

  Serial.println(
    "RETURN MODE"
  );
}

// ========================================================
// PROCESS BOOK
// ========================================================

void processBook(
  bool issue
) {

  if (issue) {

    bookIssued = true;

    Blynk.virtualWrite(
      V1,
      "Issued"
    );

    Blynk.logEvent(
      "book_issued",
      "Book issued to " +
      studentName
    );

    showOLED(
      "BOOK ISSUED",
      bookName,
      "EXIT IN 2 SEC"
    );

    // Start automatic exit gate sequence after successful issue
    exitSequence = true;
    exitOpenTime = millis();

    Serial.println(
      "BOOK ISSUED - EXIT GATE STARTED"
    );
  }

  else {

    bookIssued = false;

    Blynk.virtualWrite(
      V1,
      "Available"
    );

    Blynk.logEvent(
      "book_returned",
      "Book returned by " +
      studentName
    );

    showOLED(
      "BOOK RETURNED",
      bookName,
      "SUCCESS"
    );

    Serial.println(
      "BOOK RETURNED"
    );
  }

  mode =
    NORMAL_MODE;
}

// ========================================================
// RFID READER
// ========================================================

void readRFID() {

  if (
    millis() - lastRFIDTime < 1000
  ) {
    return;
  }

  if (
    !rfid.PICC_IsNewCardPresent()
  ) {
    return;
  }

  if (
    !rfid.PICC_ReadCardSerial()
  ) {
    return;
  }

  lastRFIDTime =
    millis();

  Serial.print(
    "UID: "
  );

  for (
    byte i = 0;
    i < rfid.uid.size;
    i++
  ) {

    if (
      rfid.uid.uidByte[i] < 0x10
    ) {
      Serial.print("0");
    }

    Serial.print(
      rfid.uid.uidByte[i],
      HEX
    );

    Serial.print(" ");
  }

  Serial.println();

  // ======================================================
  // STUDENT CARD
  // ======================================================

  if (
    checkUID(
      rfid.uid.uidByte,
      STUDENT_UID
    )
  ) {

    // Normal attendance / exit
    if (
      mode == NORMAL_MODE
    ) {

      processStudentNormal();
    }

    // Issue book
    else if (
      mode == ISSUE_MODE
    ) {

      showOLED(
        "STUDENT VERIFIED",
        studentName,
        "SCAN BOOK"
      );

      Serial.println(
        "STUDENT VERIFIED - ISSUE"
      );
    }

    // Return book
    else if (
      mode == RETURN_MODE
    ) {

      showOLED(
        "STUDENT VERIFIED",
        studentName,
        "SCAN BOOK"
      );

      Serial.println(
        "STUDENT VERIFIED - RETURN"
      );
    }
  }

  // ======================================================
  // BOOK CARD
  // ======================================================

  else if (
    checkUID(
      rfid.uid.uidByte,
      BOOK_UID
    )
  ) {

    if (
      mode == ISSUE_MODE
    ) {

      processBook(true);
    }

    else if (
      mode == RETURN_MODE
    ) {

      processBook(false);
    }

    else {

      showOLED(
        "BOOK DETECTED",
        "USE ISSUE/RETURN",
        "BUTTON"
      );
    }
  }

  // ======================================================
  // UNKNOWN CARD
  // ======================================================

  else {

    securityAlert(
      "Unknown RFID"
    );
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ========================================================
// BUTTONS
// ========================================================

void checkButtons() {

  if (
    millis() - lastButtonTime < 500
  ) {
    return;
  }

  // ------------------------------------------------------
  // ISSUE BUTTON
  // ------------------------------------------------------

  if (
    digitalRead(
      ISSUE_BUTTON_PIN
    ) == LOW
  ) {

    startIssueMode();

    lastButtonTime =
      millis();

    while (
      digitalRead(
        ISSUE_BUTTON_PIN
      ) == LOW
    ) {

      delay(10);
    }
  }

  // ------------------------------------------------------
  // RETURN BUTTON
  // ------------------------------------------------------

  if (
    digitalRead(
      RETURN_BUTTON_PIN
    ) == LOW
  ) {

    startReturnMode();

    lastButtonTime =
      millis();

    while (
      digitalRead(
        RETURN_BUTTON_PIN
      ) == LOW
    ) {

      delay(10);
    }
  }
}

// ========================================================
// SETUP
// ========================================================

void setup() {

  Serial.begin(
    115200
  );

  // ======================================================
  // GPIO
  // ======================================================

  pinMode(
    IR_PIN,
    INPUT
  );

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );

  pinMode(
    RED_LED_PIN,
    OUTPUT
  );

  pinMode(
    GREEN_LED_PIN,
    OUTPUT
  );

  pinMode(
    ISSUE_BUTTON_PIN,
    INPUT_PULLUP
  );

  pinMode(
    RETURN_BUTTON_PIN,
    INPUT_PULLUP
  );

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  digitalWrite(
    RED_LED_PIN,
    LOW
  );

  digitalWrite(
    GREEN_LED_PIN,
    LOW
  );

  // ======================================================
  // OLED
  // ======================================================

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      0x3C
    )
  ) {

    Serial.println(
      "OLED ERROR"
    );
  }

  // OLED brightness / contrast
  display.ssd1306_command(
    SSD1306_SETCONTRAST
  );

  display.ssd1306_command(
    50
  );

  // ======================================================
  // RFID
  // ======================================================

  SPI.begin(
    18,
    19,
    23,
    RFID_SS_PIN
  );

  rfid.PCD_Init();

  // ======================================================
  // SERVO
  // ======================================================

  gateServo.setPeriodHertz(
    50
  );

  gateServo.attach(
    SERVO_PIN,
    500,
    2400
  );

  gateServo.write(
    GATE_CLOSED_ANGLE
  );

  // ======================================================
  // BLYNK + WIFI
  // ======================================================

  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  // ======================================================
  // INDIA TIME - UTC +5:30
  // ======================================================

  configTime(
    19800,
    0,
    "pool.ntp.org",
    "time.nist.gov"
  );

  // ======================================================
  // INITIAL STATE
  // ======================================================

  updateBlynk();

  oledMessageUntil = 0;

  showNormalScreen();

  Serial.println();
  Serial.println(
    "================================"
  );
  Serial.println(
    "       SMART LIBRARY"
  );
  Serial.println(
    "       SYSTEM READY"
  );
  Serial.println(
    "================================"
  );
}

// ========================================================
// LOOP
// ========================================================

void loop() {

  Blynk.run();

  checkButtons();

  readRFID();

  entryGateControl();

  exitGateControl();

  // Normal OLED live clock
  if (
    mode == NORMAL_MODE &&
    !exitSequence &&
    millis() > oledMessageUntil
  ) {

    showNormalScreen();
  }

  delay(100);
}