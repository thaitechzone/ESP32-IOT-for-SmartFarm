/* ============================================================================
 *  Lab11 : อ่านเซนเซอร์ความชื้นดิน RS485 (Modbus RTU) แสดงผลบน OLED
 *          เวอร์ชันใช้ไลบรารี ModbusMaster
 *  Board  : ESP32 Devkit V2
 *  Sensor : RS-SD-N01-TR  (Shandong Renke)  Soil Moisture Sensor Type 485
 *           อ้างอิงคู่มือ Soil-Moisture-Sensor-485-Type-Instruction-Manual.pdf
 * ----------------------------------------------------------------------------
 *  สเปกเซนเซอร์ (คู่มือหัวข้อ 1.3)
 *    ไฟเลี้ยง       : 5~24V DC  กินไฟสูงสุด 30 mA
 *    ช่วงวัด        : 0~100 %
 *    ความแม่นยำ     : +-3 % ในช่วง 0-53 %   และ +-5 % ในช่วง 53-100 %
 *    เวลาตอบสนอง   : น้อยกว่า 1 วินาที (Lab นี้อ่านทุก 2 วินาที จึงเหลือเฟือ)
 *    พื้นที่ที่วัดได้  : ทรงกระบอกรัศมี 7 ซม. รอบเข็มกลาง
 *    ระดับกันน้ำ    : IP68 จุ่มน้ำได้
 *
 *  พารามิเตอร์การสื่อสาร (คู่มือหัวข้อ 5.1 - 5.3)
 *    Slave ID   : 0x01 (ค่าจากโรงงาน)
 *    Baud rate  : 9600 8N1 ไม่มี parity (ตั้งเป็น 2400 / 4800 ได้ด้วย)
 *    Protocol   : Modbus RTU  ใช้ฟังก์ชัน 0x03 เท่านั้น
 *    รีจิสเตอร์  : 0x0001 (PLC address 40002) = ค่าความชื้น อ่านอย่างเดียว
 *                 ค่าที่ส่งมาถูกคูณ 100 ไว้แล้ว
 *    ทางกายภาพ  : RS485 ผ่านชิป MAX13487 ต่อกับ Serial0 (UART0) ของบอร์ด
 *
 *  ตัวอย่างจากคู่มือหัวข้อ 5.4.1
 *    ส่งไป   01 03 00 01 00 01 D5 CA
 *    ตอบกลับ 01 03 02 0F 2D 7D A9   ->  0x0F2D = 3885  ->  38.85 %
 *
 *  ไลบรารีที่ต้องติดตั้ง
 *    - "ModbusMaster" by Doc Walker (4-20ma)     <- ตัวใหม่ของ Lab นี้
 *        ติดตั้งผ่าน Library Manager ได้เลย ค้นหาคำว่า ModbusMaster
 *        หรือโหลดจาก https://github.com/4-20ma/ModbusMaster
 *    - Adafruit SSD1306 + Adafruit GFX (มีแล้วจาก Lab4)
 *
 *  MAX13487 เป็นชิปแบบ AutoDirection
 *    ไม่ต้องมีขา DE/RE ให้ซอฟต์แวร์ควบคุมทิศทางรับ-ส่ง
 *    ปกติการใช้ ModbusMaster กับ MAX485 ต้องเขียน preTransmission()
 *    เพื่อสั่ง DE เป็น HIGH ก่อนส่ง และ postTransmission() สั่งกลับเป็น LOW
 *    บอร์ดนี้ไม่ต้องทำเลย เพราะชิปสลับทิศทางเองเมื่อตรวจพบข้อมูลที่ขา DI
 *
 *  !!!!!!!!!!!!!!!!  คำเตือนสำคัญที่สุดของ Lab นี้  !!!!!!!!!!!!!!!!
 *  RS485 บนบอร์ดนี้ใช้ UART0 (GPIO1/GPIO3) ซึ่งเป็นตัวเดียวกับพอร์ต USB
 *
 *    1. ห้ามใช้ Serial.print() เพื่อ debug เด็ดขาด
 *       ทุกตัวอักษรจะถูกส่งออกสาย RS485 ไปกวนเซนเซอร์
 *       Lab นี้จึงยึด "จอ OLED" เป็นช่องทางแสดงผลหลัก
 *
 *    2. ถอดสาย A+/B- ออกก่อนอัปโหลดโปรแกรมทุกครั้ง
 *
 *    3. ถ้าต้องการ debug ทาง Serial ให้ใช้ UART2 ที่ขาว่างแทน
 *       ต่อ USB-TTL เข้าที่ GPIO25 (TX) และ GPIO26 (RX)
 *
 *  การต่อสาย (คู่มือหัวข้อ 3.3 ตารางสีสาย)
 *    สายแดง   Power 5~24V  -> ไฟเลี้ยง 12V ของบอร์ด
 *    สายดำ    Power GND    -> GND ร่วมกับบอร์ด
 *    สายเหลือง 485-A       -> เทอร์มินัล A+ ของบอร์ด
 *    สายขาว   485-B        -> เทอร์มินัล B- ของบอร์ด
 *
 *    สายเขียว (ชีลด์) -> ต่อเข้า "ขั้วลบ" ของไฟเลี้ยง คือ GND
 *
 *      !! สายเขียวเส้นนี้สำคัญมากและมองข้ามกันบ่อยที่สุด !!
 *      คู่มือระบุว่าเส้นนี้เป็นตัวเลือกโหมดการทำงานของเซนเซอร์
 *          ต่อเข้าขั้วลบ (GND) -> โหมดวัดค่าปกติ (acquisition mode)
 *          ต่อเข้าขั้วบวก (+V) -> โหมดตั้งค่า (setting mode)
 *      ถ้าปล่อยลอยหรือเผลอต่อเข้าขั้วบวก เซนเซอร์จะเข้าโหมดตั้งค่า
 *      แล้วไม่ตอบคำสั่ง Modbus ตามปกติ อาการคือขึ้น No Reply ตลอด
 *      ทั้งที่สาย A/B และไฟเลี้ยงถูกต้องหมดแล้ว
 *
 *    จัมเปอร์ SW Mode บนบอร์ด ต้องเสียบ/กด ให้อยู่ตำแหน่ง RS485
 *    สาย A และ B ห้ามสลับกันเด็ดขาด (คู่มือหัวข้อ 3.2)
 *    ถ้าเดินสายยาวมากหรือมีอุปกรณ์หลายตัว ควรใส่ตัวต้านทานปลายสาย 120 โอห์ม
 * ==========================================================================*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ModbusMaster.h>
#include "pins_config.h"


// ======================== ค่าที่ตั้งได้ (แก้ตรงนี้) ========================
#define RS485          Serial          // UART0 = GPIO1 (TX) / GPIO3 (RX)
constexpr uint32_t RS485_BAUDRATE = 9600;

constexpr uint8_t  SLAVE_ID  = 0x01;   // ที่อยู่จากโรงงาน (คู่มือหัวข้อ 5.2)
constexpr uint16_t REG_START = 0x0001; // !! ค่าความชื้นอยู่ที่ 0x0001 ไม่ใช่ 0x0000
constexpr uint8_t  REG_QTY   = 1;      // มีรีจิสเตอร์เดียว อ่านทีละ 1 พอ

/* เลือกฟังก์ชันที่ใช้อ่าน (0 = ใช้ 0x03, 1 = ใช้ 0x04)
 * คู่มือหัวข้อ 5.2 ระบุชัดว่า "This transmitter only uses function code 0x03"
 * จึงต้องเป็น 0 เสมอสำหรับเซนเซอร์รุ่นนี้
 * ปุ่มนี้เหลือไว้เผื่อนำโค้ดไปใช้กับเซนเซอร์ยี่ห้ออื่นที่ใช้ 0x04 */
