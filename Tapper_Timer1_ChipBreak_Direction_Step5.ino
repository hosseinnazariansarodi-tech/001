
#include <Adafruit_GFX.h>
#include <KS0108_GLCD.h>
#include <EEPROM.h>

// =====================================================
// GLCD 128x64 - KS0108
// Arduino Mega 2560
// =====================================================

KS0108_GLCD display = KS0108_GLCD(
  22, 23, 24,
  25, 26, 27, 28,
  29, 30, 31, 32,
  33, 34,
  35
);

// =====================================================
// MACHINE / SERVO CONFIGURATION
// =====================================================

const float GEAR_RATIO = 25.0;

const float SERVO_MAX_RPM = 2000.0;

const float MACHINE_MAX_DEPTH = 100.0;

// تعداد پالس برای یک دور کامل موتور
// فعلاً موقت
const long PULSES_PER_MOTOR_REV = 1000;

// 

//=====================================================
// SERVO PINS
// =====================================================

#define SERVO_PULSE_PIN 44
#define SERVO_DIR_PIN   42
#define SERVO_ON_PIN    40

// =====================================================
// KEYS
// =====================================================

#define KEY_UP       2
#define KEY_DOWN     3

#define KEY_ENTER    4
#define KEY_MENU     5

// شستی های روی دسته
#define HANDLE_START 6
#define HANDLE_STOP  7

// =====================================================
// PARAMETERS
// =====================================================

struct Parameters
{
  float pitch;
  float depth;
  int speed;
  int torque;

  float torqueDelay;
  float forward;
  float back;
  byte chipBreak;
  byte tapDirection;
};

Parameters parameters;
Parameters savedParameters;

// =====================================================
// EEPROM
// =====================================================

#define EEPROM_MARK 123

// 

//=====================================================
// MODES
// =====================================================

bool settingsMode = false;
bool manualMode = false;
bool autoMode = false;
bool editMode = false;

// =====================================================
// MENU
// =====================================================

byte menuPosition = 0;

byte parameterPosition = 0;

// =====================================================
// MANUAL
// =====================================================

int manualSpeed;
float manualDepth = 0.0;

unsigned long handleReleaseTime = 0;
bool handleWasPressed = false;

// شمارنده پالس دستی
long manualPulseCount = 0;

// زمان‌بندی پالس
volatile uint32_t timerPulseEvents = 0;

// =====================================================
// AUTO
// =====================================================

enum AutoState
{
  AUTO_READY,
  AUTO_RUNNING,
  AUTO_STOPPED,
  AUTO_RETURNING,
  AUTO_FINISHED
};

AutoState autoState = AUTO_READY;

bool startEnabled = true;

unsigned long stopButtonTime = 0;
bool stopButtonPressed = false;
bool stopReturnHeld = false;

unsigned long autoStartTime = 0;

float autoDepth = 0.0;

// تعداد پالس مورد نیاز سیکل اتوماتیک
long autoTargetPulses = 0;
long autoPulseCount = 0;

// =====================================================
// CHIP BREAK STATE
// FORWARD / RETURN are spindle revolutions per phase
// =====================================================
bool chipBreakForwardPhase = true;
long chipBreakRemainingPulses = 0;
long chipBreakForwardPulses = 0;
long chipBreakBackPulses = 0;

// =====================================================
// SERVO MOVEMENT
// =====================================================

bool servoMoving = false;

// =====================================================
// CALCULATIONS
// =====================================================

// حداکثر سرعت مجاز اسپیندل
float getMaxSpindleRPM()
{
  if (GEAR_RATIO <= 0.0)
    return 0.0;

  return SERVO_MAX_RPM / GEAR_RATIO;
}

// دور اسپیندل مورد نیاز برای عمق
float calculateSpindleRevolutions(float 

depth, float pitch)
{
  if (pitch <= 0.0)
    return 0.0;

  return depth / pitch;
}

// دور موتور سروو مورد نیاز
float calculateMotorRevolutions(float depth, float pitch)
{
  return calculateSpindleRevolutions(depth, pitch)
         * GEAR_RATIO;
}

// تعداد پالس مورد نیاز
long calculatePulses(float depth, float pitch)
{

  float motorRevolutions =
    calculateMotorRevolutions(depth, pitch);

  return (long)(motorRevolutions *
                PULSES_PER_MOTOR_REV);
}

// عمق متناظر با تعداد پالس
float calculateDepthFromPulses(long pulses, float pitch)
{
  if (PULSES_PER_MOTOR_REV <= 0)
    return 0.0;

  float motorRevolutions =
    (float)pulses / PULSES_PER_MOTOR_REV;

  float spindleRevolutions =
    motorRevolutions / GEAR_RATIO;

  return spindleRevolutions * pitch;
}

// =====================================================
// STARTUP
// =====================================================

