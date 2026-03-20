# 🎛️ CNC Pendant for FluidNC

**Arduino Nano ESP32 tabanlı (farklı mikroişlemci kullanabilirsiniz), ST7789 TFT ekranlı, FluidNC uyumlu CNC kumanda paneli.**

> Tasarım & Geliştirme: **VOLTveTORK**
> Sürüm: **v3.01** — Menu System + Spindle Gauge

---

## 📸 Özellikler

- ✅ FluidNC ile doğrudan UART haberleşmesi (115200 baud)
- ✅ Gerçek zamanlı **Machine Position (MPos)** ve **Work Position (WPos)** gösterimi
- ✅ Rotary encoder ile hassas jog kontrolü (interrupt tabanlı, 60ms throttle)
- ✅ 4 buton: HOME / ZERO / EKSEN / HIZ-STEP
- ✅ Encoder kısa tıklama + uzun basma (500ms) desteği
- ✅ X, Y, Z eksen seçimi
- ✅ Durum göstergesi: IDLE / RUN / HOLD / ALARM / HOME / JOG / DOOR
- ✅ Homed durumu takibi
- ✅ Popup bildirimleri
- ✅ 10 saniye hareketsizlikte menüden otomatik çıkış

### 🆕 v3.01 — Menü Sistemi & Spindle Gauge

- ✅ **Encoder tıklama ile menü erişimi** (ana ekranda tıkla → menüye gir)
- ✅ **Spindle Kontrolü** — 270° analog gauge ile gerçek zamanlı RPM gösterimi
  - Encoder ile hedef hız ayarı (500 RPM adımlarla, 0–20000 RPM)
  - Renk kodlu ark: Yeşil (<7K) / Sarı (<14K) / Kırmızı (>14K)
  - Tıklama ile M3/M5 komutu gönderimi
- ✅ **Jog Hızı Seçimi** — 1000 / 2000 / 3000 mm/dk
- ✅ **Step Boyutu Seçimi** — 0.100 / 0.500 / 1.000 mm
- ✅ **Soğutma Kontrolü** — KAPALI (M9) / FLOOD (M8) / MIST (M7)
- ✅ **Uzun basma ile geri dönüş** (alt menülerden üst menüye)

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
| SW (Buton) | A0 |
| GND | GND |

> ⚠️ **Not:** D12 (GPIO47) encoder SW için sorunlu olabilir, bu yüzden A0 pini kullanılmaktadır.

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
3. `pendant.ino` dosyasını açın
4. Kütüphaneleri yükleyin
5. **Upload** edin

---

## 🖥️ Ekran Düzeni

### Ana Ekran
```
┌─────────────────────────────────────────────────────┐
│ [IDLE]    CNC PENDANT    [HOMED]               X   │
├──────────────────┬──────────────────────────────────┤
│   MACHINE POS    │         WORK POS                 │
├──────────────────┼──────────────────────────────────┤
│ X  +125.000      │  +25.000                         │
│ Y   -45.500      │   -5.500   ← aktif eksen (sarı)  │
│ Z    +0.000      │   +0.000                         │
├──────────────────┴──────────────────────────────────┤
│ F:500  SPD:0  STEP:0.100  JF:2000                   │
│ [HOME] [ZERO] [AXIS] [SPEED]  Tikla:Menu            │
└─────────────────────────────────────────────────────┘
```

### Menü Ekranı
```
┌─────────────────────────────────────────────────────┐
│              CNC MENU                               │
├─────────────────────────────────────────────────────┤
│  > Spindle Kontrolu                                 │
│    Jog Hizi                                         │
│    Step Boyutu                                      │
│    Sogutma                                          │
│    Geri Don                                         │
├─────────────────────────────────────────────────────┤
│  Encoder: Sec  |  Tikla: Gir                        │
└─────────────────────────────────────────────────────┘
```