#define USE_FUNC_04    0

/* ตัวหารสำหรับแปลงค่าดิบเป็นเปอร์เซ็นต์
 * คู่มือหัวข้อ 5.3 ระบุว่า "Upload data is 100 times the actual value"
 * ค่าที่ส่งมาจึงถูกคูณ 100 ไว้แล้ว ต้องหาร 100 กลับ
 *
 * ตัวอย่างจากคู่มือหัวข้อ 5.4.1
 *   คำตอบ  01 03 02 0F 2D 7D A9
 *   ข้อมูล  0x0F2D = 3885  ->  3885 / 100 = 38.85 %
 *
 * ค่าดิบแสดงอยู่บนจอด้วย ใช้ตรวจสอบได้ทันทีว่าตั้งถูกไหม */
constexpr float MOISTURE_DIVISOR = 100.0;

/* เปิดโหมดสแกนรีจิสเตอร์ตอนบูต (1 = เปิด, 0 = ปิด)
 * ปิดไว้เป็นค่าเริ่มต้น เพราะคู่มือระบุตำแหน่งรีจิสเตอร์ไว้ชัดเจนแล้ว ไม่ต้องเดา
 * เปิดเมื่อต้องการยืนยันการเชื่อมต่อ หรือเมื่อนำโค้ดไปใช้กับเซนเซอร์ตัวอื่น */
