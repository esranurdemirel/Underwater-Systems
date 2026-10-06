# Underwater Vehicle Control System

This project contains the control software for an underwater vehicle (ROV/AUV). The system is composed of an Arduino microcontroller handling low-level motor controls and safety features, and a Python script running on a higher-level processor (like a Jetson Nano or PC) for mission planning and execution.

## Components

### 1. Arduino Firmware (`video.ino`)
The Arduino code is responsible for:
- Controlling 6 thrusters/motors via PWM signals (using standard ESC protocols).
- Handling an Emergency Stop (E-Stop) using an MFRC522 NFC/RFID reader.
- Communicating with the main computer via Serial (115200 baud).
- Receiving and executing time-based movement commands (Forward, Turn, Circle, Stop).
- Implementing a safety timeout (stops motors if no heartbeat/command is received from the main computer for 2 seconds).

### 2. Python Mission Manager (`video.py`)
The Python script is responsible for:
- Sending parameters and sequential commands to the Arduino via Serial communication.
- Executing a predefined autonomous mission based on user-defined PWM values and durations.
- Handling asynchronous communication to track the completion of tasks and monitor E-Stop events triggered by the Arduino.
- Keeping the connection alive with a heartbeat (`PING`) mechanism.

### 3. ESC Test Interface (`esc_test.html`)
An HTML interface for testing ESC (Electronic Speed Controller) configurations and motor responses.

## Hardware Requirements
- Arduino (Mega/Uno)
- 6x Brushless Motors with ESCs
- MFRC522 RFID/NFC Module (for E-Stop)
- Jetson Nano / Raspberry Pi / PC (for running the Python script)

## Setup & Usage

### 1. Arduino Setup
1. Connect the ESC signal pins to Arduino pins 2, 3, 4, 5, 6, 7.
2. Connect the MFRC522 module (SDA to Pin 53, RST to Pin 46 on Arduino Mega).
3. Flash `video.ino` to the Arduino using the Arduino IDE.

### 2. Python Setup
Ensure you have the `pyserial` library installed:
```bash
pip install pyserial
```

### 3. Running the Mission
1. Connect the Arduino to your computer via USB (default port in code is `/dev/ttyACM0`).
2. Run the Python script:
```bash
python3 video.py
```
3. Enter the requested PWM values and durations, or press Enter to use the defaults.
4. Press ENTER when ready to start the 5-second countdown before the mission begins.

#### Default Mission Sequence (Görev Sırası):
When the script runs, the underwater vehicle performs the following predefined sequence:
1. **İleri Git** (Move Forward)
2. **Sağa 90 Derece Dön** (Turn 90° Right)
3. **İleri Git** (Move Forward)
4. **Kendi Etrafında 360 Derece Dön** (Circle 360°)
5. **Sağa 90 Derece Dön** (Turn 90° Right)
6. **İleri Git** (Move Forward)
7. **Sağa 90 Derece Dön** (Turn 90° Right)
8. **İleri Git** (Move Forward)
9. **Dur** (Stop) - Returns to the starting position.
