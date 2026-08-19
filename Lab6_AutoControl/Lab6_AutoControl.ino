/* ============================================================================
 *  Lab6 : ระบบควบคุมอัตโนมัติ  DHT11 -> Relay  พร้อมแสดงผลบน OLED
 *  Board : ESP32 Devkit V2
 * ----------------------------------------------------------------------------
 *  เงื่อนไขการทำงาน
 *    อุณหภูมิ >= ค่าที่ตั้งไว้  ->  เปิด Relay1 นานตามเวลาที่กำหนด (พัดลม)
 *    ความชื้น <= ค่าที่ตั้งไว้  ->  เปิด Relay2 นานตามเวลาที่กำหนด (พ่นหมอก)
 *
 *  ปรับค่าทั้งหมดได้ที่หัวข้อ "ค่าที่ตั้งได้" ด้านล่าง หน่วยเป็นนาที
 *  ใส่ทศนิยมได้ เช่น 0.5 = 30 วินาที (สะดวกตอนทดสอบ)
 *
 *  Lab นี้ต้องเป็น non-blocking เท่านั้น เพราะมีตัวจับเวลา 2 ชุดเดินพร้อมกัน
 *  ถ้าใช้ delay() รอ Relay1 ครบเวลา ระบบจะมองไม่เห็นความชื้นเลยตลอดช่วงนั้น
 *
 *  หมายเหตุ : Serial0 ใช้สายร่วมกับ RS485 -> ถอดสาย A+/B- ก่อนทดสอบ
 * ==========================================================================*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include "pins_config.h"

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);


// ========================== ค่าที่ตั้งได้ (แก้ตรงนี้) ======================

// ----- อุณหภูมิ -> Relay1 (พัดลมระบายความร้อน) -----
constexpr float TEMP_ON_C        = 32.0;   // อุณหภูมิถึงกี่องศาจึงเปิด
constexpr float RELAY1_RUN_MIN   = 1.0;    // เปิดนานกี่นาที
constexpr float RELAY1_REST_MIN  = 2.0;    // พักกี่นาทีก่อนเปิดรอบถัดไป

// ----- ความชื้น -> Relay2 (พ่นหมอกเพิ่มความชื้น) -----
constexpr float HUMI_ON_PCT      = 50.0;   // ความชื้นถึงกี่ % จึงเปิด
constexpr float RELAY2_RUN_MIN   = 1.0;    // เปิดนานกี่นาที
constexpr float RELAY2_REST_MIN  = 2.0;    // พักกี่นาทีก่อนเปิดรอบถัดไป

/* ทิศทางการเปรียบเทียบความชื้น
 *   1 = ความชื้น "ต่ำกว่า" ค่าที่ตั้ง จึงเปิด  -> งานพ่นหมอก เพิ่มความชื้น
 *   0 = ความชื้น "สูงกว่า" ค่าที่ตั้ง จึงเปิด  -> งานระบายอากาศ ลดความชื้น
 *
 * ค่าเริ่มต้นตั้งเป็นแบบพ่นหมอก ซึ่งพบบ่อยกว่าในโรงเรือน
 * ถ้าต้องการใช้ลดความชื้น เปลี่ยนเลข 1 เป็น 0 บรรทัดล่างนี้ได้เลย */
#define HUMI_TRIGGER_WHEN_BELOW  1

// ----- รอบเวลาการทำงานของระบบ -----
constexpr uint32_t DHT_INTERVAL_MS  = 2000;   // อ่านเซนเซอร์ทุกกี่ ms
constexpr uint32_t OLED_INTERVAL_MS = 1000;   // อัปเดตจอทุกกี่ ms

// แปลงนาทีเป็นมิลลิวินาที
constexpr uint32_t RELAY1_RUN_MS  = (uint32_t)(RELAY1_RUN_MIN  * 60000.0);
constexpr uint32_t RELAY1_REST_MS = (uint32_t)(RELAY1_REST_MIN * 60000.0);
constexpr uint32_t RELAY2_RUN_MS  = (uint32_t)(RELAY2_RUN_MIN  * 60000.0);
constexpr uint32_t RELAY2_REST_MS = (uint32_t)(RELAY2_REST_MIN * 60000.0);