void drawGear(byte rotation)
{
  int cx = 105;
  int cy = 21;

  display.drawCircle(cx, cy, 12, KS0108_ON);
  display.drawCircle(cx, cy, 9, KS0108_ON);

  display.fillCircle(cx, cy, 4, KS0108_OFF);

  if (rotation == 0)
  {
    display.fillRect(cx - 2, cy - 19, 5, 7, KS0108_ON);
    display.fillRect(cx - 2, cy + 13, 5, 7, KS0108_ON);

    display.fillRect(cx - 19, cy - 2, 7, 5, KS0108_ON);
    display.fillRect(cx + 12, cy - 2, 7, 5, KS0108_ON);

    display.fillRect(cx - 15, cy - 15, 5, 5, KS0108_ON);
    display.fillRect(cx + 10, cy - 15, 5, 5, KS0108_ON);

    display.fillRect(cx - 15, cy + 10, 5, 5, KS0108_ON);
    display.fillRect(cx + 10, cy + 10, 5, 5, KS0108_ON);

  }

  else if (rotation == 1)
  {
    display.fillRect(cx - 14, cy - 17, 7, 5, KS0108_ON);
    display.fillRect(cx + 8, cy - 17, 7, 5, KS0108_ON);

    display.fillRect(cx - 14, cy + 14, 7, 5, KS0108_ON);
    display.fillRect(cx + 8, cy + 14, 7, 5, KS0108_ON);

    display.fillRect(cx - 20, cy - 2, 7, 5, KS0108_ON);
    display.fillRect(cx + 13, cy - 2, 7, 5, KS0108_ON);

    display.fillRect(cx - 20, cy + 7, 5, 7, KS0108_ON);

    display.fillRect(cx + 15, cy - 14, 5, 7, KS0108_ON);
  }

  else if (rotation == 2)
  {
    display.fillRect(cx - 17, cy - 14, 7, 5, KS0108_ON);
    display.fillRect(cx + 10, cy + 10, 7, 5, KS0108_ON);

    display.fillRect(cx + 10, cy - 14, 7, 5, KS0108_ON);
    display.fillRect(cx - 17, cy + 10, 7, 5, KS0108_ON);

    display.fillRect(cx - 20, cy - 2, 7, 5, KS0108_ON);
    display.fillRect(cx + 13, cy - 2, 7, 5, KS0108_ON);

    display.fillRect(cx - 2, cy - 20, 5, 7, KS0108_ON);
    display.fillRect(cx - 2, cy + 13, 5, 7, KS0108_ON);
  }

  else
  {
    display.fillRect(cx - 17, cy - 14, 7, 5, KS0108_ON);
    display.fillRect(cx + 10, cy + 10, 7, 5, KS0108_ON);

    display.fillRect(cx + 10, cy - 14, 7, 5, KS0108_ON);
    display.fillRect(cx - 17, cy + 10, 7, 5, KS0108_ON);

    display.fillRect(cx - 2, cy - 20, 5, 7, KS0108_ON);
    display.fillRect(cx - 2, cy + 13, 5, 7, 

KS0108_ON);

    display.fillRect(cx - 20, cy - 2, 7, 5, KS0108_ON);
    display.fillRect(cx + 13, cy - 2, 7, 5, KS0108_ON);
  }
}

// =====================================================
// STARTUP ANIMATION
// =====================================================

void startupAnimation()
{
  unsigned long startTime = millis();

  unsigned long lastGearFrame = 0;
  unsigned long lastTextChar = 0;

  byte gearFrame = 0;
  byte textLength = 0;

  const char companyText[] = "KYAN SANAT NOVIN";

  while (millis() - startTime < 5000)
  {
    unsigned long now = millis();

    if (now - lastGearFrame >= 150)
    {
      lastGearFrame = now;

      gearFrame++;

      if (gearFrame >= 4)
        gearFrame = 0;

    }

    if (now - startTime >= 2000)
    {
      if (now - lastTextChar >= 100)
      {
        lastTextChar = now;

        if (textLength < 16)
          textLength++;
      }
    }

    display.clearDisplay();

    display.setTextColor(KS0108_ON);
    display.setTextSize(2);

    display.setCursor(3, 10);

    display.print("K.S.N");


    drawGear(gearFrame);

    if (textLength > 0)
    {
      display.setTextSize(1);

      display.setCursor(15, 48);

      for (byte i = 0; i < textLength; i++)
      {
        display.print(companyText[i]);
      }
    }

    display.display();
  }

  display.clearDisplay();
  display.display();
}


// =====================================================
// DEFAULT PARAMETERS
// =====================================================

void setDefaultParameters()
{
  parameters.pitch = 1.00;
  parameters.depth = 10.0;

  parameters.speed = 50;

  parameters.torque = 50;
  parameters.torqueDelay = 0.50;
  parameters.forward = 2.0;
  parameters.back = 1.0;
  parameters.chipBreak = 1;

  parameters.tapDirection = 0;
}

// =====================================================
// EEPROM SAVE
// =====================================================

void saveParameters()
{
  EEPROM.put(0, EEPROM_MARK);
  EEPROM.put(4, parameters);

  savedParameters = parameters;
}

// ===================================

//==================
// EEPROM LOAD
// =====================================================

void loadParameters()
{
  int mark;

  EEPROM.get(0, mark);

  if (mark != EEPROM_MARK)
  {
    setDefaultParameters();
    saveParameters();
  }
  else
  {
    EEPROM.get(4, parameters);
    savedParameters = parameters;


    float maxRPM = getMaxSpindleRPM();

    if (parameters.speed > (int)maxRPM)
      parameters.speed = (int)maxRPM;

    if (parameters.depth > MACHINE_MAX_DEPTH)
      parameters.depth = MACHINE_MAX_DEPTH;
  }
}

// =====================================================
// MAIN MENU
// =====================================================

