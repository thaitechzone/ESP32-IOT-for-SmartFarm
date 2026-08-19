/* ============================================================================
 *  Lab10 : Web Dashboard สำหรับ Smart Farm  (ต่อยอดจาก Lab9)
 *  Board  : ESP32 Devkit V2
 * ----------------------------------------------------------------------------
 *  สิ่งที่เพิ่มจาก Lab9
 *    1. Web Server ในตัว ESP32 เปิดหน้า Dashboard ผ่านเบราว์เซอร์ได้เลย
 *    2. รีเลย์แต่ละตัวเลือกโหมดทำงานได้ 3 แบบ
 *         สั่งเอง      - กดเปิด/ปิดจากหน้าเว็บหรือ MQTT
 *         ตั้งเวลา     - เริ่มทำงานเวลา HH:MM ทุกวัน นานกี่นาทีก็ตั้งได้
 *         อัตโนมัติ    - ดูค่าอุณหภูมิหรือความชื้น เทียบกับเกณฑ์ที่ตั้งไว้
 *    3. ตั้งค่าทั้งหมดถูกบันทึกลง NVS ไม่หายเมื่อไฟดับ
 *    4. หน้า Dashboard แสดงรายการ MQTT topic ทั้งหมดให้ด้วย
 *
 *  วิธีใช้ : ดู IP บนจอ OLED แล้วเปิดเบราว์เซอร์ไปที่ http://<IP ที่เห็น>
 *           เครื่องที่เปิดต้องอยู่ในวง WiFi เดียวกับบอร์ด
 *
 *  ไลบรารีที่ต้องติดตั้ง (เหมือน Lab9 ทุกอย่าง ไม่ต้องเพิ่มใหม่)
 *    PubSubClient / DHT sensor library / Adafruit Unified Sensor
 *    Adafruit SSD1306 / Adafruit GFX Library
 *    ส่วน WebServer.h และ Preferences.h มากับ ESP32 core อยู่แล้ว
 *
 *  !! ความปลอดภัย !!
 *  หน้าเว็บนี้ไม่มีระบบล็อกอิน ใครก็ตามที่อยู่ในวง WiFi เดียวกันสั่งรีเลย์ได้
 *  และ broker.hivemq.com เป็น broker สาธารณะที่ไม่มีการยืนยันตัวตนเช่นกัน
 *  เหมาะกับการเรียนรู้และเครือข่ายภายในบ้านเท่านั้น
 *  ห้ามเปิด port ออกสู่อินเทอร์เน็ตโดยตรงเด็ดขาด
 * ==========================================================================*/

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ctype.h>
#include <time.h>
#include "pins_config.h"
#include "dashboard_html.h"


// ======================== ค่าที่ต้องแก้ก่อนใช้งาน ==========================
const char *WIFI_SSID = "myHome_2.4GHz";
const char *WIFI_PASS = "0939391546";

const char *MQTT_HOST = "broker.hivemq.com";
constexpr uint16_t MQTT_PORT = 1883;

#define TOPIC_BASE "smartfarm/thait7429"

// ----- NTP โซนเวลาไทย -----
const char *NTP_SERVER1 = "th.pool.ntp.org";
const char *NTP_SERVER2 = "pool.ntp.org";
const char *NTP_SERVER3 = "time.google.com";
const char *TZ_BANGKOK  = "ICT-7";      // -7 หมายถึง UTC+7 (รูปแบบ POSIX กลับด้าน)

// ----- Topic ส่งออก -----
const char *TOPIC_TEMP   = TOPIC_BASE "/temperature";
const char *TOPIC_HUMI   = TOPIC_BASE "/humidity";
const char *TOPIC_STATUS = TOPIC_BASE "/status";
const char *TOPIC_ST_R1  = TOPIC_BASE "/status/relay1";
const char *TOPIC_ST_R2  = TOPIC_BASE "/status/relay2";
const char *TOPIC_ST_R3  = TOPIC_BASE "/status/relay3";