#define SCAN_ON_BOOT   0
constexpr uint16_t SCAN_FROM = 0x0000;
constexpr uint16_t SCAN_TO   = 0x0009;

/* โหมดค้นหาอัตโนมัติ (1 = เปิด, 0 = ปิด)
 * ใช้เมื่ออ่านค่าไม่ได้และสงสัยว่า Slave ID หรือ baud rate ถูกเปลี่ยนมาจากโรงงาน
 * จะไล่ลองทุก baud ที่เซนเซอร์รุ่นนี้รองรับ (2400/4800/9600 ตามคู่มือหัวข้อ 5.1)
 * คูณกับ Slave ID 1 ถึง 5 แล้วรายงานคู่ที่ตอบกลับมาบนจอ
 *
 * ใช้เวลานานสุดราว 30 วินาที เพราะ ModbusMaster รอ timeout 2 วินาทีต่อครั้ง
 * ถ้าไล่ครบแล้วยังไม่เจอ แปลว่าปัญหาอยู่ที่ไฟเลี้ยงหรือการต่อสาย ไม่ใช่ค่าพารามิเตอร์ */
#define AUTO_DETECT_ON_BOOT  1
constexpr uint8_t DETECT_ID_MAX = 5;

/* debug ทาง UART2 (ต้องต่อ USB-TTL ที่ GPIO25/26 ถึงจะเห็น)
 * ห้ามเปลี่ยนไปใช้ Serial เด็ดขาด เพราะจะไปชนกับ RS485 */
#define DBG_ENABLE     1
constexpr int8_t DBG_TX_PIN = 25;
constexpr int8_t DBG_RX_PIN = 26;

#if DBG_ENABLE
  #define DBG(...)  Serial2.printf(__VA_ARGS__)
#else
  #define DBG(...)
#endif

// ----- รอบเวลาการทำงาน -----
constexpr uint32_t READ_INTERVAL_MS = 2000;   // อ่านเซนเซอร์ทุกกี่ ms
constexpr uint32_t OLED_INTERVAL_MS = 500;


// ============================ โครงสร้างข้อมูล =============================
/* ประกาศ struct ไว้เหนือฟังก์ชันแรกของไฟล์เสมอ
 * เพราะ Arduino IDE แทรก prototype ของทุกฟังก์ชันไว้เหนือฟังก์ชันแรก */
struct ScanHit {
  uint16_t reg;
  uint16_t val;
  uint8_t  fc;     // ตอบกลับด้วยฟังก์ชันไหน (3 หรือ 4)
};


// ============================== อ็อบเจกต์หลัก =============================
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
ModbusMaster     node;


// ============================== ตัวแปรสถานะ ===============================
uint32_t readAt = 0, oledAt = 0;

uint16_t rawValue   = 0;                      // ค่าดิบจากรีจิสเตอร์
float    moisture   = 0.0;                    // ค่าที่แปลงเป็นเปอร์เซ็นต์แล้ว
uint8_t  lastResult = ModbusMaster::ku8MBResponseTimedOut;

uint32_t okCount  = 0;
uint32_t errCount = 0;

ScanHit  scanHits[10];
uint8_t  scanCount = 0;


