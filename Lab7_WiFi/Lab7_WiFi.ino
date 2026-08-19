/* ============================================================================
 *  Lab7 : เชื่อมต่อ WiFi และแสดง IP Address บนจอ OLED
 *  Board : ESP32 Devkit V2
 * ----------------------------------------------------------------------------
 *  ผลลัพธ์บนจอ
 *      DEMO WIFI
 *      Connected
 *      IP: 192.168.1.45
 *      SSID: MyNetwork
 *      RSSI: -62 dBm (Good)
 *
 *  ไลบรารี WiFi.h มาพร้อม ESP32 Board Package อยู่แล้ว ไม่ต้องติดตั้งเพิ่ม
 *  ส่วนจอ OLED ใช้ Adafruit SSD1306 + Adafruit GFX (ติดตั้งไว้แล้วตั้งแต่ Lab4)
 *
 *  !! สำคัญที่สุด !!  ESP32 รองรับเฉพาะ WiFi ย่านความถี่ 2.4 GHz เท่านั้น
 *  ต่อกับเราเตอร์ย่าน 5 GHz ไม่ได้เด็ดขาด เป็นสาเหตุอันดับ 1 ที่ต่อไม่ติด
 *  ถ้าเราเตอร์รวม 2 ย่านไว้ในชื่อเดียวกัน อาจต้องแยกชื่อ SSID ของย่าน 2.4 GHz ออกมา
 * ==========================================================================*/

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins_config.h"

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);


// ======================== ค่าที่ต้องแก้ก่อนใช้งาน ==========================
const char *WIFI_SSID = "myHome_2.4GHz";
const char *WIFI_PASS = "0939391546";

/* ข้อควรระวังเรื่องความปลอดภัย
 * การเขียนรหัสผ่านลงในโค้ดตรง ๆ เหมาะกับการเรียนรู้และการทดสอบเท่านั้น
 * ห้ามอัปโหลดไฟล์นี้ขึ้น GitHub หรือส่งต่อให้คนอื่นโดยไม่ลบรหัสผ่านออกก่อน
 * ในงานจริงควรเก็บลง NVS/Preferences หรือใช้ WiFiManager ตั้งค่าผ่านหน้าเว็บแทน
 */

constexpr uint32_t WIFI_TIMEOUT_MS = 20000;   // รอเชื่อมต่อนานสุดกี่ ms
constexpr uint32_t CHECK_INTERVAL  = 5000;    // ตรวจสถานะการเชื่อมต่อทุกกี่ ms


// ============================== ฟังก์ชันช่วย ==============================
// แปลงค่าความแรงสัญญาณเป็นคำอธิบายที่อ่านเข้าใจง่าย
const char *rssiQuality(int32_t rssi) {
  if (rssi >= -50) return "Excellent";
  if (rssi >= -60) return "Good";
  if (rssi >= -70) return "Fair";
  if (rssi >= -80) return "Weak";
  return "Very Weak";
}

// แปลงรหัสสถานะของ WiFi เป็นสาเหตุที่อ่านรู้เรื่อง ใช้ตอนต่อไม่ติด
const char *wifiStatusText(wl_status_t s) {
  switch (s) {
    case WL_CONNECTED:       return "เชื่อมต่อสำเร็จ";
    case WL_NO_SSID_AVAIL:   return "ไม่พบชื่อ WiFi นี้ (ตรวจชื่อ หรืออาจเป็นย่าน 5GHz)";
    case WL_CONNECT_FAILED:  return "รหัสผ่านไม่ถูกต้อง";
    case WL_CONNECTION_LOST: return "สัญญาณหลุด";
    case WL_DISCONNECTED:    return "ยังไม่ได้เชื่อมต่อ";
    case WL_IDLE_STATUS:     return "กำลังรอ";
    default:                 return "ไม่ทราบสาเหตุ";
  }
}


// ============================== การแสดงผลบนจอ =============================
/* วาดหน้าจอทั้งหมดในฟังก์ชันเดียว
 * status = ข้อความบรรทัดที่ 2 เช่น "Connected" / "Connecting" / "Failed" */
void drawScreen(const char *status, bool showDetail) {
  oled.clearDisplay();

  // ---- หัวข้อตัวใหญ่ ----
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.println("DEMO WIFI");

  // ---- สถานะ ----
  oled.setTextSize(1);
  oled.setCursor(0, 22);
  oled.println(status);

  if (showDetail) {
    // ---- IP Address : ข้อมูลสำคัญที่สุดของ Lab นี้ ----
    oled.setCursor(0, 34);
    oled.printf("IP: %s", WiFi.localIP().toString().c_str());

    // ---- ชื่อเครือข่ายและความแรงสัญญาณ ----
    oled.setCursor(0, 46);
    oled.printf("SSID: %s", WIFI_SSID);

    int32_t rssi = WiFi.RSSI();
    oled.setCursor(0, 56);
    oled.printf("RSSI: %d (%s)", rssi, rssiQuality(rssi));
  }

  oled.display();     // สำคัญ! ต้องเรียกจอถึงจะอัปเดต
}


