// Pico1 : Receive UART command and control LED
#define LED_BUILTIN 25

void setup() {
  Serial.begin(115200);      // USB debug
  Serial1.begin(115200);    // UART0 on GPIO0(TX), GPIO1(RX)

  while (!Serial) delay(100);
  Serial.println("Pico1 ready");

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  if (Serial1.available()) {
    char c = Serial1.read();
    Serial.print("RX = ");
    Serial.println(c);

    if (c == '1') {
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.println("Received: ON");
    }
    else if (c == '0') {
      digitalWrite(LED_BUILTIN, LOW);
      Serial.println("Received: OFF");
    }
  }
}