// ----- Topic รับคำสั่ง -----
const char *TOPIC_CMD_R1 = TOPIC_BASE "/cmd/relay1";
const char *TOPIC_CMD_R2 = TOPIC_BASE "/cmd/relay2";
const char *TOPIC_CMD_R3 = TOPIC_BASE "/cmd/relay3";
const char *TOPIC_CMDLED = TOPIC_BASE "/cmd/led";
const char *TOPIC_CMDALL = TOPIC_BASE "/cmd/all";
const char *TOPIC_CMDSUB = TOPIC_BASE "/cmd/#";

// ----- รอบเวลาการทำงาน -----
constexpr uint32_t DHT_INTERVAL_MS  = 2000;
constexpr uint32_t PUBLISH_INTERVAL = 5000;
constexpr uint32_t OLED_INTERVAL_MS = 500;
constexpr uint32_t TIME_INTERVAL_MS = 250;
constexpr uint32_t MQTT_RETRY_MS    = 5000;
constexpr uint32_t WIFI_TIMEOUT_MS  = 20000;

/* ตัวตัดอัตโนมัติกันรีเลย์ค้าง ใช้เฉพาะโหมด "สั่งเอง" เท่านั้น
 * โหมดตั้งเวลามีเวลาสิ้นสุดของตัวเองอยู่แล้ว
 * ส่วนโหมดอัตโนมัติอาจต้องเปิดพัดลมยาวหลายชั่วโมงตามสภาพอากาศจริง
 * ถ้าเอาตัวตัดนี้ไปใช้กับสองโหมดนั้นจะกลายเป็นตัดการทำงานที่ถูกต้องทิ้ง */
constexpr float    RELAY_MAX_ON_MIN = 30.0;
constexpr uint32_t RELAY_MAX_ON_MS  = (uint32_t)(RELAY_MAX_ON_MIN * 60000.0);


// ============================ โครงสร้างข้อมูล =============================
/* ประกาศ struct ไว้เหนือฟังก์ชันแรกของไฟล์เสมอ
 * เพราะ Arduino IDE แทรก prototype ของทุกฟังก์ชันไว้เหนือฟังก์ชันแรก */

enum { MODE_MANUAL = 0, MODE_SCHEDULE = 1, MODE_AUTO = 2 };
enum { SRC_TEMP = 0, SRC_HUMI = 1 };

// ค่าตั้งของรีเลย์ 1 ตัว ส่วนนี้ถูกบันทึกลง NVS
struct RelayCfg {
  uint8_t  mode;        // MODE_MANUAL / MODE_SCHEDULE / MODE_AUTO
  bool     manualOn;    // สถานะที่สั่งเอง (ไม่บันทึก บูตใหม่เริ่มที่ปิดเสมอ)
  uint8_t  onHour;      // โหมดตั้งเวลา : เริ่มกี่โมง
  uint8_t  onMin;       //               : เริ่มกี่นาที
  uint16_t runMin;      //               : ทำงานนานกี่นาที
  uint8_t  src;         // โหมดอัตโนมัติ : ดูค่าจาก SRC_TEMP หรือ SRC_HUMI
  bool     above;       //               : true = ค่ามากกว่าเกณฑ์จึงเปิด
  float    thr;         //               : เกณฑ์ที่ใช้ตัดสิน
  float    hyst;        //               : ช่วงหน่วง กันรีเลย์สั่นรอบจุดตัด
};

// สถานะขณะทำงานจริง ส่วนนี้อยู่ใน RAM อย่างเดียว
struct RelayCtl {
  uint8_t     pin;
  const char *name;
  const char *cmdTopic;
  const char *statusTopic;
  bool        on;
  uint32_t    onAt;      // เวลาที่เริ่มเปิด (millis)
  int16_t     lastDay;   // tm_yday ที่ตารางเวลาทำงานไปแล้ว กันสั่งซ้ำในวันเดียวกัน
};

constexpr uint8_t RELAY_COUNT = 3;

RelayCtl relays[RELAY_COUNT] = {
  { RELAY1_PIN, "R1", TOPIC_CMD_R1, TOPIC_ST_R1, false, 0, -1 },
  { RELAY2_PIN, "R2", TOPIC_CMD_R2, TOPIC_ST_R2, false, 0, -1 },
  { RELAY3_PIN, "R3", TOPIC_CMD_R3, TOPIC_ST_R3, false, 0, -1 },
};

