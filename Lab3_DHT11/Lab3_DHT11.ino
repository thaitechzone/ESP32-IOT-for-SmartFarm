/* ============================================================================
 *  Lab3 : DHT11 — อ่านอุณหภูมิและความชื้น แสดงผลทาง Serial Port
 *  Board : ESP32 Devkit V2      Sensor : DHT11 ที่ GPIO15
 * ----------------------------------------------------------------------------
 *  สิ่งที่ Lab นี้สอน
 *    1. ติดตั้งและใช้ไลบรารี DHT ของ Adafruit
 *    2. อ่านเซนเซอร์ตามรอบเวลาแบบ non-blocking (ไม่ใช้ delay)
 *    3. ตรวจจับการอ่านล้มเหลวด้วย isnan() — เซนเซอร์ราคาถูกพลาดได้เสมอ
 *    4. คำนวณ Heat Index (อุณหภูมิที่ร่างกายรู้สึกจริง)
 *    5. เก็บสถิติ ค่าต่ำสุด/สูงสุด และอัตราการอ่านสำเร็จ
 *
 *  ไลบรารีที่ต้องติดตั้งก่อน (Tools > Manage Libraries...)
 *    - "DHT sensor library"      by Adafruit
 *    - "Adafruit Unified Sensor" by Adafruit   <- IDE จะถามให้ติดตั้งอัตโนมัติ
 *
 *  การต่อสาย  (บอร์ดนี้ต่อมาให้แล้ว ไม่ต้องเดินสายเพิ่ม)
 *    DHT11 VCC  -> 3.3V
 *    DHT11 DATA -> GPIO15   + ตัวต้านทาน pull-up 4.7k-10k ไปยัง 3.3V
 *    DHT11 GND  -> GND
 *
 *  ข้อจำกัดของ DHT11 ที่ต้องรู้
 *    - อ่านได้ไม่เกิน 1 ครั้ง/วินาที (Lab นี้ตั้งไว้ 2 วินาที เผื่อความปลอดภัย)
 *    - ความละเอียด 1 องศา / 1 %  ทศนิยมจึงเป็น .00 เสมอ ไม่ใช่อาการเสีย
 *    - วัดได้ 0-50 C (คลาดเคลื่อน +-2 C) และ 20-80 %RH (คลาดเคลื่อน +-5 %)
 *    - ถ้าต้องการทศนิยมจริงและช่วงวัดกว้างกว่านี้ ต้องเปลี่ยนเป็น DHT22
 *
 *  หมายเหตุ : Serial0 ใช้สายร่วมกับ RS485 บนบอร์ดนี้
 *             ให้ถอดสาย A+/B- ออกก่อนทดสอบ Lab นี้
 * ==========================================================================*/

#include "pins_config.h"
#include <DHT.h>

// สร้างอ็อบเจกต์เซนเซอร์ : ค่า DHT_PIN และ DHT_TYPE มาจาก pins_config.h
DHT dht(DHT_PIN, DHT_TYPE);


// ============================ ค่าคงที่เรื่องเวลา ============================
const uint32_t DHT_INTERVAL_MS = 2000;   // อ่านเซนเซอร์ทุกกี่ ms (ห้ามต่ำกว่า 1000)
const uint32_t LED_ON_MS       = 100;    // LED กะพริบสั้น ๆ เป็นสัญญาณว่ายังไม่แฮงค์
const uint32_t LED_OFF_MS      = 900;


// ======================== ตัวแปรเก็บค่าและสถิติ ============================
uint32_t dhtAt     = 0;      // เวลาที่อ่านเซนเซอร์ครั้งล่าสุด
uint32_t okCount   = 0;      // จำนวนครั้งที่อ่านสำเร็จ
uint32_t failCount = 0;      // จำนวนครั้งที่อ่านล้มเหลว

float lastTemp = NAN;        // ค่าล่าสุดที่อ่านได้
float lastHumi = NAN;

float tempMin = NAN, tempMax = NAN;   // ค่าต่ำสุด/สูงสุดตั้งแต่เปิดเครื่อง
float humiMin = NAN, humiMax = NAN;

/* ทำไมค่าเริ่มต้นเป็น NAN ไม่ใช่ 0 ?
 *   ถ้าตั้ง tempMin = 0 ระบบจะเข้าใจว่าเคยวัดได้ 0 องศาแล้ว
 *   ค่าจริง 28 องศาจะไม่มีทางต่ำกว่า 0 ทำให้ tempMin ค้างที่ 0 ตลอดกาล
 *   NAN แปลว่า "ยังไม่มีข้อมูล" ซึ่งตรงกับความจริงมากกว่า
 */


// ===================== งานที่ 1 : LED heartbeat ============================
uint32_t ledAt    = 0;
bool     ledState = false;

void taskHeartbeat(uint32_t now) {
  uint32_t wait = ledState ? LED_ON_MS : LED_OFF_MS;
  if (now - ledAt < wait) return;

  ledAt    = now;
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}


// ==================== งานที่ 2 : อ่าน DHT11 ================================
void updateMinMax(float t, float h) {
  if (isnan(tempMin) || t < tempMin) tempMin = t;
  if (isnan(tempMax) || t > tempMax) tempMax = t;
  if (isnan(humiMin) || h < humiMin) humiMin = h;
  if (isnan(humiMax) || h > humiMax) humiMax = h;
}

