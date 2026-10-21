# Ayrıntılı başvuru

[← Türkçe ana sayfa](../README.tr.md) · [English reference](reference.md)

Pin ayarları, zamanlama, mimari, yardımcı araçlar ve geliştirme bilgileri. İlk kurulum için ana sayfayı izleyin.

## Ayar başvurusu

### `include/settings.h`

| Ayar | Varsayılan | Anlamı |
|---|---|---|
| `PIN_OLED_SDA`, `PIN_OLED_SCL` | 4, 5 | Ekranın I2C pinleri |
| `PIN_ENC_CLK`, `PIN_ENC_DT`, `PIN_ENC_SW` | 6, 7, 10 | Düğme pinleri (buradaki varsayılanlar C3 içindir; diğer çiplerin kendi değerleri var) |
| `PIN_BUZZER` | 3 | Buzzer pini; `-1` = buzzer yok |
| `BUZZER_PASSIVE` | 0 | 0 = aktif varsayılanları, 1 = pasif varsayılanları; ses modu web panelinden seçilir |
| `BUZZER_ACTIVE_LOW` | 0 | Buzzer modülü pin LOW iken ötüyorsa 1 |
| `ENC_REVERSE` | 0 | Saat yönünde çevirmek geri gidiyorsa 1 yapın |
| `ENC_EDGES_PER_DETENT` | 4 | Düğmenin bir tıkı bir yazıcı atlıyorsa 2 deneyin |
| `TIMEZONE` | `<+03>-3` | Bitiş saati için POSIX saat dilimi, örn. `CET-1CEST,M3.5.0,M10.5.0/3` |
| `CAROUSEL_MS` | 10000 | Otomatik modda her yazıcının ekranda kalma süresi |
| `MANUAL_TIMEOUT_MS` | 45000 | Düğmeye dokunulmazsa otomatik mod bu süre sonra devam eder |
| `DONE_ALERT_MS` | 300000 | Biten iş bu süre boyunca `DONE` yanıp söner, sonra yazıcı sabit `IDLE` gösterir |
| `SCREEN_SLEEP_MS` | 300000 | Hiçbir şey basılmıyorsa bu süre sonra ekran kapanır; 0 = hiç |
| `CLOUD_POLL_MS`, `CLOUD_FINISH_POLL_MS` | 300000, 60000 | Normal / bitişe yakın bulut kontrol aralığı |
| `LAN_POLL_MS`, `LAN_FINISH_POLL_MS` | 30000, 10000 | Normal / bitişe yakın LAN kontrol aralığı |
| `FINISH_SOON_MIN` | 10 | Sık kontrole geçilen tahmini kalan dakika |
| `AUTO_LISTEN_MS` | 3500 | Sınırlı bir ziyarette durum alındıktan sonraki dinleme süresi |
| `FIRST_REPORT_WAIT_MS` | 7000 | Bağlanıp sessiz kalan yazıcıdan vazgeçme süresi |
| `CONNECT_TIMEOUT_S`, `HANDSHAKE_TIMEOUT_S` | 3, 8 | TCP ve TLS zaman aşımları |
| `OFFLINE_RETRY_MS` | 60000 | Erişilemeyen yazıcı ya da reddedilen bilgi için yeniden deneme aralığı |
| `FOCUS_RETRY_MS` | 8000 | O yazıcı ekrandayken yeniden deneme aralığı |
| `PUSHALL_MIN_INTERVAL_MS` | 330000 | Yazıcı başına normal tam durum isteği aralığı (5,5 dk) |
| `FINISH_CONFIRM_MS` | 60000 | Çalışan işin son tahmini süresi sıfıra ulaştığında tam durum doğrulaması |
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
| `esp32s3`, `esp32`, `esp32c6`, `esp32s2` | Diğer çipler, yalnızca derleme denendi, bkz. [Hangi ESP32 çalışır?](../README.tr.md#uyumluluk) |

**Doğal USB'li** bir kart için (ayrı USB-UART çipi yok) `build_flags` içindeki iki `-DARDUINO_USB_...` satırını silin. Başka kartlar için `board` değerini değiştirin. Platform [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino çekirdeği 3), flash 40 MHz DIO'ya zorlanır (bkz. [Sorun giderme](../README.tr.md#sorun-giderme)).

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

ESP32-C3 aynı anda **tek MQTT/TLS oturumu** tutar. Poller, ekran seçiminden bağımsız olarak süresi gelen yazıcıları ziyaret eder; en çok gecikeni önce seçer. Ziyaretler sınırlıdır, böylece elle tek yazıcıya odaklanmak diğerlerini aç bırakmaz. Ziyaretler arasında bağlantı açık tutulur; LAN ziyaretleri arasında mümkünse tüm uzak yazıcıları dinleyen ortak bulut oturumuna dönülür. Bu, karma LAN/bulut kurulumunda ek TLS bağlantıları gerektirir. Yalnızca LAN kullanılıyorsa son ziyaret edilen yazıcı bağlı kalır.

### Durum protokolü

Yazıcı 8883 portunda (TLS) bir MQTT sunucusu çalıştırır. Cihaz `bblp` kullanıcısı ve yazıcının **erişim koduyla** giriş yapar, `device/<seriNo>/report` konusuna abone olur ve `device/<seriNo>/request` konusuna `{"pushing":{"command":"pushall"}}` yayınlayarak bir kez tam durum ister. Bulut yazıcılarında sunucu Bambu'nundur, kullanıcı `u_<hesap numaranız>`, şifre anahtarınızdır; konular aynıdır. Protokol topluluk tarafından [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)'da belgelenmiştir.

Kullanılan alanlar (hepsi `print` altında): `gcode_state`, `mc_percent`, `mc_remaining_time`, `layer_num`, `total_layer_num`, `nozzle_temper`, `nozzle_target_temper`, `bed_temper`, `bed_target_temper`, `chamber_temper`, `print_error`, `hms`, `wifi_signal`, `subtask_name` / `gcode_file`, `spd_lvl`, `spd_mag`, `stg_cur`, `cooling_fan_speed`, `nozzle_diameter`, `ams.tray_now`, `ams.ams[].tray[].tray_type` ve `vt_tray.tray_type`. Her mesaj ayrıştırılırken süzülür; 9 KB'lık bir rapor yalnızca birkaç yüz bayt RAM tutar.

### Bilinmesi gerekenler

- **Kısmi raporlar.** A1 gibi yazıcılar yalnızca *değişen* alanları gönderir. Yazılım bunları hatırlanan duruma ekler; ilk ziyarette ve normalde en çok **5,5 dakikada bir** tam durum ister (`pushall`; sıfır süre doğrulaması aşağıdaki istisnadır): Bambu, yazıcı işlemcisi takılabileceği için daha sık istenmemesini öneriyor.
- **Uyarlanabilir kontrol ve sınırları.** Son kalan-süre alanının kendi zaman damgası tutulur; sıcaklık mesajları bu saati yenilemez. Yalnızca çalışan baskılarda geçen süre düşülerek son 10 dakikaya girildiği hesaplanır; bu hesap baskıyı kendiliğinden bitmiş saymaz. Bulut kontrolü 5 dk → 1 dk, LAN kontrolü 30 sn → 10 sn olur. Bunlar ziyaret sonundan başlayan hedef aralıklardır; çok sayıda yazıcı, bağlantı/NTP zaman aşımı ve Wi-Fi kesintisi gecikme ekleyebilir. **Tam raporla toparlanma:** `pushall` normalde 330 sn arayla gönderilir. Çalışan işin son tahmini süresi sıfıra indiğinde yazıcının gerçek durumunu doğrulamak için 60 sn aralığına geçilir. Duraklatılmış, boşta, bitmiş veya kalan süresi bilinmeyen işlerde normal aralık korunur. Bağlantı kapalıyken kaçırılan tek seferlik FINISH değişikliği sonraki tam rapora kadar fark edilmeyebilir. Açık oturumdaki FINISH hemen ses/ekran uyarısını tetikler; tahmini süre sıfıra indi diye uyarı verilmez.
- **Açılış taraması.** Karusel başlamadan önce her yazıcı bir kez okunur (LAN yazıcıları tek tek, sonra tüm bulut yazıcıları için tek bir bulut oturumu); altı yazıcı için yaklaşık on saniye sürer.
- **Geçen süre bir tahmindir.** Yazıcılar kalan süreyi ve yüzdeyi bildirir, harcanan süreyi değil. Geçen süre, %1-99 arası için `kalan × yüzde / (100 − yüzde)` ile hesaplanır; yaklaşıktır ve cihaz açılmadan önce biten bir baskı için bilinmez.
- **Bitiş saati.** `şimdi + kalan süre`; NTP ve `TIMEZONE` kullanılır.
- **Yeniden deneme.** Erişilemeyen yazıcı için 60 sn (deneme sonunda ekranda seçiliyse 8 sn) beklenir. Reddedilen bilgiler için en az 60 sn beklenir; ekran seçimi bu beklemeyi sıfırlamaz. Bağlantı kaybı çevrimdışı olarak kaydedilir.
- **TLS.** Yazıcılar yerel ağda kendinden imzalı sertifika kullanır, olduğu gibi kabul edilir. Bulut sunucusunun sertifikası, anahtar gönderilmeden önce dahili Mozilla CA paketiyle doğrulanır.
- **İş adları.** Gösterilen ad `subtask_name`'deki iş adıdır (yoksa `gcode_file`'daki dosya adı). Bambu Studio'dan gönderilen işler *proje* adını bildirir ve bu ad bir projenin her plakasında aynıdır; bu yüzden plaka numarası eklenir: `Organizer (plate 2)`.
- **Dosya adları** ekran yazı tipi için ASCII'ye çevrilir: Türkçe harfler sadeleştirilir (ğ → g), desteklenmeyenler `?` olur.
- **Bellek.** 2026-10-21 tarihindeki 45 saniyelik C3 açılış kontrolünde yaklaşık 94 KB boş heap, en düşük yaklaşık 75 KB görüldü. Bu, uzun süreli sızıntı testi değildir. Yazılım 3 MB’lık bölümde yaklaşık 1,3 MB’tır (OTA yok).
- **Ekran uykusu.** 5 dakika boyunca hiçbir yazıcı basmıyor, duraklamış, hazırlanıyor ya da hatalı değilse ekran kapanır; düğmeye dokunmak veya yeni bir etkinlik uyandırır.
- **Açılış tanılaması.** Günlük, ekranın nerede yanıt verdiğini yazar (`[oled] found at 0x3C ...`); yapılandırılan pinlerde sessizse tüm diğer pin çiftlerini tarayıp nereye bağlı olduğunu söyler.

## Proje yapısı

```
platformio.ini            derleme ortamları (esp32c3, demo), platform ve flash ayarları
include/settings.h        pinler, süreler, dil, saat dilimi
include/secrets.example.h git'e girmeyen secrets.h için şablon
src/main.cpp              setup + loop, periyodik durum satırı
src/Config.*              yazıcı listesi ve bilgiler (NVS + secrets.h varsayılanları)
src/Web.*                 dahili HTTP ayar sayfası ve kurulum erişim noktası (AP)
src/State.*               paylaşılan durum tablosu (kilitli)
src/Poller.*              MQTT oturumları, ayrıştırma, yeniden deneme, bulut, demo verisi
src/Input.*               düğme çözme (kesme) ve buton (tıklama / uzun / çok uzun basış)
src/Buzzer.*              isteğe bağlı buzzer: aktif ve pasif buzzer için ses kalıpları
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

- **Ne nerede durur.** Wi-Fi şifresi, erişim kodları, seri numaraları ve bulut anahtarı yalnızca `include/secrets.h` içinde (git'e girmez; `.bak` kopyaları ve `secrets.*.h` de hariç) ve kaydedilen web ayarları için cihazın flash/NVS belleğinde bulunur. Göndermeden önce `git status` kontrol edin.
- **Bulut anahtarı Bambu hesabınızın şifresidir**, yaklaşık üç ay boyunca. `--cloud` ile cihazın flash'ında durur; cihazı anahtar gibi görün: kaybederseniz ya da başkasına verirseniz Bambu şifrenizi değiştirin. Yalnızca LAN kullanımı hiç anahtar gerektirmez.
- **Salt okunur.** Yazılımın yayınladığı tek mesaj durum isteğidir (`pushall`). Yazıcılarınızı kontrol edemez.
- **LAN TLS'i doğrulanmaz.** Yazıcılar kendinden imzalı sertifika kullanır; ağınıza erişimi olan biri yazıcı gibi davranabilir. Cihaz erişim kodunu yalnızca `secrets.h` içindeki adrese gönderir.
- **Resmi olmayan API.** Hesap araçları ve bulut modu Bambu'nun belgelenmemiş bulut API'sini kullanır. Bambu bunu her an değiştirebilir veya kısıtlayabilir; bir hesabın otomatik kullanımı hoşlarına gitmeyebilir. Yalnızca kendi yazıcılarınız için, kendi sorumluluğunuzda kullanın.

## Zamanlama regresyon testleri

Ana bilgisayarda C++ derleyicisiyle çalıştırın (çıktı dosyasını depo dışında tutun):

```powershell
g++ -std=c++11 -Wall -Wextra -Werror tests/poll_schedule_test.cpp -o "$env:TEMP/bambu-poll-test.exe"
& "$env:TEMP/bambu-poll-test.exe"
```

Testler; normal/sık kontrol aralıklarını, veri gelmeden bitişe yaklaşmayı, bilinmeyen/duraklatılmış süreyi, odak kaynaklı aç bırakmayı, kimlik doğrulama beklemesini ve `millis()` taşmasını kapsar. Donanımda gerçek FINISH bildirimi ayrıca denenmelidir.

## Katkı

Sorunlar ve çekme istekleri memnuniyetle karşılanır. En faydalı rapor, **çalışan ya da çalışmayan bir modeldir**: lütfen yazıcı modelini, firmware sürümünü, LAN mı bulut mu olduğunu ve bir otomatik geçiş turunun seri günlüğünü ekleyin (önce seri numaranızı ve kodlarınızı silin). Değişikliği `pio run -e esp32c3` ve `pio run -e demo` ile deneyin; ikisi de uyarısız derlenmeli.

## Teşekkür ve lisans

Resmi değildir, Bambu Lab ile ilgisi yoktur. Protokol bilgisi topluluk projesi [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)'dan gelir. Kütüphaneler: [U8g2](https://github.com/olikraus/u8g2), [PubSubClient](https://github.com/knolleary/pubsubclient) ve [ArduinoJson](https://arduinojson.org/); platform: [pioarduino](https://github.com/pioarduino/platform-espressif32).

[MIT Lisansı](../LICENSE) ile yayınlanmıştır.
