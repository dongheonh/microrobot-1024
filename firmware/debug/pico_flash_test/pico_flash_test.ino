void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  delay(1000);                 // while(!Serial) 제거
  Serial.println("pico works");
}

void loop() {
  static int cnt = 0;

  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  digitalWrite(LED_BUILTIN, LOW);
  delay(250);

  Serial.print("cnt=");
  Serial.println(cnt++);
}