void taskReadDHT(uint32_t now) {
  if (now - dhtAt < DHT_INTERVAL_MS) return;   // ยังไม่ถึงรอบ -> ไปทำงานอื่น
  dhtAt = now;

  /* หมายเหตุความจริง : ฟังก์ชัน read ของไลบรารีนี้ "หยุดรอ" ประมาณ 25 ms
   * เพราะโปรโตคอล 1-wire ของ DHT บังคับให้ต้องดึงสาย LOW ค้างไว้ 18 ms
   * เป็นข้อจำกัดของตัวเซนเซอร์เอง เลี่ยงไม่ได้ด้วยไลบรารีมาตรฐาน
   *
   * แต่เราคุมได้ว่าจะให้เกิดขึ้นถี่แค่ไหน — ตรงนี้คือ 25 ms ทุก 2 วินาที
   * คิดเป็น 1.25% ของเวลาทั้งหมด อีก 98.75% loop() ยังว่างทำงานอื่นได้ปกติ
   */
  float h = dht.readHumidity();
  float t = dht.readTemperature();        // ค่าเริ่มต้นเป็นองศาเซลเซียส

  // ---- ตรวจสอบความถูกต้องก่อนใช้งานเสมอ ----
  if (isnan(h) || isnan(t)) {
    failCount++;
    Serial.printf("[%6us] อ่านค่าไม่สำเร็จ (ครั้งที่ %u) — ",
                  now / 1000, failCount);
    Serial.println("ตรวจสาย DATA, ไฟเลี้ยง 3.3V และตัวต้านทาน pull-up");
    return;
  }

  okCount++;
  lastTemp = t;
  lastHumi = h;
  updateMinMax(t, h);

  /* Heat Index = อุณหภูมิที่ร่างกายรู้สึกจริง เมื่อรวมผลของความชื้นเข้าไปด้วย
   * ความชื้นสูงทำให้เหงื่อระเหยยาก ร่างกายจึงรู้สึกร้อนกว่าตัวเลขที่วัดได้
   * พารามิเตอร์ตัวที่ 3 : false = องศาเซลเซียส, true = ฟาเรนไฮต์
   * สูตรนี้ออกแบบมาสำหรับอากาศร้อน ถ้าต่ำกว่า 26 C ค่าจะไม่ค่อยมีความหมาย
   */
  float hi = dht.computeHeatIndex(t, h, false);

  uint32_t total   = okCount + failCount;
  uint32_t percent = (okCount * 100) / total;

  Serial.printf("[%6us] Temp:%5.1f C | Humi:%5.1f %% | รู้สึกเหมือน:%5.1f C | สำเร็จ %u/%u (%u%%)\n",
                now / 1000, t, h, hi, okCount, total, percent);
}


// ==================== งานที่ 3 : สรุปสถิติทุก 30 วินาที ====================
const uint32_t SUMMARY_MS = 30000;
uint32_t summaryAt = 0;

void taskSummary(uint32_t now) {
  if (now - summaryAt < SUMMARY_MS) return;
  summaryAt = now;

  if (okCount == 0) {
    Serial.println("--- ยังไม่เคยอ่านค่าสำเร็จเลย ตรวจการต่อสายเซนเซอร์ ---");
    return;
  }

  Serial.println("-------------------------------------------------------");
  Serial.printf("  สรุปรอบ %u นาที\n", now / 60000);
  Serial.printf("  อุณหภูมิ : ต่ำสุด %.1f C   สูงสุด %.1f C   ล่าสุด %.1f C\n",
                tempMin, tempMax, lastTemp);
  Serial.printf("  ความชื้น : ต่ำสุด %.1f %%   สูงสุด %.1f %%   ล่าสุด %.1f %%\n",
                humiMin, humiMax, lastHumi);
  Serial.printf("  อ่านสำเร็จ %u ครั้ง / ล้มเหลว %u ครั้ง\n", okCount, failCount);
  Serial.println("-------------------------------------------------------");
}


// ================================ SETUP ===================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  dht.begin();     // เริ่มต้นเซนเซอร์ ต้องเรียกก่อนอ่านค่าเสมอ

  Serial.println("\n=======================================================");
  Serial.println("  Lab3 : DHT11 — อุณหภูมิและความชื้น");
  Serial.println("=======================================================");
  Serial.printf ("  เซนเซอร์ : DHT11 ที่ GPIO%d\n", DHT_PIN);
  Serial.printf ("  รอบการอ่าน : ทุก %u ms\n", DHT_INTERVAL_MS);
  Serial.println("  ช่วงที่วัดได้ : 0-50 C (+-2 C) / 20-80 %RH (+-5 %)");
  Serial.println("-------------------------------------------------------");
  Serial.println("  ทศนิยมเป็น .0 เสมอ เพราะ DHT11 มีความละเอียด 1 หน่วย");
  Serial.println("  ไม่ใช่อาการเสีย ถ้าต้องการทศนิยมจริงให้เปลี่ยนเป็น DHT22");
  Serial.println("=======================================================\n");

  /* ไม่เรียก dht.read() ตรงนี้ เพราะเซนเซอร์ต้องใช้เวลาตั้งตัวราว 1-2 วินาที
   * หลังจ่ายไฟ การอ่านทันทีมักได้ NaN ปล่อยให้ taskReadDHT อ่านตามรอบดีกว่า */
}


// ================================= LOOP ===================================
void loop() {
  uint32_t now = millis();

  taskHeartbeat(now);   // LED กะพริบ — พิสูจน์ว่า loop ยังวิ่งอยู่
  taskReadDHT(now);     // อ่านเซนเซอร์ตามรอบ
  taskSummary(now);     // สรุปสถิติทุก 30 วินาที

  // ยังไม่มี delay() ในโปรแกรม — พร้อมเพิ่มงานอื่นได้ทันที
}
