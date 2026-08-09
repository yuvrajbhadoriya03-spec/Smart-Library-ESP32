# 📚 Smart Library ESP32

An IoT-based Smart Library Management System built using ESP32, RFID, IR sensor, Servo Motor, OLED display and Blynk.

The project is designed to automate student attendance, library entry/exit, book issue/return and gate control.

## 🚀 Features

- 🎫 RFID-based student identification
- 🕐 Automatic attendance with entry and exit time
- 📖 Optional book issue and return system
- 🔖 RFID-based book identification
- 🚪 Automatic library gate using IR sensor and Servo Motor
- 🖥️ OLED display for time and system status
- 📱 Blynk IoT dashboard
- 🚨 Security alert for unauthorized RFID cards
- 💡 LED and buzzer status indication

## ⚙️ Working

### 1. Student Entry

1. Student approaches the library gate.
2. IR sensor detects the student.
3. Servo motor opens the gate.
4. Student scans the RFID card.
5. Attendance is recorded with entry time.
6. Student enters the library.

### 2. Book Issue

Book issue is optional.

1. Student presses the **Issue Book** button.
2. Student RFID card is scanned.
3. Book RFID card is scanned.
4. The book is marked as issued to that student.

### 3. Book Return

1. Student presses the **Return Book** button.
2. Student RFID card is scanned.
3. Book RFID card is scanned.