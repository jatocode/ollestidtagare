#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

Adafruit_7segment matrix = Adafruit_7segment();

unsigned long timelapse = 0;
unsigned long start = 0;

// För att inte trigga massa gånger när "strålen bryts"
unsigned long lastDistChange = 0;
const unsigned int graceTime = 1000;
// Man måste röra sig minst såhär mycket i mm
const unsigned int minDiff = 300;
unsigned long lastDistance = 0;

// Vet inte hur jag ska presentera, men vi kan ju räkna ändå
int varv = 0;

// Blinkenlights
unsigned long blink = 0;
int led = LOW;

void setup() { 
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT); 

    matrix.begin(0x70);

    // Bitmasks som skriver Olle på segmenten
    matrix.writeDigitRaw(0, 1 + 2 + 4 + 8 + 16 + 32);
    matrix.writeDigitRaw(1, 8 + 16 + 32);
    matrix.writeDigitRaw(3, 8 + 16 + 32);
    matrix.writeDigitRaw(4, 1 + 8 + 16 + 32 + 64);
    matrix.writeDisplay();

    start = millis();

}

void loop() {
    // Avståndet blir tiden * ljudhastigheten delat med två
    int distance = readUltrasonicDistance(26, 27) * 0.340 / 2;  

    if (startTimer(distance)) {
        timelapse = millis() - start;
        Serial.println(timelapse);
        Serial.println(varv);
    }

    // Blinka lite så vi vet att vi lever
    if (millis() - blink > 500) {
        blink = millis();
        led = led == LOW ? HIGH : LOW;
        Serial.print("Avstånd:" );
        Serial.println(distance);
    }

    digitalWrite(LED_BUILTIN, led);
    delay(200);
}

bool startTimer(int dist) {
    int ddiff = dist - lastDistance;
    lastDistance = dist;
    if (abs(ddiff) >= minDiff) {
        unsigned long tdiff = millis() - lastDistChange;
        if (tdiff >= graceTime) {
            lastDistChange = millis();
            varv++;
            return true;
        }
    }
    return false;
}

long readUltrasonicDistance(int triggerPin, int echoPin) {
    pinMode(triggerPin, OUTPUT);  // Clear the trigger
    digitalWrite(triggerPin, LOW);
    delayMicroseconds(2);
    // Sets the trigger pin to HIGH state for 10 microseconds
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(triggerPin, LOW);
    pinMode(echoPin, INPUT);
    // Reads the echo pin, and returns the sound wave travel time in
    // microseconds
    return pulseIn(echoPin, HIGH);
}