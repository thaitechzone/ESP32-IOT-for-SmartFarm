# ESP32 Devkit V2 Board — IO Reference (Smart Farm IoT)

เอกสารอ้างอิงขา GPIO และอุปกรณ์บนบอร์ด **ESP32 Devkit V2** (บอร์ดฐาน 3 รีเลย์ + OLED + RS485/RS232)
ใช้เป็นแหล่งอ้างอิงหลักก่อนเขียนโค้ดทุกครั้ง เพื่อไม่ให้จองขาชนกัน

---

## 1. ภาพรวมบอร์ด

| หัวข้อ | รายละเอียด |
|---|---|
| MCU | ESP32-WROOM-32 (โมดูล ESP32 DevKit เสียบบนบอร์ดฐาน) |
| ไฟเลี้ยง | **9–12 VDC** (ซิลค์บนบอร์ดระบุรองรับ 9~24 VDC/AC) เข้าที่เทอร์มินัล Supply |
| USB | Micro-USB บนโมดูล ESP32 (ใช้อัปโหลดโปรแกรม + Serial Monitor) |
| จอแสดงผล | OLED I2C (ปกติ SSD1306 128×64) |
| เอาต์พุต | รีเลย์ 3 ช่อง (NO / C / NC) |
| อินพุต | สวิตช์กด 3 ตัว (SW1–SW3) + อินพุตแยกกราวด์ opto 2 ช่อง (ISO1, ISO2) |
| เซนเซอร์ | DHT (อุณหภูมิ-ความชื้น), LDR (แสง) |
| สื่อสาร | RS485 (A+ / B−) หรือ RS232 เลือกด้วยจัมเปอร์ SW Mode |
| ปุ่มบนบอร์ด | BOOT, Reset |

---

## 2. ตารางขา GPIO (สรุปใช้งานจริง)

| GPIO | อุปกรณ์ | ทิศทาง | Logic | หมายเหตุสำคัญ |
|:---:|---|---|---|---|
| **17** | Relay 1 | OUTPUT | **Active LOW** (0 = ทำงาน) | — |
| **16** | Relay 2 | OUTPUT | **Active LOW** | — |
| **4**  | Relay 3 | OUTPUT | **Active LOW** | เป็น ADC2 (ใช้ analog ไม่ได้ตอนเปิด WiFi) |
| **34** | SW1 (ปุ่มกด) | **INPUT อย่างเดียว** | **Active LOW** (กด = 0) | ไม่มี pull-up ภายใน — บอร์ดมี R 10k ภายนอกให้แล้ว |
| **35** | SW2 (ปุ่มกด) | **INPUT อย่างเดียว** | **Active LOW** | ไม่มี pull-up ภายใน — ใช้ R 10k บนบอร์ด |
| **32** | SW3 (ปุ่มกด) | INPUT | **Active LOW** | ใช้ `INPUT_PULLUP` ได้ |
| **33** | ISO1 (opto input) | INPUT | **Active LOW** (0 = ทำงาน) | ต่อจากเทอร์มินัล IN1 |
| **27** | ISO2 (opto input) | INPUT | **Active LOW** | ต่อจากเทอร์มินัล IN2 |
| **15** | DHT (Data) | I/O | 1-Wire protocol | เป็น Strapping pin — ดูข้อ 5.3 |
| **14** | LDR (แสง) | **DIGITAL IN** | สว่าง / มืด (0 หรือ 1) | **ใช้ `digitalRead()` เท่านั้น — ห้ามใช้ `analogRead()`** ดูข้อ 5.1 |
| **2**  | GPIO2 / LED ออนบอร์ด | OUTPUT | Active HIGH | เป็น Strapping pin — ห้ามต่อโหลดดึงตอนบูต |
| **21** | I2C SDA (OLED) | I/O | — | ค่า default ของ ESP32 — **ยืนยันกับบอร์ดจริงอีกครั้ง** |
| **22** | I2C SCL (OLED) | I/O | — | ค่า default ของ ESP32 — **ยืนยันกับบอร์ดจริงอีกครั้ง** |
| **1 / 3** | TX0 / RX0 → RS485 | UART | — | **ชนกับ USB Serial** ดูข้อ 5.2 |

