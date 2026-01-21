// Raspberry Pi Pico 2 - Basic LED Blink Test
#define LED_BUILTIN 25   // default onboard LED pin (GP25)

void setup() {
  Serial.begin(115200);
  while(!Serial) delay(100);
  Serial.println("pico works");
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  static int cnt = 0;
  digitalWrite(LED_BUILTIN, HIGH);
  delay(50);

  digitalWrite(LED_BUILTIN, LOW);
  delay(50); 
  Serial.print("print test = ");
  Serial.println(cnt++);
}
