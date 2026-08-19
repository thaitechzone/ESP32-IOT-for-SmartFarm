/* ============================================================================
 *  Lab9 : ต่อยอดจาก Lab8 - เพิ่มนาฬิกาจริงจาก NTP (โซน Asia/Bangkok)
 *         DHT11 -> MQTT + ควบคุมรีเลย์ 3 ตัว + แสดงเวลาและ IP บน OLED
 *  Board : ESP32 Devkit V2
 * ----------------------------------------------------------------------------
 *  สิ่งที่เพิ่มจาก Lab8
 *    1. ดึงเวลาจริงจาก NTP server ตั้งโซนเวลาไทย (UTC+7)
 *    2. แสดงเวลา วันที่ และ IP Address บนจอ OLED
 *    3. แนบเวลาไปกับข้อมูล JSON ที่ส่งขึ้น MQTT
 *
 *  ทำไมต้องมีเวลาจริง
 *    ESP32 ไม่มีนาฬิกาในตัว ทุกครั้งที่บูตจะเริ่มนับจากศูนย์ใหม่หมด
 *    millis() บอกได้แค่ "ผ่านมากี่มิลลิวินาทีตั้งแต่เปิดเครื่อง" ไม่ใช่เวลาจริง
 *    ข้อมูลเซนเซอร์ที่ไม่มีเวลากำกับ นำไปทำกราฟย้อนหลังหรือหาสาเหตุปัญหาไม่ได้เลย
 * ----------------------------------------------------------------------------
 *  ไลบรารีที่ต้องติดตั้งเพิ่ม (Tools > Manage Libraries...)
 *    - "PubSubClient" by Nick O'Leary
 *    - DHT sensor library / Adafruit Unified Sensor  (มีแล้วจาก Lab3)
 *    - Adafruit SSD1306 / Adafruit GFX Library       (มีแล้วจาก Lab4)
 *
 *  Broker : broker.hivemq.com พอร์ต 1883  ไม่ต้องสมัคร ไม่ต้องใช้รหัสผ่าน
 *
 *  ---------------------------- รายการ Topic ทั้งหมด ----------------------------
 *  ส่งออก (ESP32 -> ผู้ใช้)
 *    <base>/temperature        29.0
 *    <base>/humidity           62.0
 *    <base>/status             {"temp":29.0,"humi":62.0,"r1":1,"r2":0,"r3":0,...}
 *    <base>/status/relay1      ON หรือ OFF     (retained)
 *    <base>/status/relay2      ON หรือ OFF     (retained)
 *    <base>/status/relay3      ON หรือ OFF     (retained)
 *
 *  รับคำสั่ง (ผู้ใช้ -> ESP32)   ส่งได้ทั้ง on / off / toggle / 1 / 0
 *    <base>/cmd/relay1
 *    <base>/cmd/relay2
 *    <base>/cmd/relay3
 *    <base>/cmd/led
 *    <base>/cmd/all            ส่ง off เพื่อปิดรีเลย์ทุกตัวพร้อมกัน (ปุ่มฉุกเฉิน)
 *
 *  !! ข้อควรระวังเรื่องความปลอดภัย - อ่านก่อนต่ออุปกรณ์จริง !!
 *  broker.hivemq.com เป็น broker สาธารณะ ไม่มีการยืนยันตัวตนใด ๆ
 *  ใครก็ตามในโลกที่รู้ชื่อ topic ของคุณ สั่งเปิด-ปิดรีเลย์ได้ทันที
 *    - ใช้กับหลอดไฟทดลองหรือโหลดจำลองเท่านั้น
 *    - อย่าต่อปั๊มน้ำหรือฮีตเตอร์ทิ้งไว้โดยไม่มีคนดูแล
 *    - งานจริงต้องย้ายไป HiveMQ Cloud (ฟรี 100 อุปกรณ์) ที่มีรหัสผ่านและ TLS
 *  ระบบนี้มีตัวตัดอัตโนมัติกันรีเลย์ค้าง ดูค่า RELAY_MAX_ON_MIN ด้านล่าง
 * ==========================================================================*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ctype.h>        // ใช้ tolower() ตอนแปลคำสั่งที่รับเข้ามา
#include <time.h>         // ใช้ทำนาฬิกาจาก NTP
#include "pins_config.h"


// ======================== ค่าที่ต้องแก้ก่อนใช้งาน ==========================
const char *WIFI_SSID = "myHome_2.4GHz";
const char *WIFI_PASS = "0939391546";

const char *MQTT_HOST = "broker.hivemq.com";
constexpr uint16_t MQTT_PORT = 1883;

/* -------------------------- ตั้งค่านาฬิกา NTP ----------------------------
 * ใส่ NTP server ไว้ 3 ตัว ถ้าตัวแรกล่มระบบจะไปใช้ตัวถัดไปเอง
 * ตัวแรกเป็นเซิร์ฟเวอร์ในไทย ตอบเร็วที่สุดเพราะอยู่ใกล้
 *
 * TZ_BANGKOK = "ICT-7" คือรูปแบบ POSIX TZ ไม่ใช่ชื่อโซนแบบ "Asia/Bangkok"
 *   ICT = ชื่อย่อโซนเวลา (Indochina Time)
 *   -7  = ระวังตรงนี้! รูปแบบ POSIX ใช้เครื่องหมาย "กลับด้าน" จากที่เราคุ้นเคย
 *         เขียน -7 หมายถึง UTC+7 (ต้องบวก 7 ชั่วโมงจาก UTC)
 *         ถ้าเผลอเขียน +7 จะได้เวลาผิดไป 14 ชั่วโมง
 *   ไทยไม่มีการปรับเวลาตามฤดูกาล (DST) จึงไม่ต้องใส่ส่วนที่สองของสตริง
 */
