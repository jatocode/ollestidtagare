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
    if (startTimer(distance)) {
        timelapse = millis() - start;

        // Gör om ms till läsbar tid så vi kan visa det
        byte tid[4];
        milliSecondsToTime(timelapse, tid);
        start = millis();

        Serial.print(distance);
        Serial.print(" mm - ");
        Serial.print(timelapse);
        Serial.print(" ms, varv: ");
        Serial.print(varv);
        Serial.println();

        if (tid[1] > 0) {
            // Minuter:Sekunder 
            matrix.print(tid[1] * 100 + tid[2]);
            matrix.writeDigitRaw(2, 2 + 4);
        } else {
            // Sekunder:Hundradelar
            matrix.print(tid[2] * 100 + tid[3]);
            matrix.writeDigitRaw(2, 2 + 8);
        }
        matrix.writeDisplay();
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
    if(dist > 10000) {
        // Outside range, dont care
        dist = 10000;
    }
    int ddiff = dist - lastDistance;
    int prevLast = lastDistance;
    lastDistance = dist;
    if (abs(ddiff) >= minDiff) {
        unsigned long tdiff = millis() - lastDistChange;
        if (tdiff >= graceTime) {
            Serial.print("Time diff: ");
            Serial.println(tdiff);
            Serial.print("Distance diff:");
            Serial.print(abs(ddiff));
            Serial.print(", distance: ");
            Serial.print(dist);
            Serial.print(", previous distance: ");
            Serial.println(prevLast);
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