> **หมายเหตุ:** ขา I2C (21/22) ในผังภาพไม่ได้ระบุเลขไว้ ผมใส่ค่า default ของ ESP32 ไว้ก่อน
> ถ้าจอไม่ขึ้น ให้รัน I2C Scanner (ข้อ 8) เพื่อหาขาและแอดเดรสจริง

---

## 3. ขาที่ยังว่าง (ใช้ต่อเซนเซอร์เพิ่มได้)

ขาที่ **ไม่ได้ถูกจอง** โดยอุปกรณ์บนบอร์ด และแนะนำให้ใช้:

| GPIO | ความสามารถ | เหมาะกับ |
|:---:|---|---|
| 36 (VP), 39 (VN) | **ADC1** — input อย่างเดียว | **soil moisture / pH / EC** — ใช้ได้ขณะ WiFi ทำงาน |
| 25, 26 | ADC2 + DAC + UART2 ได้ | Serial debug แยก, เอาต์พุตอนาล็อก |
| 5, 18, 19, 23 | SPI (VSPI) | SD Card, LoRa (SX1278), จอ TFT |
| 13 | I/O ทั่วไป | DS18B20, ปุ่มเพิ่ม |

**กฎง่ายๆ:** เซนเซอร์ analog ทุกตัวให้ต่อ **GPIO 32–39 (ADC1)** เท่านั้น ถ้าระบบใช้ WiFi

---

## 4. Header ขา (คัดลอกไปใช้ได้เลย)

สร้างไฟล์ `pins_config.h`:

```cpp
#pragma once

// ---------- Relay Output (Active LOW) ----------
#define RELAY1_PIN   17
#define RELAY2_PIN   16
#define RELAY3_PIN    4

#define RELAY_ON     LOW
#define RELAY_OFF    HIGH

// ---------- Push Button (Active LOW) ----------
#define SW1_PIN      34   // input-only, ใช้ pull-up ภายนอกบนบอร์ด
#define SW2_PIN      35   // input-only, ใช้ pull-up ภายนอกบนบอร์ด
#define SW3_PIN      32   // ใช้ INPUT_PULLUP ได้

// ---------- Opto-isolated Input (Active LOW) ----------
#define ISO1_PIN     33   // เทอร์มินัล IN1
#define ISO2_PIN     27   // เทอร์มินัล IN2

// ---------- Sensor ----------
#define DHT_PIN      15
#define DHT_TYPE     DHT22   // เปลี่ยนเป็น DHT11 ถ้าใช้รุ่น 11

// ---------- LDR : ใช้เป็น Digital Input ----------
#define LDR_PIN      14
#define LDR_DARK     HIGH    // สถานะตอน "มืด" — ยืนยันด้วยสเก็ตช์ข้อ 5.1 ก่อน
#define LDR_STABLE_MS 3000   // ต้องนิ่งกี่ ms ถึงจะยอมรับว่าเปลี่ยนสถานะจริง

// ---------- Onboard ----------
#define LED_PIN       2

// ---------- I2C OLED ----------
#define I2C_SDA      21
#define I2C_SCL      22
#define OLED_ADDR    0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT  64

// ---------- RS485 (ใช้ Serial0 — ชนกับ USB debug) ----------
#define RS485_SERIAL Serial
#define RS485_BAUD   9600
```

ตัวอย่าง `setup()` เริ่มต้นที่ปลอดภัย:

