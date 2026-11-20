
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define IN1 12
#define IN2 14
#define IN3 26
#define IN4 27

#define WIFI_CHANNEL 1

volatile bool motorsRunning = false;
volatile bool commandReceived = false;

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

// Compatible with Arduino-ESP32 3.x
void onDataRecv(
  const esp_now_recv_info_t *info,
  const uint8_t *data,
  int len
) {
  if (len <= 0 || len >= 16) return;

  char command[16] = {};
  memcpy(command, data, len);
  command[len] = '\0';

  if (strcmp(command, "START") == 0) {
    motorsRunning = true;
    commandReceived = true;
  } else if (strcmp(command, "STOP") == 0) {
    motorsRunning = false;
    commandReceived = true;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopMotors();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_channel(
    WIFI_CHANNEL,
    WIFI_SECOND_CHAN_NONE
  );

  Serial.print("Car MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed!");
    while (true) {
      stopMotors();
      delay(1000);
    }
  }

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("AWC car ready.");
  Serial.println("Waiting for START/STOP...");
}

void loop() {
  if (commandReceived) {
    noInterrupts();
    bool running = motorsRunning;
    commandReceived = false;
    interrupts();

    if (running) {
      Serial.println("START received");
      moveForward();
    } else {
      Serial.println("STOP received");
      stopMotors();
    }
  }

  delay(10);
}