void showMainMenu()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(KS0108_ON);

  display.setCursor(40, 2);
  display.print("MAIN MENU");

  display.drawLine(0, 12, 127, 12, KS0108_ON);

  display.setCursor(25, 22);

  if (menuPosition == 0)
    display.print("> AUTO");
  else
    display.print("  AUTO");

  display.setCursor(25, 36);


  if (menuPosition == 1)
    display.print("> MANUAL");
  else
    display.print("  MANUAL");

  display.setCursor(25, 50);

  if (menuPosition == 2)
    display.print("> SETTINGS");
  else
    display.print("  SETTINGS");

  display.display();
}

// =====================================================
// SETTINGS
// 

//=====================================================

void showSettings()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(40, 0);
  display.setTextColor(KS0108_ON);
  display.print("SETTINGS");

  display.drawLine(0, 9, 127, 9, KS0108_ON);

  byte first;

  if (parameterPosition < 4)
    first = 0;
  else if (parameterPosition < 8)
    first = 4;
  else

    first = 8;

  for (byte row = 0; row < 4; row++)
  {
    byte p = first + row;

    if (p >= 10)
      break;

    int y = 14 + row * 12;

    if (p == parameterPosition && editMode)
    {
      display.fillRect(0, y - 1, 127, 10, KS0108_ON);
      display.setTextColor(KS0108_OFF);
    }
    else
    {
      display.setTextColor(KS0108_ON);
    }


    display.setCursor(0, y);

    if (p == parameterPosition)
      display.print(">");
    else
      display.print(" ");

    if (p == 0)
      display.print("PITCH");

    if (p == 1)
      display.print("DEPTH");

    if (p == 2)
      display.print("SPEED");

    if (p == 3)
      display.print("TORQUE");

    if (p == 4)

      display.print("T.DELAY");

    if (p == 5)
      display.print("FORWARD");

    if (p == 6)
      display.print("RETURN");

    if (p == 7)
      display.print("CHIP BREAK");

    if (p == 8)
      display.print("TAP DIR");

    if (p == 9)
      display.print("SAVE");

    display.setCursor(88, y);

    if (p == 0)
      display.print(parameters.pitch, 2);


    if (p == 1)
      display.print(parameters.depth, 1);

    if (p == 2)
      display.print(parameters.speed);

    if (p == 3)
    {
      display.print(parameters.torque);
      display.print("%");
    }

    if (p == 4)
      display.print(parameters.torqueDelay, 2);

    if (p == 5)
      display.print(parameters.forward, 2);

    if (p == 6)

      display.print(parameters.back, 2);

    if (p == 7)
    {
      if (parameters.chipBreak)
        display.print("ON");
      else
        display.print("OFF");
    }

    if (p == 8)
    {
      if (parameters.tapDirection == 0)
        display.print("M3");
      else
        display.print("M4");
    }

    display.setTextColor(KS0108_ON);
  }

  display.display();
}

// =====================================================
// CHANGE PARAMETER
// =====================================================

void changeParameter(int dir)
{
  if (parameterPosition == 0)
  {
    parameters.pitch += 0.01 * dir;

    if (parameters.pitch < 0.01)
      parameters.pitch = 0.01;

    if (parameters.pitch > 20.00)

      parameters.pitch = 20.00;
  }

  else if (parameterPosition == 1)
  {
    parameters.depth += 0.1 * dir;

    if (parameters.depth < 0.1)
      parameters.depth = 0.1;

    if (parameters.depth > MACHINE_MAX_DEPTH)
      parameters.depth = MACHINE_MAX_DEPTH;
  }

  else if (parameterPosition == 2)
  {
    float maxRPM = getMaxSpindleRPM();

    parameters.speed += 5 * dir;


    if (parameters.speed < 10)
      parameters.speed = 10;

    if (parameters.speed > (int)maxRPM)
      parameters.speed = (int)maxRPM;
  }

  else if (parameterPosition == 3)
  {
    parameters.torque += dir;

    if (parameters.torque < 1)
      parameters.torque = 1;

    if (parameters.torque > 100)
      parameters.torque = 100;
  }

  else if (parameterPosition == 4)
  {

    parameters.torqueDelay += 0.01 * dir;

    if (parameters.torqueDelay < 0.01)
      parameters.torqueDelay = 0.01;

    if (parameters.torqueDelay > 10.00)
      parameters.torqueDelay = 10.00;
  }

  else if (parameterPosition == 5)
  {
    parameters.forward += dir;

    if (parameters.forward < 1)
      parameters.forward = 1;

    if (parameters.forward > 20)
      parameters.forward = 20;
  }

  else if (parameterPosition == 6)

  {
    parameters.back += dir;

    if (parameters.back < 1)
      parameters.back = 1;

    if (parameters.back > 20)
      parameters.back = 20;
  }

  else if (parameterPosition == 7)
  {
    parameters.chipBreak = !parameters.chipBreak;
  }

  else if (parameterPosition == 8)
  {
    if (dir > 0)
      parameters.tapDirection = 1;
    else

      parameters.tapDirection = 0;
  }
}

// =====================================================
// EDIT PARAMETER
// =====================================================

void editParameter()
{
  byte key;

  if (digitalRead(KEY_UP) == LOW)
    key = KEY_UP;
  else
    key = KEY_DOWN;

  unsigned long start = millis();
  unsigned long lastChange = 0;

  bool first = true;

  while (digitalRead(key) == LOW)
  {
    unsigned long now = millis();

    unsigned long holdTime = now - start;

    unsigned long interval;

    if (holdTime < 500)
      interval = 300;
    else if (holdTime < 1500)
      interval = 120;
    else
      interval = 50;

    if (first || now - lastChange >= interval)

    {
      lastChange = now;
      first = false;

      if (key == KEY_UP)
        changeParameter(1);
      else
        changeParameter(-1);

      showSettings();
    }
  }
}