// ===================== โครงสร้างข้อมูลของรีเลย์อัตโนมัติ ===================
/* ประกาศ struct ไว้เหนือฟังก์ชันแรกเสมอ
 * เพราะ Arduino IDE แทรก prototype ของทุกฟังก์ชันไว้เหนือฟังก์ชันแรกของไฟล์
 * ถ้าวาง struct ไว้ข้างล่าง จะได้ error: 'AutoRelay' was not declared in this scope
 * (บทเรียนเดียวกับที่เจอใน Lab2)
 */
struct AutoRelay {
  uint8_t  pin;
  const char *name;
  bool     on;          // สถานะปัจจุบัน
  uint32_t startAt;     // เวลาที่เริ่มเปิดรอบนี้
  uint32_t stopAt;      // เวลาที่ปิดรอบล่าสุด
  uint32_t runMs;       // เปิดนานเท่าไหร่
  uint32_t restMs;      // ต้องพักนานเท่าไหร่ก่อนเปิดรอบถัดไป
  bool     everRun;     // เคยทำงานแล้วหรือยัง
  uint32_t cycles;      // นับจำนวนรอบที่ทำงานไปแล้ว
};

AutoRelay fan  = { RELAY1_PIN, "R1", false, 0, 0, RELAY1_RUN_MS, RELAY1_REST_MS, false, 0 };
AutoRelay mist = { RELAY2_PIN, "R2", false, 0, 0, RELAY2_RUN_MS, RELAY2_REST_MS, false, 0 };


// ============================ ตัวแปรค่าเซนเซอร์ ============================
uint32_t dhtAt   = 0;
uint32_t oledAt  = 0;
float    temp    = NAN;
float    humi    = NAN;
bool     sensorOK = false;

/* DHT11 อ่านพลาดเป็นครั้งคราวเป็นเรื่องปกติ ไม่ใช่ความผิดปกติของระบบ
 * ถ้าพลาดครั้งเดียวแล้วขึ้น Sensor Error ทันที จอจะกะพริบข้อความนี้บ่อยจนน่ารำคาญ
 * จึงยอมให้พลาดติดกันได้ถึง 3 ครั้ง (ราว 6 วินาที) ก่อนถือว่าเซนเซอร์มีปัญหาจริง
 * ระหว่างนั้นยังใช้ค่าล่าสุดที่อ่านได้ต่อไป */
uint8_t  failStreak = 0;
constexpr uint8_t MAX_FAIL_STREAK = 3;


// ======================= ตรรกะควบคุมรีเลย์อัตโนมัติ ========================
/* จังหวะการทำงานของรีเลย์ 1 ตัว
 *
 *   [ปิด] --เงื่อนไขเป็นจริง--> [เปิด runMs] --ครบเวลา--> [พัก restMs] --> [ปิด]
 *
 * ทำไมต้องมีช่วง "พัก" ?
 *   ถ้าไม่มี พอครบเวลาปิดปุ๊บ เงื่อนไขยังเป็นจริงอยู่ ระบบจะสั่งเปิดใหม่ทันที
 *   กลายเป็นเปิด-ปิดติดกันไม่หยุด ปั๊มและคอมเพรสเซอร์จะพังเร็วมาก
 *   อีกทั้ง DHT11 มีความคลาดเคลื่อน +-2 C ค่าที่อ่านได้จะแกว่งไปมารอบจุดตัด
 *   ช่วงพักจึงทำหน้าที่กันการสั่งงานถี่เกินไปไปในตัว
 */
