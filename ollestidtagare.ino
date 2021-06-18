#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <RFID.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#define EEPROM_SIZE 300
#define EEPROM_SSID 0
#define EEPROM_PASS 100
#define EEPROM_CODE 200
#define MAX_EEPROM_LEN 50  // Max length to read ssid/passwd

// Set these to your desired credentials.
const char *ssid = "loin32wifi";
const char *password = "supersecret";
const char *mdnsname = "loin32";

String ipaddress = "";
String savedSSID = "";
String savedPASS = "";
String networks = "";
bool connected = false;

WiFiServer server(80);
DNSServer dnsServer;

const char *mqtt_server = "192.168.68.108";
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// See README.md for an easy way to create these strings
// main html page as one line
const String mainHtmlOutput =
    "<!DOCTYPE html><html><head>		<title>ESP32</title>	"
    "	<link rel=\"stylesheet\" "
    "href=\"https://cdnjs.cloudflare.com/ajax/libs/mini.css/3.0.1/"
    "mini-default.min.css\">		<style>.button.large "
    "{text-align:center;padding:2em ; margin: "
    "1em;font-size:2em;color:black}</style></head><body>		<h1 "
    "align=\"center\">ESP32 action</h1>		<br/><br/>		<div "
    "class=\"row cols-sm-10\">				<a class=\"button "
    "large\" onClick='run(\"A\")' href=\"#\">Blink LED</a>		"
    "		<a class=\"button large\" onClick='run(\"B\")' "
    "href=\"#\">V&aring;gform: <span id=\"wave\">%WAVE%</span></a>	"
    "	</div>		<div><small>Connected to WiFi: "
    "%WIFI%</small></div>		<script>			"
    "	async function run(param) {					"
    "	let result = await fetch('/' + param);	let r = await result.json(); "
    "document.getElementById('wave').innerHTML = r.wave; "
    "console.log(r);					/* Use result for "
    "something - or not */				}		"
    "</script>		</body></html>";
// access point, select wifi as one line
const String apHtmlOutput =
    "<!DOCTYPE html><html><head>	<title>ESP32 connect to "
    "Wifi</title>	<link rel=\"stylesheet\" "
    "href=\"https://cdnjs.cloudflare.com/ajax/libs/mini.css/3.0.1/"
    "mini-default.min.css\"></head><body>	<h1 align=\"center\">ESP32 "
    "connect to Wifi</h1>	<br /><br />	<form action=\"/C\" "
    "method=\"GET\">		<fieldset>			"
    "<legend>Connect to wifi</legend>			<div class=\"col-sm-12 "
    "col-md-6\">				<select name=\"ssid\">	"
    "				%SSIDLIST%				"
    "</select>			</div>			<div "
    "class=\"row\">				<div class=\"col-sm-8 "
    "col-md-8\">					<label "
    "for=\"password\">Password</label>					<input "
    "name=\"p\" type=\"password\" id=\"password\" placeholder=\"Password\" "
    "/>				</div>			</div>		"
    "	<button class=\"submit\">Connect</button>			"
    "</div>		</fieldset>	</form></body></html>";

