// Pico2 : Blink + send UART command to Pico1
#define LED_BUILTIN 25

void setup() {
  Serial.begin(115200);      // USB debug
  Serial1.begin(115200);    // UART0 on GPIO0(TX), GPIO1(RX)

  while (!Serial) delay(100);
  Serial.println("Pico2 works");

  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  static int cnt = 0;

  // LED ON
  digitalWrite(LED_BUILTIN, HIGH);
  Serial1.write('1');              // tell Pico1: ON
  Serial.println("Send: 1 (ON)");
  delay(50);

  // LED OFF
  digitalWrite(LED_BUILTIN, LOW);
  Serial1.write('0');              // tell Pico1: OFF
  Serial.println("Send: 0 (OFF)");
  delay(50);

  Serial.print("print test = ");
  Serial.println(cnt++);
}