/* ค่าตั้งต้นเมื่อยังไม่เคยบันทึกอะไรลง NVS
 * ทุกตัวเริ่มที่โหมด "สั่งเอง" และสถานะปิด เพื่อความปลอดภัยตอนติดตั้งครั้งแรก */
RelayCfg cfgs[RELAY_COUNT] = {
  { MODE_MANUAL, false,  6, 0, 15, SRC_TEMP, true,  32.0, 1.0 },
  { MODE_MANUAL, false, 18, 0, 15, SRC_HUMI, false, 50.0, 3.0 },
  { MODE_MANUAL, false,  6, 30, 10, SRC_TEMP, true, 35.0, 1.0 },
};


// ============================== อ็อบเจกต์หลัก =============================
Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
WebServer    server(80);
Preferences  prefs;

String clientId;


// ============================== ตัวแปรสถานะ ===============================
uint32_t dhtAt = 0, pubAt = 0, oledAt = 0, mqttRetryAt = 0, timeAt = 0;

float    temp = NAN, humi = NAN;
bool     sensorOK   = false;
uint8_t  failStreak = 0;
constexpr uint8_t MAX_FAIL_STREAK = 3;

uint32_t pubCount  = 0;
uint32_t pubFailed = 0;
bool     ledOn     = false;

struct tm tmNow      = {0};
bool      timeSynced = false;


// ========================= บันทึกค่าตั้งลง NVS ============================
/* NVS คือพื้นที่ใน flash ที่เขียนค่าเก็บไว้ได้ ข้อมูลไม่หายเมื่อไฟดับ
 * เก็บทั้งอาร์เรย์เป็นก้อนเดียว (putBytes) ง่ายกว่าแยกทีละฟิลด์
 *
 * ข้อควรระวัง : flash เขียนซ้ำได้จำกัดราว 100,000 ครั้ง
 * จึงบันทึกเฉพาะตอนผู้ใช้กดบันทึกค่าตั้งเท่านั้น
 * ไม่บันทึกทุกครั้งที่รีเลย์เปลี่ยนสถานะ ไม่งั้น flash จะพังใน 1-2 ปี */
void saveConfig() {
  prefs.begin("lab10", false);
  prefs.putBytes("cfg", cfgs, sizeof(cfgs));
  prefs.end();
  Serial.println(">> บันทึกค่าตั้งลง NVS แล้ว");
}

void loadConfig() {
  prefs.begin("lab10", true);          // true = เปิดแบบอ่านอย่างเดียว
  size_t n = prefs.getBytesLength("cfg");
  if (n == sizeof(cfgs)) {
    prefs.getBytes("cfg", cfgs, sizeof(cfgs));
    Serial.println(">> โหลดค่าตั้งจาก NVS สำเร็จ");
  } else {
    Serial.println(">> ยังไม่มีค่าตั้งใน NVS ใช้ค่าเริ่มต้นแทน");
  }
  prefs.end();

  /* บังคับให้ทุกตัวเริ่มที่ "ปิด" เสมอหลังบูต
   * ถ้าจำสถานะเปิดไว้แล้วไฟดับตอนกลางคืน พอไฟมาปั๊มจะเดินเองโดยไม่มีใครรู้ */
  for (uint8_t i = 0; i < RELAY_COUNT; i++) cfgs[i].manualOn = false;
}


// ============================ นาฬิกาจาก NTP ===============================
/* ใช้ time() + localtime_r() แทน getLocalTime()
 * เพราะ getLocalTime() มี delay() รออยู่ข้างในนานถึง 5 วินาที
 * ซึ่งจะบล็อกทั้งระบบจนจอค้างและ MQTT หลุด */
void taskUpdateTime(uint32_t now) {
  if (now - timeAt < TIME_INTERVAL_MS) return;
  timeAt = now;

  time_t epoch;
  time(&epoch);
  localtime_r(&epoch, &tmNow);

  bool synced = (tmNow.tm_year > (2016 - 1900));   // ก่อนซิงค์จะเป็นปี 1970
  if (synced && !timeSynced) {
    Serial.printf(">> ซิงค์เวลาจาก NTP สำเร็จ : %04d-%02d-%02d %02d:%02d:%02d\n",
                  tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday,
                  tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  }
  timeSynced = synced;
}

void timeStampFull(char *buf, size_t n) {
  if (timeSynced) strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tmNow);
  else            snprintf(buf, n, "no-time");
}


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