// ================================ SETUP ===================================
void setup() {
  Serial.begin(115200);

  // ---------- เริ่มต้นจอ ----------
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ไม่พบจอ OLED - จะแสดงผลทาง Serial อย่างเดียว");
  }
  oled.setTextColor(SSD1306_WHITE);

  /* ปิดการตัดบรรทัดอัตโนมัติ
   * ถ้าชื่อ SSID ยาวเกินความกว้างจอ ค่าเริ่มต้นจะขึ้นบรรทัดใหม่ให้
   * ซึ่งจะไปทับบรรทัด RSSI ด้านล่าง ปิดไว้แล้วให้ตัดตกขอบจอไปเลยอ่านง่ายกว่า */
  oled.setTextWrap(false);

  Serial.println("\n===========================================");
  Serial.println("  Lab7 : เชื่อมต่อ WiFi");
  Serial.println("===========================================");
  Serial.printf ("  SSID : %s\n", WIFI_SSID);
  Serial.println("-------------------------------------------");

  // ---------- เริ่มเชื่อมต่อ WiFi ----------
  /* WiFi.mode(WIFI_STA) สำคัญมาก
   * STA = Station คือโหมดที่ ESP32 เป็น "ลูกข่าย" ไปเกาะเราเตอร์
   * ถ้าไม่กำหนด ESP32 อาจเปิดโหมด AP (ปล่อยสัญญาณเอง) ค้างไว้ด้วย
   * ซึ่งกินไฟเพิ่มโดยเปล่าประโยชน์ และทำให้การเชื่อมต่อไม่นิ่ง */
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  drawScreen("Connecting...", false);
  Serial.print("กำลังเชื่อมต่อ");

  uint32_t startAt = millis();
  uint8_t  dots    = 0;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAt < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");

    // แสดงจุดวิ่งบนจอ ให้รู้ว่าระบบยังทำงานอยู่ ไม่ได้ค้าง
    dots = (dots + 1) % 4;
    char buf[20] = "Connecting";
    for (uint8_t i = 0; i < dots; i++) strcat(buf, ".");
    drawScreen(buf, false);
  }
  Serial.println();

  // ---------- ตรวจผลลัพธ์ ----------
  if (WiFi.status() == WL_CONNECTED) {
    drawScreen("Connected", true);

    Serial.println("  เชื่อมต่อสำเร็จ");
    Serial.printf ("  IP Address : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf ("  Gateway    : %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf ("  Subnet     : %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf ("  MAC        : %s\n", WiFi.macAddress().c_str());
    Serial.printf ("  RSSI       : %d dBm (%s)\n", WiFi.RSSI(), rssiQuality(WiFi.RSSI()));
    Serial.println("===========================================");
    Serial.println("  ลอง ping IP นี้จากคอมพิวเตอร์ในวงเดียวกันได้เลย");
  } else {
    drawScreen("Failed!", false);

    Serial.println("  เชื่อมต่อไม่สำเร็จ");
    Serial.printf ("  สาเหตุ : %s\n", wifiStatusText(WiFi.status()));
    Serial.println("-------------------------------------------");
    Serial.println("  สิ่งที่ควรตรวจ");
    Serial.println("   1. เราเตอร์ต้องเป็นย่าน 2.4 GHz (ESP32 ไม่รองรับ 5 GHz)");
    Serial.println("   2. ชื่อ SSID และรหัสผ่านสะกดถูกต้อง ตัวพิมพ์เล็ก-ใหญ่มีผล");
    Serial.println("   3. สัญญาณแรงพอ ลองย้ายบอร์ดเข้าใกล้เราเตอร์");
    Serial.println("===========================================");
  }
}


// ================================= LOOP ===================================
uint32_t checkAt      = 0;
bool     wasConnected = false;

void loop() {
  uint32_t now = millis();

  if (now - checkAt < CHECK_INTERVAL) return;
  checkAt = now;

  bool isConnected = (WiFi.status() == WL_CONNECTED);

  // ---- เพิ่งหลุดการเชื่อมต่อ ----
  if (wasConnected && !isConnected) {
    Serial.println("!! WiFi หลุด - กำลังเชื่อมต่อใหม่");
    drawScreen("Reconnecting", false);
    WiFi.reconnect();
  }

  // ---- เพิ่งกลับมาเชื่อมต่อได้ ----
  if (!wasConnected && isConnected) {
    Serial.printf(">> เชื่อมต่อกลับมาแล้ว IP: %s\n",
                  WiFi.localIP().toString().c_str());
  }

  // ---- ปกติดี : อัปเดตค่าความแรงสัญญาณบนจอ ----
  if (isConnected) {
    drawScreen("Connected", true);
  }

  wasConnected = isConnected;
}