```cpp
#include "pins_config.h"

void setup() {
  // ตั้งรีเลย์เป็น OFF "ก่อน" ตั้ง pinMode
  // ไม่งั้นรีเลย์จะกระตุกทำงาน 1 ครั้งทุกครั้งที่บูต
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  pinMode(SW1_PIN, INPUT);          // 34 — input-only, ห้ามใส่ INPUT_PULLUP
  pinMode(SW2_PIN, INPUT);          // 35 — input-only, ห้ามใส่ INPUT_PULLUP
  pinMode(SW3_PIN, INPUT_PULLUP);   // 32 — มี pull-up ภายใน

  pinMode(ISO1_PIN, INPUT_PULLUP);
  pinMode(ISO2_PIN, INPUT_PULLUP);

  // LDR เป็นวงจรแบ่งแรงดัน — ต้องใช้ INPUT เปล่าเท่านั้น
  // ถ้าใส่ INPUT_PULLUP จะมี R ภายใน 45k มาขนานกับวงจร ทำให้จุดตัดเพี้ยน
  pinMode(LDR_PIN, INPUT);

  pinMode(LED_PIN, OUTPUT);
}

// อ่านปุ่ม: Active LOW → กดแล้วได้ true
inline bool isPressed(uint8_t pin) { return digitalRead(pin) == LOW; }

// สั่งรีเลย์: on = true → ทำงาน
inline void relayWrite(uint8_t pin, bool on) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
}
```

---

## 5. ข้อควรระวัง (อ่านก่อนเขียนโค้ด)

### 5.1 LDR บน GPIO14 — ใช้เป็น Digital Input เท่านั้น

**สรุปกฎ:** ขานี้อ่านด้วย `digitalRead(14)` เท่านั้น **ห้ามใช้ `analogRead(14)`**

**เหตุผล:** GPIO14 เป็นขาของ **ADC2** ซึ่ง ESP32 ยกให้ WiFi ใช้งาน พอเรียก `WiFi.begin()` แล้ว
`analogRead(14)` จะคืนค่า 0 หรือค่าขยะทันที (เป็นข้อจำกัดของชิป ไม่ใช่ความผิดพลาดของบอร์ด)

แต่ **`digitalRead()` เดินผ่าน GPIO matrix คนละทางกับวงจร ADC** จึงทำงานได้ปกติแม้ WiFi เปิดอยู่
บอร์ดนี้จึงออกแบบให้ใช้ LDR แบบ digital ตั้งแต่ต้น — ได้ผลลัพธ์เป็น **สว่าง / มืด** ซึ่งเพียงพอ
สำหรับงาน Smart Farm ทั่วไป (เปิดไฟกลางคืน, นับชั่วโมงแสง, สั่งม่านบังแดด)

#### ขั้นที่ 1 — หาขั้ว (polarity) ของวงจรก่อน

วงจรแบ่งแรงดันของ LDR ต่อได้ 2 แบบ ให้ทดสอบก่อนว่าบอร์ดนี้เป็นแบบไหน:

```cpp
void setup() {
  Serial.begin(115200);
  pinMode(14, INPUT);          // INPUT เปล่า ห้าม INPUT_PULLUP
}
void loop() {
  Serial.println(digitalRead(14) ? "HIGH" : "LOW");
  delay(300);
}
```

เอามือบัง LDR แล้วดูว่าค่าเปลี่ยนเป็นอะไร จากนั้นตั้งค่าใน `pins_config.h`:

| ผลที่ได้ตอนบัง LDR (มืด) | ตั้งค่า |
|---|---|
| ขึ้น `HIGH` | `#define LDR_DARK HIGH` |
| ขึ้น `LOW`  | `#define LDR_DARK LOW` |

> ถ้าค่าไม่เปลี่ยนเลยไม่ว่าจะบังหรือไม่บัง แปลว่าจุดตัดของวงจรไม่ตรงกับระดับแสงในห้อง
> ให้ทดสอบใหม่โดยส่องไฟฉายใส่ตรงๆ เทียบกับปิดไฟให้มืดสนิท

#### ขั้นที่ 2 — ใส่ตัวกรองกันค่าแกว่ง (สำคัญ)

