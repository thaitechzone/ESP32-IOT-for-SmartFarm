/* ---------------------------------------------------------------
 *  Lab4 : ทดสอบจอ OLED (I2C)  แสดงข้อความ 2 บรรทัด
 *  Board : ESP32 Devkit V2   จอ : SSD1306 128x64 ที่ 0x3C
 *
 *  ไลบรารีที่ต้องติดตั้ง (Tools > Manage Libraries...)
 *    - Adafruit SSD1306
 *    - Adafruit GFX Library
 * -------------------------------------------------------------*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins_config.h"

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);   // -1 = ไม่มีขา reset

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA, I2C_SCL);                 // SDA=21, SCL=22

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ไม่พบจอ OLED - ตรวจสาย I2C และแอดเดรส");
    while (true) delay(1000);                   // หยุดรอ ไม่ต้องทำอะไรต่อ
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  oled.setCursor(0, 0);                         // บรรทัดที่ 1
  oled.println("Hello ESP32");

  oled.setCursor(0, 16);                        // บรรทัดที่ 2
  oled.println("test OLED");

  oled.display();                               // สำคัญ! ต้องเรียกจอถึงจะอัปเดต

  Serial.println("แสดงข้อความบนจอเรียบร้อย");
}

void loop() {
  delay(1000);                                  // ไม่มีอะไรต้องทำ
}