const char *NTP_SERVER1 = "th.pool.ntp.org";
const char *NTP_SERVER2 = "pool.ntp.org";
const char *NTP_SERVER3 = "time.google.com";
const char *TZ_BANGKOK  = "ICT-7";

/* !! ต้องเปลี่ยนบรรทัดล่างนี้ให้เป็นชื่อเฉพาะของคุณ !!
 * broker สาธารณะมีคนใช้ทั่วโลก ถ้าใช้ชื่อซ้ำกับคนอื่นข้อมูลจะปนกันมั่ว
 * และที่แย่กว่านั้นคือคนอื่นจะสั่งรีเลย์ของคุณได้ */
#define TOPIC_BASE "smartfarm/thait7429"

// ----- Topic ส่งออก : ค่าเซนเซอร์ -----
const char *TOPIC_TEMP   = TOPIC_BASE "/temperature";
const char *TOPIC_HUMI   = TOPIC_BASE "/humidity";
const char *TOPIC_STATUS = TOPIC_BASE "/status";

// ----- Topic ส่งออก : สถานะรีเลย์ (ส่งแบบ retained) -----
const char *TOPIC_ST_R1  = TOPIC_BASE "/status/relay1";
const char *TOPIC_ST_R2  = TOPIC_BASE "/status/relay2";
const char *TOPIC_ST_R3  = TOPIC_BASE "/status/relay3";

// ----- Topic รับคำสั่ง : รีเลย์ทั้ง 3 ตัว -----
const char *TOPIC_CMD_R1 = TOPIC_BASE "/cmd/relay1";
const char *TOPIC_CMD_R2 = TOPIC_BASE "/cmd/relay2";
const char *TOPIC_CMD_R3 = TOPIC_BASE "/cmd/relay3";

// ----- Topic รับคำสั่ง : อื่น ๆ -----
const char *TOPIC_CMDLED = TOPIC_BASE "/cmd/led";
const char *TOPIC_CMDALL = TOPIC_BASE "/cmd/all";
const char *TOPIC_CMDSUB = TOPIC_BASE "/cmd/#";   // subscribe ทีเดียวครอบทุกคำสั่ง

/* ตัวตัดอัตโนมัติกันรีเลย์ค้าง (failsafe)
 * ถ้าสั่งเปิดรีเลย์จากอินเทอร์เน็ตแล้ว WiFi หลุด หรือคนสั่งลืมปิด
 * ปั๊มจะเดินค้างไม่มีกำหนด น้ำท่วมแปลงหรือมอเตอร์ไหม้ได้
 * ค่านี้บังคับปิดเองเมื่อเปิดค้างครบเวลาที่กำหนด
 * ใส่ 0 เพื่อปิดการทำงานของฟังก์ชันนี้ (ไม่แนะนำถ้าต่อโหลดจริง) */
constexpr float RELAY_MAX_ON_MIN = 10.0;
constexpr uint32_t RELAY_MAX_ON_MS = (uint32_t)(RELAY_MAX_ON_MIN * 60000.0);

// ----- รอบเวลาการทำงาน -----
constexpr uint32_t DHT_INTERVAL_MS   = 2000;
constexpr uint32_t PUBLISH_INTERVAL  = 5000;
constexpr uint32_t OLED_INTERVAL_MS  = 500;    // อัปเดตถี่ขึ้น ให้วินาทีเดินสวย
constexpr uint32_t TIME_INTERVAL_MS  = 250;    // อ่านเวลาจากระบบทุกกี่ ms
constexpr uint32_t MQTT_RETRY_MS     = 5000;
constexpr uint32_t WIFI_TIMEOUT_MS   = 20000;