LDR เป็นสัญญาณ analog ที่ค่อยๆ ไล่ระดับ ไม่ใช่สวิตช์ที่ตัดฉับ ช่วง **เช้ามืดกับพลบค่ำ**
แรงดันจะไต่ผ่านจุดตัดของขา digital อย่างช้าๆ ทำให้อ่านค่าได้ **สลับ HIGH/LOW รัวๆ เป็นนาที**

ถ้าเอาค่าดิบไปสั่งรีเลย์ตรงๆ → **ไฟหรือปั๊มจะกระพริบเปิด-ปิดหลายสิบครั้ง** หน้าสัมผัสรีเลย์พังเร็วมาก

ทางแก้คือกรองด้วยเวลา — ต้องนิ่งค้างไว้ครบตามกำหนดถึงจะยอมรับว่าเปลี่ยนสถานะจริง:

```cpp
static bool     ldrState   = false;   // สถานะที่ยืนยันแล้ว (true = มืด)
static bool     ldrLastRaw = false;
static uint32_t ldrChangeAt = 0;

void ldrUpdate() {
  bool raw = (digitalRead(LDR_PIN) == LDR_DARK);
  uint32_t now = millis();

  if (raw != ldrLastRaw) {
    ldrLastRaw  = raw;
    ldrChangeAt = now;                       // ค่าเพิ่งขยับ เริ่มจับเวลาใหม่
  } else if (raw != ldrState &&
             (now - ldrChangeAt) >= LDR_STABLE_MS) {
    ldrState = raw;                          // นิ่งครบเวลาแล้ว ยอมรับ
  }
}

bool isDark()  { return ldrState; }
bool isLight() { return !ldrState; }
```

เรียก `ldrUpdate()` ทุกรอบใน `loop()` แล้วใช้ `isDark()` ตัดสินใจ — ฟังก์ชันนี้ไม่บล็อก
และการลบแบบ `unsigned` ทำให้ทนการวน overflow ของ `millis()` ที่ 49 วันได้ถูกต้อง

ค่าแนะนำของ `LDR_STABLE_MS`:

| งาน | ค่าที่เหมาะ |
|---|---|
| เปิด-ปิดไฟส่องสว่าง | `3000` – `10000` ms |
| นับชั่วโมงแสงต่อวัน | `30000` ms (30 วินาที) |
| สั่งม่าน/มู่ลี่ | `60000` ms — กันเมฆบังชั่วครู่ |

#### ขั้นที่ 3 (ถ้าต้องการ) — ใช้ Interrupt แทนการ poll

เหมาะกับโหมดประหยัดไฟที่ต้องการให้ ESP32 ตื่นเมื่อแสงเปลี่ยน:

```cpp
attachInterrupt(digitalPinToInterrupt(LDR_PIN), onLightChange, CHANGE);
```

> ยังต้องกรองเวลาอยู่ดี เพราะช่วงพลบค่ำจะยิง interrupt รัวมาก
> ใน ISR ให้แค่ตั้งธง `volatile bool` แล้วไปประมวลผลจริงใน `loop()`

#### ถ้าจำเป็นต้องได้ค่าแสงเป็นตัวเลขจริงๆ

การอ่านแบบ digital บอกได้แค่ "สว่าง/มืด" และ **ปรับจุดตัดในซอฟต์แวร์ไม่ได้เลย** เพราะจุดตัด
ถูกกำหนดตายตัวด้วย R ในวงจรบนบอร์ด ถ้างานต้องการค่าความสว่างเป็นตัวเลข (lux) ให้เลือก:

1. **ต่อ BH1750 เข้า I2C bus เดิม (GPIO21/22)** — วิธีที่ดีที่สุด ได้ค่า lux จริง ไม่กิน ADC เลย
2. เดินสาย LDR ไป **GPIO36 หรือ GPIO39 (ADC1)** แล้วใช้ `analogRead()` ได้ตามปกติแม้ WiFi เปิด