// ======================= ตัวช่วยจัดการ RS485 บนบอร์ดนี้ ===================
/* ModbusMaster เปิดให้ฝังโค้ดของเราเข้าไปได้ 2 จุด
 *   preTransmission()  - เรียกก่อนเริ่มส่งข้อมูล
 *   postTransmission() - เรียกหลังส่งข้อมูลเสร็จ (หลัง flush แล้ว)
 *
 * ปกติสองจุดนี้มีไว้สั่งขา DE/RE ของชิป MAX485
 * บอร์ดนี้ใช้ MAX13487 ที่สลับทิศทางเอง จึงไม่ต้องใช้ preTransmission เลย
 *
 * แต่ postTransmission ยังมีประโยชน์อยู่ ใช้ทิ้ง "เสียงสะท้อน"
 *
 * เสียงสะท้อน (echo) คืออะไร
 *   วงจร RS485 บางแบบจะส่งข้อมูลที่เราเพิ่งส่งออกไป กลับเข้ามาที่ขา RX ด้วย
 *   ModbusMaster จะเข้าใจผิดว่านั่นคือคำตอบจากเซนเซอร์ แล้วรายงาน CRC Error
 *   อาการคือ "ต่อสายถูกทุกอย่างแต่ขึ้น CRC Error ตลอด"
 *
 *   ตรงนี้จึงรอให้ไบต์สุดท้ายสะท้อนกลับมาครบก่อน แล้วล้างบัฟเฟอร์รับทิ้ง
 *   ถ้าบอร์ดไม่สะท้อน คำสั่งนี้ก็ไม่ทำอะไร ไม่มีผลเสีย
 *
 *   ปลอดภัยเพราะมาตรฐาน Modbus บังคับให้อุปกรณ์ลูกต้องเงียบอย่างน้อย
 *   3.5 ตัวอักษร (ราว 3.6 ms ที่ 9600) ก่อนตอบกลับ
 *   การล้างบัฟเฟอร์ภายใน 1.2 ms จึงไม่มีทางไปกินคำตอบจริง */
void postTransmission() {
  delayMicroseconds(1200);                 // ราว 1 ไบต์ที่ 9600 bps
  while (RS485.available()) RS485.read();
}

/* แปลรหัสผลลัพธ์ของ ModbusMaster เป็นข้อความสั้นสำหรับแสดงบนจอ
 * จอ OLED ใช้ฟอนต์ในตัวของ Adafruit GFX ซึ่งมีเฉพาะตัวอักษรภาษาอังกฤษ
 * ข้อความบนจอจึงต้องเป็นอังกฤษทั้งหมด ภาษาไทยจะกลายเป็นสัญลักษณ์มั่ว */
/* เรียกอ่านรีจิสเตอร์ โดยเลือกฟังก์ชันได้
 * แยกออกมาเป็นฟังก์ชันเดียว จะได้ไม่ต้องเขียน if ซ้ำหลายที่ */
uint8_t mbRead(uint16_t reg, uint8_t qty, bool useFc04) {
  return useFc04 ? node.readInputRegisters(reg, qty)
                 : node.readHoldingRegisters(reg, qty);
}

const char *mbResultShort(uint8_t r) {
  switch (r) {
    case ModbusMaster::ku8MBSuccess:            return "OK";
    case ModbusMaster::ku8MBIllegalFunction:    return "Illegal Func";
    case ModbusMaster::ku8MBIllegalDataAddress: return "Bad Register";
    case ModbusMaster::ku8MBIllegalDataValue:   return "Bad Value";
    case ModbusMaster::ku8MBSlaveDeviceFailure: return "Slave Failure";
    case ModbusMaster::ku8MBInvalidSlaveID:     return "Wrong ID";
    case ModbusMaster::ku8MBInvalidFunction:    return "Invalid Func";
    case ModbusMaster::ku8MBResponseTimedOut:   return "No Reply";
    case ModbusMaster::ku8MBInvalidCRC:         return "CRC Error";
    default:                                    return "Unknown";
  }
}