void autoRelayUpdate(AutoRelay &r, bool condition, uint32_t now) {
  // ---- กำลังเปิดอยู่ : รอจนครบเวลาแล้วปิด ----
  if (r.on) {
    if (now - r.startAt >= r.runMs) {
      r.on      = false;
      r.stopAt  = now;
      r.everRun = true;
      digitalWrite(r.pin, RELAY_OFF);
      Serial.printf(">> %s ครบเวลา ปิดอัตโนมัติ (รอบที่ %u)\n", r.name, r.cycles);
    }
    return;
  }

  // ---- กำลังปิดอยู่ : ตรวจว่าพร้อมเปิดรอบใหม่หรือยัง ----
  if (!condition) return;                                  // เงื่อนไขยังไม่ถึง
  if (r.everRun && (now - r.stopAt < r.restMs)) return;    // ยังพักไม่ครบ

  r.on      = true;
  r.startAt = now;
  r.cycles++;
  digitalWrite(r.pin, RELAY_ON);
  Serial.printf(">> %s เริ่มทำงาน นาน %.1f นาที (รอบที่ %u)\n",
                r.name, r.runMs / 60000.0, r.cycles);
}

// เวลาที่เหลือของรอบปัจจุบัน หน่วยวินาที
uint32_t relayRemainSec(const AutoRelay &r, uint32_t now) {
  if (!r.on) return 0;
  uint32_t elapsed = now - r.startAt;
  if (elapsed >= r.runMs) return 0;
  return (r.runMs - elapsed + 999) / 1000;
}

// เวลาพักที่เหลือ หน่วยวินาที (0 = พร้อมทำงานแล้ว)
uint32_t relayRestSec(const AutoRelay &r, uint32_t now) {
  if (r.on || !r.everRun) return 0;
  uint32_t elapsed = now - r.stopAt;
  if (elapsed >= r.restMs) return 0;
  return (r.restMs - elapsed + 999) / 1000;
}


// ========================== งานที่ 1 : อ่านเซนเซอร์ ========================
void taskReadDHT(uint32_t now) {
  if (now - dhtAt < DHT_INTERVAL_MS) return;
  dhtAt = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    failStreak++;
    Serial.printf("!! อ่านเซนเซอร์ไม่สำเร็จ (ติดกัน %u ครั้ง)\n", failStreak);

    if (failStreak >= MAX_FAIL_STREAK && sensorOK) {
      sensorOK = false;      // พลาดติดกันมากพอแล้ว ถือว่าเซนเซอร์มีปัญหาจริง
      Serial.println("!! ถือว่าเซนเซอร์มีปัญหา - ระงับการสั่งงานรอบใหม่");
    }
    return;
  }

  failStreak = 0;
  sensorOK   = true;
  temp = t;
  humi = h;

  Serial.printf("[%6us] T:%.1fC H:%.1f%% | R1:%-3s R2:%-3s\n",
                now / 1000, temp, humi,
                fan.on  ? "ON" : "OFF",
                mist.on ? "ON" : "OFF");
}


// ========================= งานที่ 2 : ตรรกะควบคุม =========================
void taskControl(uint32_t now) {
  /* ถ้าเซนเซอร์อ่านไม่ได้ ให้ส่ง condition = false
   * รีเลย์ที่กำลังทำงานอยู่จะยังนับเวลาจนครบตามปกติ (ไม่ตัดกลางคัน)
   * แต่จะไม่เริ่มรอบใหม่ จนกว่าจะอ่านค่าได้อีกครั้ง
   * หลักการ : ไม่สั่งงานอุปกรณ์จริงโดยอาศัยข้อมูลที่เชื่อถือไม่ได้ */
  bool tempCond = false;
  bool humiCond = false;

  if (sensorOK) {
    tempCond = (temp >= TEMP_ON_C);

#if HUMI_TRIGGER_WHEN_BELOW
    humiCond = (humi <= HUMI_ON_PCT);
#else
    humiCond = (humi >= HUMI_ON_PCT);
#endif
  }

  autoRelayUpdate(fan,  tempCond, now);
  autoRelayUpdate(mist, humiCond, now);
}


// ========================== งานที่ 3 : แสดงผล OLED ========================
// เขียนสถานะรีเลย์ 1 ตัวลงจอที่ตำแหน่ง x,y
void drawRelayStatus(const AutoRelay &r, uint32_t now, int16_t x, int16_t y) {
  oled.setCursor(x, y);
  if (r.on) {
    oled.printf("%s ON %us", r.name, relayRemainSec(r, now));
  } else {
    uint32_t rest = relayRestSec(r, now);
    if (rest > 0) oled.printf("%s w%us", r.name, rest);   // w = waiting (พัก)
    else          oled.printf("%s OFF", r.name);
  }
}