### 5.2 RS485 ใช้ Serial0 ร่วมกับพอร์ต USB

RS485 บนบอร์ดต่อกับ **UART0 (GPIO1/GPIO3)** ซึ่งเป็นตัวเดียวกับที่ใช้อัปโหลดโปรแกรมและ Serial Monitor

ผลที่ตามมา:
- `Serial.println("debug")` ทุกบรรทัด **จะถูกส่งออกสาย RS485 ด้วย** → อุปกรณ์ slave อาจรวน
- ขณะอัปโหลดโปรแกรม สัญญาณจะวิ่งออก RS485 → **ควรถอดสาย A+/B− ออกก่อนอัปโหลด**
- ถ้าอุปกรณ์ RS485 ส่งข้อมูลกลับมาตอนกำลังอัปโหลด อาจทำให้อัปโหลดล้มเหลว

**ทางแก้:**
- ตอนใช้งานจริงกับ RS485/Modbus ให้ **เลิกใช้ `Serial.print()` เพื่อ debug ทั้งหมด**
- ย้าย debug ไปออกจอ OLED แทน หรือส่งขึ้น MQTT
- หรือแยก UART2 ออกไปที่ขาว่างสำหรับ debug โดยเฉพาะ:
  ```cpp
  Serial2.begin(115200, SERIAL_8N1, 26, 25);  // RX=GPIO26, TX=GPIO25
  ```

### 5.3 Strapping Pins — GPIO2, GPIO15

ขาสองตัวนี้ ESP32 อ่านค่าตอนบูตเพื่อตัดสินใจโหมดการทำงาน:

| ขา | บทบาทตอนบูต | ผลกระทบ |
|---|---|---|
| GPIO2 | ต้องลอยหรือ LOW ตอนเข้าโหมด flash | ถ้าถูกดึงขึ้น HIGH แรงๆ อาจอัปโหลดไม่ได้ |
| GPIO15 | ถ้าเป็น LOW ตอนบูต จะปิดข้อความ boot log | DHT มี pull-up อยู่แล้วจึงปกติดี |

ใช้งานตามผังนี้โดยทั่วไปไม่มีปัญหา แต่ถ้าอัปโหลดไม่ผ่านให้สงสัยสองขานี้ก่อน

### 5.4 รีเลย์กระตุกตอนบูต

ระหว่างที่ ESP32 กำลังบูต ขา GPIO จะอยู่ในสถานะ floating ชั่วครู่ ทำให้รีเลย์อาจ **คลิกทำงาน 1 ครั้ง** ทุกครั้งที่เปิดเครื่อง

ถ้าคุมปั๊มน้ำหรือวาล์ว ให้:
- เขียน `digitalWrite(pin, HIGH)` **ก่อน** `pinMode(pin, OUTPUT)` (ตามตัวอย่างข้อ 4)
- ถ้ายังกระตุกอยู่ ให้เลือกใช้หน้าสัมผัส NC/NO ให้ถูกด้าน หรือเพิ่ม R pull-up 10k ที่ขาควบคุมรีเลย์

### 5.5 GPIO34 / GPIO35 เป็น Input-only

- **ตั้งเป็น OUTPUT ไม่ได้** — คอมไพล์ผ่านแต่ไม่ทำงาน
- **ไม่มี pull-up/pull-down ภายใน** — `INPUT_PULLUP` ไม่มีผลใดๆ
- บอร์ดนี้มี R 10k ภายนอกให้แล้ว (เห็นสัญลักษณ์ 10k บนผัง) จึงใช้ `pinMode(pin, INPUT)` ได้เลย
- ถ้าต่อปุ่มเพิ่มเองที่ขา 34/35/36/39 **ต้องใส่ R pull-up 10k เองเสมอ**

---

## 6. ขั้นตอนอัปโหลดโปรแกรม

### ตั้งค่าใน Arduino IDE

