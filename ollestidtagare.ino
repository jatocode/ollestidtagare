void setup() { 
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT); 
}

void loop() {
    // Avståndet blir tiden * ljudhastigheten delat med två
    int distance = readUltrasonicDistance(27, 27) * 0.340 / 2;  
    Serial.print("Avstånd:" );
    Serial.println(distance);

    delay(200);
}

long readUltrasonicDistance(int triggerPin, int echoPin) {
    pinMode(triggerPin, OUTPUT);  // Clear the trigger
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