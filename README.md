# Fluidnc-CNC-Pendant v2.6

# 🎛️ CNC Pendant for FluidNC

**Arduino Nano ESP32 tabanlı (Herhangi bir işlemci olur), ST7789 TFT ekranlı, FluidNC uyumlu CNC kumanda paneli.**

> Tasarım & Geliştirme: **VOLTveTORK**

---

## 📸 Özellikler

- ✅ FluidNC ile doğrudan UART haberleşmesi
- ✅ Gerçek zamanlı **Machine Position (MPos)**
- ✅ Rotary encoder ile hassas jog kontrolü
- ✅ 4 buton: HOME / ZERO / EKSEN / HIZ-STEP
- ✅ 5 farklı jog adım boyutu: 0.001 / 0.01 / 0.1 / 1.0 / 10.0 mm
- ✅ 4 farklı jog hızı: 100 / 500 / 1000 / 3000 mm/dk
- ✅ X, Y, Z eksen seçimi
- ✅ Durum göstergesi: IDLE / RUN / HOLD / ALARM / HOME / JOG
- ✅ Homed durumu takibi
- ✅ Popup bildirimleri

---

## 🔧 Donanım

| Bileşen | Model |
|---|---|
| Mikrodenetleyici | Arduino Nano ESP32 |
| Ekran | 1.9" ST7789 IPS TFT (320×170) |
| Encoder | EC11 Rotary Encoder (SW dahil) |
| CNC Kontrolcü | FluidNC (ESP32-S3) |

---

## 📌 Pin Bağlantıları

### Rotary Encoder
| Encoder | Nano ESP32 |
|---|---|
| CLK (Out A) | D2 |
| DT (Out B) | D3 |
| SW (Buton) | D12 |
| GND | GND |

### Butonlar (Aktif LOW — dahili pull-up)
| Buton | Nano ESP32 |
|---|---|
| HOME | D4 |
| ZERO | D5 |
| EKSEN | A6 |
| HIZ/STEP | A7 |

### ST7789 TFT Ekran (SPI)
| TFT Pin | Nano ESP32 | Not |
|---|---|---|
| SCL / SCK | D13 | Otomatik (SPI) |
| SDA / MOSI | D11 | Otomatik (SPI) |
| CS | D10 | |
| DC | D6 | |
| RES / RST | D7 | |
| BLK | 3.3V | Arka ışık |
| VCC | 3.3V | |
| GND | GND | |

### UART → FluidNC ESP32-S3
| Nano ESP32 | ESP32-S3 | Yön |
|---|---|---|
| D9 (RX1) | GPIO38 (TX) | FluidNC → Pendant |
| D8 (TX1) | GPIO39 (RX) | Pendant → FluidNC |
| GND | GND | Ortak toprak |

> ⚠️ **TX → RX çapraz bağlanır!**

---

## ⚙️ FluidNC Yapılandırması

`config.yaml` dosyasına aşağıdaki bölümleri ekleyin:

```yaml
uart1:
  txd_pin: gpio.38
  rxd_pin: gpio.39
  baud: 115200
  mode: 8N1

uart_channel1:
  uart_num: 1
  report_interval_ms: 75
  message_level: Error
```

> `uart_channel1` sayesinde FluidNC pozisyon bilgisini **otomatik olarak** her 75ms'de gönderir — `?` sorgusu gerekmez.

---

## 💻 Yazılım Kurulumu

### Gerekli Kütüphaneler (Arduino Library Manager)

```
Adafruit ST7789
Adafruit GFX Library
```

### Yükleme

1. Arduino IDE'yi açın
2. **Board**: `Arduino Nano ESP32` seçin
3. `pendant_nano_esp32_v2.8.ino` dosyasını açın
4. Kütüphaneleri yükleyin
5. **Upload** edin

---

## 🖥️ Ekran Düzeni

```
┌─────────────────────────────────────────────────────┐
│ [IDLE]    CNC PENDANT    [HOMED]               X    │          
├──────────────────┼──────────────────────────────────┤
│ X  +125.000      │                                  │
│ Y   -45.500      │                                  │
│ Z    +0.000      │                                  │
├──────────────────┴──────────────────────────────────┤
│ F:500mm/dk   STEP:0.100mm   JF:1000                 │
└─────────────────────────────────────────────────────┘
```

---

## 🕹️ Kullanım

### Jog (Encoder)
Encoder'ı çevirerek seçili eksende seçili adım boyutunda hareket ettirin.

### Buton Fonksiyonları
| Buton | Kısa Basış |
|---|---|
| **HOME** | Tüm eksenleri home'la (`$H`) |
| **ZERO** | Aktif ekseni sıfırla (G92) |
| **EKSEN** | X → Y → Z → X döngüsü |
| **HIZ/STEP** | Adım boyutunu büyüt; turdan sonra hızı artır |

### Jog Adım Seçimi
`HIZ/STEP` butonuna her basışta adım büyür:
```
0.001 → 0.010 → 0.100 → 1.000 → 10.00 → (tekrar) 0.001
```
Son adımdan sonra jog hızı bir sonrakine geçer:
```
100 → 500 → 1000 → 3000 mm/dk
```

---

## 📁 Dosya Yapısı

```
CNC_Pendant/
├── pendant_nano_esp32_v2.6       # Ana pendant kodu (Nano ESP32)
├── 3D Boyutlu Tasarım Dosyası    # Stl
└── README.md                     # Bu dosya
```

---

## 🔌 Bağlantı Şeması

```
Arduino Nano ESP32 (Herhangi bir işlemci olur)              FluidNC ESP32-S3
─────────────────────────────────────────────────
D9  (RX1)  ←───────────────── GPIO38 (UART1 TX)
D8  (TX1)  ───────────────────→ GPIO39 (UART1 RX)
GND        ─────────────────── GND

ST7789 TFT          EC11 Encoder        Butonlar
───────────         ────────────        ────────
D13 ← SCK           D2 ← CLK           D4 ← HOME
D11 ← MOSI          D3 ← DT            D5 ← ZERO
D10 ← CS            D12 ← SW           A6 ← EKSEN
D6  ← DC            GND ← GND          A7 ← HIZ
D7  ← RST
3V3 ← BLK/VCC
GND ← GND
```

---

## 📜 Lisans

Bu proje açık kaynaklıdır. Kişisel ve eğitim amaçlı serbestçe kullanılabilir.

---

## 👤 Geliştirici

**VOLTveTORK**

İnstagram profili: https://www.instagram.com/voltvetork/
