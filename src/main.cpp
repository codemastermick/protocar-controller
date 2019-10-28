#include <Arduino.h>

#include <printf.h>
#include <SPI.h>
#include "RF24.h"

byte addresses[][6] = {"cntlr", "robot"};
/****************** User Config ***************************/
/***      Set this radio as radio number 0 or 1         ***/
bool radioNumber = 0;
/* Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins D9 & D10 */
RF24 radio(9, 10); // CE, CSN
/**********************************************************/

/**
* Create a data structure for transmitting and receiving data
* This allows many variables to be easily sent and received in a single transmission
* See http://www.cplusplus.com/doc/tutorial/structures/
*/
struct controlStruct
{
  int speed;       // Between -512 and 512
  int bearing;     // Between -512 and 512
  bool honk;       // True if pressed
  bool headlights; // True to activate headlights
} controlPackage;  // Control instructions from remote

#define DRIVE_PIN A0   // up/down axis
#define TURNING_PIN A1 // left/right axis
#define LIGHTS_PIN 7   // headlight control
#define HORN_PIN 8     // joystick button

int xPosition = 0;
int yPosition = 0;
int hornState = 0;
int mapX = 0;
int mapY = 0;
int lightState = 0;

bool hasInput = false;
int inputThreshold = 50;

bool headlightsActive = false;

void getValues()
{
  xPosition = map(analogRead(DRIVE_PIN), 0, 1023, -512, 512);   // remap the values before packing them
  yPosition = map(analogRead(TURNING_PIN), 0, 1023, -512, 512); // remap the values before packing them
  hornState = digitalRead(HORN_PIN);
  lightState = digitalRead(LIGHTS_PIN);

  if (xPosition > -inputThreshold && xPosition < inputThreshold &&
      yPosition > -inputThreshold && yPosition < inputThreshold &&
      hornState == 1 && lightState == 0)
  {
    hasInput = false;
  }
  else
  {
    hasInput = true;
  }
}

void setupPackage(int x, int y, int horn)
{
  if (x < -inputThreshold || x > inputThreshold)
  {
    controlPackage.speed = x;
  }
  else
  {
    controlPackage.speed = 0;
  }

  if (y < -inputThreshold || y > inputThreshold)
  {
    controlPackage.bearing = y;
  }
  else
  {
    controlPackage.bearing = 0;
  }

  if (horn == 1)
  {
    controlPackage.honk = horn;
  }
  else
  {
    controlPackage.honk = 0;
  }

  if (lightState == 1)
  {
    headlightsActive = !headlightsActive;
  }
  controlPackage.headlights = headlightsActive;
}

void printValues()
{
  Serial.print("Direction: ");
  Serial.print(controlPackage.speed);
  Serial.print(" | Bearing: ");
  Serial.print(controlPackage.bearing);
  Serial.print(" | Honk: ");
  Serial.print(controlPackage.honk);
  Serial.print(" | Lights: ");
  Serial.println(controlPackage.headlights);
}

void sendPackage()
{
  radio.stopListening(); // First, stop listening so we can talk.

  Serial.print("Now sending:\t\t");
  printValues();
  if (!radio.write(&controlPackage, sizeof(controlPackage)))
  {
    Serial.println();
    Serial.println("**********************");
    Serial.println("*   FAILED TO SEND   *");
    Serial.println("**********************");
  }

  radio.startListening();                      // Now, continue listening
  unsigned long started_waiting_at = micros(); // Set up a timeout period, get the current microseconds
  boolean timeout = false;                     // Set up a variable to indicate if a response was received or not

  while (!radio.available())
  { // While nothing is received
    if (micros() - started_waiting_at > 200000)
    { // If waited longer than 200ms, indicate timeout and exit while loop
      timeout = true;
      break;
    }
  }

  if (timeout)
  { // Describe the results
    Serial.println("Failed, response timed out.");
  }
  else
  {
    // Grab the response, compare, and send to debugging spew
    radio.read(&controlPackage, sizeof(controlPackage));
    Serial.print("Got response:\t\t");
    printValues();
  }

  // Try again 1s later
  delay(250);
}

void configureRadio()
{
  radio.begin();

  // Set the PA Level low to prevent power supply related issues since this is a
  // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);

  // Open a writing and reading pipe on each radio, with opposite addresses
  if (radioNumber)
  {
    radio.openWritingPipe(addresses[0]);
    radio.openReadingPipe(0, addresses[1]);
  }
  else
  {
    radio.openWritingPipe(addresses[1]);
    radio.openReadingPipe(1, addresses[0]);
  }

  // Start the radio listening for data
  radio.startListening();
  radio.stopListening();
  radio.printDetails();
}

void setup()
{
  Serial.begin(9600);
  pinMode(DRIVE_PIN, INPUT);
  pinMode(TURNING_PIN, INPUT);
  pinMode(HORN_PIN, INPUT_PULLUP);
  pinMode(LIGHTS_PIN, INPUT_PULLUP);

  printf_begin();
  configureRadio();
}

void loop()
{
  getValues();
  setupPackage(xPosition, yPosition, !hornState);
  sendPackage();
  Serial.println();
}