// =====================================================
// MANUAL SCREEN
// =====================================================


void showManual()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(KS0108_ON);

  display.setCursor(43, 0);
  display.print("MANUAL");

  display.drawLine(0, 9, 127, 9, KS0108_ON);

  display.setCursor(5, 16);
  display.print("SPEED");

  display.setCursor(75, 16);
  display.print(manualSpeed);
  display.print(" RPM");

  display.setCursor(5, 28);

  display.print("PITCH");

  display.setCursor(75, 28);
  display.print(parameters.pitch, 2);
  display.print(" mm");

  display.setCursor(5, 40);
  display.print("DEPTH");

  display.setCursor(75, 40);
  display.print(manualDepth, 1);
  display.print(" mm");

  display.setCursor(5, 52);
  display.print("TORQUE");

  display.drawRect(50, 51, 74, 10, KS0108_ON);

  int torqueBar = parameters.torque;

  int barWidth = map(torqueBar, 0, 100, 0, 70);

  if (barWidth > 0)
    display.fillRect(52, 53, barWidth, 6, KS0108_ON);

  display.display();
}

// =====================================================
// MANUAL SPEED
// =====================================================

void changeManualSpeed(int dir)
{
  float maxRPM = getMaxSpindleRPM();


  manualSpeed += dir * 5;

  if (manualSpeed < 10)
    manualSpeed = 10;

  if (manualSpeed > (int)maxRPM)
    manualSpeed = (int)maxRPM;
}

void manualSpeedControl()
{
  byte key;

  if (digitalRead(KEY_UP) == LOW)
    key = KEY_UP;
  else
    key = KEY_DOWN;

  unsigned long start = millis();
  unsigned long lastChange = 0;


  bool first = true;

  while (digitalRead(key) == LOW)
  {
    unsigned long now = millis();

    unsigned long holdTime = now - start;

    unsigned long interval;

    if (holdTime < 500)
      interval = 300;
    else if (holdTime < 1500)
      interval = 120;
    else
      interval = 50;

    if (first || now - lastChange >= interval)
    {
      lastChange = now;

      first = false;

      if (key == KEY_UP)
        changeManualSpeed(1);
      else
        changeManualSpeed(-1);

      showManual();
    }
  }
}

// =====================================================
// HARDWARE TIMER1 - SERVO PULSE GENERATOR
// Arduino Mega 2560
// PULSE = Pin 44 = PL5
// DIR   = Pin 42 = PL7
// =====================================================

void setupTimer1()
{
  noInterrupts();

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A  = 0;

  // CTC Mode
  TCCR1B |= _BV(WGM12);

  // Prescaler = 8
  TCCR1B |= _BV(CS11);

  // Timer1 interrupt initially OFF
  TIMSK1 = 0;

  // Arduino Mega pin 44 = PL5
  DDRL |= _BV(PL5);
  PORTL &= ~_BV(PL5);

  timerPulseEvents = 0;

  interrupts();
}

// =====================================================
// TIMER1 START / SET PULSE FREQUENCY
// =====================================================

void timer1SetPulseRPM(float spindleRPM)
{
  if (spindleRPM <= 0.0)
  {
    timer1StopPulse();
    return;
  }

  // دور موتور = دور اسپیندل × نسبت گیربکس
  double motorRPM =
    (double)spindleRPM * (double)GEAR_RATIO;

  // تعداد پالس در ثانیه
  double pulseHz =
    (motorRPM * (double)PULSES_PER_MOTOR_REV) / 60.0;

  if (pulseHz < 1.0)
    pulseHz = 1.0;

  // ISR پایه را Toggle می‌کند، بنابراین فرکانس وقفه
  // دو برابر فرکانس واقعی PULSE است.
  double compareValue =
    ((double)F_CPU / (16.0 * pulseHz)) - 1.0;

  if (compareValue < 0.0)
    compareValue = 0.0;

  if (compareValue > 65535.0)
    compareValue = 65535.0;

  noInterrupts();

  OCR1A = (uint16_t)(compareValue + 0.5);
  TCNT1 = 0;
  timerPulseEvents = 0;

  // فعال کردن Compare A interrupt
  TIMSK1 |= _BV(OCIE1A);

  interrupts();
}

// =====================================================
// TIMER1 STOP
// =====================================================

void timer1StopPulse()
{
  noInterrupts();

  TIMSK1 &= ~_BV(OCIE1A);
  TCNT1 = 0;
  PORTL &= ~_BV(PL5);
  timerPulseEvents = 0;

  interrupts();
}

// =====================================================
// TIMER1 ISR
// =====================================================

ISR(TIMER1_COMPA_vect)
{
  // ساخت PULSE روی پایه 44
  PORTL ^= _BV(PL5);

  // فقط لبه صعودی = یک پالس
  if (PORTL & _BV(PL5))
  {
    timerPulseEvents++;
  }
}

// =====================================================
// گرفتن تعداد پالس‌های تولیدشده توسط Timer1
// =====================================================

uint32_t takeTimerPulseEvents()
{
  uint32_t count;

  noInterrupts();
  count = timerPulseEvents;
  timerPulseEvents = 0;
  interrupts();

  return count;
}

// =====================================================
// TAP DIRECTION
// M3 = normal tapping direction
// M4 = reverse tapping direction
// =====================================================

bool tapForwardDirection()
{
  return (parameters.tapDirection == 0) ? LOW : HIGH;
}