// ==================== โครงสร้างข้อมูลของรีเลย์ที่สั่งผ่าน MQTT ==============
/* ประกาศ struct ไว้เหนือฟังก์ชันแรกของไฟล์เสมอ
 * เพราะ Arduino IDE แทรก prototype ของทุกฟังก์ชันไว้เหนือฟังก์ชันแรก
 * (บทเรียนเดียวกับที่เจอใน Lab2 และ Lab6) */
struct RelayCtl {
  uint8_t     pin;
  const char *name;          // ชื่อที่ใช้แสดงผล เช่น "R1"
  const char *cmdTopic;      // topic ที่รับคำสั่ง
  const char *statusTopic;   // topic ที่รายงานสถานะ
  bool        on;
  uint32_t    onAt;          // เวลาที่เริ่มเปิด ใช้กับตัวตัดอัตโนมัติ
};

/* ผูกรีเลย์แต่ละตัวเข้ากับ topic ของมัน
 * ชื่อ topic ทั้งหมดประกาศไว้ในหมวด "Topic" ด้านบนแล้ว ไม่มีการเขียนซ้ำตรงนี้
 * ถ้าต้องการเปลี่ยนชื่อ topic ให้แก้ที่เดียวด้านบนพอ */
RelayCtl relays[3] = {
  { RELAY1_PIN, "R1", TOPIC_CMD_R1, TOPIC_ST_R1, false, 0 },
  { RELAY2_PIN, "R2", TOPIC_CMD_R2, TOPIC_ST_R2, false, 0 },
  { RELAY3_PIN, "R3", TOPIC_CMD_R3, TOPIC_ST_R3, false, 0 },
};
constexpr uint8_t RELAY_COUNT = 3;


// ============================== อ็อบเจกต์หลัก =============================
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

String clientId;      // ต้องไม่ซ้ำกับใครใน broker สาธารณะ สร้างจาก MAC


// ============================== ตัวแปรสถานะ ===============================
uint32_t dhtAt = 0, pubAt = 0, oledAt = 0, mqttRetryAt = 0, timeAt = 0;

// ----- นาฬิกาจาก NTP -----
struct tm tmNow    = {0};    // เวลาปัจจุบันแยกเป็น ชั่วโมง/นาที/วินาที/วันที่
bool      timeSynced = false; // ซิงค์เวลาจาก NTP สำเร็จแล้วหรือยัง

float    temp = NAN, humi = NAN;
bool     sensorOK  = false;
uint8_t  failStreak = 0;
constexpr uint8_t MAX_FAIL_STREAK = 3;

uint32_t pubCount  = 0;
uint32_t pubFailed = 0;
bool     ledOn     = false;


// ============================== ฟังก์ชันช่วย ==============================
const char *mqttStateText(int state) {
  switch (state) {
    case -4: return "หมดเวลารอ broker ตอบกลับ";
    case -3: return "การเชื่อมต่อหลุด";
    case -2: return "ต่อ broker ไม่ได้ (ตรวจชื่อ host และอินเทอร์เน็ต)";
    case -1: return "ตัดการเชื่อมต่อแล้ว";
    case  0: return "เชื่อมต่อสำเร็จ";
    case  1: return "broker ไม่รองรับ MQTT เวอร์ชันนี้";
    case  2: return "Client ID ไม่ถูกต้อง";
    case  3: return "broker ไม่พร้อมให้บริการ";
    case  4: return "ชื่อผู้ใช้หรือรหัสผ่านผิด";
    case  5: return "ไม่ได้รับอนุญาต";
    default: return "ไม่ทราบสาเหตุ";
  }
}

// ============================ นาฬิกาจาก NTP ===============================
/* อ่านเวลาปัจจุบันจากนาฬิกาภายในของ ESP32
 *
 * ทำไมไม่ใช้ getLocalTime() ที่ไลบรารีมีให้
 *   getLocalTime() มีการ delay() รออยู่ข้างใน (ค่าเริ่มต้นรอนานถึง 5 วินาที)
 *   ถ้ายังซิงค์เวลาไม่สำเร็จ มันจะบล็อกทั้งระบบทุกครั้งที่เรียก
 *   จอค้าง เซนเซอร์ไม่ถูกอ่าน และ MQTT หลุดเพราะตอบ ping ไม่ทัน
 *
 *   ใช้ time() + localtime_r() ตรง ๆ แทน ทำงานเสร็จทันทีไม่มีการรอเลย
 *
 * วิธีดูว่าซิงค์สำเร็จหรือยัง
 *   ก่อนซิงค์ นาฬิกาจะเริ่มนับจากปี 1970 (เวลามาตรฐานของระบบ Unix)
 *   ถ้าอ่านได้ปีมากกว่า 2016 แปลว่าได้เวลาจริงจาก NTP มาแล้วแน่นอน
 */
