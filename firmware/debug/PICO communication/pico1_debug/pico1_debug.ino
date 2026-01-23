// pico1.ino
// Receive '1' / '0' from Pico2 via UART and control LED

#include <Arduino.h>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // UART0: TX=GP0, RX=GP1
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(115200);
}

void loop() {
  if (Serial1.available()) {
    char c = Serial1.read();

    if (c == '1') {
      digitalWrite(LED_BUILTIN, HIGH);
    } else if (c == '0') {
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}
