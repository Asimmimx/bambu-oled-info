# Bambu OLED Info

Bambu Lab yazıcılarınızın ne yaptığını gösteren küçük bir masa üstü cihaz: **ad, ilerleme çubuğu, kalan süre, geçen süre, bitiş saati, sıcaklıklar, katman ve dosya adı**. Bir ESP32-C3, 0.91" OLED ekran ve bir döner düğmeden oluşur. Yazıcılarla doğrudan yerel ağınızdan konuşur: sunucu, uygulama ya da bulut gerekmez.

*[English](README.md)*

![Bağlantı şeması](docs/wiring.svg)

## Durum

| | |
|---|---|
| Donanımda doğrulandı | ESP32-C3 (CH343 USB-UART kart), SSD1306 128x32 OLED, 3x **A1 mini** (firmware 01.08.01.00), normal bulut modunda, LAN Only Mode gerekmeden |
| Yazıldı, donanımda henüz doğrulanmadı | Döner düğme girişi, A1, P1/X1, H2 serisi, X2D, A2L |

## Gerekenler

- **ESP32-C3** geliştirme kartı
- **0.91" 128x32 SSD1306 I2C OLED** (pinler: GND, VCC, SCK/SCL, SDA)
- **KY-040 döner düğme** (isteğe bağlı: yazıcı seçmenizi sağlar)
- Birkaç jumper kablo ve USB kablosu
- [PlatformIO](https://platformio.org/)

## Bağlantı

| OLED | ESP32-C3 | | KY-040 | ESP32-C3 |
|---|---|---|---|---|
| GND | GND | | CLK | GPIO6 |
| VCC | 3V3 | | DT | GPIO7 |
| SCK (SCL) | GPIO5 | | SW | GPIO10 |
| SDA | GPIO4 | | + | 3V3 (5V değil) |
| | | | GND | GND |

Modüllerin üzerindeki pin *etiketlerine* bakın, sırasına değil. Pinler [`include/settings.h`](include/settings.h) içinde değiştirilebilir.

## Hızlı başlangıç

```bash
git clone https://github.com/Asimmimx/bambu-oled-info
cd bambu-oled-info
copy include\secrets.example.h include\secrets.h
```

1. **Yazıcıları bulun** (ad, seri no, IP). Aynı ağdaki bir bilgisayardan:

   ```bash
   python tools/find_printers.py
   ```

2. **`include/secrets.h` dosyasını düzenleyin:** Wi-Fi bilgileri ve her yazıcı için bir satır. Bu dosya git'e girmez.

3. **Erişim kodlarını alın.** İki yol var:
   - Yazıcıda *LAN Only Mode*'u açın; kod anahtarın altında görünür. Not: bu mod yazıcıyı Bambu Cloud'dan ayırır.
   - Ya da yazıcılara dokunmadan, Bambu hesabınızdan bir kez okuyun (betik Bambu'nun resmi olmayan bulut API'sini kullanır):

     ```bash
     python tools/fetch_access_codes.py            # e-posta + şifre (+ e-posta kodu)
     python tools/fetch_access_codes.py --token    # ya da makerworld.com'daki "token" çerezini yapıştırın
     ```

     Kodları `secrets.h` dosyasına yazar, başka hiçbir şey saklamaz. Sonrasında her şey yereldir.

4. **Derleyip yükleyin:**

   ```bash
   pio run -e esp32c3 -t upload
   ```

   Windows'ta Git Bash değil, PowerShell veya cmd kullanın.

Önce ekranları görmek isterseniz: `pio run -e demo -t upload` sahte yazıcılarla tüm durumları gösterir.

## Kullanım

| Eylem | Sonuç |
|---|---|
| Hiçbir şey | Her yazıcı 10 sn gösterilir, sonra sıradaki |
| Düğmeyi çevirmek | Yazıcı seçer; otomatik geçiş 45 sn durur |
| Tıklamak | Sonraki ekran: **İlerleme** → **Detay** → **Genel bakış** |
| Uzun basış | Otomatik geçişi aç / kapat |

Hata veren yazıcının başlığı yanıp söner. 5 dakika boyunca hiçbir şey basılmıyorsa ekran kapanır (OLED yanması için); düğmeye dokunmak veya yeni bir baskı ekranı uyandırır.

## Birden fazla yerdeki yazıcılar

Cihaz yalnızca bağlı olduğu Wi-Fi'yi görür. Başka yerdeki yazıcılar için kendi `secrets.h` dosyasıyla ikinci bir cihaz kurun.

## Sorun giderme

- **Kart hiç yazı vermiyor, yüklemeden sonra bile.** Birçok ESP32-C3 kartı flash'ını 80 MHz'de okuyamaz; önyükleyici `partition 0 invalid magic number` ile döngüye girer. Bu depo `platformio.ini` içinde 40 MHz DIO ve güncel Arduino çekirdeğini zorlar.
- **Ekran boş.** `pio device monitor` ile seri çıktıya bakın: `[oled] found at 0x3C ...` yazar; bulamazsa diğer pin çiftlerini tarayıp ekranın nerede yanıt verdiğini söyler.
- **`AUTH`**: erişim kodu yanlış. **`OFF`**: yazıcı kapalı ya da bu ağdan erişilemiyor.
- Ekran yazı tipinde ş ı ğ İ yok; adlarda düz harf kullanın.

## Uyarı

Resmi değildir, Bambu Lab ile ilgisi yoktur. Protokol topluluk tarafından belgelenmiştir ([OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)) ve her firmware güncellemesiyle değişebilir. Bulut betiği belgelenmemiş bir API kullanır ve çalışmayı bırakabilir. Firmware yalnızca yazıcı durumunu *okur*; durum isteği dışında komut göndermez.

## Lisans

[MIT](LICENSE)