void taskUpdateTime(uint32_t now) {
  if (now - timeAt < TIME_INTERVAL_MS) return;
  timeAt = now;

  time_t epoch;
  time(&epoch);                       // อ่านเวลาระบบเป็นวินาทีตั้งแต่ปี 1970
  localtime_r(&epoch, &tmNow);        // แปลงเป็นเวลาท้องถิ่นตามโซนที่ตั้งไว้

  bool synced = (tmNow.tm_year > (2016 - 1900));   // tm_year นับจากปี 1900

  if (synced && !timeSynced) {        // เพิ่งซิงค์สำเร็จครั้งแรก
    Serial.printf(">> ซิงค์เวลาจาก NTP สำเร็จ : %04d-%02d-%02d %02d:%02d:%02d\n",
                  tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                  tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  }
  timeSynced = synced;
}

// สร้างข้อความเวลาแบบเต็ม ใช้กับ JSON และ Serial
void timeStampFull(char *buf, size_t n) {
  if (timeSynced) strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmNow);
  else            snprintf(buf, n, "no-time");
}


/* แปลข้อความคำสั่งเป็นค่าที่ใช้งานได้
 *  คืน  1 = เปิด, 0 = ปิด, -1 = สลับสถานะ, -2 = ไม่รู้จักคำสั่งนี้
 * รองรับหลายรูปแบบเพื่อให้สั่งจากที่ไหนก็สะดวก และไม่สนตัวพิมพ์เล็ก-ใหญ่ */
int parseCommand(const char *msg) {
  char m[16] = {0};
  for (uint8_t i = 0; i < sizeof(m) - 1 && msg[i]; i++) m[i] = tolower(msg[i]);

  if (!strcmp(m, "on")  || !strcmp(m, "1") || !strcmp(m, "true"))  return  1;
  if (!strcmp(m, "off") || !strcmp(m, "0") || !strcmp(m, "false")) return  0;
  if (!strcmp(m, "toggle"))                                        return -1;
  return -2;
}


// ========================= จัดการรีเลย์และรายงานสถานะ ======================
/* ส่งสถานะรีเลย์ขึ้น MQTT แบบ retained
 *
 * retained คืออะไร : broker จะจำข้อความล่าสุดของ topic นี้ไว้
 * ใครที่ subscribe เข้ามาทีหลังจะได้รับสถานะปัจจุบันทันที ไม่ต้องรอรอบถัดไป
 * สำคัญมากสำหรับ dashboard หรือแอปมือถือ ที่เปิดขึ้นมาแล้วต้องรู้สถานะเลย
 * ถ้าไม่ใส่ retained หน้าจอจะว่างเปล่าจนกว่ารีเลย์จะเปลี่ยนสถานะครั้งถัดไป */
void publishRelayStatus(RelayCtl &r) {
  if (!mqtt.connected()) return;
  mqtt.publish(r.statusTopic, r.on ? "ON" : "OFF", true);   // true = retained
}

void publishAllRelayStatus() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) publishRelayStatus(relays[i]);
}

// สั่งรีเลย์ พร้อมรายงานสถานะขึ้น MQTT ทันทีที่เปลี่ยน
void relaySet(RelayCtl &r, bool on, const char *reason) {
  if (r.on == on) {                      // สถานะเดิมอยู่แล้ว ไม่ต้องทำอะไร
    publishRelayStatus(r);               // แต่ยืนยันสถานะกลับไปให้ผู้สั่งรู้
    return;
  }

  r.on = on;
  if (on) r.onAt = millis();
  digitalWrite(r.pin, on ? RELAY_ON : RELAY_OFF);

  Serial.printf(">> %s = %-3s  (%s)\n", r.name, on ? "ON" : "OFF", reason);
  publishRelayStatus(r);                 // แจ้งทันทีที่เปลี่ยน ไม่รอรอบ publish
}

/* ตัวตัดอัตโนมัติ : ปิดรีเลย์ที่เปิดค้างนานเกินกำหนด
 * ป้องกันกรณีสั่งเปิดแล้ว WiFi หลุด หรือผู้สั่งลืมปิด */
