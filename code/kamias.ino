//ito mga libary na ginamit natin para sa project
#include <Wire.h> //ito nakadependent si liquidCrystal_I2C, lahat ng I2C device (with SDA, SCL) need nito para gumana ng ayos
#include <LiquidCrystal_I2C.h> //library natin for the LCD with I2C
#include <Streaming.h> //para sa printing sa serial at lcd
#include <Chrono.h> //ito yung timer na ginamit natin, para maiwasan ang paggamit ng delay
#include <DHT.h>  //library para sa dht22
#include <Bounce2.h> //para iwasan yung pagbabasa ng noise sa signal sa button B


//pin kung saan nakakabit sa arduino
#define AC_MOTOR_F_PIN 37 
#define AC_MOTOR_R_PIN 39
#define HOPPER_PIN 41
#define FAN_PIN 43
#define PNEUMATIC_PIN 45
#define HEATER_PIN 11
#define Relay_extra_1 47
#define Relay_extra_1 33

#define DHTPIN 10
#define DHTTYPE DHT22

#define STEP_PIN 23
#define DIR_PIN 22
#define ENABLE_PIN 24

#define A_PIN A4
#define B_PIN A3
#define KAMIAS_SENSOR A1
#define CONVEYOR_SENSOR A0

#define LINEAR_A_PIN A14
#define LINEAR_B_PIN A13

#define LIMIT_UP_PIN A12
#define LIMIT_DOWN_PIN A11

#define INTERUPTOR_PIN A2

Bounce debouncer = Bounce();

LiquidCrystal_I2C lcd(0x27, 20, 4); //declaration ng LCD variable, with address na 0x27 i2c, 20 column at 4 row

//declaration ng mga timer
Chrono myChrono; //gamit sa main process
Chrono fanChrono; //gamit sa fan


int mode = 0; //para malaman kung saan part na tayo sa main process
int modeFloor = 0; //para malaman kng saan part na tayo ng process ng switching floor
int initializeMode = 0; //para malaman kung saan part na tayo sa process of initializing
int homingMode = 0; //para malaman kung saan part na tayo sa homing proccess ng conveyor

unsigned long stepCnt = 0; //ito nilalagay natin yung step count natin sa conveyor simula sa home position, unsigned siya para imbis yung value ay -32,768 to 32,767 ay magiging 0 to 65,535, wala siya negative value
int kamiasCnt = 0; //dito iistore kung pang ilang kamias na nalagay sa conveyor
int layerCnt = 0; //dito iistore kung ilang layer na sa container
int floorCnt = 0; //dito iistore kung pang ilang floor na

// dito iistore yung count down sa heating
int sec;
int min;
int hr;

//step per slot sa conveyor simula sa home position
int slotSteps[10] = { 0, 300, 600, 900, 1200, 1500, 1800, 2100, 2400, 2700 };

DHT dht(DHTPIN, DHTTYPE); //declaration ng DHT, DHT ay yung pagkuha natin ng temperature

float temp; //dito iistore yung temp

bool inPaused = false; //dito iistore kung nakapause siya

void setup() {
  Serial.begin(9600); //para gumana yung serial natin para sa mga logs and control
  Serial.setTimeout(100); //set timeout to 100ms para hindi ganoon kalaki yung delay kung may sinend tayo value

//declaration of pin kung siya output, input, or input_pullup
  pinMode(AC_MOTOR_F_PIN, OUTPUT);
  pinMode(AC_MOTOR_R_PIN, OUTPUT);
  pinMode(HOPPER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PNEUMATIC_PIN, OUTPUT);
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(LINEAR_A_PIN, OUTPUT);
  pinMode(LINEAR_B_PIN, OUTPUT);

  pinMode(LIMIT_UP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_DOWN_PIN, INPUT_PULLUP);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);

  pinMode(KAMIAS_SENSOR, INPUT);
  pinMode(CONVEYOR_SENSOR, INPUT);

  pinMode(INTERUPTOR_PIN, INPUT);

  pinMode(A_PIN, INPUT_PULLUP);
  pinMode(B_PIN, INPUT_PULLUP);