int parseCommand(const char *msg) {
  char m[16] = {0};
  for (uint8_t i = 0; i < sizeof(m) - 1 && msg[i]; i++) m[i] = tolower(msg[i]);
  if (!strcmp(m, "on")  || !strcmp(m, "1") || !strcmp(m, "true"))  return  1;
  if (!strcmp(m, "off") || !strcmp(m, "0") || !strcmp(m, "false")) return  0;
  if (!strcmp(m, "toggle"))                                        return -1;
  return -2;
}


// ========================= จัดการรีเลย์และรายงานสถานะ ======================
void publishRelayStatus(RelayCtl &r) {
  if (!mqtt.connected()) return;
  mqtt.publish(r.statusTopic, r.on ? "ON" : "OFF", true);   // true = retained
}

void publishAllRelayStatus() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) publishRelayStatus(relays[i]);
}

void relaySet(RelayCtl &r, bool on, const char *reason) {
  if (r.on == on) { publishRelayStatus(r); return; }

  r.on = on;
  if (on) r.onAt = millis();
  digitalWrite(r.pin, on ? RELAY_ON : RELAY_OFF);

  Serial.printf(">> %s = %-3s  (%s)\n", r.name, on ? "ON" : "OFF", reason);
  publishRelayStatus(r);
}

// เวลาที่เหลือของรอบปัจจุบัน หน่วยวินาที ใช้แสดงบนหน้าเว็บและ OLED
uint32_t relayRemainSec(uint8_t i) {
  RelayCtl &r = relays[i];
  RelayCfg &c = cfgs[i];
  if (!r.on) return 0;

  uint32_t limitMs = 0;
  if      (c.mode == MODE_SCHEDULE) limitMs = (uint32_t)c.runMin * 60000UL;
  else if (c.mode == MODE_MANUAL)   limitMs = RELAY_MAX_ON_MS;
  else return 0;                    // โหมดอัตโนมัติไม่มีเวลาสิ้นสุดตายตัว

  uint32_t elapsed = millis() - r.onAt;
  if (elapsed >= limitMs) return 0;
  return (limitMs - elapsed + 999) / 1000;
}


