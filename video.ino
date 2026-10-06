#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <string.h>

// --- KONFİGÜRASYON VE PİNLER ---
const int MOTOR_PINS[6] = {2, 3, 4, 5, 6, 7};
const int NEUTRAL_PWM = 1480;

#define RST_PIN 46
#define SS_PIN 53 // SDA

// --- NESNELER ---
Servo motors[6];
MFRC522 mfrc522(SS_PIN, RST_PIN);

// --- DURUM MAKİNESİ (STATE MACHINE) ---
enum State
{
  IDLE,
  MOVING_FORWARD,
  TURNING_90,
  CIRCLING_360
};
State currentState = IDLE;

// --- GÖREV DEĞİŞKENLERİ ---
int fwdLeftPWM = 1600, fwdRightPWM = 1600;
int turnLeftPWM = 1600, turnRightPWM = 1360;
int circleLeftPWM = 1600, circleRightPWM = 1360;

// Motorlara sürekli gönderilecek hedefler
int targetLeftPWM = NEUTRAL_PWM;
int targetRightPWM = NEUTRAL_PWM;

float moveTimerSec = 0.0;
float moveDurationSec = 0.0;

// --- SİSTEM & GÜVENLİK DEĞİŞKENLERİ ---
bool emergencyStop = false;
unsigned long lastRFIDTime = 0;
unsigned long lastLoopTimeMs = 0;
unsigned long lastHeartbeatMs = 0;

// Seri Port Okuma Tamponu
char rxBuffer[64];
uint8_t rxIndex = 0;

// --- FONKSİYON PROTOTİPLERİ ---
void setupMotors();
void checkNFC();
void parseSerial();
void processCommand(const char *cmd);
void executeState(float dt);
void setMotors(int left_pwm, int right_pwm);

void setup()
{
  Serial.begin(115200);
  SPI.begin();

  setupMotors();
  mfrc522.PCD_Init();

  lastLoopTimeMs = millis();
  lastHeartbeatMs = millis();
}

void loop()
{
  unsigned long nowMs = millis();
  float dt = (nowMs - lastLoopTimeMs) / 1000.0; // saniye cinsinden
  lastLoopTimeMs = nowMs;

  checkNFC();
  parseSerial();

  // Jetson'dan 2 saniye boyunca sinyal/komut gelmezse güvenlik timeout'u
  bool timeoutActive = (nowMs - lastHeartbeatMs > 2000);

  // Görev işleticisi sadece güvenliyken ilerler
  if (!emergencyStop && !timeoutActive)
  {
    executeState(dt);
  }

  // Motorlara sürekli (sürekli PWM beslemesi) PWM gönderilir
  if (emergencyStop || timeoutActive)
  {
    setMotors(NEUTRAL_PWM, NEUTRAL_PWM);
  }
  else
  {
    setMotors(targetLeftPWM, targetRightPWM);
  }
}

// ==========================================
// MOTOR KONTROL FONKSİYONLARI
// ==========================================
void setupMotors()
{
  for (int i = 0; i < 6; i++)
  {
    motors[i].attach(MOTOR_PINS[i], 1000, 2000);
    motors[i].writeMicroseconds(NEUTRAL_PWM);
  }
  delay(5000); // 5 saniye ESC arm beklemesi
}

void setMotors(int left_pwm, int right_pwm)
{
  motors[0].writeMicroseconds(left_pwm);
  motors[1].writeMicroseconds(left_pwm);
  motors[2].writeMicroseconds(1480);

  motors[3].writeMicroseconds(right_pwm);
  motors[4].writeMicroseconds(right_pwm);
  motors[5].writeMicroseconds(1480);
}

// ==========================================
// NFC FONKSİYONLARI
// ==========================================
void checkNFC()
{
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return;

  unsigned long now = millis();
  if (now - lastRFIDTime > 2000)
  { // 2 Saniye Debounce
    lastRFIDTime = now;
    emergencyStop = !emergencyStop;

    if (emergencyStop)
    {
      Serial.println("ESTOP_ON");
    }
    else
    {
      Serial.println("ESTOP_OFF");
    }
  }
  mfrc522.PICC_HaltA();
}