bool tapReturnDirection()
{
  return (parameters.tapDirection == 0) ? HIGH : LOW;
}

// =====================================================
// SERVO COMMANDS
// =====================================================

void servoStart()
{
  digitalWrite(SERVO_ON_PIN, HIGH);
  servoMoving = true;

  // سرعت مناسب بر اساس حالت فعلی
  float rpm;

  if (manualMode)
    rpm = manualSpeed;
  else
    rpm = parameters.speed;

  timer1SetPulseRPM(rpm);
}

void servoStop()
{
  servoMoving = false;

  timer1StopPulse();

  digitalWrite(SERVO_ON_PIN, LOW);
}

// =====================================================
// MANUAL - CW
// =====================================================

void servoCW()
{
  digitalWrite(SERVO_DIR_PIN, tapForwardDirection());

  if (!servoMoving)
    servoStart();

  uint32_t pulses = takeTimerPulseEvents();

  if (pulses == 0)
    return;

  long maxPulses =
    calculatePulses(MACHINE_MAX_DEPTH, parameters.pitch);

  if (manualPulseCount < maxPulses)
  {
    if ((uint32_t)(maxPulses - manualPulseCount) < pulses)
      manualPulseCount = maxPulses;
    else
      manualPulseCount += (long)pulses;
  }

  manualDepth = calculateDepthFromPulses(
    manualPulseCount,
    parameters.pitch
  );

  if (manualDepth > MACHINE_MAX_DEPTH)
    manualDepth = MACHINE_MAX_DEPTH;

  showManual();
}

// =====================================================
// MANUAL - CCW
// =====================================================

void servoCCW()
{
  digitalWrite(SERVO_DIR_PIN, tapReturnDirection());

  if (!servoMoving)
    servoStart();

  uint32_t pulses = takeTimerPulseEvents();

  if (pulses == 0)
    return;

  if ((uint32_t)manualPulseCount < pulses)
    manualPulseCount = 0;
  else
    manualPulseCount -= (long)pulses;

  manualDepth = calculateDepthFromPulses(
    manualPulseCount,
    parameters.pitch
  );

  if (manualDepth < 0.0)
    manualDepth = 0.0;

  showManual();
}

// =====================================================
// MANUAL HANDLE CONTROL
// =====================================================

void manualHandleControl()
{
  bool button6 = (digitalRead(HANDLE_START) == LOW);
  bool button7 = (digitalRead(HANDLE_STOP) == LOW);

  // هر دو همزمان = توقف کامل و بدون حرکت
  if (button6 && button7)
  {
    servoStop();
    handleWasPressed = true;
    handleReleaseTime = 0;
    return;
  }

  // شستی 6 = حرکت در جهت افزایش عمق
  if (button6)
  {
    handleWasPressed = true;
    handleReleaseTime = 0;
    servoCW();
    return;
  }

  // شستی 7 = حرکت در جهت کاهش عمق
  if (button7)
  {
    handleWasPressed = true;
    handleReleaseTime = 0;
    servoCCW();
    return;
  }

  // هیچ شستی فشرده نیست = توقف فوری
  servoStop();

  // تازه شستی رها شده: تایمر 3 ثانیه را شروع کن
  if (handleWasPressed)
  {
    handleWasPressed = false;
    handleReleaseTime = millis();
  }

  // اگر 3 ثانیه هیچ شستی‌ای زده نشد، عمق صفر شود
  if (!handleWasPressed &&
      handleReleaseTime != 0 &&
      millis() - handleReleaseTime >= 3000)
  {
    manualDepth = 0.0;
    manualPulseCount = 0;
    handleReleaseTime = 0;
    showManual();
  }
}

// =====================================================
// MANUAL CONTROL
// =====================================================

void manualControl()
{
  // خروج از MANUAL با کلید MENU
  if (digitalRead(KEY_MENU) == LOW)
  {
    while (digitalRead(KEY_MENU) == LOW)
      delay(10);

    servoStop();
    manualMode = false;
    manualSpeed = savedParameters.speed;
    digitalWrite(SERVO_PULSE_PIN, LOW);

    showMainMenu();
    delay(150);
    return;
  }

  // تغییر سرعت دستی با UP
  if (digitalRead(KEY_UP) == LOW)
  {
    manualSpeedControl();
    showManual();
    delay(100);
    return;
  }

  // تغییر سرعت دستی با DOWN
  if (digitalRead(KEY_DOWN) == LOW)
  {
    manualSpeedControl();
    showManual();
    delay(100);
    return;
  }

  // کنترل شستی‌های دسته
  manualHandleControl();
}

// =====================================================
// TORQUE
// =====================================================

int readTorque()
{
  // فعلاً ورودی واقعی گشتاور به Arduino وصل نشده است.
  // تا زمان اتصال CN5/MON2، مقدار صفر یعنی محدودیت گشتاور فعال نیست.
  return 0;
}

bool torqueLimitReached()
{
  int torque = readTorque();
  return (torque >= parameters.torque);
}

// =====================================================
// AUTO RETURN PULSE GENERATION
// =====================================================

