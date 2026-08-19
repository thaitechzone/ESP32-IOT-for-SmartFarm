/* ============================================================================
 *  Lab2 : Non-Blocking Programming  (ไม่ใช้ delay() แม้แต่ตัวเดียว)
 *  Board : ESP32 Devkit V2
 * ----------------------------------------------------------------------------
 *  โจทย์ : ทำ 5 งานพร้อมกัน โดยไม่มีงานไหนหยุดรองานอื่น
 *
 *    งานที่ 1  LED heartbeat        กะพริบ 500ms ติด / 2000ms ดับ
 *    งานที่ 2  อ่านปุ่ม 3 ตัว        พร้อม debounce 50ms
 *    งานที่ 3  SW1 -> สลับ Relay1    กดครั้งเดียวเปลี่ยนสถานะ 1 ครั้ง
 *              SW2 -> สลับ Relay2
 *    งานที่ 4  SW3 -> Relay3 ติด 5 วินาทีแล้วดับเอง (จำลองการรดน้ำ)
 *    งานที่ 5  รายงานสถานะทาง Serial ทุก 1 วินาที + นับรอบ loop()
 *
 *  หัวใจของ Lab นี้ : ตัวเลข loop/s ที่พิมพ์ออกมา
 *    - แบบ non-blocking  -> ได้หลายแสนรอบต่อวินาที  (ปุ่มไม่มีทางพลาด)
 *    - ถ้าใส่ delay(2000) -> เหลือไม่ถึง 1 รอบต่อวินาที (ปุ่มหายเกือบหมด)
 *
 *  หลักการ : แทนที่จะ "หยุดรอ" ให้ถามว่า "ถึงเวลาหรือยัง" แล้วผ่านไปทำงานอื่น
 *      if (millis() - lastTime >= INTERVAL) { lastTime = millis(); ...ทำงาน... }
 *
 *  หมายเหตุ : Serial0 ใช้สายร่วมกับ RS485 บนบอร์ดนี้
 *             ให้ถอดสาย A+/B- ออกก่อนทดสอบ Lab นี้
 * ==========================================================================*/

#include "pins_config.h"

// ============================ ค่าคงที่เรื่องเวลา ============================
const uint32_t LED_ON_MS      = 500;    // LED ติดนานเท่าไหร่
const uint32_t LED_OFF_MS     = 2000;   // LED ดับนานเท่าไหร่
const uint32_t DEBOUNCE_MS    = 50;     // ปุ่มต้องนิ่งกี่ ms ถึงเชื่อว่ากดจริง
const uint32_t WATERING_MS    = 5000;   // Relay3 ติดนานเท่าไหร่แล้วดับเอง
const uint32_t REPORT_MS      = 1000;   // รายงานสถานะทุกกี่ ms

/* ---------------------------------------------------------------------------
 *  ทำไมทุกตัวแปรเวลาต้องเป็น uint32_t ?
 *
 *  millis() คืนค่า uint32_t ซึ่งจะวนกลับเป็น 0 ทุก ๆ 49.7 วัน
 *  ถ้าเขียนเงื่อนไขแบบ  (millis() > lastTime + INTERVAL)  -> พังตอนวน
 *  แต่ถ้าเขียนแบบ       (millis() - lastTime >= INTERVAL) -> ถูกต้องเสมอ
 *
 *  เพราะการลบของ unsigned จะ "วนกลับ" ให้เองอัตโนมัติ
 *  เช่น  now = 10, lastTime = 4294967290  ->  10 - 4294967290 = 16  (ถูกต้อง)
 *  ห้ามใช้ int หรือ long ธรรมดาเด็ดขาด เพราะจะได้ค่าติดลบแล้วเงื่อนไขพัง
 * -------------------------------------------------------------------------*/


// ========================= โครงสร้างข้อมูลของปุ่ม ==========================
/* !! ต้องประกาศ struct ตรงนี้ คือ "เหนือฟังก์ชันแรกของไฟล์" เสมอ !!
 *
 * Arduino IDE ไม่ได้ส่งไฟล์ .ino ให้คอมไพเลอร์ตรง ๆ แต่จะสร้าง prototype
 * ของทุกฟังก์ชันให้อัตโนมัติ แล้วแทรกไว้ "เหนือฟังก์ชันแรก" ของไฟล์
 *
 * ถ้าวาง struct ไว้ใต้ฟังก์ชันแรก จะเกิดลำดับแบบนี้:
 *      void buttonUpdate(Button &b, uint32_t now);   <- prototype ที่ IDE แทรกให้
 *      void taskHeartbeat(uint32_t now) { ... }
 *      struct Button { ... };                        <- กว่าจะเจอก็สายไปแล้ว
 *
 * คอมไพเลอร์อ่านจากบนลงล่าง จึงเจอคำว่า Button ก่อนรู้จักมัน -> error
 *      error: variable or field 'buttonUpdate' declared void
 *      error: 'Button' was not declared in this scope
 *
 * กฎจำง่าย : ใน .ino ให้ประกาศ struct / class / enum / typedef
 *            ไว้บนสุดก่อนฟังก์ชันทั้งหมดเสมอ
 *            (หรือย้ายไปไว้ในไฟล์ .h แล้ว #include เข้ามา)
 */
