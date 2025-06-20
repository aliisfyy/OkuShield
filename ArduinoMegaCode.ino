#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include "SevSeg.h"
SevSeg sevseg;

float displayTimeSecs = 1; 
float displayTime = (displayTimeSecs * 5000);
long startNumber = 0; 
long endNumber = 9999; 

// card id = BD D8 30 21
// Nama = AlexanderBinHudson
// ic = 060229-10-9876

// Pin 
const int trigPin = 7;
const int echoPin = 6;
const int ledGreenPin = 2;    // green LED pin
const int ledRedPin = 3;      // red LED pin
const int buzzerPin = 4;      // Buzzer pin 
const int SS_PIN = 53;        // Slave Select pin for RFID (UNO)
const int RST_PIN = 5;        // Reset pin for RFID (UNO)
const int button_pin = 42;

// 7-segment display pin configuration
int pinA = 32, pinB = 28, pinC = 25, pinD = 23;
int pinE = 22, pinF = 31, pinG = 26;
int pinDP = 24;
int D1 = 33, D2 = 30, D3 = 29, D4 = 27;

long duration;
int distance;
int currentTotalCar;
int prevTotalCar = 0;
int period = 0;
unsigned long startTime = 0;      // Store the time when the object is detected
bool timerActive = false;         // To track if the timer is running
bool buzzerActive = false;        // To track if the buzzer has been activated
bool rfidAuthenticated = false;   // To track if RFID card is authenticated
bool cardMatch;
bool parkDurOn;
bool carIn;
bool buttonOn;
String nameold, namenew;

// Initialize the LCD
LiquidCrystal_I2C lcd(0x27,  16, 2);

// RFID instance
MFRC522 rfid(SS_PIN, RST_PIN);

unsigned long buzzerStartTime = 0;           // Buzzer timing
const unsigned long buzzerInterval = 500;    // 0.5 second interval for the siren effect

//*****************************************************************************************//
void setup() {
  //Ultrasonic Sensor pin
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(ledRedPin, OUTPUT);     // Set the Red LED pin as output
  pinMode(ledGreenPin, OUTPUT);   // Set the Green LED pin as output
  pinMode(buzzerPin, OUTPUT);     // Set the buzzer pin as output

  //Serial
  Serial.begin(9600);
  pinMode(button_pin, INPUT_PULLUP);

  //LCD
  lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();

  //SPI
  SPI.begin();            // Init SPI bus
  rfid.PCD_Init();        // Init MFRC522 module

  //OKUshield activate
  Serial.println("Park and scan to start");
  buttonOn = false;

  // SEVSEG
  // Pin configuration for 7-segment display
  byte numDigits = 4;
  byte digitPins[] = {D1, D2, D3, D4};  // Use previously defined pin numbers
  byte segmentPins[] = {pinA, pinB, pinC, pinD, pinE, pinF, pinG, pinDP};  // Use previously defined pin numbers
  
  bool resistorsOnSegments = false;
  byte hardwareConfig = COMMON_ANODE;
  bool updateWithDelays = false;
  bool leadingZeros = true;
  bool disableDecPoint = true;
  
  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments,
               updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(2);  // brightness
}