// ==========================================
// SERİ HABERLEŞME VE PARSİNG
// ==========================================
void parseSerial()
{
  while (Serial.available() > 0)
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (rxIndex > 0)
      {
        rxBuffer[rxIndex] = '\0';
        processCommand(rxBuffer);
        rxIndex = 0;
      }
    }
    else if (rxIndex < 63)
    {
      rxBuffer[rxIndex++] = c;
    }
  }
}

void processCommand(const char *cmd)
{
  lastHeartbeatMs = millis(); // Herhangi bir iletişim güvenlik sayacını sıfırlar

  if (strncmp(cmd, "PING", 4) == 0)
  {
    return; // Sadece bağlantıyı canlı tutmak için
  }
  else if (strncmp(cmd, "SET_FORWARD", 11) == 0)
  {
    sscanf(cmd, "SET_FORWARD %d %d", &fwdLeftPWM, &fwdRightPWM);
  }
  else if (strncmp(cmd, "SET_TURN", 8) == 0)
  {
    sscanf(cmd, "SET_TURN %d %d", &turnLeftPWM, &turnRightPWM);
  }
  else if (strncmp(cmd, "SET_CIRCLE", 10) == 0)
  {
    sscanf(cmd, "SET_CIRCLE %d %d", &circleLeftPWM, &circleRightPWM);
  }
  else if (strncmp(cmd, "FORWARD_START", 13) == 0)
  {
    int sec = 0;
    sscanf(cmd, "FORWARD_START %d", &sec);
    moveDurationSec = sec;
    moveTimerSec = 0.0;
    currentState = MOVING_FORWARD;
  }
  else if (strncmp(cmd, "TURN90_RIGHT", 12) == 0)
  {
    int sec = 0;
    sscanf(cmd, "TURN90_RIGHT %d", &sec);
    moveDurationSec = sec;
    moveTimerSec = 0.0;
    currentState = TURNING_90;
  }
  else if (strncmp(cmd, "CIRCLE360", 9) == 0)
  {
    int sec = 0;
    sscanf(cmd, "CIRCLE360 %d", &sec);
    moveDurationSec = sec;
    moveTimerSec = 0.0;
    currentState = CIRCLING_360;
  }
  else if (strncmp(cmd, "STOP", 4) == 0)
  {
    currentState = IDLE;
    targetLeftPWM = NEUTRAL_PWM;
    targetRightPWM = NEUTRAL_PWM;
    Serial.println("DONE");
  }
}

// ==========================================
// GÖREV İŞLETİCİ (SÜRE BAZLI)
// ==========================================
void executeState(float dt)
{
  if (currentState == IDLE)
  {
    targetLeftPWM = NEUTRAL_PWM;
    targetRightPWM = NEUTRAL_PWM;
    return;
  }

  // Ortak Zaman Sayacı
  moveTimerSec += dt;

  // Hedef PWM'leri Belirleme
  if (currentState == MOVING_FORWARD)
  {
    targetLeftPWM = fwdLeftPWM;
    targetRightPWM = fwdRightPWM;
  }
  else if (currentState == TURNING_90)
  {
    targetLeftPWM = turnLeftPWM;
    targetRightPWM = turnRightPWM;
  }
  else if (currentState == CIRCLING_360)
  {
    targetLeftPWM = circleLeftPWM;
    targetRightPWM = circleRightPWM;
  }

  // Süre Dolduğunda Dur ve Sonraki Göreve Geç
  if (moveTimerSec >= moveDurationSec)
  {
    targetLeftPWM = NEUTRAL_PWM;
    targetRightPWM = NEUTRAL_PWM;
    currentState = IDLE;
    Serial.println("DONE");
  }
}