// คำอธิบายเต็มภาษาไทย ใช้กับ DBG ที่ออกทาง Serial2 (เทอร์มินัลรองรับ UTF-8)
const char *mbResultFull(uint8_t r) {
  switch (r) {
    case ModbusMaster::ku8MBSuccess:
      return "สำเร็จ";
    case ModbusMaster::ku8MBIllegalDataAddress:
      return "ไม่มีรีจิสเตอร์นี้ - ลองใช้โหมดสแกนหาตำแหน่งที่ถูกต้อง";
    case ModbusMaster::ku8MBIllegalFunction:
      return "เซนเซอร์ไม่รองรับฟังก์ชัน 0x03 - ลองเปลี่ยนเป็น 0x04";
    case ModbusMaster::ku8MBInvalidSlaveID:
      return "คำตอบมาจาก Slave ID อื่น - ตรวจว่ามีอุปกรณ์อื่นบนสายเดียวกันไหม";
    case ModbusMaster::ku8MBResponseTimedOut:
      return "ไม่มีคำตอบ - ตรวจสาย A+/B-, ไฟเลี้ยง, จัมเปอร์ SW Mode และ Slave ID";
    case ModbusMaster::ku8MBInvalidCRC:
      return "ข้อมูลเพี้ยน - อาจสลับสาย A/B, baud rate ไม่ตรง หรือเจอเสียงสะท้อน";
    default:
      return "ไม่ทราบสาเหตุ";
  }
}


// ========================== งานที่ 1 : อ่านเซนเซอร์ ========================
void taskReadSensor(uint32_t now) {
  if (now - readAt < READ_INTERVAL_MS) return;
  readAt = now;

  /* เทียบกับเวอร์ชันที่เขียน Modbus เอง ตรงนี้เหลือแค่ 2 บรรทัด
   * ไลบรารีจัดการให้หมดทั้งการสร้างเฟรม คำนวณ CRC ส่ง รอคำตอบ และตรวจสอบ */
  uint8_t r = mbRead(REG_START, REG_QTY, USE_FUNC_04);
  lastResult = r;

  if (r == node.ku8MBSuccess) {
    okCount++;
    rawValue = node.getResponseBuffer(0);     // ดึงค่าจากรีจิสเตอร์ตัวแรก
    moisture = rawValue / MOISTURE_DIVISOR;
    DBG("OK  raw=%u (0x%04X)  moisture=%.1f %%\n", rawValue, rawValue, moisture);
  } else {
    errCount++;
    DBG("ERR 0x%02X %s : %s\n", r, mbResultShort(r), mbResultFull(r));
  }

  /* ล้างบัฟเฟอร์คำตอบทุกครั้ง ไม่งั้นค่าเก่าจะค้างอยู่
   * ถ้ารอบถัดไปอ่านไม่สำเร็จแล้วเผลอไปอ่านบัฟเฟอร์ จะได้ค่าเก่ามาโดยไม่รู้ตัว */
  node.clearResponseBuffer();
}


// ========================== งานที่ 2 : แสดงผล OLED ========================
void taskDisplay(uint32_t now) {
  if (now - oledAt < OLED_INTERVAL_MS) return;
  oledAt = now;

  oled.clearDisplay();

  // ---- หัวข้อ ----
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("SOIL MOISTURE");
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  if (lastResult == ModbusMaster::ku8MBSuccess) {
    // ---- ค่าความชื้นตัวใหญ่ ----
    oled.setTextSize(2);
    oled.setCursor(0, 14);
    oled.printf("%.1f %%", moisture);

    // ---- แถบระดับความชื้น มองแวบเดียวรู้ว่าดินแห้งหรือแฉะ ----
    int w = (int)(moisture * 1.26);            // 100 % = 126 พิกเซล
    if (w < 0)   w = 0;
    if (w > 126) w = 126;
    oled.drawRect(0, 33, 128, 7, SSD1306_WHITE);
    oled.fillRect(1, 34, w, 5, SSD1306_WHITE);

    // ---- ค่าดิบ ใช้ตรวจสอบว่าตั้งตัวหารถูกไหม ----
    oled.setTextSize(1);
    oled.setCursor(0, 44);
    oled.printf("raw %u (0x%04X)", rawValue, rawValue);
  } else {
    // ---- แสดงสาเหตุที่อ่านไม่ได้ ----
    oled.setTextSize(2);
    oled.setCursor(0, 14);
    oled.print("-- ERR");

    oled.setTextSize(1);
    oled.setCursor(0, 34);
    oled.print(mbResultShort(lastResult));
    oled.setCursor(0, 44);
    oled.printf("code 0x%02X", lastResult);
  }

  // ---- แถบล่าง : พารามิเตอร์การเชื่อมต่อและสถิติสำเร็จ/ล้มเหลว ----
  oled.setTextSize(1);
  oled.setCursor(0, 55);
  oled.printf("ID:%02X %u  %lu/%lu",
              SLAVE_ID, (unsigned)RS485_BAUDRATE,
              (unsigned long)okCount, (unsigned long)errCount);

  oled.display();
}


