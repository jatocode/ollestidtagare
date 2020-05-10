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
    int distance = readUltrasonicDistance(27, 27) * 0.340 / 2;

   // Serial.println(distance);

    if (startTimer(distance)) {
        timelapse = millis() - start;
        matrix.print(distance);
        matrix.writeDisplay();
        Serial.println(timelapse);
        Serial.println(varv);

        // Ok, vi har kört ett varv. Vill skriva ut tiden lite snyggare än i ms
        byte tid[4];
        milliSecondsToTime(timelapse, tid);

        Serial.print(tid[0]);
        Serial.print(":");
        Serial.print(tid[1]);
        Serial.print(":");
        Serial.print(tid[2]);
        Serial.print(".");
        Serial.print(tid[3]);
        Serial.println();
    }

    // Blinka lite så vi vet att vi lever
    if (millis() - blink > 500) {
        blink = millis();
        led = led == LOW ? HIGH : LOW;
    }

    digitalWrite(LED_BUILTIN, led);
    delay(20);
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

void milliSecondsToTime(unsigned long ms, byte *tider) {
    int sec = floor(ms / 1000);
    int min = floor(sec / 60);
    int hour = floor(min / 60);

    // Blir weird med sec/min/hour mfl som kan ha värden över 60/60/24, så fixa
    sec = sec % 60;
    min = min % 60;
    hour = hour % 24;

    unsigned long totalsec = hour * 60 * 60 + min * 60 + sec;
    unsigned long hundradels = round(
        (ms - totalsec * 1000) / 10);  // Kanske går en omväg men borde funka

    // Vill inte skriva ut här, borde bara vara konv.
    tider[0] = hour;
    tider[1] = min;
    tider[2] = sec;
    tider[3] = hundradels;
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