//since yung button B nakakaproblema sa noise kung nagoopen yung AC, doon namin nilagay yung bouncer
  debouncer.attach(B_PIN);
  debouncer.interval(50); //kung pag on niya ay less than 50ms neglect na yung reading

  allOff(); //patayin lahat ng motor, at nakaretract lahat ng linear
  dht.begin(); //initialize DHT
  Wire.begin(); //initialize I2C
  lcd.begin(); //initialize LCD
  lcd.backlight();
  lcd.setCursor(0, 0), lcd << F("====================");
  lcd.setCursor(0, 1), lcd << F("       KAMIAS       ");
  lcd.setCursor(0, 2), lcd << F(" CUTTING AND DRYING ");
  lcd.setCursor(0, 3), lcd << F("====================");
  delay(2000);
  mode = 30; //sa mode 30 magsisimula kasi yun ang initialization
}

float temperature() { //function pagkuha ng temperature galing sa DHT22 module
  return dht.readTemperature();
}

void loop() { // sa void loop, yung normal process lang dapat nagana
  normalProccess();
}

//function natin pra sa pause, kung nakapause iintayin niya mapindot ang 'A' Button, tas kung hindi ay iintayin niya mapindot ang 'B' para ipause yung button
//kung pause, nakapause din yung chrono natin, tas patay lahat ng motor natin, pag naresume, ireresume din natin yung count ng timer natin
bool onPause() { 
  if (inPaused) {
    if (isAPressed()) {
      lcd.setCursor(0, 0), lcd << F("====================");
      lcd.setCursor(0, 1), lcd << F("     ON PROCCESS    ");
      lcd.setCursor(0, 2), lcd << F("    'B' to Pause    ");
      lcd.setCursor(0, 3), lcd << F("====================");
      if (mode == 9) {
        if (layerCnt >= 6) {
          motorForward();
        }
      } else if (mode == 3) {
        hopperOn();
      }
      inPaused = false;
      myChrono.resume();
      fanChrono.resume();
    }
  } else if (mode == 30 || mode == 7) {

  } else {
    if (isBPressed()) {
      modeFloor = 0;
      inPaused = true;
      myChrono.stop();
      fanChrono.stop();
      hopperOff();
      acMotorOff();
      fanOff();
      heaterOff();
      motorStop();
    }
  }
  return inPaused;
}