/* !! ห้ามรับ now จาก loop() มาใช้ตรงนี้เด็ดขาด !!
 *
 * loop() อ่าน now = millis() ไว้ตั้งแต่ต้นรอบ
 * แต่ relaySet() ตั้ง r.onAt = millis() ระหว่าง mqtt.loop() ซึ่งเกิดทีหลัง
 * ค่า onAt จึง "มากกว่า" now เสมอเมื่อเพิ่งสั่งเปิดรีเลย์ในรอบนั้น
 *
 * ผลคือ now - onAt ได้ค่าติดลบ และเพราะเป็น uint32_t จึงวนกลับเป็นเลขมหาศาล
 *      10000 - 10003 = -3  ->  4294967293
 * ซึ่งมากกว่าเวลาตัดอัตโนมัติ ระบบจึงสั่งปิดรีเลย์ทันทีในรอบเดียวกัน
 * อาการคือรีเลย์ติดแวบเดียวแล้วดับ ทุกตัว ทุกคำสั่ง
 *
 * ทางแก้ : อ่าน millis() สดตรงนี้เอง
 * millis() เดินหน้าอย่างเดียวไม่ถอยหลัง ค่าที่ได้จึงไม่มีทางน้อยกว่า onAt
 * และการลบแบบ unsigned ยังคงทนการวน overflow ที่ 49 วันได้ถูกต้องเหมือนเดิม
 *
 * บทเรียน : ค่าเวลาที่เอามาเทียบกัน ต้องมาจากแหล่งเดียวกันเสมอ
 *           การส่ง snapshot ของเวลาข้ามฟังก์ชันที่สร้าง timestamp ใหม่ระหว่างทาง
 *           คือต้นเหตุของบั๊กประเภทนี้ */
void taskRelayFailsafe() {
  if (RELAY_MAX_ON_MS == 0) return;      // ปิดการทำงานของฟังก์ชันนี้

  uint32_t now = millis();               // อ่านสดเสมอ ไม่รับค่าจากภายนอก

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    RelayCtl &r = relays[i];
    if (!r.on) continue;
    if (now - r.onAt < RELAY_MAX_ON_MS) continue;

    Serial.printf("!! %s เปิดค้างครบ %.1f นาที - ตัดอัตโนมัติเพื่อความปลอดภัย\n",
                  r.name, RELAY_MAX_ON_MIN);
    relaySet(r, false, "failsafe timeout");
  }
}


// ===================== รับข้อความที่ส่งเข้ามาจาก MQTT ======================
/* payload ไม่ใช่ string ที่จบด้วย \0 ต้องใช้ length กำหนดขอบเขตเองเสมอ
 * ถ้าเผลออ่านเลย length จะได้ข้อมูลขยะติดมาด้วย */
void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  char msg[32] = {0};
  unsigned int n = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
  memcpy(msg, payload, n);

  Serial.printf("<< [%s] : %s\n", topic, msg);

  int cmd = parseCommand(msg);
  if (cmd == -2) {
    Serial.printf("   ไม่รู้จักคำสั่ง '%s' (ใช้ได้: on / off / toggle / 1 / 0)\n", msg);
    return;
  }

  // ---- คำสั่งปิดทุกตัวพร้อมกัน (ปุ่มฉุกเฉิน) ----
  if (!strcmp(topic, TOPIC_CMDALL)) {
    bool target = (cmd == 1);            // toggle ไม่มีความหมายกับคำสั่งรวม
    for (uint8_t i = 0; i < RELAY_COUNT; i++) relaySet(relays[i], target, "cmd/all");
    return;
  }

  // ---- คำสั่งคุมรีเลย์รายตัว ----
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (strcmp(topic, relays[i].cmdTopic) != 0) continue;
    bool target = (cmd == -1) ? !relays[i].on : (cmd == 1);
    relaySet(relays[i], target, "MQTT command");
    return;
  }

  // ---- คำสั่งคุม LED ออนบอร์ด ----
  if (!strcmp(topic, TOPIC_CMDLED)) {
    ledOn = (cmd == -1) ? !ledOn : (cmd == 1);
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    Serial.printf(">> LED = %s\n", ledOn ? "ON" : "OFF");
  }
}


// ========================= การเชื่อมต่อ MQTT ==============================
void ensureMqtt(uint32_t now) {
  if (mqtt.connected()) return;
  if (now - mqttRetryAt < MQTT_RETRY_MS) return;
  mqttRetryAt = now;

  Serial.printf("กำลังเชื่อมต่อ MQTT (%s)... ", clientId.c_str());

  if (mqtt.connect(clientId.c_str())) {
    Serial.println("สำเร็จ");

    /* subscribe ด้วย wildcard # ครั้งเดียว ครอบคลุมทุกคำสั่งใต้ /cmd/
     * ประหยัดกว่าการ subscribe ทีละ topic และเพิ่มคำสั่งใหม่ได้โดยไม่ต้องแก้ตรงนี้ */
    mqtt.subscribe(TOPIC_CMDSUB);
    Serial.printf("   subscribe : %s\n", TOPIC_CMDSUB);

    // ประกาศสถานะรีเลย์ปัจจุบันทันทีที่ต่อติด ให้ dashboard เห็นค่าที่ถูกต้อง
    publishAllRelayStatus();
  } else {
    Serial.printf("ล้มเหลว rc=%d (%s)\n",
                  mqtt.state(), mqttStateText(mqtt.state()));
  }
}