//*****************************************************************************************//
void loop() {
  // Initial LCD message
  lcd.setCursor(0, 0);

  //Ultrasonic sensor read
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  // Print the distance in the Serial Monitor
  //Serial.print("Distance: ");
  //Serial.println(distance);

  //-------------------------------------------

  // Object detection logic
  if (distance < 10) {
    if (!timerActive && !rfidAuthenticated) {   // Only start the timer if RFID is not authenticated
      startTime = millis();                     // Start the timer
      timerActive = true;                       // Timer Active
      buzzerActive = false;                     // Reset buzzer state
      digitalWrite(ledRedPin, HIGH);            // Turn on the LED
      buzzerStartTime = millis();               // Start buzzer timing
    }
    
    //Timer Start
    if (timerActive) {
      unsigned long currentTime = millis() - startTime; // Calculate elapsed time

      // SEVSEG
      if (startNumber <= endNumber) {
        // Count-up logic
        for (long i = 0; i <= displayTime; i++) {
          sevseg.setNumber(startNumber, 0);  // Display current number on 7-segment display
          sevseg.refreshDisplay();
        } 
        startNumber++;  // Increment the start number
      }
      

      // Display time on the LCD only if it changes
      static unsigned long lastDisplayedTime = 0;
      if (currentTime / 1000 != lastDisplayedTime) {
        lastDisplayedTime = currentTime / 1000;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Scan Your Card");
      }

      // Check if 6 seconds have passed to activate the buzzer with siren sound
      if (currentTime / 1000 >= 6 && !buzzerActive) {
        buzzerActive = true; // Set buzzer state to active
        //Serial.println("Buzzer activated!");
      }

      // Siren effect logic with 0.5-second alternating
      if (buzzerActive) {
        unsigned long currentMillis = millis();
        unsigned long elapsedBuzzerTime = currentMillis - buzzerStartTime;

        // Calculate the time within the 1-second cycle
        if ((elapsedBuzzerTime % (2 * buzzerInterval)) < buzzerInterval) {
          tone(buzzerPin, 1500); // Frequency of 1000 Hz for 0.5 seconds
        } else {
          noTone(buzzerPin); // Turn off buzzer for 0.5 seconds
        }
      }
    }
  } else {
    // Reset everything only if the object moves out of range
    if (distance >= 10) {
      timerActive = false; // Stop the timer
      rfidAuthenticated = false; // Reset RFID status
      parkDurOn = false;
      noTone(buzzerPin);     // Turn off the buzzer
      digitalWrite(ledGreenPin, LOW); // Turn off the LED
      digitalWrite(ledRedPin, LOW); // Turn off the RFID LED
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("    Welcome!   ");
      lcd.setCursor(0, 1);
      lcd.print(" Scan your card ");
      delay(1000);

      //SEVSEG
      startNumber = 0; // Reset counter when no car is detected
      sevseg.setNumber(0, 0);
      sevseg.refreshDisplay();

      // Print the reset message in the Serial Monitor
      //Serial.println("System reset due to object removal");
    }
  }

  //-------------------------------------------

  if (cardMatch && distance < 10 && !carIn){
  carIn = true;
  }
    if (carIn && distance >= 10)
    {

    Serial.print("Parking Duration :");
    Serial.print(period);
    Serial.println("s");
    Serial.println("-------------------------------------------");
    carIn = false;
    }

  //-------------------------------------------
  //parking duration
  if (parkDurOn) {
    unsigned long currentTime = millis() - startTime; // Calculate elapsed time

    int hours = period / 30;
    long fee = (hours + 1)*2;
    // Button
    byte buttonState = digitalRead(button_pin);

    if (buttonState == 1 && cardMatch && rfidAuthenticated && distance < 10) {
      static unsigned long lastDisplayedTime = 0;
    if (currentTime / 1000 != lastDisplayedTime) {
      lastDisplayedTime = currentTime / 1000;
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("    Welcome");

      lcd.setCursor(0, 1);
      lcd.print(nameold);
      period= lastDisplayedTime;
      
       if (startNumber <= endNumber) {
        // Count-up logic
        for (long i = 0; i <= displayTime; i++) {
          sevseg.setNumber(startNumber, 0);  // Display current number on 7-segment display
          sevseg.refreshDisplay();
        } 
        startNumber++;  // Increment the start number
      }
    }
    }
    else {
    // Display time on the LCD only if it changes
    lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Toll Fee: ");
      lcd.print(fee);
      lcd.print(" MYR");

       if (startNumber <= endNumber) {
        // Count-up logic
        for (long i = 0; i <= displayTime; i++) {
          sevseg.setNumber(startNumber, 0);  // Display current number on 7-segment display
          sevseg.refreshDisplay();
        } 
        startNumber++;  // Increment the start number
      }

      // Print the timer value in the Serial Monitor
    //Serial.print("Timer: ");
    //Serial.print(currentTime / 1000); // Show seconds
    //Serial.println(" s");
  }
  }
  //-------------------------------------------

  if ( ! rfid.PICC_IsNewCardPresent()) return;  // Reset the loop if no new card presen
  if ( ! rfid.PICC_ReadCardSerial())   return;

  // RFID Detection
  //Serial.print(F("UID tag :"));
  String content = " ";
  for (byte i = 0; i < rfid.uid.size; i++){
    //Serial.print(rfid.uid.uidByte[i], HEX);
    content.concat(String(rfid.uid.uidByte[i], HEX));
  }
  
  Serial.println();
  content.toUpperCase();
  if ((content.substring(1) == "BDD83021")){
    cardMatch = true;
    tone(buzzerPin, 1500);
  }
  else{
    cardMatch = false;
    tone(buzzerPin, 1500);
  }
  
  //-------------------------------------------

  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  byte block;
  byte len;
  MFRC522::StatusCode status;

  //-------------------------------------------

  Serial.println("-------------------------------------------");
  Serial.print("#");
  Serial.println(currentTotalCar + 1);
  
  // Name
  Serial.print("Name: ");

  byte buffer1[18];
  block = 4;
  len = 18;

  // Get User Name
  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(rfid.uid));
  status = rfid.MIFARE_Read(block, buffer1, &len);

  // Print User Name
  String name = " ";
  for (uint8_t i = 1; i < 17; i++)
  {
    if (buffer1[i] != 32)
    {
      Serial.write(buffer1[i]);
      name.concat(String(rfid.uid.uidByte[i], HEX));
      namenew = char(buffer1[i]);
      nameold = nameold + namenew;
    }
    content.toUpperCase();
  }
  Serial.println();
  
  //-------------------------------------------

  // IC
  Serial.println("IC Num: ");
  
  byte buffer2[18];
  block = 1;

  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 1, &key, &(rfid.uid));
  status = rfid.MIFARE_Read(block, buffer2, &len);

  //PRINT LAST NAME
  for (uint8_t i = 0; i < 16; i++) {
    Serial.write(buffer2[i]);
  }
  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  //----------------------------------------

  // Card Match 
  if (cardMatch) {
    rfidAuthenticated = true;             // Set RFID authenticated to true
    digitalWrite(ledGreenPin, HIGH);      // Turn on the new LED for RFID
    digitalWrite(ledRedPin, LOW);
    currentTotalCar = prevTotalCar + 1;
    prevTotalCar = currentTotalCar;
    noTone(buzzerPin);

    timerActive = false;                  // Set timer off

    lcd.setCursor(0, 0);
    lcd.print("Card Authorised!");
    delay(1500);

    parkDurOn = true;
  }
  else {
    Serial.println("Wrong Card");                      // LCD print
    lcd.setCursor(0, 1);
    lcd.print(" Wrong Card ");
    parkDurOn = false;
    noTone(buzzerPin);
  }
  Serial.print("test");
  Serial.print(nameold);

  //-------------------------------------------
  // data log print

  // Short delay to prevent rapid updates
delay(1000);
}