### Spindle Gauge Ekranı
```
┌─────────────────────────────────────────────────────┐
│ SPINDLE KONTROLU                    [M3 AKTIF]      │
├─────────────────────────┬───────────────────────────┤
│                         │  HEDEF HIZ:               │
│    ╭───270° gauge───╮   │     12000                 │
│    │     12500       │   │      RPM                  │
│    │      RPM        │   │                           │
│    ╰────────────────╯   │   CALISIYOR               │
│                         │                           │
├─────────────────────────┴───────────────────────────┤
│  Cevir: Hiz Ayarla  |  Tikla: M3 Gonder            │
└─────────────────────────────────────────────────────┘
```

---

## 🕹️ Kullanım

### Ana Ekran Kontrolleri

| Kontrol | Fonksiyon |
|---|---|
| **Encoder çevirme** | Seçili eksende jog hareketi |
| **Encoder tıklama** | Menüye giriş |
| **HOME butonu** | Tüm eksenleri home'la (`$H`) |
| **ZERO butonu** | Aktif ekseni sıfırla (`G92`) |
| **EKSEN butonu** | X → Y → Z → X döngüsü |
| **HIZ/STEP butonu** | Adım boyutunu büyüt; turdan sonra hızı artır |

### Menü Navigasyonu

| Kontrol | Fonksiyon |
|---|---|
| **Encoder çevirme** | Menü öğeleri arasında gezinme |
| **Encoder kısa tıklama** | Seçili öğeye gir / onayla |
| **Encoder uzun basma (500ms)** | Üst menüye / ana ekrana geri dön |
| **Herhangi bir buton** | Alt ekranlardan ana ekrana dön |
| **10s hareketsizlik** | Otomatik ana ekrana dönüş |

### Menü Öğeleri

| Menü | Açıklama |
|---|---|
| **Spindle Kontrolü** | 270° gauge ile RPM gösterimi, encoder ile hedef hız ayarı (500 RPM adım), tıklama ile M3 S[hız] veya M5 gönderimi |
| **Jog Hızı** | 1000 / 2000 / 3000 mm/dk arasında seçim |
| **Step Boyutu** | 0.100 / 0.500 / 1.000 mm arasında seçim |
| **Soğutma** | KAPALI (M9) / FLOOD (M8) / MIST (M7) kontrolü |
| **Geri Dön** | Ana ekrana dönüş |

### Jog Parametreleri

**Adım Boyutları:**
```
0.100 → 0.500 → 1.000 mm
```

**Jog Hızları:**
```
1000 → 2000 → 3000 mm/dk
```

> HIZ/STEP butonuna her basışta adım büyür. Son adımdan 0.100'e döndüğünde jog hızı bir sonraki kademeye geçer.

---

## 📡 Durum Göstergeleri

| Durum | Renk | Açıklama |
|---|---|---|
| IDLE | 🟢 Yeşil | Makine boşta |
| RUN | 🔵 Cyan | Program çalışıyor |
| HOLD | 🟡 Sarı | Duraklatıldı |
| ALARM | 🔴 Kırmızı | Alarm durumu |
| HOME | 🟠 Turuncu | Homing işlemi |
| JOG | 🔵 Cyan | Jog hareketi |
| DOOR | 🔴 Kırmızı | Kapı açık |

---

## 📁 Dosya Yapısı

```
CNC_Pendant/
├── pendant.ino         # Ana pendant kodu (v3.01)
├── config.yaml         # FluidNC yapılandırma dosyası
└── README.md           # Bu dosya
```

---

## 🔌 Bağlantı Şeması

```
Arduino Nano ESP32              FluidNC ESP32-S3
─────────────────────────────────────────────────
D9  (RX1)  ←───────────────── GPIO38 (UART1 TX)
D8  (TX1)  ───────────────────→ GPIO39 (UART1 RX)
GND        ─────────────────── GND

ST7789 TFT          EC11 Encoder        Butonlar
───────────         ────────────        ────────
D13 ← SCK           D2 ← CLK           D4 ← HOME
D11 ← MOSI          D3 ← DT            D5 ← ZERO
D10 ← CS            A0 ← SW            A6 ← EKSEN
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

FluidNC projesi: [github.com/bdring/FluidNC](https://github.com/bdring/FluidNC)
