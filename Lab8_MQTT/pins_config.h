#pragma once
// ---------- Onboard ----------
#define LED_PIN       2

// ---------- Relay Output (Active LOW) ----------
#define RELAY1_PIN   17
#define RELAY2_PIN   16
#define RELAY3_PIN    4

#define RELAY_ON     LOW
#define RELAY_OFF    HIGH

// ---------- Push Button (Active LOW) ----------
// บอร์ดมี R pull-up 10k ภายนอกให้แล้วทั้ง 3 ตัว จึงใช้ pinMode(pin, INPUT) เฉยๆ
#define SW1_PIN      34   // input-only : ใส่ INPUT_PULLUP ไม่ได้ (ไม่มี pull-up ภายใน)
#define SW2_PIN      35   // input-only : ใส่ INPUT_PULLUP ไม่ได้ (ไม่มี pull-up ภายใน)
#define SW3_PIN      32   // ขาปกติ : มี pull-up ภายในให้ใช้ แต่บอร์ดมีภายนอกแล้วจึงไม่ต้อง

// ---------- Opto-isolated Input (Active LOW) ----------
#define ISO1_PIN     33   // เทอร์มินัล IN1
#define ISO2_PIN     27   // เทอร์มินัล IN2

// ---------- Sensor ----------
#define DHT_PIN      15
#define DHT_TYPE     DHT11   // เปลี่ยนเป็น DHT22 ถ้าใช้รุ่น 22

// ---------- LDR : ใช้เป็น Digital Input เท่านั้น ----------
// GPIO14 เป็น ADC2 ซึ่ง WiFi ยึดไปใช้ -> analogRead() ใช้ไม่ได้เมื่อเปิด WiFi
#define LDR_PIN      14
#define LDR_DARK     HIGH    // สถานะตอน "มืด" : ยังไม่ยืนยัน ต้องทดสอบขั้วก่อนใช้งานจริง
#define LDR_STABLE_MS 3000   // ต้องนิ่งกี่ ms ถึงจะยอมรับว่าเปลี่ยนสถานะจริง

// ---------- I2C OLED ----------
#define I2C_SDA      21
#define I2C_SCL      22
#define OLED_ADDR    0x3C
#define OLED_WIDTH  128
#define OLED_HEIGHT  64

// ---------- RS485 (ใช้ Serial0 — ชนกับ USB debug) ----------
#define RS485_SERIAL Serial
#define RS485_BAUD   9600
