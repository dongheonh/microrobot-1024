#include <Wire.h>
#include <Adafruit_MLX90393.h>

Adafruit_MLX90393 sensor;

void setup() {
  Serial.begin(115200);
  while(!Serial) {}

  Wire.begin();

  // Optional but helps: faster I2C if stable
  // Wire.setClock(400000);

  if (!sensor.begin_I2C(0x18)) {
    Serial.println("MLX90393 init failed at 0x18");
    while (1) delay(10);
  }
  Serial.println("MLX90393 found at 0x18");

  sensor.setGain(MLX90393_GAIN_1X);
  sensor.setResolution(MLX90393_X, MLX90393_RES_16);
  sensor.setResolution(MLX90393_Y, MLX90393_RES_16);
  sensor.setResolution(MLX90393_Z, MLX90393_RES_16);
  sensor.setOversampling(MLX90393_OSR_1);
  sensor.setFilter(MLX90393_FILTER_0);

  Serial.println("t_us,x_uT,y_uT,z_uT");
}

void loop() {
  float x, y, z;
  if (sensor.readData(&x, &y, &z)) {

    Serial.print(x, 3);
    Serial.print(',');
    Serial.print(y, 3);
    Serial.print(',');
    Serial.println(z, 3);
  }
  delay(50);
}