void autoReturnPulse()
{
  // برگشت به سمت بالا / صفر
  digitalWrite(SERVO_DIR_PIN, tapReturnDirection());

  if (autoPulseCount <= 0)
  {
    autoPulseCount = 0;
    autoDepth = 0.0;
    stopReturnHeld = false;
    servoStop();
    autoState = AUTO_FINISHED;
    stopButtonTime = millis();
    showAuto();
    return;
  }

  if (!servoMoving)
    servoStart();

  uint32_t pulses = takeTimerPulseEvents();

  if (pulses == 0)
    return;

  if (pulses >= (uint32_t)autoPulseCount)
    autoPulseCount = 0;
  else
    autoPulseCount -= (long)pulses;

  autoDepth = calculateDepthFromPulses(
    autoPulseCount,
    parameters.pitch
  );

  if (autoDepth < 0.0)
    autoDepth = 0.0;

  showAuto();

  if (autoPulseCount == 0)
  {
    autoDepth = 0.0;
    stopReturnHeld = false;
    servoStop();
    autoState = AUTO_FINISHED;
    stopButtonTime = millis();
    showAuto();
  }
}

// =====================================================
// AUTO SCREEN
// =====================================================

void showAuto()
{
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(KS0108_ON);

  display.setCursor(45, 0);
  display.print("AUTO");

  display.drawLine(0, 9, 127, 9, KS0108_ON);

  display.setCursor(3, 15);
  display.print("SPEED");

  display.setCursor(75, 15);
  display.print(parameters.speed);
  display.print(" RPM");

  display.setCursor(3, 27);
  display.print("PITCH");

  display.setCursor(75, 27);
  display.print(parameters.pitch, 2);
  display.print(" mm");

  display.setCursor(3, 39);
  display.print("DEPTH");

  display.setCursor(75, 39);
  display.print(autoDepth, 1);
  display.print(" mm");

  display.setCursor(3, 51);
  display.print("TORQUE");

  display.drawRect(50, 50, 74, 11, KS0108_ON);

  int torque = readTorque();

  int barWidth = map(torque, 0, 100, 0, 70);

  if (barWidth > 0)
    display.fillRect(52, 52, barWidth, 7, KS0108_ON);

  display.display();
}

// =====================================================
// AUTO START
// =====================================================

void startAutoCycle()
{
  if (!startEnabled || autoState != AUTO_READY)
    return;

  startEnabled = false;
  stopButtonPressed = false;
  stopReturnHeld = false;
  autoState = AUTO_RUNNING;
  autoDepth = 0.0;
  autoPulseCount = 0;

  chipBreakForwardPhase = true;
  chipBreakForwardPulses = 0;
  chipBreakBackPulses = 0;
  chipBreakRemainingPulses = 0;

  autoTargetPulses = calculatePulses(
    parameters.depth,
    parameters.pitch
  );

  autoStartTime = millis();

  // برای جلوگیری از یک لبه ناخواسته هنگام شروع
  servoStop();
  showAuto();
}

// =====================================================
// AUTO STOP
// =====================================================

void stopAutoCycle()
{
  servoStop();
  autoState = AUTO_STOPPED;
  showAuto();
}

// =====================================================
// AUTO RETURN
// =====================================================

void returnAuto()
{
  if (autoPulseCount <= 0)
  {
    autoPulseCount = 0;
    autoDepth = 0.0;
    stopReturnHeld = false;
    servoStop();
    autoState = AUTO_FINISHED;
    stopButtonTime = millis();
    showAuto();
    return;
  }

  autoState = AUTO_RETURNING;
  stopReturnHeld = false;
  servoStop();
  showAuto();
}

// =====================================================
// AUTO FORWARD PULSE GENERATION
// =====================================================

