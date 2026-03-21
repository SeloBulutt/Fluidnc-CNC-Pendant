# 🎛️ CNC Pendant for FluidNC

**Arduino Nano ESP32 tabanlı (farklı mikroişlemci kullanabilirsiniz), ST7789 TFT ekranlı, FluidNC uyumlu CNC kumanda paneli.**

> Tasarım & Geliştirme: **VOLTveTORK**
> Sürüm: **v4.01** — WiFi bağlantısı, Soft Reset ve Duraklama

---

## 📸 Özellikler

### Temel Özellikler
- ✅ FluidNC ile **UART + WiFi/TCP** çift kanallı haberleşme
- ✅ Gerçek zamanlı **Machine Position (MPos)** ve **Work Position (WPos)** gösterimi
- ✅ Rotary encoder ile hassas jog kontrolü (interrupt tabanlı, 60ms throttle)
- ✅ 4 buton: HOME / ZERO / EKSEN / HIZ-STEP
- ✅ Encoder kısa tıklama + uzun basma (500ms) desteği
- ✅ X, Y, Z eksen seçimi
- ✅ Durum göstergesi: IDLE / RUN / HOLD / ALARM / HOME / JOG / DOOR
- ✅ Homed durumu takibi
- ✅ Popup bildirimleri
- ✅ 10 saniye hareketsizlikte menüden otomatik çıkış

### 🆕 v4.01 — WiFi, Soft Reset & Duraklama

- ✅ **WiFi Bağlantısı** — ESP32 WiFi üzerinden FluidNC'ye TCP bağlantısı (Port 23)
  - Ağ tarama (RSSI sinyal gücü göstergesi ile)
  - Karakter tekerleği ile şifre girişi (maskeli gösterim)
  - FluidNC IP adresi ayarı (oktet oktet düzenleme)
  - NVS ile ayarların kalıcı hafızaya kaydı
  - Otomatik bağlantı (kayıtlı SSID varsa başlangıçta bağlanır)
- ✅ **UART/TCP Otomatik Geçiş** — UART aktifse UART, değilse TCP kullanılır (500ms timeout)
- ✅ **Header'da bağlantı durumu** — `[TCP]` yeşil / `[Wifi]` sarı / `[]` kırmızı
- ✅ **Footer'da FluidNC IP** adresi gösterimi
- ✅ **Duraklama / Devam** — SPEED butonu ile Feed Hold (`!`) ve Cycle Start (`~`)
- ✅ **Otomatik & Manuel Uyku Modu (Deep Sleep)** — 2 dakika işlem yapılmazsa veya HOME tuşuna uzun basıldığında cihaz 10µA tüketen uyku moduna geçer.
- ✅ **Batarya Görüntüleme** — Sağ üst köşede pil yüzdesini (`[🔋 78%]`) gösterir (A1 pini üzerinden voltaj okuma).
- ✅ **Arka Işık (BLK) Kontrolü** — Uyku modunda ekranın arkasındaki LED aydınlatmasını tamamen keser (A2 pini).
- ✅ **Alarm Kurtarma** — Alarm durumunda:
  - HOME butonu → Kilidi Aç (`$X`)
  - ZERO butonu → Soft Reset (`0x18`)
- ✅ **Bağlamsal Footer** — Makine durumuna göre değişen yardım metni
- ✅ **HOME butonu = Geri / Uyku** — Menü ve alt ekranlarda geri dönüş, ana ekranda 1.5 sn basılı tutulursa derin uyku.

