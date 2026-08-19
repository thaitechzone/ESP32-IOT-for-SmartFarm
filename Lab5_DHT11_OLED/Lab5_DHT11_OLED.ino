/* ---------------------------------------------------------------
 *  Lab5 : DHT11 + OLED  แสดงอุณหภูมิและความชื้นบนจอ
 *  Board : ESP32 Devkit V2
 *    DHT11 -> GPIO15        จอ OLED -> I2C (SDA 21, SCL 22) 0x3C
 *
 *  ไลบรารีที่ต้องติดตั้ง (ติดตั้งไปแล้วตั้งแต่ Lab3 และ Lab4)
 *    - DHT sensor library      (Adafruit)
 *    - Adafruit Unified Sensor (Adafruit)
 *    - Adafruit SSD1306        (Adafruit)
 *    - Adafruit GFX Library    (Adafruit)
 * -------------------------------------------------------------*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "pins_config.h"

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ไม่พบจอ OLED - ตรวจสาย I2C และแอดเดรส");
    while (true) delay(1000);
  }

  dht.begin();

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 24);
  oled.println("Starting...");
  oled.display();

  Serial.println("Lab5 : DHT11 + OLED");
  delay(2000);              // รอเซนเซอร์ตั้งตัวก่อนอ่านครั้งแรก
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();      // องศาเซลเซียส

  // ----- วาดหัวข้อ -----
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("DHT11 Monitor");
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // ----- แสดงค่า -----
  if (isnan(t) || isnan(h)) {           // ต้องเช็คทุกครั้ง DHT11 พลาดได้บ่อย
    oled.setTextSize(1);
    oled.setCursor(0, 28);
    oled.println("Sensor Error!");
    oled.setCursor(0, 42);
    oled.println("Check wiring...");
    Serial.println("อ่านค่าไม่สำเร็จ - ตรวจสาย DATA และไฟเลี้ยง");
  } else {
    oled.setTextSize(2);                // ตัวใหญ่ อ่านได้จากระยะไกล
    oled.setCursor(0, 18);
    oled.printf("T %.1f C", t);
    oled.setCursor(0, 42);
    oled.printf("H %.1f %%", h);

    Serial.printf("Temp: %.1f C   Humi: %.1f %%\n", t, h);
  }

  oled.display();                       // สำคัญ! ต้องเรียกจอถึงจะอัปเดต

  delay(2000);                          // DHT11 อ่านได้ไม่เกิน 1 ครั้ง/วินาที
}