// ======================= ตรรกะควบคุมรีเลย์ทั้ง 3 โหมด =====================
void taskControl() {
  /* อ่าน millis() สดตรงนี้เอง ห้ามรับค่า now จาก loop()
   * เพราะ relaySet() ตั้ง onAt ด้วย millis() ที่อาจเกิดหลัง now
   * ทำให้ now - onAt ติดลบแล้ววนกลับเป็นเลขมหาศาล (บั๊กที่เจอใน Lab8) */
  uint32_t now = millis();

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    RelayCtl &r = relays[i];
    RelayCfg &c = cfgs[i];

    switch (c.mode) {

      // ---------- โหมดสั่งเอง : ทำตามที่ผู้ใช้กด ----------
      case MODE_MANUAL:
        if (r.on != c.manualOn) relaySet(r, c.manualOn, "manual");
        break;

      // ---------- โหมดตั้งเวลา : เริ่มตามนาฬิกาจริง ----------
      case MODE_SCHEDULE: {
        if (!timeSynced) break;                  // ไม่มีเวลาจริง ไม่กล้าสั่งงาน

        if (r.on) {
          uint32_t runMs = (uint32_t)c.runMin * 60000UL;
          if (now - r.onAt >= runMs) relaySet(r, false, "หมดเวลาตามตาราง");
        } else {
          /* เช็คว่าถึงเวลาที่ตั้งไว้หรือยัง
           * lastDay กันไม่ให้สั่งซ้ำ เพราะเงื่อนไขนาที HH:MM เป็นจริงอยู่นานถึง 60 วินาที
           * ถ้าไม่กัน ระบบจะสั่งเปิดซ้ำหลายพันครั้งภายในนาทีเดียว */
          if (tmNow.tm_hour == c.onHour &&
              tmNow.tm_min  == c.onMin  &&
              r.lastDay != tmNow.tm_yday) {
            r.lastDay = tmNow.tm_yday;
            relaySet(r, true, "ถึงเวลาตามตาราง");
          }
        }
        break;
      }

      // ---------- โหมดอัตโนมัติ : ตัดสินจากค่าเซนเซอร์ ----------
      case MODE_AUTO: {
        if (!sensorOK) break;                    // ไม่สั่งงานด้วยข้อมูลที่เชื่อไม่ได้

        float v = (c.src == SRC_TEMP) ? temp : humi;
        bool  want;

        /* ช่วงหน่วง (hysteresis) คือหัวใจของโหมดนี้
         *
         * ถ้าใช้เกณฑ์เดียวตรง ๆ เช่น "เกิน 32 องศาเปิด ต่ำกว่า 32 ปิด"
         * พออุณหภูมิแกว่งอยู่แถว 32.0 พอดี รีเลย์จะเปิด-ปิดสลับกันรัวมาก
         * ยิ่ง DHT11 คลาดเคลื่อน +-2 องศา ยิ่งแกว่งหนัก หน้าสัมผัสพังเร็ว
         *
         * วิธีแก้คือแยกจุดเปิดกับจุดปิดออกจากกัน
         *   เกณฑ์ 32 ช่วงหน่วง 1  ->  เปิดเมื่อถึง 32.0  แต่ปิดเมื่อลดต่ำกว่า 31.0
         * ต้องเปลี่ยนแปลงจริงจังถึงจะสั่งงาน เสียงรบกวนเล็กน้อยไม่ทำให้สลับสถานะ */
        if (c.above) want = r.on ? (v > c.thr - c.hyst) : (v >= c.thr);
        else         want = r.on ? (v < c.thr + c.hyst) : (v <= c.thr);

        if (want != r.on) relaySet(r, want, "auto");
        break;
      }
    }
  }
}

/* ตัดรีเลย์ที่เปิดค้างนานเกินกำหนด ใช้เฉพาะโหมดสั่งเองเท่านั้น */
void taskRelayFailsafe() {
  if (RELAY_MAX_ON_MS == 0) return;
  uint32_t now = millis();

  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    RelayCtl &r = relays[i];
    if (!r.on || cfgs[i].mode != MODE_MANUAL) continue;
    if (now - r.onAt < RELAY_MAX_ON_MS) continue;

    Serial.printf("!! %s เปิดค้างครบ %.0f นาที - ตัดอัตโนมัติเพื่อความปลอดภัย\n",
                  r.name, RELAY_MAX_ON_MIN);
    cfgs[i].manualOn = false;
    relaySet(r, false, "failsafe timeout");
  }
}


// ===================== รับข้อความที่ส่งเข้ามาจาก MQTT ======================
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

  /* คำสั่งจาก MQTT จะสลับรีเลย์ตัวนั้นมาเป็นโหมด "สั่งเอง" เสมอ
   * ถ้าไม่ทำแบบนี้ ตรรกะอัตโนมัติจะสั่งกลับทันทีในรอบถัดไป
   * ผู้ใช้จะงงว่าสั่งเปิดแล้วทำไมดับเองใน 1 วินาที */

  // ---- ปิดทุกตัวพร้อมกัน (ปุ่มฉุกเฉิน) ----
  if (!strcmp(topic, TOPIC_CMDALL)) {
    bool target = (cmd == 1);
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
      cfgs[i].mode     = MODE_MANUAL;
      cfgs[i].manualOn = target;
    }
    taskControl();
    return;
  }

  // ---- คุมรีเลย์รายตัว ----
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (strcmp(topic, relays[i].cmdTopic) != 0) continue;
    cfgs[i].mode     = MODE_MANUAL;
    cfgs[i].manualOn = (cmd == -1) ? !relays[i].on : (cmd == 1);
    taskControl();
    return;
  }

  // ---- คุม LED ออนบอร์ด ----
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
    mqtt.subscribe(TOPIC_CMDSUB);
    publishAllRelayStatus();
  } else {
    Serial.printf("ล้มเหลว rc=%d (%s)\n", mqtt.state(), mqttStateText(mqtt.state()));
  }
}