// ========================== งานที่ 1 : อ่านเซนเซอร์ ========================
void taskReadDHT(uint32_t now) {
  if (now - dhtAt < DHT_INTERVAL_MS) return;
  dhtAt = now;

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    failStreak++;
    if (failStreak >= MAX_FAIL_STREAK && sensorOK) {
      sensorOK = false;
      Serial.println("!! เซนเซอร์มีปัญหา - หยุดส่งค่าอุณหภูมิ/ความชื้น");
    }
    return;
  }

  failStreak = 0;
  sensorOK   = true;
  temp = t;
  humi = h;
}


// ========================= งานที่ 2 : ส่งขึ้น MQTT =========================
void taskPublish(uint32_t now) {
  if (now - pubAt < PUBLISH_INTERVAL) return;
  pubAt = now;

  if (!mqtt.connected()) return;

  // สถานะรีเลย์ส่งซ้ำเสมอ แม้เซนเซอร์เสีย เพราะเป็นข้อมูลคนละส่วนกัน
  publishAllRelayStatus();

  if (!sensorOK) return;                        // ไม่ส่งค่าที่เชื่อถือไม่ได้

  char bufT[12], bufH[12], bufTime[24], bufJson[256];
  snprintf(bufT, sizeof(bufT), "%.1f", temp);
  snprintf(bufH, sizeof(bufH), "%.1f", humi);
  timeStampFull(bufTime, sizeof(bufTime));

  bool ok1 = mqtt.publish(TOPIC_TEMP, bufT);
  bool ok2 = mqtt.publish(TOPIC_HUMI, bufH);

  /* JSON รวมทุกอย่างไว้ในข้อความเดียว สะดวกสำหรับเก็บลงฐานข้อมูล
   * ใส่ "time" เป็นเวลาจริงไว้ด้วย ทำให้ข้อมูลนำไปทำกราฟย้อนหลังได้
   * ถ้ามีแค่ uptime จะรู้เพียงว่า "หลังบูตไปกี่วินาที" ซึ่งรีเซ็ตทุกครั้งที่ไฟดับ */
  snprintf(bufJson, sizeof(bufJson),
           "{\"time\":\"%s\",\"temp\":%.1f,\"humi\":%.1f,"
           "\"r1\":%d,\"r2\":%d,\"r3\":%d,\"rssi\":%d,\"uptime\":%u}",
           bufTime, temp, humi,
           relays[0].on ? 1 : 0,
           relays[1].on ? 1 : 0,
           relays[2].on ? 1 : 0,
           WiFi.RSSI(), now / 1000);
  bool ok3 = mqtt.publish(TOPIC_STATUS, bufJson);

  if (ok1 && ok2 && ok3) {
    pubCount++;
    Serial.printf("[%6us] >> ส่งสำเร็จ #%u : %s\n", now / 1000, pubCount, bufJson);
  } else {
    pubFailed++;
    Serial.printf("!! ส่งไม่สำเร็จ (ครั้งที่ %u)\n", pubFailed);
  }
}