void taskDisplay(uint32_t now) {
  if (now - oledAt < OLED_INTERVAL_MS) return;
  oledAt = now;

  oled.clearDisplay();

  // ---- หัวข้อ ----
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("AUTO CONTROL");
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  if (!sensorOK) {
    oled.setTextSize(1);
    oled.setCursor(0, 26);
    oled.println("Sensor Error!");
    oled.setCursor(0, 40);
    oled.println("Check DHT11 wiring");
    oled.display();
    return;
  }

  // ---- ค่าอุณหภูมิ (ตัวใหญ่) + จุดตัดที่ตั้งไว้ ----
  oled.setTextSize(2);
  oled.setCursor(0, 14);
  oled.printf("T %.1fC", temp);

  oled.setTextSize(1);
  oled.setCursor(92, 18);
  oled.printf(">%.0f", TEMP_ON_C);

  // ---- ค่าความชื้น (ตัวใหญ่) + จุดตัดที่ตั้งไว้ ----
  oled.setTextSize(2);
  oled.setCursor(0, 32);
  oled.printf("H %.0f%%", humi);

  oled.setTextSize(1);
  oled.setCursor(92, 36);
#if HUMI_TRIGGER_WHEN_BELOW
  oled.printf("<%.0f", HUMI_ON_PCT);
#else
  oled.printf(">%.0f", HUMI_ON_PCT);
#endif

  // ---- แถบสถานะรีเลย์ล่างสุด ----
  oled.setTextSize(1);
  drawRelayStatus(fan,  now,  0, 54);
  drawRelayStatus(mist, now, 66, 54);

  oled.display();
}


// ================================ SETUP ===================================
void setup() {
  Serial.begin(115200);

  // ตั้งรีเลย์เป็น OFF ก่อน pinMode เสมอ กันรีเลย์กระตุกตอนบูต
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  Wire.begin(I2C_SDA, I2C_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ไม่พบจอ OLED - ระบบควบคุมจะทำงานต่อโดยไม่มีจอ");
    // ไม่หยุดโปรแกรม เพราะระบบรดน้ำต้องทำงานได้แม้จอเสีย
  }

  dht.begin();

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 24);
  oled.println("Starting...");
  oled.display();

  Serial.println("\n=======================================================");
  Serial.println("  Lab6 : ระบบควบคุมอัตโนมัติ DHT11 -> Relay");
  Serial.println("=======================================================");
  Serial.printf ("  Relay1 : เปิดเมื่ออุณหภูมิ >= %.1f C\n", TEMP_ON_C);
  Serial.printf ("           ทำงาน %.1f นาที แล้วพัก %.1f นาที\n",
                 RELAY1_RUN_MIN, RELAY1_REST_MIN);
#if HUMI_TRIGGER_WHEN_BELOW
  Serial.printf ("  Relay2 : เปิดเมื่อความชื้น <= %.1f %%\n", HUMI_ON_PCT);
#else
  Serial.printf ("  Relay2 : เปิดเมื่อความชื้น >= %.1f %%\n", HUMI_ON_PCT);
#endif
  Serial.printf ("           ทำงาน %.1f นาที แล้วพัก %.1f นาที\n",
                 RELAY2_RUN_MIN, RELAY2_REST_MIN);
  Serial.println("=======================================================\n");
}


// ================================= LOOP ===================================
void loop() {
  uint32_t now = millis();

  taskReadDHT(now);    // อ่านเซนเซอร์ทุก 2 วินาที
  taskControl(now);    // ตัดสินใจเปิด/ปิดรีเลย์
  taskDisplay(now);    // อัปเดตจอทุก 1 วินาที

  // ไม่มี delay() — ตัวจับเวลาทั้ง 2 ชุดจึงเดินพร้อมกันได้อย่างอิสระ
}