// ===================== โหมดสแกนหาตำแหน่งรีจิสเตอร์ ========================
/* ไล่อ่านรีจิสเตอร์ทีละตัว เก็บเฉพาะตัวที่ตอบกลับมา
 * ใช้ครั้งแรกตอนไม่รู้ว่าเซนเซอร์วางค่าความชื้นไว้ที่รีจิสเตอร์ไหน
 * ทำงานตอนบูตครั้งเดียว จึงยอมให้บล็อกได้
 *
 * หมายเหตุ : ถ้าเซนเซอร์ไม่ตอบเลย ModbusMaster จะรอนานถึง 2 วินาทีต่อรีจิสเตอร์
 * สแกน 10 ตัวจึงอาจใช้เวลาถึง 20 วินาที ระหว่างนั้นจอจะค้างที่เลขเดิม
 * ถ้าเจอแบบนั้นแปลว่าเป็นปัญหาสายหรือ Slave ID ไม่ใช่ปัญหารีจิสเตอร์ */
#if SCAN_ON_BOOT
void scanPass(bool useFc04) {
  const uint8_t fcNum = useFc04 ? 4 : 3;

  for (uint16_t reg = SCAN_FROM; reg <= SCAN_TO; reg++) {
    // ---- แสดงความคืบหน้าบนจอ ----
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.printf("SCANNING  FC 0x%02X", fcNum);
    oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(0, 18);
    oled.printf("0x%04X", reg);
    oled.setTextSize(1);
    oled.setCursor(0, 44);
    oled.printf("found %u", scanCount);
    oled.display();

    uint8_t r = mbRead(reg, 1, useFc04);
    if (r == node.ku8MBSuccess &&
        scanCount < (sizeof(scanHits) / sizeof(scanHits[0]))) {
      scanHits[scanCount].reg = reg;
      scanHits[scanCount].val = node.getResponseBuffer(0);
      scanHits[scanCount].fc  = fcNum;
      DBG("SCAN FC%02X 0x%04X = %u\n", fcNum, reg, scanHits[scanCount].val);
      scanCount++;
    } else {
      DBG("SCAN FC%02X 0x%04X -> %s\n", fcNum, reg, mbResultShort(r));
    }
    node.clearResponseBuffer();
  }
}

void scanRegisters() {
  scanCount = 0;

  scanPass(false);                     // รอบแรกลองฟังก์ชัน 0x03 ก่อน

  /* ถ้ารอบแรกไม่เจออะไรเลย ค่อยลองฟังก์ชัน 0x04
   * ไม่ลองทั้งสองแบบตั้งแต่แรกเพื่อไม่ให้เสียเวลาโดยไม่จำเป็น
   * เพราะเซนเซอร์ที่ไม่ตอบจะกินเวลา timeout 2 วินาทีต่อรีจิสเตอร์ */
  if (scanCount == 0) {
    DBG("FC 0x03 ไม่พบอะไรเลย ลองใหม่ด้วย FC 0x04\n");
    scanPass(true);
  }

  // ---- สรุปผลค้างไว้ให้อ่าน ----
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.printf("SCAN RESULT (%u)", scanCount);
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  if (scanCount == 0) {
    oled.setCursor(0, 20);
    oled.println("No register found");
    oled.setCursor(0, 32);
    oled.println("check wiring/baud/ID");
  } else {
    /* แสดงเลขฟังก์ชันด้วย (F3 หรือ F4) เพราะถ้าเจอด้วย F4
     * ต้องกลับไปตั้ง USE_FUNC_04 เป็น 1 ไม่งั้นการอ่านปกติจะยังล้มเหลว */
    for (uint8_t i = 0; i < scanCount && i < 5; i++) {
      oled.setCursor(0, 14 + i * 10);
      oled.printf("%04X F%u %-5u %.1f%%",
                  scanHits[i].reg, scanHits[i].fc, scanHits[i].val,
                  scanHits[i].val / MOISTURE_DIVISOR);
    }
  }
  oled.display();

  /* ค้างหน้าสรุปไว้ 8 วินาทีให้จดค่าได้ทัน
   * ตรงนี้บล็อกโดยตั้งใจ เพราะยังไม่ได้เข้าสู่การทำงานปกติ */
  delay(8000);
}
#endif


