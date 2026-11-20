
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>

#define IR1_PIN 13
#define IR2_PIN 27
#define SERVO_PIN 14

#define MAX_OCCUPANCY 10
#define WIFI_CHANNEL 1

Servo gateServo;

uint8_t broadcastAddress[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

int occupancy = 0;

enum CrossingState {
  IDLE,
  FIRST_SENSOR_A,
  FIRST_SENSOR_B
};

CrossingState crossing = IDLE;

unsigned long crossingTime = 0;
unsigned long gateOpenedAt = 0;

const unsigned long CROSSING_TIMEOUT = 1000;
const unsigned long GATE_OPEN_TIME = 10000;

bool gateOpen = false;

void sendCommand(const char *command) {
  esp_now_send(
    broadcastAddress,
    (const uint8_t *)command,
    strlen(command) + 1
  );

  Serial.print("ESP-NOW command: ");
  Serial.println(command);
}

void updateOccupancy(int change) {
  occupancy += change;
  occupancy = constrain(occupancy, 0, MAX_OCCUPANCY);

  Serial.print("Occupancy: ");
  Serial.print(occupancy);
  Serial.print("/");
  Serial.println(MAX_OCCUPANCY);

  if (occupancy >= MAX_OCCUPANCY) {
    sendCommand("STOP");
  } else {
    sendCommand("START");
  }
}

void openGate() {
  gateServo.write(90);
  gateOpenedAt = millis();
  gateOpen = true;
}

void setup() {
  Serial.begin(115200);

  pinMode(IR1_PIN, INPUT_PULLUP);
  pinMode(IR2_PIN, INPUT_PULLUP);

  gateServo.setPeriodHertz(50);
  gateServo.attach(SERVO_PIN, 500, 2400);
  gateServo.write(0);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(
    WIFI_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );

  Serial.print("Counter MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed!");
    while (true) delay(1000);
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(
    peerInfo.peer_addr,
    broadcastAddress,
    6
  );

  peerInfo.channel = WIFI_CHANNEL;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Broadcast peer failed!");
    while (true) delay(1000);
  }

  sendCommand("START");
  Serial.println("AWC counter ready.");
}

void loop() {
  bool a = digitalRead(IR1_PIN) == LOW;
  bool b = digitalRead(IR2_PIN) == LOW;

  unsigned long now = millis();

  // Detect which sensor was crossed first.
  if (crossing == IDLE) {
    if (a && !b) {
      crossing = FIRST_SENSOR_A;
      crossingTime = now;
    } else if (b && !a) {
      crossing = FIRST_SENSOR_B;
      crossingTime = now;
    }
  } else {
    if (now - crossingTime > CROSSING_TIMEOUT) {
      crossing = IDLE;
    } else if (a && b) {
      if (crossing == FIRST_SENSOR_A) {
        updateOccupancy(+1);
        openGate();
      } else if (crossing == FIRST_SENSOR_B) {
        updateOccupancy(-1);
        openGate();
      }

      crossing = IDLE;
    }
  }

  // Close the gate after 10 seconds.
  if (gateOpen &&
      now - gateOpenedAt >= GATE_OPEN_TIME) {
    gateServo.write(0);
    gateOpen = false;
  }

  delay(10);
}