void normalProccess() {
  if (onPause()) { //kung nakapause siya ito lalabas natin sa lcd
    lcd.setCursor(0, 0), lcd << F("====================");
    lcd.setCursor(0, 1), lcd << F("  ON SYSTEM PAUSE   ");
    lcd.setCursor(0, 2), lcd << F("  'A' to Continue   ");
    lcd.setCursor(0, 3), lcd << F("====================");
    return;
  }
  switch (mode) { 
    case 0: //check kung napindot na yung A bago pumunta sa next process
      lcd.setCursor(0, 0), lcd << F("====================");
      lcd.setCursor(0, 1), lcd << F("    PRESS BUTTON    ");
      lcd.setCursor(0, 2), lcd << F("    'A' TO START    ");
      lcd.setCursor(0, 3), lcd << F("====================");

      if (isAPressed()) { //pag napindot punta na sa mode 1 tas display yung value
        mode = 1;
        lcd.setCursor(0, 0), lcd << F("====================");
        lcd.setCursor(0, 1), lcd << F("     ON PROCCESS    ");
        lcd.setCursor(0, 2), lcd << F("    'B' to Pause    ");
        lcd.setCursor(0, 3), lcd << F("====================");
        myChrono.restart();
      }
      break;

    case 1: //iextend yung linear actuator na 18sec
      motorForward();
      if (myChrono.hasPassed(18000)) {
        motorStop();
        mode = 2;
      }
      break;

    case 2: //buhayin yung hopper
      hopperOn();
      mode = 3;
      break;

    case 3: //kung manadetect na nahulog na kamias, patayin na yung hopper tas next process na tayo
      if (isKamiasDetected()) {

        hopperOff();
        mode = 4;
      }
      break;

    case 4: //update yung count natin sa kamias for that conveyor tas check kung natapos na yung last slot sa conveyor para malaman kung ano sunod na process na gagawin
      kamiasCnt++;
      if (kamiasCnt >= 10) {
        kamiasCnt = 0;
        mode = 7;
        hopperOff();
        delay(1000);
        myChrono.restart();
      } else {
        mode = 5;
      }
      break;

    case 5: //imove yung conveyor sa sunod na slot
      if (nextSlot(kamiasCnt)) {
        mode = 2;
      }
      break;

    case 7: //extend natin yung cutter for 2sec tas after nun ay babalik na for 2sec ulit
      pneumaticExtend();
      if (myChrono.hasPassed(2000)) {
        pneumaticRetract();
        myChrono.restart();
        delay(2000);
        mode = 8;
      }
      break;

    case 8: //extend ulit yung conveyor for 10sec tas update yung count ng layer tas check kung tapos na lahat ng layer para malaman kung ano sunod na process na gagawin
      motorForward();
      if (myChrono.hasPassed(10000)) {
        motorStop();
        mode = 9;
        layerCnt++;
        if (layerCnt >= 6) {
          motorForward();
          myChrono.restart();
        }
      }
      break;

    case 9: //ihome ulit yung conveyor
      if (homingConveyor()) {
        mode = 10;
      }
      break;

    case 10: //check ulit kung ilang layer na natapos para malaman kung ano sunod na process ang gagawin
      Serial << "layerCnt :: " << layerCnt << endl;
      if (layerCnt >= 6) {
        layerCnt = 0;
        mode = 11;

      } else {
        mode = 2;
      }
      break;

    case 11: //extend yung linear actuator hanggang 45sec
      motorForward();
      if (myChrono.hasPassed(45000)) {
        myChrono.restart();
        motorStop();
        mode = 12;
        delay(1000);
      }
      break;

    case 12: //rectract na ulit yung linear actuator to 120sec
      motorBackward();
      if (myChrono.hasPassed(120000)) {
        motorStop();
        mode = 13;
        modeFloor = 0;
      }
      break;

    case 13: //check kung ilan floor na natapos para malaman kung poproceed na sa heating
      if (floorCnt >= 4) {
        floorCnt = 0;
        mode = 20;
      } else {
        myChrono.restart();
        mode = 14;
      }
      break;

    case 14: //go to next floor tas update yung counter ng floor
      if (nextFloor()) {
        myChrono.restart();
        motorStop();
        mode = 15;
        floorCnt++;
      }

      break;

    case 15: //sa simula ng floor extend muna yung linear ng 18sec
      motorForward();
      if (myChrono.hasPassed(18000)) {
        motorStop();
        mode = 2;
      }
      break;

    case 20: //iready for heating, make sure lang yung value ay naka 8hr tas nakarestart yung timer
      mode = 21;
      myChrono.restart();
      sec = 0;
      min = 0;
      hr = 8;
      lcd.clear();
      break;

    case 21: //kunin yung temperature tas every 1 second ay inuupdate yung countdown, kung natapos ang countdown punta na sa sunod na process, may fan controller din tayo dito, para hindi direderetso buhay si fan
      temp = temperature();
      if (isnan(temp)) {
        return;
      }
      fanControl();
      if (temp < 70) {
        heaterOn();
      } else if (temp > 75) {
        heaterOff();
      }

      if (myChrono.hasPassed(1000)) {
        myChrono.restart();
        --sec;
        if (sec < 0) {
          sec = 59;
          --min;
          if (min < 0) {
            min = 59;
            --hr;
            if (hr <= 0) {
              heaterOff();
              mode = 30;
            }
          }
        }
      }
      lcd.setCursor(0, 0), lcd << F("====================");
      lcd.setCursor(0, 1), lcd << F("       DRYING       ");
      lcd.setCursor(0, 2), lcd << F("TIME : ") << intToStringWithLeadingZero(hr) << F(":") << intToStringWithLeadingZero(min) << F(":")
                               << intToStringWithLeadingZero(sec) << F("  ");
      lcd.setCursor(0, 3), lcd << "TEMP : " << temp << "C     ";
      break;

    case 30:
      if (initialize()) {
        mode = 0;
      }
      break;
  }
}

void fanControl() { //fan Controll natin na gagawin ay pag buhay si fan mamatay siya in 30sec tas kung patay ay mabubuhay siya in 10sec
  if (isFanOn()) {
    if (fanChrono.hasPassed(30000)) {
      fanChrono.restart();
      fanOff();
    }
  } else {
    if (fanChrono.hasPassed(10000)) {
      fanChrono.restart();
      fanOn();
    }
  }
}