| หัวข้อ | ค่าที่ใช้ |
|---|---|
| Board | `ESP32 Dev Module` |
| Upload Speed | `921600` (ถ้าไม่นิ่งให้ลดเป็น `115200`) |
| Flash Frequency | `80 MHz` |
| Flash Mode | `QIO` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Default 4MB with spiffs` |
| Core Debug Level | `None` |
| Port | COM ที่ขึ้นตอนเสียบ USB |

### วิธีอัปโหลด

1. **ถอดสาย RS485 (A+/B−) ออกก่อน** — กันสัญญาณรบกวนตอน flash
2. เสียบสาย Micro-USB
3. กด **BOOT ค้างไว้**
4. ยังกด BOOT อยู่ → กด **Reset** 1 ครั้งแล้วปล่อย (หรือกด Upload ใน IDE)
5. เมื่อขึ้น `Connecting...` แล้วเริ่ม `Writing at 0x...` → **ปล่อยปุ่ม BOOT ได้**

> บอร์ดส่วนใหญ่มีวงจร auto-reset จึงไม่ต้องกด BOOT
> แต่ถ้าขึ้น `Failed to connect to ESP32: Timed out waiting for packet header` ให้ใช้วิธีกด BOOT ตามด้านบน

### จัมเปอร์ SW Mode (RS485 / RS232)

| สถานะจัมเปอร์ | โหมด |
|---|---|
| **กด / เสียบจัมเปอร์** | RS485 |
| **ลอย / ถอดจัมเปอร์** | RS232 |

---

## 7. ไลบรารีที่ต้องติดตั้ง

ติดตั้งผ่าน Library Manager (`Sketch → Include Library → Manage Libraries`):

| ไลบรารี | ใช้กับ |
|---|---|
| `DHT sensor library` by Adafruit | DHT บน GPIO15 |
| `Adafruit Unified Sensor` | dependency ของตัวบน |
| `Adafruit SSD1306` + `Adafruit GFX Library` | จอ OLED |
| `PubSubClient` | MQTT (ถ้าใช้) |
| `ModbusMaster` | RS485 Modbus RTU (ถ้าใช้) |
| `ArduinoJson` | รับ-ส่งข้อมูล JSON |

### ESP32 Board Package

`File → Preferences → Additional Board Manager URLs` ใส่:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

---

## 8. จอ OLED (I2C)

จอบนบอร์ดนี้ต่อผ่าน **I2C bus** ใช้ชิป SSD1306 ขนาด 128×64 แอดเดรสปกติ `0x3C`

| หัวข้อ | ค่า |
|---|---|
| Interface | I2C (2 เส้น: SDA + SCL) |
| SDA | GPIO21 *(ค่า default ของ ESP32 — ยืนยันด้วย scanner ข้อ 8.1)* |
| SCL | GPIO22 *(ค่า default ของ ESP32 — ยืนยันด้วย scanner ข้อ 8.1)* |
| Address | `0x3C` (บางรุ่นเป็น `0x3D`) |
| ความละเอียด | 128×64 (บางรุ่น 128×32) |
| ไฟเลี้ยง | 3.3V |

**ข้อดีของ I2C บนบอร์ดนี้:** ใช้แค่ 2 ขา และ **ต่อพ่วงอุปกรณ์อื่นบนเส้นเดียวกันได้อีกหลายตัว**
เช่น BH1750 (แสง), SHT31 (อุณหภูมิ-ความชื้น), DS3231 (นาฬิกา), ADS1115 (ADC ภายนอก)
ขอแค่แอดเดรสไม่ชนกัน — เป็นทางขยายระบบที่ประหยัดขาที่สุดสำหรับบอร์ดนี้

### 8.1 I2C Scanner — รันก่อนเสมอถ้าจอไม่ติด

```cpp
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(21, 22);          // SDA=21, SCL=22 — ลองสลับ/เปลี่ยนถ้าไม่เจอ
  Serial.println("\nI2C Scanner");

  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("พบอุปกรณ์ที่แอดเดรส 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) Serial.println("ไม่พบอุปกรณ์ I2C — ตรวจสายและขา SDA/SCL");
}

