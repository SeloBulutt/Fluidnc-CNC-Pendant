# 🎛️ CNC Pendant for FluidNC

**Arduino Nano ESP32 tabanlı (farklı mikroişlemci kullanabilirsiniz), ST7789 TFT ekranlı, FluidNC uyumlu CNC kumanda paneli.**

> Tasarım & Geliştirme: **VOLTveTORK**
> Sürüm: **v6.0** — mDNS Otomatik IP Keşfi, TCP Donma Çözümü, UART Öncelikli İletişim, WiFi bağlantısı, Soft Reset ve Duraklama, Uyku Modu, Feed & Spindle Override, Sınırsız Menü Süresi

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
- ✅ **Alt menülerde sınırsız bekleme süresi** (Otomatik çıkış kaldırıldı)

### 🆕 v6.0 — mDNS Otomatik IP Keşfi, TCP Donma Çözümü, UART Öncelikli İletişim

#### 🔧 Çözülen Sorunlar

| Sorun | Açıklama | Çözüm |
|---|---|---|
| **TCP Bağlantı Donması** | `tcpClient.connect()` FluidNC'ye ulaşamadığında ~5 saniye boyunca sistemi tamamen donduruyordu. Bu her 5 saniyede bir tekrarlanarak "5sn dondur, 5sn çalış" döngüsü oluşturuyordu. | `connect()` çağrısına **1000ms timeout** parametresi eklendi. Sistem artık en fazla 1 saniye bloke olur. |
| **Manuel IP Girişi Zorluğu** | FluidNC'nin IP adresini kullanıcının encoder ile tek tek girmesi gerekiyordu. IP adresi değiştiğinde tekrar elle güncelleme yapılmalıydı. | **mDNS (ESPmDNS)** ile FluidNC'nin ağda yayınladığı `fluidnc.local` adresi otomatik keşfedilir. WiFi bağlantısı kurulduğunda IP otomatik bulunur. |
| **UART Aktifken Gereksiz TCP Denemeleri** | UART bağlantısı aktifken bile TCP reconnect denemeleri yapılıyor, bu her denemede sistemi kısa süreli donduruyordu. | UART aktifken TCP reconnect denemeleri **tamamen engellendi**. Sadece UART pasif olduğunda TCP bağlantısı denenir. |
| **Header'da Belirsiz Bağlantı Göstergesi** | Bağlantı durumu ikonunda UART mı yoksa WiFi mı kullanıldığı net anlaşılmıyordu. | Header'da **`[UART]`** (yeşil) ve **`[WiFi]`** (yeşil/sarı) olarak ayrı ayrı gösterilir. Aktif kanal net şekilde belirtilir. |
| **TCP Reconnect Sıklığı** | Başarısız TCP bağlantı denemelerinde her 5 saniyede bir yeniden deneme yapılıyordu, bu da sürekli donma hissi yaratıyordu. | **Üstel geri çekilme (exponential backoff)** eklendi: İlk 3 deneme → 5sn, sonraki 7 → 15sn, ardından → 30sn aralıklarla deneme yapılır. |

#### ✨ Yeni Özellikler

- ✅ **mDNS ile Otomatik IP Keşfi** — WiFi bağlantısı kurulduğunda FluidNC'nin ağdaki IP adresi **otomatik olarak keşfedilir** (`ESPmDNS` kütüphanesi). Manuel IP girişine gerek kalmaz. TCP bağlantısı 3 kez başarısız olursa mDNS ile IP tekrar aranır.
- ✅ **UART Öncelikli İletişim** — UART bağlantısı aktifse (500ms içinde veri geliyorsa) tüm komutlar UART üzerinden gönderilir. UART pasif olduğunda otomatik olarak TCP'ye geçilir. UART aktifken TCP reconnect denemeleri yapılmaz.
- ✅ **Header'da Akıllı Bağlantı Göstergesi** — `[UART]` yeşil (UART aktif) / `[WiFi]` yeşil (TCP bağlı) / `[WiFi]` sarı (WiFi bağlı ama TCP yok) / `[]` kırmızı (bağlantı yok)
- ✅ **Üstel Geri Çekilmeli TCP Reconnect** — Başarısız bağlantı denemelerinde aralık otomatik olarak artar (5s → 15s → 30s). Her 3 başarısız denemede mDNS ile IP tekrar keşfedilir.
- ✅ **WiFi Menüsünde "Oto IP Bul"** — Manuel IP giriş ekranı kaldırıldı, yerine **mDNS tabanlı otomatik IP keşfi** butonu eklendi. Tek tıklama ile FluidNC'nin IP'si ağda aranır ve bulunursa kaydedilir.