### Menü Sistemi (v3.01'den)
- ✅ **Spindle Kontrolü** — 270° analog gauge, 500 RPM adımlı hedef hız, M3/M5 gönderimi
- ✅ **Jog Hızı Seçimi** — 1000 / 2000 / 3000 mm/dk
- ✅ **Step Boyutu Seçimi** — 0.100 / 0.500 / 1.000 mm
- ✅ **Soğutma Kontrolü** — KAPALI (M9) / FLOOD (M8) / MIST (M7)
- ✅ **WiFi Ayarları** — Ağ Tara / Şifre Gir / IP Ayarla / Bağlan-Kes

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
| BLK | **A2** | Arka ışık (Uyku modu için A2'den kontrol edilir) |
| VCC | 3.3V | |
| GND | GND | |

### Pil Ölçümü (Güç Yönetimi)
| Sensör / Pin | Nano ESP32 | Yön |
|---|---|---|
| TP4056 B+ ucu | **A1** (GPIO2) | 100K+100K Voltaj bölücü üzerinden |

### UART → FluidNC ESP32-S3
| Nano ESP32 | ESP32-S3 | Yön |
|---|---|---|
| D9 (RX1) | GPIO38 (TX) | FluidNC → Pendant |
| D8 (TX1) | GPIO39 (RX) | Pendant → FluidNC |
| GND | GND | Ortak toprak |

> ⚠️ **TX → RX çapraz bağlanır!**

---

## 🔋 Güç Yönetimi ve Batarya (Opsiyonel)

Kablo bağımlılığından kurtulmak için Pendant'a bir Li-ion pil entegre edebilirsiniz. En kararlı ve ekran parlaklığını etkilemeyen 5V yükseltici (MT3608) dizilimi aşağıdaki gibidir:

```text
  [Li-ion Pil]
       │
      (B+/B-)
       ▼
  ┌────────────┐ (OUT+)  ┌──────────────┐ (OUT+ 5V)  ┌───────────────┐
  │ TP4056     ├────────►│ MT3608 Boost ├───────────►│ Arduino Nano  │ (VIN'e Girin)
  │ Şarj + Koru│ (OUT-)  │ Voltaj Yüks. │ (OUT- GND) │ ESP32 (VIN/GND)│
  └────────────┘         └──────────────┘            └───────────────┘
```
1. **TP4056:** USB takılıyken mili şarj eder. Sistem 2.5V altına düştüğünde çıkışı kesip pili korur (DW01A'lı versiyon).
2. **MT3608 Regülatör:** Pil voltajı ne kadar düşerse düşsün, her zaman anlık olarak 5.0V çıkışa (trimpot ile ayarlayın) yükseltir. Nano ESP32 bu temiz 5V'u kendi kaliteli 3.3V regülatöründen geçirip ekranı sorunsuz besler.

> **Uyku Modu Farkı:** Cihaz hiçbir haberleşme almazsa veya **HOME tuşuna 1.5 sn basılı tutulursa** her şey (ekran ışığı dahil) kapatılır. Bu modda pil ömrü aylarca dayanabilir.

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

> `WiFi.h`, `Preferences.h` ve `SPI.h` kütüphaneleri ESP32 çekirdeği ile birlikte gelir.

### Yükleme

1. Arduino IDE'yi açın
2. **Board**: `Arduino Nano ESP32` seçin
3. `pendant.ino` dosyasını açın
4. Kütüphaneleri yükleyin
5. **Upload** edin

---

## � Haberleşme Mimarisi

```
                   ┌─────────────────────┐
                   │   CNC Pendant       │
                   │  (Nano ESP32)       │
                   └──┬──────────┬───────┘
                      │          │
              UART    │          │  WiFi/TCP
           (115200)   │          │  (Port 23)
                      │          │
                   ┌──┴──────────┴───────┐
                   │     FluidNC         │
                   │    (ESP32-S3)       │
                   └─────────────────────┘
```

- **UART öncelikli:** UART'tan 500ms içinde veri geliyorsa komutlar UART üzerinden gönderilir
- **Otomatik TCP geçişi:** UART pasifse ve WiFi/TCP bağlıysa komutlar TCP üzerinden gönderilir
- Pendant her 250ms'de `?` sorgusu göndererek durum bilgisi alır (hem UART hem TCP)
- TCP bağlantısı kesilirse 5 saniyede bir yeniden bağlanma denenir

---

## 🖥️ Ekran Düzeni

### Ana Ekran
```
┌─────────────────────────────────────────────────────┐
│ [IDLE]   CNC PENDANT  [HOMED]  [TCP]  [🔋 %78]  X  │
├──────────────────┬──────────────────────────────────┤
│   MACHINE POS    │         WORK POS                 │
├──────────────────┼──────────────────────────────────┤
│ X  +125.000      │  +25.000                         │
│ Y   -45.500      │   -5.500   ← aktif eksen (sarı)  │
│ Z    +0.000      │   +0.000                         │
├──────────────────┴──────────────────────────────────┤
│ F:500  SPD:0  STEP:0.100  FNC:192.168.1.105         │
│ [HOME] [ZERO] [AXIS] [SPEED]  Tikla:Menu            │
└─────────────────────────────────────────────────────┘
```

### Bağlamsal Footer Metinleri
| Durum | Footer |
|---|---|
| IDLE / HOME | `[HOME] [ZERO] [AXIS] [SPEED]  Tikla:Menu` |
| RUN / JOG | `[SPEED] Duraklat` |
| HOLD | `[SPEED] Devam Et` |
| ALARM | `[ZERO] Reset  [HOME] Kilit Ac` |

### Menü Ekranları
- **CNC Menü** → Spindle Kontrolü, Jog Hızı, Step Boyutu, Soğutma, WiFi Ayarları
- **Spindle Gauge** → 270° analog gauge + hedef hız paneli
- **WiFi Ayarları** → Ağ Tara / Şifre Gir / IP Ayarla / Bağlan-Kes
- **WiFi Ağ Tarama** → SSID listesi + sinyal gücü (dBm + ikon)
- **WiFi Şifre Girişi** → Karakter tekerleği, maskeli gösterim, backspace
- **WiFi IP Ayarı** → 4 oktet ayrı ayrı düzenleme

---

## 🕹️ Kullanım

### Ana Ekran Kontrolleri

| Kontrol | Fonksiyon |
|---|---|
| **Encoder çevirme** | Seçili eksende jog hareketi |
| **Encoder tıklama** | Menüye giriş |
| **HOME Butonu (Kısa)** | Tüm eksenleri home'la (`$H`) / Alarm'da kilit aç (`$X`) |
| **HOME Butonu (Uzun 1.5s)** | Cihazı derin uykuya al (Kapat) |
| **ZERO butonu** | Aktif ekseni sıfırla (`G92`) / Alarm'da soft reset (`0x18`) |
| **EKSEN butonu** | X → Y → Z → X döngüsü |
| **HIZ/STEP butonu** | Adım/hız ayarı / RUN'da duraklat (`!`) / HOLD'da devam (`~`) |

### Menü Navigasyonu

| Kontrol | Fonksiyon |
|---|---|
| **Encoder çevirme** | Menü öğeleri arasında gezinme |
| **Encoder kısa tıklama** | Seçili öğeye gir / onayla |
| **Encoder uzun basma (500ms)** | Üst menüye geri dön / Şifre ekranında karakter sil |
| **HOME butonu** | Geri dön (tüm menü ve alt ekranlarda) |
| **AXIS butonu** | WiFi şifre/IP ekranlarında onayla ve kaydet |
| **10s hareketsizlik** | Otomatik ana ekrana dönüş |

### WiFi Kurulum Adımları

1. Menü → **WiFi Ayarları** → **Ağ Tara** ile ağları listeleyin
2. Encoder ile ağ seçin, tıklayarak onaylayın
3. **Şifre Gir** ekranında encoder ile karakter seçip tıklayarak ekleyin
   - Uzun basma = son karakteri sil, AXIS butonu = kaydet
4. **IP Adresi Ayarla** ile FluidNC IP adresini girin
   - Encoder ile değer değiştir, tıkla ile sonraki oktet, AXIS = kaydet
5. **Bağlan / Kes** ile bağlantıyı başlatın

> Ayarlar NVS'ye kaydedilir, bir sonraki açılışta otomatik bağlanır.

---

## 📡 Durum Göstergeleri

### Makine Durumu
| Durum | Renk | Açıklama |
|---|---|---|
| IDLE | 🟢 Yeşil | Makine boşta |
| RUN | 🔵 Cyan | Program çalışıyor |
| HOLD | 🟡 Sarı | Duraklatıldı |
| ALARM | 🔴 Kırmızı | Alarm durumu |
| HOME | 🟠 Turuncu | Homing işlemi |
| JOG | 🔵 Cyan | Jog hareketi |
| DOOR | 🔴 Kırmızı | Kapı açık |

### Bağlantı Durumu (Header)
| İkon | Renk | Anlamı |
|---|---|---|
| `[TCP]` | 🟢 Yeşil | WiFi + TCP bağlı |
| `[Wifi]` | 🟡 Sarı | WiFi bağlı, TCP bağlantısı yok |
| `[]` | 🔴 Kırmızı | Bağlantı yok |

---

## 📁 Dosya Yapısı

```
CNC_Pendant/
├── pendant.ino         # Ana pendant kodu (v4.01)
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

        WiFi/TCP (Port 23)
        ←────────────────→     (kablosuz alternatif)

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