void loop() {}
```

ผลที่ควรได้: `พบอุปกรณ์ที่แอดเดรส 0x3C` (หรือ `0x3D` ในจอบางรุ่น)

### 8.2 ตัวอย่างใช้งานจริง — แสดงค่าเซนเซอร์บนจอ

ต้องติดตั้ง `Adafruit SSD1306` + `Adafruit GFX Library` ก่อน

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pins_config.h"

Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);  // -1 = ไม่มีขา reset

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);          // Fast mode 400kHz — จอลื่นขึ้นชัดเจน

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // จอไม่ตอบสนอง — กะพริบ LED ออนบอร์ดแจ้งเตือน แล้วไปต่อ
    pinMode(LED_PIN, OUTPUT);
    for (int i = 0; i < 6; i++) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(150);
    }
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("Smart Farm");
  oled.display();                 // ต้องเรียก display() ทุกครั้ง ไม่งั้นจอไม่อัปเดต
}

void showStatus(float temp, float humi, bool dark, bool pumpOn) {
  oled.clearDisplay();

  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("SMART FARM");
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  oled.setTextSize(2);            // ตัวใหญ่สำหรับค่าที่ต้องอ่านไกล
  oled.setCursor(0, 16);
  oled.printf("%.1fC", temp);
  oled.setCursor(66, 16);
  oled.printf("%.0f%%", humi);

  oled.setTextSize(1);
  oled.setCursor(0, 40);
  oled.printf("Light: %s", dark ? "DARK" : "BRIGHT");
  oled.setCursor(0, 52);
  oled.printf("Pump : %s", pumpOn ? "ON" : "OFF");

  oled.display();
}
```

**ข้อควรระวังของจอ OLED:**

| เรื่อง | รายละเอียด |
|---|---|
| ลืม `display()` | เขียนข้อความแล้วจอไม่เปลี่ยน — สาเหตุอันดับ 1 ของ "จอเสีย" |
| อัปเดตถี่เกินไป | เรียก `display()` ทุกรอบ loop จะกินเวลามาก **ควรอัปเดตทุก 500–1000 ms พอ** |
| จอไม่ติดแล้วโปรแกรมค้าง | อย่าใช้ `while(1);` ตอน `begin()` ล้มเหลว ระบบรดน้ำต้องทำงานต่อได้แม้จอเสีย |
| Burn-in | ถ้าโชว์ภาพนิ่ง 24 ชม. พิกเซลจะเสื่อม — ควรดับจอเมื่อไม่มีคนดู (`oled.clearDisplay()` + `display()`) |
| RAM | ไลบรารี Adafruit กินบัฟเฟอร์ 1KB สำหรับ 128×64 — ปกติไม่มีปัญหาบน ESP32 |
| ใช้ร่วมกับ FreeRTOS | ถ้าเขียนจอจากหลาย task ต้องมี mutex ครอบ ไม่งั้นภาพเพี้ยน |

---

## 9. สเก็ตช์ทดสอบบอร์ด (Smoke Test)

ใช้ยืนยันว่าขาทุกตัวถูกต้องตามเอกสารนี้ ก่อนเริ่มเขียนระบบจริง