// ============================== Web Server ================================
// ---- หน้าเว็บหลัก ----
void handleRoot() {
  // send_P อ่านข้อมูลจาก PROGMEM (flash) ส่งออกไปโดยไม่ต้องคัดลอกลง RAM ก่อน
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

/* ---- API : ส่งสถานะทั้งหมดเป็น JSON ----
 * หน้าเว็บเรียกทุก 2 วินาทีเพื่ออัปเดตตัวเลขบนจอ */
void handleStatus() {
  char tbuf[24];
  timeStampFull(tbuf, sizeof(tbuf));

  String j = "{";
  j += "\"time\":\"" + String(tbuf) + "\",";
  j += "\"synced\":" + String(timeSynced ? "true" : "false") + ",";

  // ถ้าเซนเซอร์อ่านไม่ได้ ต้องส่งเลข 0 ไม่ใช่ NaN เพราะ NaN ทำให้ JSON เสีย
  j += "\"temp\":" + String(sensorOK ? temp : 0.0, 1) + ",";
  j += "\"humi\":" + String(sensorOK ? humi : 0.0, 1) + ",";
  j += "\"sensorOK\":" + String(sensorOK ? "true" : "false") + ",";

  j += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  j += "\"mqtt\":" + String(mqtt.connected() ? "true" : "false") + ",";
  j += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  j += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  j += "\"uptime\":" + String(millis() / 1000) + ",";
  j += "\"tx\":" + String(pubCount) + ",";

  // ---- รีเลย์ทั้ง 3 ตัว ----
  j += "\"relays\":[";
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    RelayCfg &c = cfgs[i];
    if (i) j += ",";
    j += "{\"id\":"     + String(i + 1);
    j += ",\"on\":"     + String(relays[i].on ? "true" : "false");
    j += ",\"mode\":"   + String(c.mode);
    j += ",\"onH\":"    + String(c.onHour);
    j += ",\"onM\":"    + String(c.onMin);
    j += ",\"runMin\":" + String(c.runMin);
    j += ",\"src\":"    + String(c.src);
    j += ",\"above\":"  + String(c.above ? "true" : "false");
    j += ",\"thr\":"    + String(c.thr, 1);
    j += ",\"hyst\":"   + String(c.hyst, 1);
    j += ",\"remain\":" + String(relayRemainSec(i));
    j += "}";
  }
  j += "],";

  // ---- ข้อมูล MQTT สำหรับแสดงบนหน้าเว็บ ----
  j += "\"mq\":{";
  j += "\"host\":\""     + String(MQTT_HOST) + "\",";
  j += "\"port\":"       + String(MQTT_PORT) + ",";
  j += "\"clientId\":\"" + clientId + "\",";
  j += "\"pub\":[";
  j += "\"" + String(TOPIC_TEMP)   + "\",";
  j += "\"" + String(TOPIC_HUMI)   + "\",";
  j += "\"" + String(TOPIC_STATUS) + "\",";
  j += "\"" + String(TOPIC_ST_R1)  + "\",";
  j += "\"" + String(TOPIC_ST_R2)  + "\",";
  j += "\"" + String(TOPIC_ST_R3)  + "\"],";
  j += "\"sub\":[";
  j += "\"" + String(TOPIC_CMD_R1) + "\",";
  j += "\"" + String(TOPIC_CMD_R2) + "\",";
  j += "\"" + String(TOPIC_CMD_R3) + "\",";
  j += "\"" + String(TOPIC_CMDLED) + "\",";
  j += "\"" + String(TOPIC_CMDALL) + "\"]";
  j += "}}";

  server.send(200, "application/json", j);
}

// ---- API : สั่งเปิด/ปิดรีเลย์ด้วยมือ ----
void handleRelayCmd() {
  int id = server.arg("id").toInt();
  if (id < 1 || id > RELAY_COUNT) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"bad id\"}");
    return;
  }

  String s = server.arg("state");
  RelayCfg &c = cfgs[id - 1];
  c.mode = MODE_MANUAL;

  if      (s == "on")  c.manualOn = true;
  else if (s == "off") c.manualOn = false;
  else                 c.manualOn = !relays[id - 1].on;

  taskControl();                     // สั่งงานทันที ไม่ต้องรอรอบถัดไป
  server.send(200, "application/json", "{\"ok\":true}");
}