void autoForwardPulse()
{
  // ---------------------------------------------------
  // CHIP BREAK OFF
  // حرکت مستقیم تا عمق نهایی
  // ---------------------------------------------------
  if (!parameters.chipBreak)
  {
    digitalWrite(SERVO_DIR_PIN, tapForwardDirection());

    if (!servoMoving)
      servoStart();

    uint32_t pulses = takeTimerPulseEvents();

    if (pulses == 0)
      return;

    if (autoPulseCount < autoTargetPulses)
    {
      uint32_t remaining =
        (uint32_t)(autoTargetPulses - autoPulseCount);

      if (pulses >= remaining)
        autoPulseCount = autoTargetPulses;
      else
        autoPulseCount += (long)pulses;
    }

    autoDepth = calculateDepthFromPulses(
      autoPulseCount,
      parameters.pitch
    );

    if (autoDepth > parameters.depth)
      autoDepth = parameters.depth;

    showAuto();

    if (autoPulseCount >= autoTargetPulses)
    {
      autoDepth = parameters.depth;
      servoStop();
      returnAuto();
    }

    return;
  }

  // ---------------------------------------------------
  // CHIP BREAK ON
  // FORWARD = تعداد دور حرکت رو به پایین
  // RETURN  = تعداد دور برگشت برای شکستن براده
  // ---------------------------------------------------

  if (chipBreakForwardPulses <= 0)
    chipBreakForwardPulses =
      (long)(parameters.forward * GEAR_RATIO * PULSES_PER_MOTOR_REV);

  if (chipBreakBackPulses <= 0)
    chipBreakBackPulses =
      (long)(parameters.back * GEAR_RATIO * PULSES_PER_MOTOR_REV);

  if (chipBreakForwardPulses <= 0)
    chipBreakForwardPulses = 1;

  if (chipBreakBackPulses <= 0)
    chipBreakBackPulses = 1;

  // اگر هنوز فاز شروع نشده، با حرکت رو به جلو شروع کن
  if (chipBreakRemainingPulses <= 0)
  {
    chipBreakForwardPhase = true;
    chipBreakRemainingPulses = chipBreakForwardPulses;
  }

  // جهت فاز فعلی
  if (chipBreakForwardPhase)
    digitalWrite(SERVO_DIR_PIN, tapForwardDirection());
  else
    digitalWrite(SERVO_DIR_PIN, tapReturnDirection());

  if (!servoMoving)
    servoStart();

  uint32_t pulses = takeTimerPulseEvents();

  if (pulses == 0)
    return;

  // ---------------------------------------------------
  // فاز حرکت رو به جلو
  // ---------------------------------------------------
  if (chipBreakForwardPhase)
  {
    // بیشتر از عمق نهایی جلو نرو
    uint32_t remainingToTarget =
      (autoPulseCount < autoTargetPulses)
      ? (uint32_t)(autoTargetPulses - autoPulseCount)
      : 0;

    if (pulses > remainingToTarget)
      pulses = remainingToTarget;

    if (pulses > (uint32_t)chipBreakRemainingPulses)
      pulses = chipBreakRemainingPulses;

    autoPulseCount += (long)pulses;

    if (chipBreakRemainingPulses >= (long)pulses)
      chipBreakRemainingPulses -= (long)pulses;
    else
      chipBreakRemainingPulses = 0;

    autoDepth = calculateDepthFromPulses(
      autoPulseCount,
      parameters.pitch
    );

    if (autoDepth > parameters.depth)
      autoDepth = parameters.depth;

    // رسیدن به عمق نهایی
    if (autoPulseCount >= autoTargetPulses)
    {
      autoPulseCount = autoTargetPulses;
      autoDepth = parameters.depth;
      servoStop();
      returnAuto();
      return;
    }

    // پایان فاز Forward -> شروع Return برای شکستن براده
    if (chipBreakRemainingPulses <= 0)
    {
      chipBreakForwardPhase = false;
      chipBreakRemainingPulses = chipBreakBackPulses;
      servoStop();
    }
  }
  // ---------------------------------------------------
  // فاز برگشت CHIP BREAK
  // ---------------------------------------------------
  else
  {
    // فقط به اندازه براده‌شکن عقب برو
    if (pulses > (uint32_t)chipBreakRemainingPulses)
      pulses = chipBreakRemainingPulses;

    if (pulses > (uint32_t)autoPulseCount)
      pulses = (uint32_t)autoPulseCount;

    autoPulseCount -= (long)pulses;

    if (chipBreakRemainingPulses >= (long)pulses)
      chipBreakRemainingPulses -= (long)pulses;
    else
      chipBreakRemainingPulses = 0;

    autoDepth = calculateDepthFromPulses(
      autoPulseCount,
      parameters.pitch
    );

    if (autoDepth < 0.0)
      autoDepth = 0.0;

    // پایان فاز Return -> دوباره Forward
    if (chipBreakRemainingPulses <= 0)
    {
      chipBreakForwardPhase = true;
      chipBreakRemainingPulses = chipBreakForwardPulses;
      servoStop();
    }
  }

  showAuto();
}

// =====================================================
// AUTO CYCLE
// =====================================================

void autoCycle()
{
  if (autoState == AUTO_RUNNING)
  {
    if (torqueLimitReached())
    {
      stopAutoCycle();
      return;
    }

    autoForwardPulse();
    return;
  }

  if (autoState == AUTO_RETURNING)
  {
    autoReturnPulse();
    return;
  }
}

// =====================================================
// AUTO HANDLE CONTROL
// =====================================================

void autoHandleControl()
{
  bool button6 = (digitalRead(HANDLE_START) == LOW);
  bool button7 = (digitalRead(HANDLE_STOP) == LOW);

  // هر دو همزمان = هیچ فرمان حرکتی
  if (button6 && button7)
  {
    servoStop();
    stopReturnHeld = false;
    return;
  }

  // تشخیص فقط لبه فشرده شدن STOP
  bool newStopPress = button7 && !stopButtonPressed;
  stopButtonPressed = button7;

  // START فقط یک بار و فقط در حالت READY
  if (button6)
  {
    if (startEnabled && autoState == AUTO_READY)
      startAutoCycle();

    return;
  }

  // STOP
  if (button7)
  {
    // اولین فشار STOP در حین کار = فقط توقف
    if (newStopPress && autoState == AUTO_RUNNING)
    {
      stopAutoCycle();
      stopButtonTime = millis();
      return;
    }

    // فشار جدید STOP بعد از توقف = ورود به برگشت دستی
    if (newStopPress && autoState == AUTO_STOPPED)
    {
      autoState = AUTO_RETURNING;
      stopReturnHeld = true;
      showAuto();
      return;
    }

    // تا وقتی STOP نگه داشته شده، برگشت ادامه دارد.
    if (autoState == AUTO_RETURNING && stopReturnHeld)
    {
      autoReturnPulse();
      return;
    }

    return;
  }

  // STOP رها شد = برگشت باید فوراً متوقف شود.
  if (stopReturnHeld)
  {
    stopReturnHeld = false;
    servoStop();

    if (autoState == AUTO_RETURNING)
      autoState = AUTO_STOPPED;

    showAuto();
    return;
  }

  // پس از پایان سیکل و گذشت 2 ثانیه، START دوباره فعال شود.
  if (autoState == AUTO_FINISHED)
  {
    if (!startEnabled)
    {
      if (millis() - stopButtonTime >= 2000)
      {
        startEnabled = true;
        autoState = AUTO_READY;
        autoDepth = 0.0;
        autoPulseCount = 0;
        showAuto();
      }
    }
  }
}