```cpp
#include "pins_config.h"

void setup() {
  Serial.begin(115200);

  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  digitalWrite(RELAY3_PIN, RELAY_OFF);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(SW3_PIN, INPUT_PULLUP);
  pinMode(ISO1_PIN, INPUT_PULLUP);
  pinMode(ISO2_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Board smoke test - กดปุ่ม SW1/SW2/SW3 เพื่อสั่งรีเลย์ 1/2/3");
}

void loop() {
  bool s1 = digitalRead(SW1_PIN) == LOW;
  bool s2 = digitalRead(SW2_PIN) == LOW;
  bool s3 = digitalRead(SW3_PIN) == LOW;

  digitalWrite(RELAY1_PIN, s1 ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY2_PIN, s2 ? RELAY_ON : RELAY_OFF);
  digitalWrite(RELAY3_PIN, s3 ? RELAY_ON : RELAY_OFF);
  digitalWrite(LED_PIN, (s1 || s2 || s3) ? HIGH : LOW);

  Serial.printf("SW:%d%d%d  ISO:%d%d  LDR:%s\n",
                s1, s2, s3,
                digitalRead(ISO1_PIN) == LOW,
                digitalRead(ISO2_PIN) == LOW,
                digitalRead(LDR_PIN) == LDR_DARK ? "DARK" : "LIGHT");
  delay(300);
}
```

> สเก็ตช์นี้ใช้ `Serial.print()` ซึ่งจะส่งออก RS485 ด้วย — ถอดสาย A+/B− ก่อนทดสอบ

---

## 10. สรุปแผนผังสาย (Wiring Summary)

```
                   ESP32 Devkit V2
  +----------------------------------------------+
  |  Power 9-12V ---> เทอร์มินัล Supply           |
  |                                              |
  |  GPIO15 ------ DHT22 (Temp/Humidity)         |
  |  GPIO14 ------ LDR   (Light) [DIGITAL only!] |
  |  GPIO21/22 --- OLED  (I2C, 0x3C)             |
  |                                              |
  |  GPIO34 <----- SW1   (Active LOW)            |
  |  GPIO35 <----- SW2   (Active LOW)            |
  |  GPIO32 <----- SW3   (Active LOW)            |
  |  GPIO33 <----- ISO1 / IN1  (Opto, Active LOW)|
  |  GPIO27 <----- ISO2 / IN2  (Opto, Active LOW)|
  |                                              |
  |  GPIO17 -----> Relay1  NO1/C1/NC1 (Act. LOW) |
  |  GPIO16 -----> Relay2  NO2/C2/NC2 (Act. LOW) |
  |  GPIO4  -----> Relay3  NO3/C3/NC3 (Act. LOW) |
  |                                              |
  |  GPIO1/3 <---> RS485 A+/B-   [ชนกับ USB !]   |
  +----------------------------------------------+

  ขาว่างแนะนำ:
    GPIO36, GPIO39      -> ADC1 : เซนเซอร์ความชื้นดิน / pH / EC
    GPIO25, GPIO26      -> UART2 สำหรับ debug แยก
    GPIO5, 18, 19, 23   -> SPI  : LoRa / SD Card
```

---

## 11. สิ่งที่ยังต้องยืนยันจากบอร์ดจริง

รายการนี้อนุมานจากค่ามาตรฐานของ ESP32 เพราะผังในภาพไม่ได้ระบุตัวเลขไว้ — ควรตรวจกับซิลค์บนบอร์ดหรือวัดด้วยมัลติมิเตอร์:

- [ ] ขา I2C ของ OLED เป็น GPIO21/22 จริงหรือไม่ (ใช้ I2C Scanner ข้อ 8 ยืนยัน)
- [ ] แอดเดรส OLED เป็น `0x3C` หรือ `0x3D`
- [ ] รุ่น DHT ที่ติดตั้งจริง — DHT11 หรือ DHT22
- [ ] ขั้วของ LDR ตอนมืดเป็น `HIGH` หรือ `LOW` (ทดสอบตามขั้นที่ 1 ในข้อ 5.1 แล้วตั้ง `LDR_DARK`)
- [ ] พิกัดกระแสหน้าสัมผัสรีเลย์ (โดยทั่วไป 10A 250VAC)

---

## 12. อ้างอิง

- [ESP32 Datasheet — Espressif](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
- [Arduino-ESP32 Core Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [ESP32 ADC2 + WiFi limitation (ESP-IDF)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html)