### v5.0 — WiFi, Soft Reset, Duraklama ve Override Kontrolü

- ✅ **Realtime Override Kontrolü** — Program çalışırken (RUN/HOLD durumunda) encoder çevrildiğinde anında FluidNC'ye (±%10) komut gönderimi. Encoder'a tıklama ile override değerini %100'e (reset) döndürme.
- ✅ **Override Kısayolu** — RUN/HOLD modunda EKSEN butonuna basarak doğrudan Feed/Spindle override ekranına erişim.
- ✅ **WiFi Bağlantısı** — ESP32 WiFi üzerinden FluidNC'ye TCP bağlantısı (Port 23)
  - Ağ tarama (RSSI sinyal gücü göstergesi ile)
  - Karakter tekerleği ile şifre girişi (maskeli gösterim)
  - ~~FluidNC IP adresi ayarı (oktet oktet düzenleme)~~ → **v6.0'da mDNS otomatik keşif ile değiştirildi**
  - NVS ile ayarların kalıcı hafızaya kaydı
  - Otomatik bağlantı (kayıtlı SSID varsa başlangıçta bağlanır)
- ✅ **UART/TCP Otomatik Geçiş** — UART aktifse UART, değilse TCP kullanılır (500ms timeout)
- ✅ **Footer'da FluidNC IP** adresi gösterimi (mDNS ile otomatik bulunan veya kayıtlı IP)
- ✅ **Duraklama / Devam** — SPEED butonu ile Feed Hold (`!`) ve Cycle Start (`~`)
- ✅ **Otomatik & Manuel Uyku Modu (Deep Sleep)** — 2 dakika işlem yapılmazsa veya HOME tuşuna uzun basıldığında cihaz 10µA tüketen uyku moduna geçer.
- ✅ **Batarya Görüntüleme** — Sağ üst köşede pil yüzdesini (`[🔋 78%]`) gösterir (A1 pini üzerinden voltaj okuma).
- ✅ **Arka Işık (BLK) Kontrolü** — Uyku modunda ekranın arkasındaki LED aydınlatmasını tamamen keser (A2 pini).
- ✅ **Alarm Kurtarma** — Alarm durumunda:
  - HOME butonu → Kilidi Aç (`$X`)
  - ZERO butonu → Soft Reset (`0x18`)
- ✅ **Bağlamsal Footer** — Makine durumuna göre değişen yardım metni
- ✅ **HOME butonu = Geri / Uyku** — Menü ve alt ekranlarda geri dönüş, ana ekranda 1.5 sn basılı tutulursa derin uyku.

### Menü Sistemi
- ✅ **Feed ve Spindle Override** — Program çalışırken menü dinamik olarak değişerek override ayar seçeneklerini gösterir.
- ✅ **Spindle Kontrolü** — 270° analog gauge, 500 RPM adımlı hedef hız, M3/M5 gönderimi
- ✅ **Jog Hızı Seçimi** — 1000 / 2000 / 3000 mm/dk
- ✅ **Step Boyutu Seçimi** — 0.100 / 0.500 / 1.000 mm
- ✅ **Soğutma Kontrolü** — KAPALI (M9) / FLOOD (M8) / MIST (M7)
- ✅ **WiFi Ayarları** — Ağ Tara / Şifre Gir / Oto IP Bul / Bağlan-Kes

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

### UART → FluidNC ESP32-S3 (Önemli Kablo Revizyonu)

Pendant ile CNC ESP32-S3 kontrol kartı arasındaki bağlantı kablosunda **5 adet kablo** kullanılmalıdır. Besleme kablosundaki parazitlerin sinyal bütünlüğünü bozmasını engellemek için sinyal ve güç topraklamaları (GND) ayrı kablolar üzerinden taşınmalıdır:

1. **TX** (Sinyal)
2. **RX** (Sinyal)
3. **Sinyal GND** (Sadece TX ve RX sinyalleri için topraklama)
4. **5V** (Güç Beslemesi)
5. **Güç GND** (Sadece 5V hattı için ayrı topraklama)