String intToStringWithLeadingZero(int num) {  //gagawin lang ay yung integer ay gagawin string tas kung 1digit lang siya ay lalagyan ng leading zero
  if (num < 10) {
    // If the number is one-digit, add a leading zero
    return "0" + String(num);
  } else {
    return String(num);
  }
}

bool homingConveyor() { //ito function natin sa pag hohome ng conveyor
  switch (homingMode) {
    case 0: //imomove natin si conveyor hanggang may madedetect si sensor
      if (isOnConveyorHome()) {
        homingMode = 1;
      } else {
        wakeStepper();
        moveStepperForward(0.25); //0.25 para mabilis yung ikot ng conveyor, yung 1 sec yung normal speed natin pag nag lalagay tayo sa slot
      }
      break;

    case 1: //move natin yung conveyor in normal speed na hanggang mawala yung sense niya sa sensor
      if (!isOnConveyorHome()) {
        homingMode = 2;
      } else {
        moveStepperForward(1);
      }
      break;


    case 2: //pabalik na yung move niya tas sobrang bagal niya ng move, ginagawa natin buong process na ito para sure tayo na sa same side talaga tigil ni conveyor kasi si sensor medyo mataba din yung sensing niya

      if (isOnConveyorHome()) {
        homingMode = 3;
      } else {
        moveStepperBackward(3);
      }

      break;

    case 3: //check lang ulit kung may nasense si conveyor bago natin imark as done

      if (isOnConveyorHome()) {
        homingMode = 0;
        stepCnt = 0;
        return true;
      } else {
        moveStepperBackward(3);
      }

      break;
  }

  return false;
}
bool initialize() { //ito ang function natin sa pag initialize ng machine, pag nagreturn true siya meaning tapos na yung pagiinitialize
  if (initializeMode == 0) {  //patayin muna natin lagay ng motor at actuator
    myChrono.restart();
    initializeMode = 1;
    allOff();
    lcd.setCursor(0, 0), lcd << F("====================");
    lcd.setCursor(0, 1), lcd << F("    INITIALIZING    ");
    lcd.setCursor(0, 2), lcd << F("    PLEASE WAIT     ");
    lcd.setCursor(0, 3), lcd << F("====================");
  } else if (initializeMode == 1) { //pabalikin muna yung linear actuator, para matapos yung pagbalik dapat natapos yung 120sec or napindot yung B button (dapat yung operator ay aware na pipindutin lang niya yung B button sa initalization kung sure siya fully retracted na yung linear)
    Serial << "Motor Back" << endl;
    motorBackward();
    if (myChrono.hasPassed(120000) || isBPressed()) {
      initializeMode = 2;
    }
  } else if (initializeMode == 2) { //make sure ulit na patay lang ng motor and actuator kasi nabuhay natin yung linear
    Serial << "All Off" << endl;
    allOff();
    initializeMode = 3;
  } else if (initializeMode == 3) { //itataas natin yung machine hanggang mapindot yung limit switch
    Serial << "MOtor Up" << endl;
    if (isOnLimitUp()) {
      acMotorOff();
      initializeMode = 4;
      delay(2000);
    } else {
      acMotorUp();
    }
  } else if (initializeMode == 4) {  //ibaba natin yung machine hanggang sa 1st floor
    Serial << "Going Next Floor" << endl;
    if (nextFloor()) {
      initializeMode = 5;
      floorCnt = 0;
      Serial << "Homing" << endl;
      delay(1500);
    }
  } else if (initializeMode == 5) { //home natin yung conveyor after yun, return na natin na true or tapos na yung initialize
    if (homingConveyor()) {
      initializeMode = 0;
      return true;
    }
  }

  return false;
}

bool nextFloor() {//ito function para pumunta si machine sa next floor
  if (isOnLimitDown()) { //hindi dapat siya aabot dito, icheck lang natin na pag nahit yung limit switch sa baba, patayin na agad natin, maiistuck siya dito sa part na ito kung sakali mangyari ito
    acMotorOff();
    return false;
  }
  if (modeFloor == 0) { //ibaba natin yung machine hanggang wala nadedetect si photo interuptor
    acMotorDown();
    if (!onInterupt()) {
      modeFloor = 1;
    }
  } else if (modeFloor == 1) { //ibaba natin yung machine hanggang may nadetect si photo interuptor tas return true at tapos na yung process
    acMotorDown();
    if (onInterupt()) {
      acMotorOff();
      modeFloor = 0;
      return true;
    }
  }
  return false;
}
//ito mga function na ginawa natin para masmadali maintindihan yung code, kasi yung digitalRead, digitalWrite yung mismo ilalagay natin, medyo nakakalito siya
bool onInterupt() {
  return digitalRead(INTERUPTOR_PIN);
}
bool isOnLimitUp() {
  return !digitalRead(LIMIT_UP_PIN);
}

