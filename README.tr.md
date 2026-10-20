# Bambu OLED Info

[![Lisans: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE) ![Platform: ESP32-C3](https://img.shields.io/badge/platform-ESP32--C3-red) ![Build: PlatformIO](https://img.shields.io/badge/build-PlatformIO-orange)

**Bambu Lab yazıcılarınızın** ne yaptığını tek bakışta gösteren küçük bir masa üstü cihaz: ilerleme çubuğu, kalan süre, geçen süre, bitiş saati, sıcaklıklar, katman, dosya adı, hız modu, filament ve yazıcının o anda ne yaptığı. Bir **ESP32-C3**, bir **0.91" OLED ekran** ve bir **döner düğmeden** oluşur.

- Wi-Fi'nizdeki yazıcılar **doğrudan yerel ağınızdan** okunur. Sunucu, telefon uygulaması ya da bulut hesabı gerekmez.
- Başka bir yerdeki yazıcılar (ikinci ev, atölye) aynı cihazla **Bambu hesabınız üzerinden** okunabilir.
- Yalnızca durumu **okur**. Baskıyı başlatmaz, duraklatmaz, durdurmaz.

*[English version / İngilizce sürüm](README.md)*

![Bağlantı şeması](docs/wiring.svg)

---

## İçindekiler

1. [Durum](#durum)
2. [Özellikler](#özellikler)
3. [Ekranlar ve düğme](#ekranlar-ve-düğme)
4. [Gerekenler](#gerekenler)
5. [Bağlantı](#bağlantı)
6. [Kurulum, adım adım](#kurulum-adım-adım)
7. [Wi-Fi'nizde olmayan yazıcılar](#wi-finizde-olmayan-yazıcılar)
8. [Ayar başvurusu](#ayar-başvurusu)
9. [Nasıl çalışır](#nasıl-çalışır)
10. [Proje yapısı](#proje-yapısı)
11. [Yardımcı araçlar](#yardımcı-araçlar)
12. [Güvenlik ve gizlilik](#güvenlik-ve-gizlilik)
13. [Sorun giderme](#sorun-giderme)
14. [Sınırlar ve fikirler](#sınırlar-ve-fikirler)
15. [Katkı](#katkı)
16. [Teşekkür ve lisans](#teşekkür-ve-lisans)

---

## Durum

| | |
|---|---|
| **Donanımda doğrulandı** | CH343 USB-UART'lı ESP32-C3 kart (iki kart), SSD1306 128x32 OLED, KY-040 düğme. Yerel ağda, normal bulut modunda, **LAN Only Mode olmadan** 3 adet **A1 mini** (firmware 01.08.01.00). Başka evdeki 3 yazıcı (sahibine göre bir A1, bir X2D ve bir A2L) Bambu Cloud üzerinden. |
| **Doğrulanmadı** | P1 / X1 / H2 serisi ve diğer tüm modeller. Hata durumları (gerçek bir baskı hatası) yalnızca dahili demo ile denendi, gerçek bir hatalı yazıcıyla değil. |

Durum protokolü aynı olduğu için diğer modeller prensipte çalışmalı ama deneyemedim. Raporlarınız çok değerli, bkz. [Katkı](#katkı).

## Özellikler

- **Altı yazıcı tek 128x32 ekranda**: tek tek (her biri 10 sn) ya da hepsi birden (genel bakış).
- Ekrandaki yazıcı için **canlı güncelleme**; diğerlerinin son bilinen durumu hatırlanır.
- **Kısmi** durum gönderen yazıcılarla (A1 ailesi) ve tam durum gönderenlerle çalışır.
- **Bitiş saati** ("17:42'de biter"): yazıcının kalan süresi ve NTP ile hesaplanır.
- Başka ağdaki yazıcılar için doğrulanmış TLS bağlantılı **bulut modu**.
- Hata başlığı **yanıp söner**, biten baskının durum etiketi de; boştaki bir yazıcı grubu ekranı **kapatır** (OLED yanması için) ve bir şey olunca uyanır.
- Türkçe ya da İngilizce etiketler (`UI_LANG_TR`), ayarlanabilir pinler, süreler ve saat dilimi.
- Hiç yazıcı olmadan tüm ekranları görmek için sahte yazıcılı **demo yazılımı**.
- Yazıcıları bulan ve erişim kodlarını okuyan iki küçük Python aracı.

## Ekranlar ve düğme

Tek **tıklama** dört ekranı sırayla gezdirir: **İlerleme → Genel bakış → Detay → Bilgi → İlerleme ...**

| Kontrol | Ne yapar |
|---|---|
| Hiçbir şey | **Otomatik mod**: her yazıcı 10 sn gösterilir (`CAROUSEL_MS`), sonra sıradaki |
| Düğmeyi çevirmek | Yazıcı seçer; otomatik mod durur, dokunmadan 45 sn sonra devam eder (`MANUAL_TIMEOUT_MS`) |
| Tıklama | Sonraki ekran (yukarıdaki sıra) |
| Uzun basış (0,8 sn) | Otomatik mod **açık / kapalı**; mesaj 3 sn ekranda kalır |
| Ekran kapalıyken dokunmak | Ekranı uyandırır (dokunuş başka bir şey yapmaz) |

Aşağıdaki çizimler yerleşim taslağıdır, fotoğraf değildir. Gerçek ekran 128x32 pikseldir, yaklaşık 21 karakter genişliğinde.

**İlerleme**: ana ekran.

```
Garaj A1                 RUN
[=========-----------]   42%
Kalan 3s26d Geçen 2s29d
```

Üstte ad ve durum etiketi, yüzdeli ilerleme çubuğu, altında kalan ve geçen süre. Yazıcı baskıya hazırlanırken son satırda ne yaptığı görünür (*Heating bed*, *Bed leveling*, *Cleaning nozzle*...). Biten baskıda ne kadar sürdüğü, boştaki yazıcıda sıcaklıkları görünür.

**Genel bakış**: tüm yazıcılar (ilk altısı) 3x2'lik bir tabloda.

```
MINI1 42%  MINI2 63%  MINI3 53%
[====---]  [=====--]  [====---]
A1    81%  X2D   done A2L   38%
[======-]  [=======]  [===----]
```

Her hücrede kısa ad (`shortName`, en çok 5 karakter), yüzde ya da durum sözcüğü (`idle`, `done`, `ERR`, `off`, `AUTH`) ve küçük bir çubuk bulunur. Seçili yazıcının hücresi ters renkli, hata veren hücre yanıp söner. Otomatik modda vurgu, cihazın konuştuğu yazıcıyı izler; düğmeyi çevirerek seçersiniz, tıklayınca o yazıcının sonraki ekranına geçilir.

**Detay**

```
Garaj A1            L118/290
N220/220 B65/65 C31
Bitis 17:42           -46dBm
altbaski.stl + altbaski.stl +
```

Katman, nozul / tabla / hazne sıcaklığı (hedef eğik çizgiden sonra; hazne yalnızca yazıcı bildiriyorsa), baskının biteceği saat (`+1` yarın demektir), Wi-Fi sinyali ve uzunsa kayan dosya adı.

**Bilgi**

```
Garaj A1                 RUN
Hiz Standard 100%
Fan 66% 0.4mm PLA
Hata yok
```

Hız modu ve yüzdesi, ana fan, nozul çapı, kullanılan filament (AMS yuvasından ya da harici makaradan) ve son satırda: hata kodu, yazıcının o anki aşaması ya da etkin HMS uyarılarının sayısı.

**İş bitince.** Bir yazıcının işi bittiğinde `DONE` etiketi İlerleme ekranında sağ üst köşede, genel bakışta ise hücresi yanıp söner; yazıcı yeni bir işe başlayana kadar sürer. Hata ise başlığın tamamını yanıp söndürür.

**Son bilinen veri.** Bir yazıcı yanıt vermeyi bırakırsa (`OFF`) ya da bilgileri reddedilirse (`AUTH`) son bilinen değerler ekranda kalır: etiket neyin yanlış olduğunu gösterir, ilerleme çubukları noktalı olur. Tam ekran mesajı yalnızca hiç veri vermemiş bir yazıcı için çıkar.

**Açılış.** İlk gördüğünüz *Yazicilar okunuyor 3/6* ekranıdır: cihaz karusel başlamadan önce her yazıcıyı bir kez okur, böylece hiçbir ekranda "veri bekleniyor" yazması gerekmez.

**Durum etiketleri**: `RUN` basıyor, `PAUSE`, `PREP` hazırlanıyor, `DONE`, `IDLE`, `ERR` hata ya da hata kodu var, `OFF` erişilemiyor, `AUTH` erişim kodu ya da bulut anahtarı reddedildi, `...` henüz veri yok.

## Gerekenler

| Parça | Notlar |
|---|---|
| **ESP32-C3** geliştirme kartı | CH343 USB-UART köprülü kartta denendi. Doğal USB'li kartlar (örn. SuperMini) küçük bir değişiklikle çalışır, bkz. [Ayar başvurusu](#ayar-başvurusu). |
| **0.91" OLED, 128x32, SSD1306, I2C** | Dört pin: GND, VCC, SCK (= SCL), SDA. Adres `0x3C` (`0x3D` de bulunur). |
| **KY-040 döner düğme** | İsteğe bağlı, ama yazıcı seçmenin yolu bu. |
| Jumper kablo, USB kablosu | |
| [PlatformIO](https://platformio.org/) | CLI ya da VS Code eklentisi. |
| Python 3 | Yalnızca yardımcı araçlar için; standart kütüphaneden başka bir şey gerekmez. |

## Bağlantı

| OLED | ESP32-C3 | | KY-040 | ESP32-C3 |
|---|---|---|---|---|
| GND | GND | | CLK | GPIO6 |
| VCC | 3V3 | | DT | GPIO7 |
| SCK (SCL) | GPIO5 | | SW | GPIO10 |
| SDA | GPIO4 | | + | 3V3 (**5V değil**) |
| | | | GND | GND |

- Modüllerin üzerindeki **pin etiketlerine** bakın, sırasına değil; sıra üreticiye göre değişir.
- KY-040 `+` pinini 3V3'e bağlayın: modülde bu pine giden pull-up dirençleri var, sinyaller böylece 3,3 V'ta kalır.
- Pinler [`include/settings.h`](include/settings.h) içinde değiştirilebilir. GPIO 2, 8, 9 (açılış pinleri), 11-17 (flash), 18-19 (USB) ve 20-21'den (UART) kaçının.
- Ek parça gerekmez: KY-040 ve çoğu OLED modülünde zaten pull-up dirençleri var, yazılım ayrıca düğme pinlerinde dahili pull-up'ları açar.

## Kurulum, adım adım

### 1. Kodu alın

```bash
git clone https://github.com/Asimmimx/bambu-oled-info
cd bambu-oled-info
copy include\secrets.example.h include\secrets.h        # macOS / Linux: cp include/secrets.example.h include/secrets.h
```

`include/secrets.h` git'e girmez: Wi-Fi şifrenizi ve yazıcıların erişim kodlarını tutar. **Asla commit etmeyin.**

### 2. Önce ekranları görün (isteğe bağlı)

```bash
pio run -e demo -t upload
```

Demo sürümü Wi-Fi ve yazıcı gerektirmez; sahte yazıcıları basıyor, boşta, bitmiş, hatalı ve çevrimdışı durumlardan geçirir.

### 3. Yazıcılarınızı bulun

Yazıcılarla aynı ağdaki bir bilgisayarda çalıştırın:

```bash
python tools/find_printers.py
```

```
Name               Model      Serial            IP              Mode     Firmware
Garage A1          A1 mini    0300DA000000000   192.168.1.148   cloud    01.08.01.00
```

Bambu yazıcıların yaydığı SSDP duyurularını dinler, hesap ya da şifre gerekmez. Her yazıcının adını, **seri numarasını** ve **IP'sini** not alın. IP'nin değişmemesi için yazıcılara sabit adres verin (yönlendiricide DHCP rezervasyonu).

### 4. `include/secrets.h` dosyasını düzenleyin

```cpp
#define WIFI_SSID     "wifi-adiniz"
#define WIFI_PASSWORD "wifi-sifreniz"

static const PrinterConfig PRINTERS[] = {
    {"Garaj A1",  "GARAJ",  "192.168.1.148", "0300DA000000000", "12345678"},
    //  ad        kısa ad   LAN adresi       seri no            erişim kodu
};
```

Ad üst satırda görünür (yaklaşık 14 karakter sığar); `shortName` (en çok 5) genel bakışta kullanılır. Düz harf kullanın: ekran yazı tipinde `ş ı ğ İ` yok.

### 5. Erişim kodlarını alın

Her yazıcının 8 karakterlik bir LAN erişim kodu vardır; yerel MQTT servisinin şifresidir. Almanın üç yolu var:

**a) Yazıcıdan** (resmi). Yazıcının ayarlarında **LAN Only Mode**'u açın (A1 ailesinde ayarlar menüsünün son sayfasında); kod anahtarın hemen altında görünür. Bedeli: LAN Only Mode yazıcıyı Bambu Cloud'dan ayırır, o yazıcı için uzaktan izleme ve buluttan baskı çalışmaz.

**b) Bambu hesabınızdan, e-posta ve şifreyle.** Yazıcılar normal bulut modunda kalır:

```bash
python tools/fetch_access_codes.py
```

E-posta ve şifrenizi sorar (yazarken görünmez) ve Bambu isterse e-postanıza gelen doğrulama kodunu da sorar. Hesabınızdaki yazıcıları listeler ve her erişim kodunu seri numarasına göre `secrets.h` dosyasına yazar.

**c) Bambu hesabınızdan, MakerWorld oturumuyla.** [makerworld.com](https://makerworld.com)'da (aynı hesap) zaten giriş yaptıysanız ve e-posta koduyla uğraşmak istemiyorsanız:

1. makerworld.com'u giriş yapılmış hâlde tarayıcıda açın.
2. Geliştirici araçlarını açın (`F12`) → **Application** (Chrome / Edge) ya da **Storage** (Firefox) → **Cookies** → `https://makerworld.com`.
3. Adı `token` olan çerezin **değerini** kopyalayın.
4. `python tools/fetch_access_codes.py --token` komutunu çalıştırıp istemde yapıştırın (ekranda görünmez).

Anahtar yaklaşık üç ay boyunca şifreniz kadar güçlüdür. Kimseyle paylaşmayın, başka bir yere yapıştırmayın. Betik onu bir kez kullanır ve saklamaz. (b) ve (c) Bambu'nun **resmi olmayan** bulut API'sini kullanır, bkz. [Güvenlik ve gizlilik](#güvenlik-ve-gizlilik).

### 6. Derleyip yükleyin

```bash
pio run -e esp32c3 -t upload
pio device monitor            # isteğe bağlı: günlüğü izleyin
```

Windows'ta bunu **PowerShell ya da cmd'den, Git Bash'ten değil** çalıştırın; platformun araç kurucusu MSys'i reddeder. İlk derleme birkaç yüz MB araç zinciri indirir.

### 7. Ne görmelisiniz

Açılışta ekranda *WiFi baglaniyor*, sonra ilk yazıcı görünür. Seri günlük (115200 baud) şöyle olur:

```
[oled] found at 0x3C (SDA=GPIO4 SCL=GPIO5)
[wifi] up, ip 192.168.1.167, rssi -47
[Garaj A1] connected in 1287 ms, free heap 105424 (largest block 77812)
[Garaj A1] pushall sent
[Garaj A1] phase=3 42% left=206min elapsed=149min layer=118/290 nozzle=220/220 ...
```

## Wi-Fi'nizde olmayan yazıcılar

Cihaz yalnızca bağlı olduğu Wi-Fi'yi görür. Başka yerdeki yazıcıları dahil etmenin iki yolu var:

**Bambu Cloud üzerinden (her şey için tek cihaz).** `secrets.h` içinde `ip` değeri `""` olan yazıcı, Bambu hesabınız üzerinden okunur. Tek komut bunu hazırlar: bulut girişinizi saklar ve hesabınızdaki, `secrets.h` içinde olmayan tüm yazıcıları ekler:

```bash
python tools/fetch_access_codes.py --token --cloud       # --token olmadan: e-posta ve şifreyle giriş
```

- Tüm bulut yazıcıları **tek** bağlantıyı paylaşır, ekranda aralarında geçiş anlıktır.
- Anahtar gönderilmeden önce, saat NTP ile ayarlandıktan sonra, sunucu sertifikası dahili CA paketiyle **doğrulanır**.
- Anahtar yazılıma gömülür ve yaklaşık **üç ay** geçerlidir. Bir bulut yazıcısında `AUTH` görünce komutu yeniden çalıştırıp kartı yeniden yükleyin.
- Yalnızca global (Çin dışı) hesaplar. Sunucu `BAMBU_CLOUD_HOST`, varsayılan `us.mqtt.bambulab.com`.
- Uzun adlar (yaklaşık 14 karakterden fazla) ekranda kısalır; `secrets.h` içinde yeniden adlandırın.

**İkinci bir cihaz.** O yazıcıların yanına kendi `secrets.h` dosyasıyla başka bir cihaz kurun. Onlarla yerel konuşur, buluta hiç ihtiyaç duymaz.

## Ayar başvurusu

### `include/settings.h`

| Ayar | Varsayılan | Anlamı |
|---|---|---|
| `PIN_OLED_SDA`, `PIN_OLED_SCL` | 4, 5 | Ekranın I2C pinleri |
| `PIN_ENC_CLK`, `PIN_ENC_DT`, `PIN_ENC_SW` | 6, 7, 10 | Düğme pinleri |
| `ENC_REVERSE` | 0 | Saat yönünde çevirmek geri gidiyorsa 1 yapın |
| `ENC_EDGES_PER_DETENT` | 4 | Düğmenin bir tıkı bir yazıcı atlıyorsa 2 deneyin |
| `UI_LANG_TR` | 1 | 1 = Türkçe etiketler, 0 = İngilizce |
| `TIMEZONE` | `<+03>-3` | Bitiş saati için POSIX saat dilimi, örn. `CET-1CEST,M3.5.0,M10.5.0/3` |
| `CAROUSEL_MS` | 10000 | Otomatik modda her yazıcının ekranda kalma süresi |
| `MANUAL_TIMEOUT_MS` | 45000 | Düğmeye dokunulmazsa otomatik mod bu süre sonra devam eder |
| `SCREEN_SLEEP_MS` | 300000 | Hiçbir şey basılmıyorsa bu süre sonra ekran kapanır; 0 = hiç |
| `AUTO_LISTEN_MS` | 3500 | Genel bakış modu: her yazıcıyı ne kadar dinlemeli |
| `FIRST_REPORT_WAIT_MS` | 7000 | Bağlanıp sessiz kalan yazıcıdan vazgeçme süresi |
| `CONNECT_TIMEOUT_S`, `HANDSHAKE_TIMEOUT_S` | 3, 8 | TCP ve TLS zaman aşımları |
| `OFFLINE_RETRY_MS` | 60000 | Erişilemeyen yazıcı ya da reddedilen bilgi için yeniden deneme aralığı |
| `FOCUS_RETRY_MS` | 8000 | O yazıcı ekrandayken yeniden deneme aralığı |
| `FOCUS_SETTLE_MS` | 450 | Bağlanmadan önce düğmenin durmasını bekleme |
| `PUSHALL_MIN_INTERVAL_MS` | 330000 | Yazıcı başına iki tam durum isteği arasındaki en kısa süre (5,5 dk) |
| `WARMUP_CLOUD_WAIT_MS` | 8000 | Açılış taraması: bulut yazıcılarının rapor vermesi için bekleme süresi |
| `HEARTBEAT_MS` | 10000 | Seri günlükteki durum satırının aralığı |
| `MQTT_BUFFER_BYTES` | 24576 | Kabul edilen en büyük MQTT mesajı; büyükler sessizce atılır |

### `include/secrets.h`

| Girdi | Anlamı |
|---|---|
| `WIFI_SSID`, `WIFI_PASSWORD` | Wi-Fi'niz (2,4 GHz) |
| `PRINTERS[]` | Her yazıcı için bir girdi: `{ad, kısaAd, ip, seriNo, erişimKodu}`. `ip` = `""` "Bambu Cloud üzerinden" demektir. En çok 8 (`src/Config.h` içindeki `MAX_PRINTERS`); genel bakış ilk 6'yı gösterir. |
| `BAMBU_CLOUD_USER`, `BAMBU_CLOUD_TOKEN`, `BAMBU_CLOUD_HOST` | Yalnızca bulut yazıcıları için; `fetch_access_codes.py --cloud` yazar |

### `platformio.ini`

| Ortam | Kullanım |
|---|---|
| `esp32c3` | Gerçek yazılım |
| `demo` | Aynı arayüz, sahte yazıcılar, Wi-Fi yok |

**Doğal USB'li** bir kart için (ayrı USB-UART çipi yok) `build_flags` içindeki iki `-DARDUINO_USB_...` satırını silin. Başka kartlar için `board` değerini değiştirin. Platform [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino çekirdeği 3), flash 40 MHz DIO'ya zorlanır (bkz. [Sorun giderme](#sorun-giderme)).

## Nasıl çalışır

### Genel resim

```
 LAN'daki yazıcılar ──┐                      ┌── Düğme ─────► input ─┐
   (MQTT/TLS :8883)   ├─► poller görevi ─► state ◄── ui (loop) ◄──────┘
 Bambu Cloud ─────────┘   (tek oturum)    (paylaşılan, kilitli)   │
   (MQTT/TLS :8883)                                              └──► OLED
```

İki görev, her yazıcının son bilinen durumunu tutan küçük, kilitli bir tabloyu paylaşır:

- **Poller** (`src/Poller.cpp`) ağı yönetir. Aynı anda tek bir MQTT sunucusuyla konuşur ve öğrendiklerini tabloya yazar.
- **Arayüz** (`src/Ui.cpp`, Arduino `loop` içinde) tabloyu okur, ekranları çizer ve düğmeyi işler. Düğmenin kendisi bir kesmede çözülür (`src/Input.cpp`).

### Neden aynı anda tek bağlantı

Bir ESP32-C3'ün yaklaşık 300 KB RAM'i vardır. Bir TLS oturumu onlarca KB tutar, altı oturum sığmaz. Bunun yerine poller **tek** oturum tutar ve onu baktığınız yere taşır: otomatik modda ve gezinirken ekrandaki yazıcıya bağlı kalır (canlı güncelleme); geçiş yaklaşık 1,3 sn sürer, bu sürede son bilinen değerler gösterilir. Genel bakışta tek bir yazıcı olmadığından poller onları sırayla ziyaret eder. Tüm bulut yazıcılarına tek bir bulut oturumu hizmet eder.

### Durum protokolü

Yazıcı 8883 portunda (TLS) bir MQTT sunucusu çalıştırır. Cihaz `bblp` kullanıcısı ve yazıcının **erişim koduyla** giriş yapar, `device/<seriNo>/report` konusuna abone olur ve `device/<seriNo>/request` konusuna `{"pushing":{"command":"pushall"}}` yayınlayarak bir kez tam durum ister. Bulut yazıcılarında sunucu Bambu'nundur, kullanıcı `u_<hesap numaranız>`, şifre anahtarınızdır; konular aynıdır. Protokol topluluk tarafından [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)'da belgelenmiştir.

Kullanılan alanlar (hepsi `print` altında): `gcode_state`, `mc_percent`, `mc_remaining_time`, `layer_num`, `total_layer_num`, `nozzle_temper`, `nozzle_target_temper`, `bed_temper`, `bed_target_temper`, `chamber_temper`, `print_error`, `hms`, `wifi_signal`, `subtask_name` / `gcode_file`, `spd_lvl`, `spd_mag`, `stg_cur`, `cooling_fan_speed`, `nozzle_diameter`, `ams.tray_now`, `ams.ams[].tray[].tray_type` ve `vt_tray.tray_type`. Her mesaj ayrıştırılırken süzülür; 9 KB'lık bir rapor yalnızca birkaç yüz bayt RAM tutar.

### Bilinmesi gerekenler

- **Kısmi raporlar.** A1 gibi yazıcılar yalnızca *değişen* alanları gönderir. Yazılım bunları hatırlanan duruma ekler; ilk ziyarette ve sonra en çok **5,5 dakikada bir** tam durum ister (`pushall`): Bambu, yazıcı işlemcisi takılabileceği için daha sık istenmemesini öneriyor.
- **Açılış taraması.** Karusel başlamadan önce her yazıcı bir kez okunur (LAN yazıcıları tek tek, sonra tüm bulut yazıcıları için tek bir bulut oturumu); altı yazıcı için yaklaşık on saniye sürer.
- **Geçen süre bir tahmindir.** Yazıcılar kalan süreyi ve yüzdeyi bildirir, harcanan süreyi değil. Geçen süre, %1-99 arası için `kalan × yüzde / (100 − yüzde)` ile hesaplanır; yaklaşıktır ve cihaz açılmadan önce biten bir baskı için bilinmez.
- **Bitiş saati.** `şimdi + kalan süre`; NTP ve `TIMEZONE` kullanılır.
- **Yeniden deneme.** Erişilemeyen yazıcı her 60 sn'de (ekrandayken 8 sn'de) denenir. Reddedilen bilgiler en çok dakikada bir denenir; böyle bir yazıcıya bakmak denemeyi öne çekmez.
- **TLS.** Yazıcılar yerel ağda kendinden imzalı sertifika kullanır, olduğu gibi kabul edilir. Bulut sunucusunun sertifikası, anahtar gönderilmeden önce dahili Mozilla CA paketiyle doğrulanır.
- **Dosya adları** ekran yazı tipi için ASCII'ye çevrilir: Türkçe harfler sadeleştirilir (ğ → g), desteklenmeyenler `?` olur.
- **Bellek.** Bağlıyken yaklaşık 100 KB heap boş kalır (görülen en düşük: 93 KB); yazılım 3 MB'lık bölümde yaklaşık 1,2 MB'tır (OTA yok).
- **Ekran uykusu.** 5 dakika boyunca hiçbir yazıcı basmıyor, duraklamış, hazırlanıyor ya da hatalı değilse ekran kapanır; düğmeye dokunmak veya yeni bir etkinlik uyandırır.
- **Açılış tanılaması.** Günlük, ekranın nerede yanıt verdiğini yazar (`[oled] found at 0x3C ...`); yapılandırılan pinlerde sessizse tüm diğer pin çiftlerini tarayıp nereye bağlı olduğunu söyler.

## Proje yapısı

```
platformio.ini            derleme ortamları (esp32c3, demo), platform ve flash ayarları
include/settings.h        pinler, süreler, dil, saat dilimi
include/secrets.example.h git'e girmeyen secrets.h için şablon
src/main.cpp              setup + loop, periyodik durum satırı
src/Config.*              yazıcı listesi ve bilgiler (secrets.h'ı okur)
src/State.*               paylaşılan durum tablosu (kilitli)
src/Poller.*              MQTT oturumları, ayrıştırma, yeniden deneme, bulut, demo verisi
src/Input.*               düğme çözme (kesme) ve buton (tıklama / uzun basış)
src/Ui.*                  ekran, ekranlar, otomatik geçiş, mesaj, ekran uykusu
tools/find_printers.py    LAN'daki yazıcıları listeler (SSDP)
tools/fetch_access_codes.py  erişim kodlarını / bulut girişini Bambu hesabından okur
docs/wiring.svg           bağlantı şeması
```

## Yardımcı araçlar

İkisi de bağımlılığı olmayan düz Python 3 betikleridir.

**`tools/find_printers.py [--seconds 40]`** Bambu yazıcıların SSDP duyurularını dinler; ad, model, seri no, IP, bağlantı modu (örn. `cloud`) ve firmware yazar. Hiçbir şey göndermez.

**`tools/fetch_access_codes.py`**

| Seçenek | Etkisi |
|---|---|
| *(yok)* | E-posta ve şifreyle giriş (gerekirse e-posta kodunu sorar) |
| `--token` | Giriş yerine makerworld.com'daki `token` çerezini yapıştırın |
| `--cloud` | Bulut girişini de `secrets.h`'a yazar ve hesaptaki diğer yazıcıları bulut yazıcısı olarak ekler |
| `--export-others` | `secrets.h`'da olmayan yazıcıları `include/secrets.others.h` dosyasına kaydeder (git'e girmez) |
| `--cn` | Çin bölgesi hesabı (yazılımın bulut modu bunun için denenmedi) |
| `--secrets YOL` | Başka bir `secrets.h` |

Yedek alır (`secrets.h.bak`), yazıcılarınızı tablo olarak yazdırır ve **şifrenizi ya da anahtarınızı asla yazdırmaz veya saklamaz** (`--cloud` ile anahtar bilerek `secrets.h` dosyasına yazılır). Kendini dürüstçe `bambu-oled-info` olarak tanıtır, Bambu uygulaması gibi davranmaz.

## Güvenlik ve gizlilik

- **Ne nerede durur.** Wi-Fi şifresi, erişim kodları, seri numaraları ve bulut anahtarı yalnızca `include/secrets.h` içinde (git'e girmez; `.bak` kopyaları ve `secrets.*.h` de hariç) ve gömülü olarak cihazın flash'ında bulunur. Göndermeden önce `git status` kontrol edin.
- **Bulut anahtarı Bambu hesabınızın şifresidir**, yaklaşık üç ay boyunca. `--cloud` ile cihazın flash'ında durur; cihazı anahtar gibi görün: kaybederseniz ya da başkasına verirseniz Bambu şifrenizi değiştirin. Yalnızca LAN kullanımı hiç anahtar gerektirmez.
- **Salt okunur.** Yazılımın yayınladığı tek mesaj durum isteğidir (`pushall`). Yazıcılarınızı kontrol edemez.
- **LAN TLS'i doğrulanmaz.** Yazıcılar kendinden imzalı sertifika kullanır; ağınıza erişimi olan biri yazıcı gibi davranabilir. Cihaz erişim kodunu yalnızca `secrets.h` içindeki adrese gönderir.
- **Resmi olmayan API.** Hesap araçları ve bulut modu Bambu'nun belgelenmemiş bulut API'sini kullanır. Bambu bunu her an değiştirebilir veya kısıtlayabilir; bir hesabın otomatik kullanımı hoşlarına gitmeyebilir. Yalnızca kendi yazıcılarınız için, kendi sorumluluğunuzda kullanın.

## Sorun giderme

| Belirti | Olası neden ve çözüm |
|---|---|
| Kart tamamen sessiz, yüklemeden sonra bile seri çıktı yok | Birçok ESP32-C3 kartı flash'ını 80 MHz'de okuyamaz: önyükleyici `partition 0 invalid magic number` ile döngüye girer (yalnızca güncel önyükleyiciyle görünür). Bu depo 40 MHz DIO ve pioarduino platformunu zorlar. Platformu değiştirirseniz bu ayarları koruyun. |
| Derleme `MSys/Mingw is not supported` ile düşüyor | `pio`'yu Git Bash'ten değil PowerShell ya da cmd'den çalıştırın. |
| OLED boş kalıyor | `pio device monitor` açın. Günlük `[oled] found at 0x3C` yazar ya da tüm pin çiftlerini tarayıp ekranın nerede yanıt verdiğini söyler. Hiçbir yerde yanıt yoksa VCC (3V3), GND ve lehim noktalarına bakın. |
| Düğme ya da buton bir şey yapmıyor | Günlük her olayı `[input] left / right / click / long press` olarak yazar; 10 sn'lik durum satırı `clk= dt= sw=` gösterir (boştayken hepsi 1, basılıyken `sw` 0). Hiç değişmeyen seviye kablo sorunudur. Dönüş ters: `ENC_REVERSE`. Bir tık atlıyor: `ENC_EDGES_PER_DETENT`. |
| Yazıcıda `AUTH` | Erişim kodu yanlış. Bulut yazıcısında: anahtarın süresi dolmuş, `fetch_access_codes.py --token --cloud` komutunu yeniden çalıştırıp kartı yeniden yükleyin. |
| Yazıcıda `OFF` | Yazıcı kapalı ya da bu ağdan erişilemiyor (başka alt ağ, misafir Wi-Fi, istemci yalıtımı). |
| Ekranda "veri bekleniyor" kalıyor | Yazıcı açılıştan beri hiç veri vermedi. Durum isteği yanıtsız kaldıysa günlükte `connected but no full report received` yazar. |
| Büyük bir yazıcı hiç güncellenmiyor | Mesajları `MQTT_BUFFER_BYTES` değerini aşıyor olabilir; günlük görülen en büyük mesajı yazar. Değeri artırın. |
| Bitiş saati yok | NTP eşitlenmedi (internet gerekir) ya da yazıcı baskıda değil. |
| Adlarda harfler eksik | Yazı tipinde `ş ı ğ İ` yok; `secrets.h` içinde düz harf kullanın. |

## Sınırlar ve fikirler

- Yalnızca birkaç model denendi, bkz. [Durum](#durum).
- Ayarlar yazılıma gömülür; henüz web kurulum sayfası yok. Wi-Fi ve yazıcıları flash'ta saklayan bir captive-portal kurulumu projeyi paylaşmayı kolaylaştırırdı.
- Bulut anahtarını cihaz yenileyemez (Bambu'nun yenileme noktası kullanılamaz), bu yüzden ~3 ayda bir yeniden yükleme gerekir. Bir ayar sayfası bunu da ortadan kaldırırdı.
- Havadan güncelleme yok (büyük bölüm OTA düzeninin yerini alır).
- HMS uyarıları yalnızca sayılır, çözülmez; geçen süre bir tahmindir.
- Fikirler: baskı bitince buzzer ya da LED, Home Assistant / MQTT çıkışı, liste görünümlü daha büyük ekran, yazıcı başına eşikler.

## Katkı

Sorunlar ve çekme istekleri memnuniyetle karşılanır. En faydalı rapor, **çalışan ya da çalışmayan bir modeldir**: lütfen yazıcı modelini, firmware sürümünü, LAN mı bulut mu olduğunu ve bir otomatik geçiş turunun seri günlüğünü ekleyin (önce seri numaranızı ve kodlarınızı silin). Değişikliği `pio run -e esp32c3` ve `pio run -e demo` ile deneyin; ikisi de uyarısız derlenmeli.

## Teşekkür ve lisans

Resmi değildir, Bambu Lab ile ilgisi yoktur. Protokol bilgisi topluluk projesi [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)'dan gelir. Kütüphaneler: [U8g2](https://github.com/olikraus/u8g2), [PubSubClient](https://github.com/knolleary/pubsubclient) ve [ArduinoJson](https://arduinojson.org/); platform: [pioarduino](https://github.com/pioarduino/platform-espressif32).

[MIT Lisansı](LICENSE) ile yayınlanmıştır.