struct Button {
  uint8_t  pin;
  bool     stable;        // สถานะที่ยืนยันแล้ว (true = กำลังกดอยู่)
  bool     lastRaw;       // ค่าดิบครั้งก่อน ใช้จับว่าค่าขยับเมื่อไหร่
  uint32_t changedAt;     // เวลาที่ค่าดิบขยับล่าสุด
  bool     pressedEvent;  // ธง "เพิ่งถูกกด" ยกขึ้น 1 ครั้งต่อการกด 1 ที
};


// ======================= งานที่ 1 : LED Heartbeat ==========================
uint32_t ledAt    = 0;       // เวลาที่ LED เปลี่ยนสถานะครั้งล่าสุด
bool     ledState = false;

void taskHeartbeat(uint32_t now) {
  uint32_t wait = ledState ? LED_ON_MS : LED_OFF_MS;

  if (now - ledAt < wait) return;   // ยังไม่ถึงเวลา -> ออกไปทำงานอื่นทันที

  ledAt    = now;
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}


// ==================== งานที่ 2 : อ่านปุ่มแบบ Debounce ======================
/* ปุ่มกดจริงไม่ได้เปลี่ยนสถานะทันที หน้าสัมผัสจะ "เด้ง" สลับ 0/1 อยู่ 1-20ms
 * ถ้าไม่กรอง กด 1 ครั้งอาจถูกนับเป็น 5-10 ครั้ง รีเลย์จะสลับรัวจนมั่ว
 *
 * วิธีกรองแบบไม่บล็อก : จับเวลาว่าค่านิ่งมานานพอหรือยัง ไม่ใช่ delay() รอ
 *
 * (นิยาม struct Button อยู่ด้านบนสุดของไฟล์ ดูเหตุผลที่นั่น)
 */
Button btnSW1 = { SW1_PIN, false, false, 0, false };
Button btnSW2 = { SW2_PIN, false, false, 0, false };
Button btnSW3 = { SW3_PIN, false, false, 0, false };

void buttonUpdate(Button &b, uint32_t now) {
  bool raw = (digitalRead(b.pin) == LOW);   // Active LOW : กด = 0

  if (raw != b.lastRaw) {
    b.lastRaw   = raw;
    b.changedAt = now;                      // ค่าเพิ่งขยับ เริ่มจับเวลาใหม่
    return;
  }

  // ค่านิ่งครบเวลาแล้ว และต่างจากสถานะที่ยืนยันไว้ -> ยอมรับว่าเปลี่ยนจริง
  if (raw != b.stable && (now - b.changedAt) >= DEBOUNCE_MS) {
    b.stable = raw;
    if (b.stable) b.pressedEvent = true;    // นับเฉพาะ "ขอบขาลง" (ตอนเริ่มกด)
  }
}

/* อ่านธงแล้วเคลียร์ทิ้ง เพื่อให้กดค้างไว้ก็ทำงานแค่ครั้งเดียว
 * ถ้าใช้ digitalRead() ตรง ๆ ไปสั่ง toggle รีเลย์จะสลับรัวตลอดเวลาที่กดค้าง */
bool wasPressed(Button &b) {
  if (!b.pressedEvent) return false;
  b.pressedEvent = false;
  return true;
}


// ==================== งานที่ 3-4 : ควบคุมรีเลย์ ============================
bool relay1On = false;
bool relay2On = false;
bool relay3On = false;

uint32_t wateringStartAt = 0;   // เวลาที่เริ่มรดน้ำ ใช้จับเวลาปิดอัตโนมัติ

void relayWrite(uint8_t pin, bool on) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
}

// งานที่ 4 : ปิด Relay3 อัตโนมัติเมื่อครบเวลา (timeout แบบไม่บล็อก)
void taskWateringTimer(uint32_t now) {
  if (!relay3On) return;                          // ไม่ได้รดอยู่ ไม่ต้องทำอะไร
  if (now - wateringStartAt < WATERING_MS) return; // ยังไม่ครบเวลา

  relay3On = false;
  relayWrite(RELAY3_PIN, false);
  Serial.println(">> Relay3 : ครบ 5 วินาที ปิดอัตโนมัติ");
}


// ==================== งานที่ 5 : รายงานสถานะ ==============================
uint32_t reportAt  = 0;
uint32_t loopCount = 0;   // นับจำนวนรอบ loop() เพื่อพิสูจน์ว่าไม่มีการหยุดรอ