// =====================================================
// AUTO CONTROL
// =====================================================

void autoControl()
{
  if (digitalRead(KEY_MENU) == LOW)
  {
    while (digitalRead(KEY_MENU) == LOW)
      delay(10);

    servoStop();

    autoMode = false;
    autoState = AUTO_READY;
    startEnabled = true;
    autoDepth = 0.0;
    autoPulseCount = 0;

    showMainMenu();
    delay(150);
    return;
  }

  autoHandleControl();

  // برگشت دستی در autoHandleControl انجام می‌شود.
  if (autoState == AUTO_RETURNING && stopReturnHeld)
    return;

  autoCycle();
}

// =====================================================
// SETTINGS CONTROL
// =====================================================

void settingsControl()

{
  if (editMode)
  {
    if (digitalRead(KEY_UP) == LOW)
    {
      editParameter();
      return;
    }

    if (digitalRead(KEY_DOWN) == LOW)
    {
      editParameter();
      return;
    }

    if (digitalRead(KEY_ENTER) == LOW)
    {
      while (digitalRead(KEY_ENTER) == LOW)
        delay(10);

      editMode = false;

      showSettings();

      delay(150);

      return;
    }

    if (digitalRead(KEY_MENU) == LOW)
    {
      while (digitalRead(KEY_MENU) == LOW)
        delay(10);

      editMode = false;

      parameters = savedParameters;

      showSettings();

      delay(150);


      return;
    }

    return;
  }

  if (digitalRead(KEY_UP) == LOW)
  {
    if (parameterPosition > 0)
      parameterPosition--;

    showSettings();

    delay(150);

    while (digitalRead(KEY_UP) == LOW)
      delay(10);

    return;
  }


  if (digitalRead(KEY_DOWN) == LOW)
  {
    if (parameterPosition < 9)
      parameterPosition++;

    showSettings();

    delay(150);

    while (digitalRead(KEY_DOWN) == LOW)
      delay(10);

    return;
  }

  if (digitalRead(KEY_ENTER) == LOW)
  {
    while (digitalRead(KEY_ENTER) == LOW)
      delay(10);

    if (parameterPosition == 9)
    {
      saveParameters();

      display.clearDisplay();

      display.setTextSize(2);
      display.setTextColor(KS0108_ON);

      display.setCursor(38, 25);
      display.print("SAVED");

      display.display();

      delay(1000);

      showSettings();

      return;
    }

    editMode = true;

    showSettings();

    delay(150);

    return;
  }

  if (digitalRead(KEY_MENU) == LOW)
  {
    while (digitalRead(KEY_MENU) == LOW)
      delay(10);

    settingsMode = false;

    parameters = savedParameters;

    showMainMenu();

    delay(150);


    return;
  }
}

// =====================================================
// MAIN MENU CONTROL
// =====================================================

void mainMenuControl()
{
  if (digitalRead(KEY_UP) == LOW)
  {
    if (menuPosition > 0)
      menuPosition--;

    showMainMenu();


    delay(150);

    while (digitalRead(KEY_UP) == LOW)
      delay(10);

    return;
  }

  if (digitalRead(KEY_DOWN) == LOW)
  {
    if (menuPosition < 2)
      menuPosition++;

    showMainMenu();

    delay(150);

    while (digitalRead(KEY_DOWN) == LOW)
      delay(10);

    return;
  }

  if (digitalRead(KEY_ENTER) == LOW)
  {
    while (digitalRead(KEY_ENTER) == LOW)
      delay(10);

    if (menuPosition == 0)
    {
      autoMode = true;

      autoState = AUTO_READY;

      startEnabled = true;

      autoDepth = 0.0;

      autoPulseCount = 0;

      showAuto();

    }

    else if (menuPosition == 1)
    {
      manualMode = true;

      manualSpeed = savedParameters.speed;

      manualDepth = 0.0;

      manualPulseCount = 0;

      handleWasPressed = false;

      showManual();
    }

    else if (menuPosition == 2)
    {
      settingsMode = true;


      editMode = false;

      parameterPosition = 0;

      parameters = savedParameters;

      showSettings();
    }

    delay(150);

    return;
  }
}

// =====================================================
// SETUP
// 

//=====================================================

void setup()
{
  Serial.begin(9600);

  // کلیدها
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_ENTER, INPUT_PULLUP);
  pinMode(KEY_MENU, INPUT_PULLUP);

  pinMode(HANDLE_START, INPUT_PULLUP);
  pinMode(HANDLE_STOP, INPUT_PULLUP);

  // سروو
  pinMode(SERVO_PULSE_PIN, OUTPUT);
  pinMode(SERVO_DIR_PIN, OUTPUT);

  pinMode(SERVO_ON_PIN, OUTPUT);

  digitalWrite(SERVO_PULSE_PIN, LOW);
  digitalWrite(SERVO_DIR_PIN, LOW);
  digitalWrite(SERVO_ON_PIN, LOW);

  setupTimer1();

  if (display.begin(KS0108_CS_ACTIVE_HIGH) == false)
  {
    Serial.println("GLCD initialization failed!");

    while (true)
    {
    }
  }

  display.clearDisplay();
  display.display();

  startupAnimation();


  loadParameters();

  showMainMenu();
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  if (manualMode)
  {
    manualControl();
    return;
  }


  if (autoMode)
  {
    autoControl();
    return;
  }

  if (settingsMode)
  {
    settingsControl();
    return;
  }

  mainMenuControl();
}