// ==================== โหมดค้นหา Slave ID และ baud rate ====================
/* ไล่ลองทุกคู่ของ baud rate กับ Slave ID จนกว่าจะเจอตัวที่ตอบกลับมา
 * ใช้ตอนอ่านค่าไม่ได้ เพื่อแยกว่าเป็นปัญหา "ค่าพารามิเตอร์" หรือ "การต่อสาย"
 *
 *   เจอ      -> ค่าพารามิเตอร์ผิด แก้ SLAVE_ID / RS485_BAUDRATE ตามที่จอบอก
 *   ไม่เจอเลย -> ปัญหาอยู่ที่ไฟเลี้ยง สายเขียว จัมเปอร์ SW Mode หรือสาย A/B
 *
 * ทำงานตอนบูตครั้งเดียว จึงยอมให้บล็อกได้ */
#if AUTO_DETECT_ON_BOOT
void autoDetect() {
  const uint32_t bauds[] = { 9600, 4800, 2400 };   // ตามที่คู่มือระบุว่ารองรับ

  for (uint8_t bi = 0; bi < 3; bi++) {
    /* เปลี่ยน baud rate โดยไม่ต้องปิดแล้วเปิดพอร์ตใหม่
     * updateBaudRate() เป็นฟังก์ชันเฉพาะของ ESP32 core */
    RS485.updateBaudRate(bauds[bi]);

    for (uint8_t id = 1; id <= DETECT_ID_MAX; id++) {
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setCursor(0, 0);
      oled.print("AUTO DETECT");
      oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
      oled.setTextSize(2);
      oled.setCursor(0, 16);
      oled.printf("%lu", (unsigned long)bauds[bi]);
      oled.setTextSize(1);
      oled.setCursor(0, 38);
      oled.printf("trying ID %02X", id);
      oled.setCursor(0, 52);
      oled.print("wait up to 30s...");
      oled.display();

      node.begin(id, RS485);
      uint8_t r = mbRead(REG_START, 1, USE_FUNC_04);
      uint16_t v = node.getResponseBuffer(0);
      node.clearResponseBuffer();

      DBG("DETECT baud %lu id 0x%02X -> %s\n",
          (unsigned long)bauds[bi], id, mbResultShort(r));

      if (r != node.ku8MBSuccess) continue;

      // ---------- เจอแล้ว ----------
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setCursor(0, 0);
      oled.print("SENSOR FOUND!");
      oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
      oled.setCursor(0, 16);
      oled.printf("baud    %lu", (unsigned long)bauds[bi]);
      oled.setCursor(0, 26);
      oled.printf("slaveID 0x%02X", id);
      oled.setCursor(0, 36);
      oled.printf("reg     0x%04X", REG_START);
      oled.setCursor(0, 46);
      oled.printf("raw %u  = %.1f%%", v, v / MOISTURE_DIVISOR);
      oled.setCursor(0, 56);
      oled.print("set these in code");
      oled.display();

      DBG("พบเซนเซอร์ : baud %lu  ID 0x%02X  raw %u\n",
          (unsigned long)bauds[bi], id, v);

      delay(10000);        // ค้างไว้ให้จดค่าได้ทัน
      return;              // ใช้ค่าที่เจอทำงานต่อได้เลยในรอบนี้
    }
  }

  // ---------- ไล่ครบแล้วไม่เจอ ----------
  RS485.updateBaudRate(RS485_BAUDRATE);   // คืนค่าเดิม
  node.begin(SLAVE_ID, RS485);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("NOT FOUND");
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  oled.setCursor(0, 15);
  oled.println("1 Power 12V in?");
  oled.setCursor(0, 25);
  oled.println("2 Green wire to GND");
  oled.setCursor(0, 35);
  oled.println("3 SW Mode = RS485");
  oled.setCursor(0, 45);
  oled.println("4 A+/B- swapped?");
  oled.setCursor(0, 55);
  oled.println("5 Unplug USB, use 12V");
  oled.display();

  DBG("ไล่ครบทุก baud และ ID แล้วไม่พบเซนเซอร์ - ตรวจไฟเลี้ยงและการต่อสาย\n");
  delay(10000);
}
#endif