bool isOnLimitDown() {
  return !digitalRead(LIMIT_DOWN_PIN);
}


bool isAPressed() {
  return !digitalRead(A_PIN);
}

bool isBPressed() {
  debouncer.update();
  return debouncer.fell();
}

bool isKamiasDetected() {
  return digitalRead(KAMIAS_SENSOR);
}

bool isOnConveyorHome() {
  return digitalRead(CONVEYOR_SENSOR);
}


void pneumaticExtend() {
  digitalWrite(PNEUMATIC_PIN, LOW);
}

void pneumaticRetract() {
  digitalWrite(PNEUMATIC_PIN, HIGH);
}

void hopperOn() {
  digitalWrite(HOPPER_PIN, LOW);
}

void hopperOff() {
  digitalWrite(HOPPER_PIN, HIGH);
}

void acMotorDown() {
  if (!isOnLimitDown()) {
    digitalWrite(AC_MOTOR_R_PIN, HIGH);
    digitalWrite(AC_MOTOR_F_PIN, LOW);

  } else {
    digitalWrite(AC_MOTOR_F_PIN, HIGH);
    digitalWrite(AC_MOTOR_R_PIN, HIGH);
  }
}

void acMotorUp() {
  if (!isOnLimitUp()) {
    digitalWrite(AC_MOTOR_F_PIN, HIGH);
    digitalWrite(AC_MOTOR_R_PIN, LOW);
  } else {
    digitalWrite(AC_MOTOR_R_PIN, HIGH);
    digitalWrite(AC_MOTOR_F_PIN, HIGH);
  }
}

void acMotorOff() {
  digitalWrite(AC_MOTOR_R_PIN, HIGH);
  digitalWrite(AC_MOTOR_F_PIN, HIGH);
}
bool isFanOn() {
  return !digitalRead(FAN_PIN);
}

void fanOn() {
  digitalWrite(FAN_PIN, LOW);
}

void fanOff() {
  digitalWrite(FAN_PIN, HIGH);
}

void heaterOn() {
  digitalWrite(HEATER_PIN, HIGH);
}

void heaterOff() {
  digitalWrite(HEATER_PIN, LOW);
}

void sleepStepper() {
  digitalWrite(ENABLE_PIN, HIGH);
}

void wakeStepper() {
  digitalWrite(ENABLE_PIN, LOW);
}

void stepperCW() {
  digitalWrite(DIR_PIN, HIGH);
}

void stepperCCW() {
  digitalWrite(DIR_PIN, LOW);
}
bool nextSlot(int slot) {
  long myStep = slotSteps[slot] * 100L;
  if (stepCnt == myStep) {
    return true;
  } else if (stepCnt > myStep) {
    moveStepperBackward(1);
  } else if (stepCnt < myStep) {
    moveStepperForward(1);
  }
  return false;
}

void moveStepperForward(float speed) {
  wakeStepper();
  stepperCCW();
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(50 * speed);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(50 * speed);
  stepCnt++;
}

void moveStepperBackward(int speed) {
  wakeStepper();
  stepperCW();
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(50 * speed);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(50 * speed);
  --stepCnt;
}

void motorStop() {
  digitalWrite(LINEAR_A_PIN, LOW);
  digitalWrite(LINEAR_B_PIN, LOW);
}

void motorForward() {
  digitalWrite(LINEAR_A_PIN, HIGH);
  digitalWrite(LINEAR_B_PIN, LOW);
}

void motorBackward() {
  digitalWrite(LINEAR_A_PIN, LOW);
  digitalWrite(LINEAR_B_PIN, HIGH);
}

void allOff() {
  pneumaticRetract();
  hopperOff();
  acMotorOff();
  fanOff();
  heaterOff();
  motorStop();
}