/* ---- API : บันทึกค่าตั้งของรีเลย์ ----
 * ส่งฟิลด์ไหนมาก็แก้เฉพาะฟิลด์นั้น ที่ไม่ส่งมาจะคงค่าเดิมไว้
 * ทุกค่าถูกจำกัดขอบเขตด้วย constrain ก่อนใช้เสมอ
 * เพราะข้อมูลจากหน้าเว็บเป็นสิ่งที่ผู้ใช้แก้ไขได้ ห้ามเชื่อโดยไม่ตรวจ */
void handleConfig() {
  int id = server.arg("id").toInt();
  if (id < 1 || id > RELAY_COUNT) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"bad id\"}");
    return;
  }

  RelayCfg &c = cfgs[id - 1];

  if (server.hasArg("mode"))   c.mode   = constrain(server.arg("mode").toInt(), 0, 2);
  if (server.hasArg("onH"))    c.onHour = constrain(server.arg("onH").toInt(), 0, 23);
  if (server.hasArg("onM"))    c.onMin  = constrain(server.arg("onM").toInt(), 0, 59);
  if (server.hasArg("runMin")) c.runMin = constrain(server.arg("runMin").toInt(), 1, 720);
  if (server.hasArg("src"))    c.src    = constrain(server.arg("src").toInt(), 0, 1);
  if (server.hasArg("above"))  c.above  = (server.arg("above").toInt() == 1);
  if (server.hasArg("thr"))    c.thr    = constrain(server.arg("thr").toFloat(), -20.0f, 120.0f);
  if (server.hasArg("hyst"))   c.hyst   = constrain(server.arg("hyst").toFloat(), 0.5f, 20.0f);

  /* เปลี่ยนโหมดแล้วต้องล้าง lastDay ด้วย
   * ไม่งั้นถ้าสลับไปโหมดอื่นแล้วกลับมาโหมดตั้งเวลาในวันเดียวกัน
   * ระบบจะจำว่า "วันนี้ทำไปแล้ว" และไม่ยอมทำงานจนกว่าจะข้ามวัน */
  if (server.hasArg("mode")) relays[id - 1].lastDay = -1;

  saveConfig();
  taskControl();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
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
      Serial.println("!! เซนเซอร์มีปัญหา - หยุดควบคุมอัตโนมัติชั่วคราว");
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

  publishAllRelayStatus();
  if (!sensorOK) return;

  char bufT[12], bufH[12], bufTime[24], bufJson[256];
  snprintf(bufT, sizeof(bufT), "%.1f", temp);
  snprintf(bufH, sizeof(bufH), "%.1f", humi);
  timeStampFull(bufTime, sizeof(bufTime));

  bool ok1 = mqtt.publish(TOPIC_TEMP, bufT);
  bool ok2 = mqtt.publish(TOPIC_HUMI, bufH);

  snprintf(bufJson, sizeof(bufJson),
           "{\"time\":\"%s\",\"temp\":%.1f,\"humi\":%.1f,"
           "\"r1\":%d,\"r2\":%d,\"r3\":%d,\"rssi\":%d,\"uptime\":%u}",
           bufTime, temp, humi,
           relays[0].on ? 1 : 0, relays[1].on ? 1 : 0, relays[2].on ? 1 : 0,
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

  // ---- นาฬิกา + วันที่ ----
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  if (timeSynced) oled.printf("%02d:%02d:%02d", tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
  else            oled.print("--:--:--");

  oled.setTextSize(1);
  if (timeSynced) {
    oled.setCursor(97, 0);
    oled.printf("%02d/%02d", tmNow.tm_mday, tmNow.tm_mon + 1);
  }
  oled.drawLine(0, 17, 127, 17, SSD1306_WHITE);

  // ---- ค่าเซนเซอร์ ----
  oled.setCursor(0, 21);
  if (sensorOK) oled.printf("T %.1fC   H %.0f%%", temp, humi);
  else          oled.print("Sensor Error!");

  // ---- สถานะรีเลย์ + ตัวอักษรบอกโหมด (M/S/A) ----
  oled.setCursor(0, 32);
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    const char m = (cfgs[i].mode == MODE_MANUAL) ? 'M'
                 : (cfgs[i].mode == MODE_SCHEDULE) ? 'S' : 'A';
    oled.printf("%d%c:%-3s ", i + 1, m, relays[i].on ? "ON" : "OFF");
  }

  // ---- IP : ใช้เปิดหน้า Dashboard ----
  oled.setCursor(0, 43);
  if (wifiOK) oled.printf("IP %s", WiFi.localIP().toString().c_str());
  else        oled.print("IP  no wifi");

  // ---- สถานะการเชื่อมต่อ ----
  oled.setCursor(0, 54);
  oled.printf("W:%s M:%s Tx:%u",
              wifiOK ? "OK" : "--", mqttOK ? "OK" : "--", pubCount);

  oled.display();
}