void taskReport(uint32_t now) {
  if (now - reportAt < REPORT_MS) return;
  reportAt = now;

  uint32_t remain = 0;
  if (relay3On) remain = (WATERING_MS - (now - wateringStartAt)) / 1000 + 1;

  Serial.printf("[%6us] loop/s:%-7u | R1:%-3s R2:%-3s R3:%-3s",
                now / 1000,
                loopCount,
                relay1On ? "ON" : "OFF",
                relay2On ? "ON" : "OFF",
                relay3On ? "ON" : "OFF");

  if (relay3On) Serial.printf(" (เหลือ %us)", remain);
  Serial.println();

  loopCount = 0;   // เริ่มนับใหม่ทุกวินาที
}


// ================================ SETUP ===================================
void setup() {
  Serial.begin(115200);

  /* สำคัญ : บน ESP32 core 3.x ต้อง pinMode(OUTPUT) ก่อน แล้วค่อย digitalWrite()
   * ถ้าสลับลำดับ digitalWrite() จะไม่ทำงานเลย (core จะข้ามไปแล้วขึ้น log error)
   * ทำให้ขาถูกตั้งเป็น OUTPUT ขณะที่ latch ยังเป็น LOW
   * ซึ่งรีเลย์ Active LOW แปลว่า "สั่งทำงาน" -> รีเลย์ติดทุกตัวตอนบูต
   * (core 2.x เขียนสลับกันได้ แต่ลำดับนี้ปลอดภัยกับทั้งสองเวอร์ชัน) */
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, RELAY_OFF);   // RELAY_OFF = HIGH
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);

  // ปุ่มทั้ง 3 ตัวมี R pull-up 10k บนบอร์ดแล้ว จึงใช้ INPUT เปล่า
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(SW3_PIN, INPUT);

  pinMode(LED_PIN, OUTPUT);

  Serial.println("\n===========================================");
  Serial.println("  Lab2 : Non-Blocking (ไม่มี delay ในโปรแกรม)");
  Serial.println("===========================================");
  Serial.println("  SW1 -> สลับ Relay1");
  Serial.println("  SW2 -> สลับ Relay2");
  Serial.println("  SW3 -> Relay3 ติด 5 วินาทีแล้วดับเอง");
  Serial.println("-------------------------------------------");
  Serial.println("  ลองกดปุ่มตอนไหนก็ได้ ระบบตอบสนองทันทีเสมอ");
  Serial.println("===========================================\n");
}


// ================================= LOOP ===================================
void loop() {
  /* เรียก millis() ครั้งเดียวต่อรอบ แล้วส่งต่อให้ทุกงานใช้ค่าเดียวกัน
   * ทำให้ทุกงานมองเห็นเวลาตรงกัน และเร็วกว่าเรียกซ้ำหลายรอบ */
  uint32_t now = millis();

  loopCount++;

  // ----- งานที่ 1 : LED heartbeat -----
  taskHeartbeat(now);

  // ----- งานที่ 2 : อ่านปุ่มทั้ง 3 ตัว -----
  buttonUpdate(btnSW1, now);
  buttonUpdate(btnSW2, now);
  buttonUpdate(btnSW3, now);

  // ----- งานที่ 3 : ปุ่มสลับรีเลย์ -----
  if (wasPressed(btnSW1)) {
    relay1On = !relay1On;
    relayWrite(RELAY1_PIN, relay1On);
    Serial.printf(">> SW1 กด : Relay1 = %s\n", relay1On ? "ON" : "OFF");
  }

  if (wasPressed(btnSW2)) {
    relay2On = !relay2On;
    relayWrite(RELAY2_PIN, relay2On);
    Serial.printf(">> SW2 กด : Relay2 = %s\n", relay2On ? "ON" : "OFF");
  }

  // ----- งานที่ 4 : ปุ่มสั่งรดน้ำแบบตั้งเวลา -----
  if (wasPressed(btnSW3)) {
    relay3On        = true;
    wateringStartAt = now;              // กดซ้ำ = เริ่มนับเวลาใหม่
    relayWrite(RELAY3_PIN, true);
    Serial.println(">> SW3 กด : Relay3 เริ่มรดน้ำ 5 วินาที");
  }
  taskWateringTimer(now);

  // ----- งานที่ 5 : รายงานสถานะ -----
  taskReport(now);

  /* ไม่มี delay() ในโปรแกรมนี้เลย
   * loop() จึงวิ่งจบรอบแล้ววนใหม่ทันที หลายแสนรอบต่อวินาที
   * ทุกงานจึงได้รับการตรวจสอบถี่มาก ไม่มีทางพลาดการกดปุ่ม */
}