| Nano ESP32 | ESP32-S3 | Açıklama |
|---|---|---|
| D9 (RX1) | GPIO38 (TX) | Sinyal Hattı (FluidNC → Pendant) |
| D8 (TX1) | GPIO39 (RX) | Sinyal Hattı (Pendant → FluidNC) |
| GND | GND | Sinyal Topraklaması (TX ve RX için) |
| VIN / 5V | 5V | Güç Beslemesi |
| GND | GND | Güç Topraklaması (Ayrı hat üzerinden) |

> ⚠️ **DİKKAT:** TX → RX çapraz bağlanır! Sinyal GND ve Güç GND hatları cihaz pinlerinde GND olarak birleşebilir ancak bağlantı kablosu boyunca birbirine değmeyen iki ayrı tel olmalıdır.

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
1. **TP4056:** Uart kablosu takılıyken pili şarj eder. Sistem 2.5V altına düştüğünde çıkışı kesip pili korur (DW01A'lı versiyon).
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

> `WiFi.h`, `Preferences.h`, `ESPmDNS.h`, `esp_sleep.h` ve `SPI.h` kütüphaneleri ESP32 çekirdeği ile birlikte gelir.

### Yükleme

1. Arduino IDE'yi açın
2. **Board**: `Arduino Nano ESP32` seçin
3. `pendant.ino` dosyasını açın
4. Kütüphaneleri yükleyin
5. **Upload** edin

---

## 📡 Haberleşme Mimarisi

```
                   ┌─────────────────────┐
                   │   CNC Pendant       │
                   │  (Nano ESP32)       │
                   └──┬──────────┬───────┘
                      │          │
              UART    │          │  WiFi/TCP
           (115200)   │          │  (Port 23)
           ÖNCELİKLİ │          │  YEDEK
                      │          │
                   ┌──┴──────────┴───────┐
                   │     FluidNC         │
                   │    (ESP32-S3)       │
                   └─────────────────────┘
```

### İletişim Öncelik Sırası
1. **UART öncelikli:** UART'tan 500ms içinde veri geliyorsa komutlar **her zaman** UART üzerinden gönderilir
2. **Otomatik TCP geçişi:** UART pasifse ve WiFi/TCP bağlıysa komutlar TCP üzerinden gönderilir
3. **UART aktifken TCP reconnect engeli:** UART bağlantısı varken TCP yeniden bağlanma denemeleri yapılmaz (gereksiz donma engellenir)
4. Pendant her 250ms'de `?` sorgusu göndererek durum bilgisi alır (hem UART hem TCP)
5. TCP bağlantısı kesilirse **üstel geri çekilme** ile yeniden bağlanma denenir (5s → 15s → 30s)

### mDNS Otomatik IP Keşfi Akışı
```
WiFi Bağlantısı Kuruldu
        │
        ▼
  mDNS Başlat ("cncpendant")
        │
        ▼
  "http.tcp" Servisi Ara
        │
   ┌────┴────┐
   │ Bulundu │──► IP Kaydet ──► TCP Bağlan
   └─────────┘
   │ Bulunamadı│──► Kayıtlı IP ile Devam Et
   └──────────┘
        │
  (TCP 3x Başarısız)
        │
        ▼
  mDNS ile Tekrar Ara
```

---

## 🖥️ Ekran Düzeni

### Ana Ekran
```
┌─────────────────────────────────────────────────────┐
│ [IDLE]   CNC PENDANT  [HOMED]  [78%]  [UART]    X  │
├──────────────────┬──────────────────────────────────┤
│   MACHINE POS    │         WORK POS                 │
├──────────────────┼──────────────────────────────────┤
│ X  +125.000      │  +25.000                         │
│ Y   -45.500      │   -5.500   ← aktif eksen (sarı)  │
│ Z    +0.000      │   +0.000                         │
├──────────────────┴──────────────────────────────────┤
│ F:500  S:0  STEP:0.100  192.168.1.105               │
│ [HOME] [ZERO] [AXIS] [SPEED]  Tikla:Menu            │
└─────────────────────────────────────────────────────┘
```

### Bağlantı Durumu Göstergesi (Header)
| İkon | Renk | Anlamı |
|---|---|---|
| `[UART]` | 🟢 Yeşil | UART bağlantısı aktif (öncelikli kanal) |
| `[WiFi]` | 🟢 Yeşil | WiFi bağlı + TCP bağlı |
| `[WiFi]` | 🟡 Sarı | WiFi bağlı, TCP bağlantısı yok |
| `[]` | 🔴 Kırmızı | Bağlantı yok |

### Bağlamsal Footer Metinleri
| Durum | Footer |
|---|---|
| IDLE / HOME | `[HOME] [ZERO] [AXIS] [SPEED]  Tikla:Menu` |
| RUN / JOG | `[SPEED] Duraklat` |
| HOLD | `[SPEED] Devam Et` |
| ALARM | `[ZERO] Reset  [HOME] Kilit Ac` |

## 🗂️ Detaylı Menü Sistemi ve Ayarlar

Kumandanın ana ekranındayken **Encoder'a bir kez tıklayarak** ana menüye girebilirsiniz. Menüde gezinmek için encoder çevrilir, seçim yapmak için tıklanır. Menülerden geri dönmek için **HOME** butonuna basabilir veya **Encoder'a uzun basabilirsiniz**. Alt menülerde ve ayarlarda **süre sınırı yoktur**, siz manuel olarak çıkmadıkça ekran ana menüye atmaz.

Makinenin çalışma durumuna (Boşta veya Çalışıyor) göre menü sistemi **dinamik olarak değişir**.

**CNC Menü (IDLE - Boşta)**
```text
┌─────────────────────────────────────────────────────┐
│                      CNC MENU                       │
├─────────────────────────────────────────────────────┤
│   > Spindle Kontrolu                                │
│     Jog Hizi                                        │
│     Step Boyutu                                     │
│     Sogutma                                         │
│     WiFi Ayarlari                                   │
├─────────────────────────────────────────────────────┤
│              Encoder: Sec  |  Tikla: Gir            │
└─────────────────────────────────────────────────────┘
```

**CNC Menü (RUN/HOLD - Çalışırken)**
```text
┌─────────────────────────────────────────────────────┐
│                      CNC MENU                       │
├─────────────────────────────────────────────────────┤
│   > Spindle Override                                │
│     Feed Override                                   │
│     Jog Hizi                                        │
│     Step Boyutu                                     │
│     Sogutma                                         │
├─────────────────────────────────────────────────────┤
│              Encoder: Sec  |  Tikla: Gir            │
└─────────────────────────────────────────────────────┘
```

### 1. Spindle Kontrolü (Makine IDLE / Boşta iken)
Makine boşta iken menüde ilk sırada yer alır. Manuel Spindle kontrolü için kullanılır.

```text
┌─────────────────────────────────────────────────────┐
│ SPINDLE KONTROLU                       [M5 KAPALI]  │
├─────────────────────────────────────────────────────┤
│           . . . .                                   │
│        .    10K    .           HEDEF HIZ:           │
│      .               .                              │
│    5K                 15K         12000             │
│   0                     20K        RPM              │
│            12000                                    │
│             RPM                KAPALI               │
├─────────────────────────────────────────────────────┤
│            Cevir: Hiz Ayarla  |  Tikla: M3 Gonder   │
└─────────────────────────────────────────────────────┘
```

- **Ekran Görünümü:** Yukarıdaki gibi 270° derece dolan analog bir devir saati (gauge) ve sağ panelde Hedef Hız kutucuğu bulunur. 
- **Kullanım:** Encoder çevrildiğinde Hedef Hız 500 RPM'lik adımlarla artar veya azalır. İstenilen hıza gelindiğinde encoder'a tıklandığında `M3` komutuyla Spindle çalıştırılır. Spindle zaten çalışıyorsa, tıklamak `M5` komutuyla Spindle'ı durdurur.

### 2. Override Menüleri (Makine RUN / HOLD iken)
Makine bir G-Code programı işlerken ana menüdeki "Spindle Kontrolü" gizlenir; yerine anlık müdahale için Override (geçersiz kılma) menüleri açılır. 

```text
┌─────────────────────────────────────────────────────┐
│ SPINDLE OVERRIDE                        [M3 AKTIF]  │
├─────────────────────────────────────────────────────┤
│           . . . .                                   │
│        .           .                                │
│      .               .                              │
│     .                 .                             │
│    50%               200%                           │
│             120%                                    │
│           SPINDLE                                   │
├─────────────────────────────────────────────────────┤
│        Cevir: +-10% Aninda  |  Tikla: %100 Reset    │
└─────────────────────────────────────────────────────┘
```

> 💡 **Kısayol:** Program çalışırken ana ekranda **EKSEN (Axis) butonuna basarak** bu menüye doğrudan kısayoldan erişebilirsiniz. Aşağıdaki seçim ekranı açılır:

```text
┌─────────────────────────────────────────────────────┐
│                 OVERRIDE SECIMI                     │
├─────────────────────────────────────────────────────┤
│                                                     │
│                > Feed Override                      │
│                  Spindle Override                   │
│                                                     │
├─────────────────────────────────────────────────────┤
│             Encoder: Sec  |  Tikla: Onayla          │
└─────────────────────────────────────────────────────┘
```
- **Feed Override (İlerleme Hızı):** Çalışan makinenin eksenlerdeki ilerleme hızını anlık olarak %10'luk adımlarla (±%10) değiştirmenizi sağlar. %0 ile %200 arasında renkli (Kırmızı/Sarı/Yeşil) bir gösterge barı üzerinden anlık yüzdenizi görebilirsiniz. Encoder çevrildiğinde **anında FluidNC'ye komut gönderilir**. Encoder'a tıklamak değeri **%100'e sıfırlar (resetler)**.
- **Spindle Override (Devir Hızı):** Aynı şekilde, spindle motorunun devrini iş parçasının durumuna göre anlık (±%10 adımlarla) artırıp azaltmanızı sağlar. Encoder çevrildiğinde **anında FluidNC'ye komut gönderilir**. Encoder'a tıklamak değeri **%100'e sıfırlar (resetler)**.

### 3. Jog Hızı (Jog Speed)
Seçili eksende manuel olarak hareket ettirilirken makinenin ulaşacağı maksimum hızı belirler.
- **Seçenekler:** 1000, 2000, 3000 mm/dk.
- **Kullanım:** Ekranda liste şeklinde görünür. Encoder çevrilerek istenilen hızın üzerine gelinir (yanında `>` işareti belirir) ve tıklanarak aktif edilir. 

### 4. Step Boyutu (Step Size)
Encoder'ın her bir tık (çıt) dönüşünde makinenin eksende ne kadar ilerleyeceğini belirler.
- **Seçenekler:** 0.100 mm, 0.500 mm, 1.000 mm.
- **Kullanım:** Hassas yaklaşım veya hızlı hareket için uygun değer encoder ile seçilir. Seçilen değer ana ekranın alt kısmında (Footer) gösterilir.

### 5. Soğutma (Coolant)
Soğutma veya hava sistemlerini manuel olarak devreye almak için kullanılır.
- **Seçenekler:**
  - **M9:** Soğutma Kapalı
  - **M8:** Flood (Su/Sıvı Soğutma)
  - **M7:** Mist (Hava/Sis Soğutma)

### 6. WiFi Ayarları ve Kurulumu
FluidNC kontrol kartına kablosuz olarak (TCP Port 23 üzerinden) bağlanmak için kullanılan detaylı ağ menüsüdür. Girilen tüm değerler ESP32'nin **kalıcı hafızasına (NVS)** kaydedilir. Cihaz her açıldığında bu bilgilere göre otomatik bağlanmayı dener.

```text
┌─────────────────────────────────────────────────────┐
│ WIFI AYARLARI                              [ON/OFF] │
├─────────────────────────────────────────────────────┤
│   > Ag Tara                                         │
│     Sifre Gir                                       │
│     Oto IP Bul                                      │
│     Baglan / Kes                                    │
│     Geri Don                                        │
├─────────────────────────────────────────────────────┤
│ AG: MyNetwork                                       │
└─────────────────────────────────────────────────────┘
```

- **Ağ Tara (Scan):** Tıklandığında ortamdaki WiFi ağları taranır. Ağlar, sinyal güçlerine (RSSI / dBm) göre listelenir. Encoder ile bağlanmak istediğiniz ağın üzerine gelip tıklayarak SSID'yi seçmiş olursunuz.
- **Şifre Gir:** Ekrandaki karakter tekerleği (harfler, sayılar ve özel karakterler) üzerinden şifre girilir. Encoder çevrilerek karakter seçilir, tıklanarak eklenir. Şifre ekranda gizlenmiş maskeli yapıda gösterilir. 
  - **Silme (Backspace):** Hatalı girişte **Encoder'a uzun basılarak** son karakter silinir.
  - **Kaydetme:** Şifre girişini bitirdiğinizde **EKSEN butonuna** basarak şifreyi kaydedersiniz.
- **🆕 Oto IP Bul (mDNS):** WiFi bağlantısı kurulu iken bu seçeneğe tıklandığında **mDNS** ile ağdaki FluidNC cihazının IP adresi otomatik aranır. Bulunursa IP kaydedilir ve TCP bağlantısı başlatılır. WiFi bağlı değilse uyarı verir. *(v6.0'da eklendi, eski "IP Ayarla" menüsünün yerini aldı)*
- **Bağlan / Kes:** Ayarlar tamamlandığında bu seçeneğe tıklayarak TCP bağlantısını başlatabilirsiniz. Bağlantı kurulduğunda ana ekranda yeşil `[WiFi]` simgesi belirir. Bağlantı sırasında mDNS ile IP otomatik keşfedilir ve TCP bağlantısı kurulur.

---

## 🕹️ Buton Fonksiyonları ve Kontroller

### Ana Ekran Kontrolleri
| Kontrol | Fonksiyon |
|---|---|
| **Encoder Çevirme** | Seçili eksende jog hareketi yaptırır |
| **Encoder Tıklama** | Ana Menüye giriş yapar |
| **HOME Butonu (Kısa)** | Tüm eksenleri Home'a gönderir (`$H`). Alarm durumunda kilit açar (`$X`). |
| **HOME Butonu (Uzun 1.5s)**| Cihazın ekranını kapatıp **Derin Uykuya (Deep Sleep)** geçirir. |
| **ZERO Butonu** | O an aktif olan ekseni sıfırlar (`G92`). Alarm durumunda Soft Reset atar (`0x18`). |
| **EKSEN Butonu** | **IDLE (Boşta):** X → Y → Z eksenleri arasında sırayla geçiş yapar. <br> **RUN/HOLD:** Override Kısayol Menüsünü açar. |
| **HIZ/STEP Butonu** | **IDLE:** Step/Hız ayarını değiştirir. <br> **RUN:** Programı Duraklatır (`!`). <br> **HOLD:** Programı devam ettirir (`~`). |

### Menü İçi Navigasyon
| Kontrol | Fonksiyon |
|---|---|
| **Encoder Çevirme** | Menü öğeleri, şifre harfleri veya IP oktetleri arasında gezinme |
| **Encoder Tıklama** | Seçili öğeye gir / onayla / bir sonraki alana geç |
| **Encoder Uzun Basma** | Bir üst menüye geri dön / Şifre ekranında son karakteri sil |
| **HOME Butonu** | Hangi menüde olursanız olun geri dönmenizi sağlar |
| **EKSEN Butonu** | Yalnızca WiFi şifre ve IP ekranlarında işlemi onaylayıp kaydetmek için kullanılır |
| **Sınırsız Bekleme** | Menülerde otomatik çıkış yoktur, çıkmak için HOME veya Encoder'a uzun basana kadar kapanmaz. |

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
| `[UART]` | 🟢 Yeşil | UART aktif — kablolu bağlantı öncelikli |
| `[WiFi]` | 🟢 Yeşil | WiFi + TCP bağlı — kablosuz iletişim aktif |
| `[WiFi]` | 🟡 Sarı | WiFi bağlı, TCP bağlantısı yok |
| `[]` | 🔴 Kırmızı | Bağlantı yok |

---

## 📁 Dosya Yapısı

```
CNC_Pendant/
├── CNC_pendant_v5.0 wifi.ino # Ana pendant kodu (v6.0)
├── config.yaml               # FluidNC yapılandırma dosyası
└── README.md                 # Bu dosya
```

---

## 🔌 Bağlantı Şeması

```
Arduino Nano ESP32              FluidNC ESP32-S3
─────────────────────────────────────────────────
D9  (RX1)  ←────(Sinyal)───── GPIO38 (UART1 TX)
D8  (TX1)  ─────(Sinyal)─────→ GPIO39 (UART1 RX)
GND        ───(Sinyal GND)─── GND
VIN / 5V   ─────(Güç 5V)───── 5V
GND        ────(Güç GND)───── GND

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