// ================================ SETUP ===================================
void setup() {
  Serial.begin(115200);

  /* บน ESP32 core 3.x ต้อง pinMode(OUTPUT) ก่อน แล้วค่อย digitalWrite()
   * ถ้าสลับลำดับ digitalWrite() จะไม่ทำงานเลย และรีเลย์ Active LOW จะติดตอนบูต */
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    pinMode(relays[i].pin, OUTPUT);
    digitalWrite(relays[i].pin, RELAY_OFF);   // RELAY_OFF = HIGH
    relays[i].on   = false;
    relays[i].onAt = 0;
  }
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

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
  loadConfig();

  Serial.println("\n=======================================================");
  Serial.println("  Lab10 : Smart Farm Web Dashboard");
  Serial.println("=======================================================");

  // ---------- WiFi ----------
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
    Serial.println("!! ต่อ WiFi ไม่สำเร็จ - Dashboard จะเปิดไม่ได้");
    Serial.println("   ตรวจว่าเป็นย่าน 2.4 GHz และรหัสผ่านถูกต้อง");
  } else {
    Serial.printf("  IP : %s   RSSI : %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  }

  // ---------- NTP ----------
  configTzTime(TZ_BANGKOK, NTP_SERVER1, NTP_SERVER2, NTP_SERVER3);

  // ---------- MQTT ----------
  clientId = "esp32-" + String((uint32_t)(ESP.getEfuseMac() >> 16), HEX);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);

  // ---------- Web Server ----------
  /* ไม่ระบุ method ให้รับได้ทั้ง GET และ POST
   * ทำให้ทดสอบจากช่อง URL ของเบราว์เซอร์ได้ตรง ๆ เช่น
   *     http://<IP>/api/relay?id=1&state=on
   * สะดวกมากตอนไล่หาปัญหา ไม่ต้องเปิด Postman หรือเขียนสคริปต์ */
  server.on("/",           handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/relay",  handleRelayCmd);
  server.on("/api/config", handleConfig);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("-------------------------------------------------------");
  Serial.printf ("  เปิด Dashboard ที่  ->  http://%s\n",
                 WiFi.localIP().toString().c_str());
  Serial.println("-------------------------------------------------------");
  Serial.printf ("  MQTT Broker : %s:%u   Client ID : %s\n",
                 MQTT_HOST, MQTT_PORT, clientId.c_str());
  Serial.printf ("  Topic ทั้งหมดดูได้ในหน้า Dashboard\n");
  Serial.printf ("  ตัวตัดอัตโนมัติ : ปิดรีเลย์โหมดสั่งเองเมื่อค้างครบ %.0f นาที\n",
                 RELAY_MAX_ON_MIN);
  Serial.println("=======================================================\n");
}


// ================================= LOOP ===================================
void loop() {
  uint32_t now = millis();

  server.handleClient();  // ตอบคำขอจากเบราว์เซอร์ ต้องเรียกถี่ ๆ เหมือน mqtt.loop()
  ensureMqtt(now);
  mqtt.loop();

  taskUpdateTime(now);
  taskReadDHT(now);
  taskControl();          // ตรรกะ 3 โหมด (อ่าน millis() เอง)
  taskRelayFailsafe();
  taskPublish(now);
  taskDisplay(now);
}