StaticJsonDocument<200> doc;

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

    Serial.println();
    Serial.println("Reading settings from EEPROM");
    EEPROM.begin(EEPROM_SIZE);

    savedSSID = readStringEEPROM(EEPROM_SSID);
    savedPASS = readStringEEPROM(EEPROM_PASS);
    Serial.println(savedSSID);
    Serial.println(savedPASS);

    // Try to guess if we got saved data or random unitialized
    if (savedSSID.length() == MAX_EEPROM_LEN ||
        savedPASS.length() == MAX_EEPROM_LEN) {
        Serial.println("Unitialized data from EEPROM");
        Serial.println("Setting up AP");
        connected = false;
    } else {
        Serial.println("Trying to connect to saved WiFi");
        connected = connectWifi(savedSSID, savedPASS);
    }

    if (!connected) {
        // Start Access Point
        Serial.println("Setting up access point");

        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        networks = scanNetworks();
        setupAP();
    }

    setupMQTT();

    Serial.print("Connected to wifi: ");
    Serial.println(savedSSID);

    server.begin();
    Serial.println("Server started");

    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else  // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount
            // SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });

    ArduinoOTA.begin();

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

    if (!connected) {
        // Captive portal. Give our IP to everything
        dnsServer.processNextRequest();
    }

    if (!mqttClient.connected()) {
        Serial.println("Reconnecting mqttClient");
        reconnect();
    }
    mqttClient.loop();  // Loopar inte, ska bara köras i loopen

    ArduinoOTA.handle();


    WiFiClient client = server.available();

    if (client) {
        String currentLine = "";
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                if (c == '\n') {
                    // if the current line is blank, you got two newline
                    // characters in a row. that's the end of the client HTTP
                    // request, so send a response:
                    if (currentLine.length() == 0) {
                        if (connected) {
                            // Connected to WiFi. Respond with main page
                            writeMainResponse(client);
                        } else {
                            // Access point. Respond with wifi-select page
                            writeAPResponse(client);
                        }
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }
        client.stop();
    }

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


void pubmqttlarm(String larm) {
    String status = "{\"aktivt\":\"" + larm + "\"}";
    char a[100];
    status.toCharArray(a, status.length() + 1);
    mqttClient.publish("olle/esp32/larm", a, true);
}

void pubmqttcode(String code) {
    String status = "{\"kod\":\"" + code + "\"}";
    char a[100];
    status.toCharArray(a, status.length() + 1);
    mqttClient.publish("olle/esp32/code", a, false);
}


void pubmqttstatus(String key, String msg, String card) {
    String status =
        "{\"" + key + "\":\"" + msg + "\",\"card\":\"" + card + "\"}";
    char a[100];
    status.toCharArray(a, status.length() + 1);
    mqttClient.publish("olle/esp32/status", a, true);
}

void setupMQTT() {
    Serial.println("Configuring MQTT server");
    mqttClient.setServer(mqtt_server, 1883);
    mqttClient.setCallback(mqttCallback);
}

void mqttCallback(char *topic, byte *message, unsigned int length) {
    //Serial.print("Message arrived on topic: ");
    //Serial.print(topic);
    //Serial.print(". Message: ");
    String messageTemp;

    for (int i = 0; i < length; i++) {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();
    if (String(topic) == "olle/tidtagare") {
        //
    }
}

void reconnect() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "OLLESTIMER-";
        clientId += String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("mqtt connected");
            mqttClient.publish("olle/tidtagare", "Olles tidtagare lever");
            // // Send door status
            // if (state == 0) {
            //     pubmqttstatus("status", "1", "locked after reset");
            // } else {
            //     pubmqttstatus("status", "0", "open after reset");
            // }
            // pubmqttcode(correctCode);
            // pubmqttlarm("0");
            // mqttClient.subscribe("olle/#");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}


bool connectionRequest(String params) {
    Serial.println(params);
    // Parse parameters by finding =
    // Format is: C?ssid=SSID&p=PASSWORD
    int i = params.indexOf('=', 0);
    int i2 = params.indexOf('&', 0);
    String ssid = params.substring(i + 1, i2);
    ssid.replace('+', ' ');  // remove html encoding

    i = params.indexOf('=', i2);
    String pass = params.substring(i + 1);

    Serial.println(ssid);
    Serial.println(pass);

    connected = connectWifi(ssid, pass);  // Globals. Me hates it
    return connected;
}

void writeMainResponse(WiFiClient client) {
    writeOkHeader(client);
    String htmlParsed = mainHtmlOutput;
    htmlParsed.replace("%WIFI%", ipaddress);
    htmlParsed.replace("%WAVE%", "not in use");
    client.println(htmlParsed);
    client.println();
    Serial.println("Wrote main response to client");
}

void writeAPResponse(WiFiClient client) {
    writeOkHeader(client);
    String htmlParsed = apHtmlOutput;
    htmlParsed.replace("%SSIDLIST%", networks);
    client.println(htmlParsed);
    client.println();
    Serial.println("Wrote AP response to client");
}

void writeOkHeader(WiFiClient client) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();
}

void writeRedirectHeader(WiFiClient client, String redirectUrl) {
    Serial.println("Redirecting to " + redirectUrl);
    client.println("HTTP/1.1 301 Moved Permanently");
    client.println("Content-type:text/html");
    client.print("Location: ");
    client.println(redirectUrl);
    client.println();
    client.println();
}

void setupAP() {
    const byte DNS_PORT = 53;

    IPAddress local_IP(10, 0, 1, 1);
    IPAddress subnet(255, 255, 0, 0);

    Serial.println("Configuring access point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    //    WiFi.softAP(ssid, password); // Using pwd
    WiFi.softAP(ssid);  // Not using pwd

    dnsServer.start(DNS_PORT, "*", local_IP);

    IPAddress myIP = WiFi.softAPIP();
    ipaddress = myIP.toString();
    Serial.println("AP ssid:" + String(ssid));
    Serial.println("AP IP address: " + ipaddress);
}

bool connectWifi(String ssid, String pass) {
    char ssidA[100];
    char passA[100];
    int i = 5;

    WiFi.disconnect();

    Serial.println("Trying to connect to " + ssid + " with passwd:" + pass);
    // WiFi.begin needs char*
    ssid.toCharArray(ssidA, 99);
    pass.toCharArray(passA, 99);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssidA, passA);

    while (i-- > 0 && WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Connection failed");
        return false;
    }
    ipaddress = WiFi.localIP().toString();
    Serial.println("WiFi connected, IP address: " + ipaddress);

    if (!MDNS.begin(mdnsname)) {
        Serial.println("Error setting up MDNS responder!");
    } else {
        Serial.println("mDNS responder started, name:" + String(mdnsname) +
                       ".local");
        MDNS.addService("http", "tcp", 80);
    }
    ipaddress += " (" + String(mdnsname) + ".local)";

    if (ssid != savedSSID || pass != savedPASS) {
        // Wifi config has changed, write working to EEPROM
        Serial.println("Writing Wifi config to EEPROM");
        writeStringEEPROM(EEPROM_SSID, ssidA);
        writeStringEEPROM(EEPROM_PASS, passA);
    }

    return true;
}

String scanNetworks() {
    Serial.println("scan start");

    // WiFi.scanNetworks will return the number of networks found
    int n = WiFi.scanNetworks();
    Serial.println("scan done");
    if (n == 0) {
        Serial.println("no networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found");
        String htmloptions = "";
        for (int i = 0; i < n; ++i) {
            // Print SSID and RSSI for each network found
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(i));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " "
                                                                      : "*");
            htmloptions += "<option>" + String(WiFi.SSID(i)) + "</option>";
            delay(10);
        }
        Serial.println(htmloptions);
        return htmloptions;
    }
    Serial.println("");
    return "";
}

void writeStringEEPROM(char add, String data) {
    int _size = data.length();
    Serial.println("Sparar");
    Serial.println(data);
    Serial.println(_size);
    if (_size > MAX_EEPROM_LEN) return;
    int i;
    for (i = 0; i < _size; i++) {
        EEPROM.write(add + i, data[i]);
    }
    EEPROM.write(add + _size,
                 '\0');  // Add termination null character for String Data
    EEPROM.commit();
}

String readStringEEPROM(char add) {
    int i;
    char data[MAX_EEPROM_LEN + 1];
    int len = 0;
    unsigned char k;
    k = EEPROM.read(add);
    while (k != '\0' && len < MAX_EEPROM_LEN)  // Read until null character
    {
        k = EEPROM.read(add + len);
        data[len] = k;
        len++;
    }
    data[len] = '\0';
    Serial.println(String(data));
    return String(data);
}