// ================================ SETUP ===================================
void setup() {
  /* เปิด UART0 ที่ 9600 สำหรับ Modbus โดยเฉพาะ
   * ระบุขา RX=GPIO3 TX=GPIO1 ให้ชัดเจน แม้จะเป็นค่าเริ่มต้นอยู่แล้ว
   * เขียนไว้เพื่อเตือนว่าขาคู่นี้ใช้ร่วมกับ USB */
  RS485.begin(RS485_BAUDRATE, SERIAL_8N1, 3, 1);

#if DBG_ENABLE
  Serial2.begin(115200, SERIAL_8N1, DBG_RX_PIN, DBG_TX_PIN);
  DBG("\n\n=== Lab11 : RS485 Soil Moisture (ModbusMaster) ===\n");
  DBG("Slave ID 0x%02X  baud %u  reg 0x%04X qty %u\n",
      SLAVE_ID, RS485_BAUDRATE, REG_START, REG_QTY);
#endif

  /* ผูก ModbusMaster เข้ากับพอร์ตอนุกรมและที่อยู่ของเซนเซอร์
   * ต้องเรียก RS485.begin() ให้เรียบร้อยก่อนเสมอ ไลบรารีไม่ได้เปิดพอร์ตให้ */
  node.begin(SLAVE_ID, RS485);

  /* ไม่ต้องตั้ง preTransmission เพราะ MAX13487 สลับทิศทางเอง
   * ตั้งเฉพาะ postTransmission ไว้ทิ้งเสียงสะท้อน */
  node.postTransmission(postTransmission);

  // ---------- จอ OLED ----------
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    /* ห้าม Serial.println() แจ้งเตือนตรงนี้ เพราะจะไปกวนสาย RS485
     * ใช้ LED ออนบอร์ดกะพริบแทน แล้วทำงานต่อ */
    pinMode(LED_PIN, OUTPUT);
    for (uint8_t i = 0; i < 6; i++) {
      digitalWrite(LED_PIN, i % 2);
      delay(150);
    }
    DBG("OLED not found\n");
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 20);
  oled.println("RS485 Soil Sensor");
  oled.setCursor(0, 32);
  oled.printf("ID:%02X  %u 8N1", SLAVE_ID, RS485_BAUDRATE);
  oled.setCursor(0, 44);
  oled.println("lib: ModbusMaster");
  oled.display();
  delay(1200);

#if AUTO_DETECT_ON_BOOT
  autoDetect();       // ไล่หา baud rate และ Slave ID ที่ถูกต้อง
#endif

#if SCAN_ON_BOOT
  scanRegisters();    // ไล่หาตำแหน่งรีจิสเตอร์
#endif
}


// ================================= LOOP ===================================
void loop() {
  uint32_t now = millis();

  taskReadSensor(now);
  taskDisplay(now);

  /* ไม่มี delay() ใน loop
   * แต่ node.readHoldingRegisters() มีการรอคำตอบอยู่ข้างใน
   *
   *   กรณีปกติที่เซนเซอร์ตอบทันที  ใช้เวลาราว 15-20 ms ทุก 2 วินาที
   *   กรณีเซนเซอร์ไม่ตอบ           รอจนครบ timeout ของไลบรารีคือ 2 วินาที
   *
   * ค่า timeout นี้ถูกกำหนดตายตัวไว้ในไฟล์ ModbusMaster.h
   *     static const uint16_t ku16MBResponseTimeout = 2000;
   * เป็นตัวแปร private จึงแก้จากโปรแกรมเราไม่ได้
   * ถ้าต้องการให้ตอบสนองเร็วขึ้นตอนเซนเซอร์หลุด ต้องแก้ในไฟล์ไลบรารีเอง
   * (สำหรับ Lab นี้ไม่มีผล เพราะเมื่อสายปกติจะไม่เคยแตะ timeout เลย) */
}
