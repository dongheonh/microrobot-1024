// pico2.ino
// Blink local LED and send '1' / '0' to Pico1 via UART

#include <Arduino.h>

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // UART0: TX=GP0, RX=GP1
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(115200);
}

void loop() {
  // LED ON + send '1'
  digitalWrite(LED_BUILTIN, HIGH);
  Serial1.write('1');
  delay(500);

  // LED OFF + send '0'
  digitalWrite(LED_BUILTIN, LOW);
  Serial1.write('0');
  delay(500);
}