// ========================== งานที่ 3 : แสดงผล OLED ========================
void taskDisplay(uint32_t now) {
  if (now - oledAt < OLED_INTERVAL_MS) return;
  oledAt = now;

  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  bool mqttOK = mqtt.connected();

  oled.clearDisplay();

  /* ผังหน้าจอ 128x64
   *   y=0    เวลา ตัวใหญ่ (size 2)      + วันที่ ตัวเล็กมุมขวา
   *   y=17   เส้นคั่น
   *   y=21   อุณหภูมิ / ความชื้น
   *   y=32   สถานะรีเลย์ทั้ง 3 ตัว
   *   y=43   IP Address
   *   y=54   สถานะ WiFi / MQTT / จำนวนครั้งที่ส่ง
   */

  // ---- แถวบน : นาฬิกา ----
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  if (timeSynced) oled.printf("%02d:%02d:%02d",
                              tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  else            oled.print("--:--:--");     // ยังซิงค์เวลาไม่ได้

  // ---- วันที่ ตัวเล็กมุมขวาบน ----
  oled.setTextSize(1);
  if (timeSynced) {
    oled.setCursor(97, 0);
    oled.printf("%02d/%02d", tmNow.tm_mday, tmNow.tm_mon + 1);  // tm_mon เริ่มที่ 0
  }
  oled.drawLine(0, 17, 127, 17, SSD1306_WHITE);

  // ---- ค่าเซนเซอร์ ----
  oled.setCursor(0, 21);
  if (sensorOK) oled.printf("T %.1fC   H %.0f%%", temp, humi);
  else          oled.print("Sensor Error!");

  // ---- สถานะรีเลย์ทั้ง 3 ตัว ----
  oled.setCursor(0, 32);
  oled.printf("%s:%-3s %s:%-3s %s:%-3s",
              relays[0].name, relays[0].on ? "ON" : "OFF",
              relays[1].name, relays[1].on ? "ON" : "OFF",
              relays[2].name, relays[2].on ? "ON" : "OFF");

  // ---- IP Address ----
  oled.setCursor(0, 43);
  if (wifiOK) oled.printf("IP %s", WiFi.localIP().toString().c_str());
  else        oled.print("IP  no wifi");

  // ---- แถบสถานะการเชื่อมต่อ ----
  oled.setCursor(0, 54);
  oled.printf("W:%s M:%s Tx:%u",
              wifiOK ? "OK" : "--",
              mqttOK ? "OK" : "--",
              pubCount);

  oled.display();
}


// ================================ SETUP ===================================
void setup() {
  Serial.begin(115200);

  /* ---------------- ตั้งรีเลย์ทุกตัวให้ OFF (Logic HIGH) ตั้งแต่เริ่มทำงาน ------
   *
   * !! ลำดับคำสั่งตรงนี้สำคัญมาก และต่างกันระหว่าง ESP32 core รุ่นเก่ากับใหม่ !!
   *
   * core 2.x  : digitalWrite() ก่อน pinMode() ได้ เพราะเขียนลง latch ไว้ล่วงหน้า
   * core 3.x  : digitalWrite() ก่อน pinMode() "ไม่ทำงานเลย" มันจะข้ามไปเฉย ๆ
   *             แล้วขึ้นข้อความใน log ว่า
   *                 "IO 17 is not set as GPIO. Execute digitalMode(17, OUTPUT) first."
   *             ผลคือขาถูกตั้งเป็น OUTPUT โดยที่ latch ยังเป็น 0 (LOW)
   *             ซึ่งรีเลย์ Active LOW แปลว่า "สั่งทำงาน" -> รีเลย์ติดทุกตัวตอนบูต
   *
   * เครื่องนี้ใช้ core 3.3.11 จึงต้องเรียง pinMode ก่อน แล้วค่อย digitalWrite
   * ลำดับนี้ปลอดภัยกับทั้งสองเวอร์ชัน ใช้เป็นมาตรฐานได้เลย
   *
   * หมายเหตุ : ยังมีช่วงสั้น ๆ ราว 1-2 ไมโครวินาที ระหว่าง pinMode กับ digitalWrite
   * ที่ขาถูกขับเป็น LOW อยู่ แต่รีเลย์เชิงกลต้องใช้เวลาดูดหลายมิลลิวินาที
   * จึงไม่ทันตอบสนอง ไม่เกิดเสียงคลิกและหน้าสัมผัสไม่ขยับ
   * --------------------------------------------------------------------------*/
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(relays[i].pin, OUTPUT);
    digitalWrite(relays[i].pin, RELAY_OFF);   // RELAY_OFF = HIGH
    relays[i].on   = false;
    relays[i].onAt = 0;
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  ledOn = false;

  Serial.println("\n>> ตั้งรีเลย์ R1 R2 R3 เป็น OFF (Logic HIGH) เรียบร้อย");

  // ---------- จอ OLED ----------
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ไม่พบจอ OLED - ระบบจะทำงานต่อโดยไม่มีจอ");
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);
  oled.clearDisplay();
  oled.setCursor(0, 24);
  oled.println("Starting...");
  oled.display();

  dht.begin();

  Serial.println("\n=======================================================");
  Serial.println("  Lab9 : DHT11 -> MQTT + รีเลย์ + นาฬิกา NTP (Asia/Bangkok)");
  Serial.println("=======================================================");

  // ---------- เชื่อมต่อ WiFi ----------
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("กำลังเชื่อมต่อ WiFi : %s", WIFI_SSID);

  uint32_t startAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAt < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("!! ต่อ WiFi ไม่สำเร็จ - ตรวจว่าเป็นย่าน 2.4 GHz และรหัสผ่านถูกต้อง");
  } else {
    Serial.printf("  IP : %s   RSSI : %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  /* ---------------- ตั้งนาฬิกาจาก NTP ----------------
   * ต้องเรียกหลังต่อ WiFi สำเร็จแล้วเท่านั้น เพราะต้องออกอินเทอร์เน็ตไปถาม
   *
   * configTzTime() ทำงานเบื้องหลังแบบไม่บล็อก สั่งแล้วผ่านไปทำงานอื่นได้เลย
   * ปกติใช้เวลา 1-3 วินาทีกว่าจะได้เวลากลับมา ระหว่างนั้นจอจะขึ้น --:--:--
   *
   * หลังจากนี้ระบบจะซิงค์เวลาใหม่ให้เองอัตโนมัติทุก 1 ชั่วโมง
   * กันนาฬิกาภายในของ ESP32 เดินคลาดเคลื่อนสะสม (ปกติเพี้ยนไม่กี่วินาทีต่อวัน)
   *
   * ใช้ configTzTime ไม่ใช่ configTime เพราะรับสตริงโซนเวลาแบบ POSIX ได้ตรง ๆ
   * รองรับประเทศที่มีการปรับเวลาตามฤดูกาลด้วย (ไทยไม่มี แต่เขียนแบบนี้ย้ายไปใช้ที่อื่นได้) */
  configTzTime(TZ_BANGKOK, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);
  Serial.printf("  ตั้งโซนเวลา : %s (UTC+7)  NTP : %s\n", TZ_BANGKOK, NTP_SERVER1);

  /* Client ID สร้างจากเลข MAC ของชิป เพื่อไม่ให้ซ้ำกับใครใน broker สาธารณะ
   * ถ้าใช้ ID ซ้ำ broker จะเตะเครื่องเก่าออกทุกครั้งที่เครื่องใหม่ต่อเข้ามา
   * กลายเป็นวนเตะกันไปมา ต่อติดแล้วหลุดทันทีตลอดเวลา */
  clientId = "esp32-" + String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);

  /* ขยายบัฟเฟอร์ของ PubSubClient จากค่าเริ่มต้น 256 ไบต์
   * ค่าเริ่มต้นต้องใส่ทั้ง header + ชื่อ topic + เนื้อข้อความ รวมกันใน 256 ไบต์
   * พอเพิ่มฟิลด์ time เข้าไปใน JSON แล้วเริ่มเฉียดขีดจำกัด
   * ถ้าล้น publish() จะคืนค่า false เงียบ ๆ ข้อความหายไปโดยไม่มีอะไรฟ้อง */
  mqtt.setBufferSize(512);

  Serial.println("-------------------------------------------------------");
  Serial.printf ("  Broker    : %s:%u\n", MQTT_HOST, MQTT_PORT);
  Serial.printf ("  Client ID : %s\n", clientId.c_str());
  Serial.println("  --- Topic ส่งออก ---");
  Serial.printf ("    %s\n", TOPIC_TEMP);
  Serial.printf ("    %s\n", TOPIC_HUMI);
  Serial.printf ("    %s\n", TOPIC_STATUS);
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    Serial.printf ("    %s\n", relays[i].statusTopic);
  Serial.println("  --- Topic รับคำสั่ง (on / off / toggle / 1 / 0) ---");
  for (uint8_t i = 0; i < RELAY_COUNT; i++)
    Serial.printf ("    %s\n", relays[i].cmdTopic);
  Serial.printf ("    %s\n", TOPIC_CMDLED);
  Serial.printf ("    %s   <- ส่ง off เพื่อปิดรีเลย์ทุกตัว\n", TOPIC_CMDALL);
  Serial.println("-------------------------------------------------------");
  Serial.printf ("  ตัวตัดอัตโนมัติ : ปิดรีเลย์เองเมื่อเปิดค้างครบ %.1f นาที\n",
                 RELAY_MAX_ON_MIN);
  Serial.println("=======================================================\n");
}


// ================================= LOOP ===================================
void loop() {
  uint32_t now = millis();

  ensureMqtt(now);        // ต่อ MQTT ใหม่อัตโนมัติถ้าหลุด (ไม่บล็อก)

  /* mqtt.loop() ต้องถูกเรียกบ่อย ๆ ห้ามขาด
   * ทำ 2 หน้าที่ : รับข้อความเข้ามา และตอบ ping ให้ broker รู้ว่ายังอยู่
   * ถ้าเรียกไม่ถี่พอ broker จะคิดว่าเครื่องตายแล้วตัดทิ้ง */
  mqtt.loop();

  taskUpdateTime(now);    // อ่านเวลาปัจจุบันจากนาฬิกาที่ซิงค์กับ NTP
  taskReadDHT(now);
  taskPublish(now);
  taskRelayFailsafe();    // ตัดรีเลย์ที่เปิดค้างนานเกินกำหนด (อ่าน millis() เอง)
  taskDisplay(now);
}
