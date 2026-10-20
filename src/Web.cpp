#include "Web.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <math.h>
#include <time.h>
#include "Buzzer.h"
#include "Config.h"
#include "Poller.h"
#include "State.h"
#include "Ui.h"

namespace {

volatile bool g_apActive = false;

#if !DEMO_MODE
WebServer g_server(80);
uint32_t g_restartAt = 0;

const char kIndexHtml[] = R"rawliteral(<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bambu OLED Monitor - Kontrol Paneli</title>
<style>
:root {
  --bg: #0f1215;
  --card: #181d22;
  --card2: #12161a;
  --border: #2a323b;
  --text: #e6edf3;
  --muted: #8b949e;
  --accent: #00ae42;
  --accent-hover: #00c84d;
  --warn: #d29922;
  --danger: #e5534b;
  --info: #388bfd;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  padding: 16px 12px 60px;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  background: var(--bg);
  color: var(--text);
  line-height: 1.45;
}
.wrap { max-width: 900px; margin: 0 auto; }
header {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 14px;
  padding-bottom: 12px;
  border-bottom: 1px solid var(--border);
}
h1 { font-size: 1.3rem; margin: 0; display: flex; align-items: center; gap: 9px; }
.dot { width: 10px; height: 10px; border-radius: 50%; background: var(--accent); display: inline-block; box-shadow: 0 0 8px var(--accent); }
.badges { display: flex; gap: 6px; flex-wrap: wrap; align-items: center; }
.badge {
  font-size: 0.76rem;
  padding: 3px 9px;
  border-radius: 999px;
  background: var(--card);
  border: 1px solid var(--border);
  color: var(--muted);
}
.tabs {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
  margin-bottom: 16px;
  border-bottom: 1px solid var(--border);
  padding-bottom: 10px;
}
.tab-btn {
  background: var(--card);
  border: 1px solid var(--border);
  color: var(--muted);
  padding: 8px 14px;
  border-radius: 8px;
  cursor: pointer;
  font-weight: 600;
  font-size: 0.86rem;
  transition: all 0.15s;
}
.tab-btn:hover { color: var(--text); border-color: #3d4854; }
.tab-btn.active {
  background: rgba(0, 174, 66, 0.16);
  border-color: var(--accent);
  color: #fff;
}
.tab-pane { display: none; }
.tab-pane.active { display: block; }
.card {
  background: var(--card);
  border: 1px solid var(--border);
  border-radius: 10px;
  padding: 16px;
  margin-bottom: 14px;
}
.card h2 {
  margin: 0 0 12px;
  font-size: 1rem;
  display: flex;
  justify-content: space-between;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
}
.hint { font-size: 0.8rem; color: var(--muted); margin: -4px 0 12px; }
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
  gap: 12px;
}
.grid-3 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 12px;
}
label { display: block; font-size: 0.8rem; color: var(--muted); margin-bottom: 4px; }
input[type="text"], input[type="password"], input[type="number"], select, textarea {
  width: 100%;
  padding: 8px 10px;
  background: #0b0e11;
  border: 1px solid var(--border);
  border-radius: 6px;
  color: var(--text);
  font-size: 0.88rem;
}
input[type="range"] { width: 100%; accent-color: var(--accent); cursor: pointer; }
input:focus, select:focus, textarea:focus { outline: none; border-color: var(--accent); }
.switch-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 9px 12px;
  background: var(--card2);
  border: 1px solid var(--border);
  border-radius: 8px;
  gap: 10px;
}
.switch-row span { font-size: 0.88rem; font-weight: 500; }
.switch-row small { display: block; font-size: 0.75rem; color: var(--muted); font-weight: 400; }
input[type="checkbox"].toggle {
  width: 40px;
  height: 22px;
  appearance: none;
  background: #2c343d;
  border-radius: 999px;
  position: relative;
  cursor: pointer;
  flex-shrink: 0;
  transition: background 0.2s;
}
input[type="checkbox"].toggle:checked { background: var(--accent); }
input[type="checkbox"].toggle::after {
  content: "";
  position: absolute;
  top: 3px;
  left: 3px;
  width: 16px;
  height: 16px;
  background: #fff;
  border-radius: 50%;
  transition: transform 0.2s;
}
input[type="checkbox"].toggle:checked::after { transform: translateX(18px); }
button {
  cursor: pointer;
  border: 1px solid var(--border);
  background: #21262d;
  color: var(--text);
  padding: 7px 13px;
  border-radius: 6px;
  font-size: 0.85rem;
  font-weight: 500;
  transition: all 0.15s;
}
button:hover { background: #2c333b; }
button:disabled { opacity: 0.5; cursor: not-allowed; }
.btn-primary {
  background: var(--accent);
  border-color: var(--accent);
  color: #fff;
  font-weight: 600;
  padding: 9px 18px;
  font-size: 0.92rem;
}
.btn-primary:hover { background: var(--accent-hover); }
.btn-info { background: rgba(56,139,253,0.18); border-color: var(--info); color: #79c0ff; }
.btn-info:hover { background: rgba(56,139,253,0.3); }
.btn-danger { color: var(--danger); border-color: rgba(229,83,75,0.4); }
.btn-danger:hover { background: rgba(229,83,75,0.15); }
.btn-sm { padding: 4px 9px; font-size: 0.78rem; }
.btn-active { background: var(--accent) !important; border-color: var(--accent) !important; color: #fff !important; }
.btn-group { display: flex; gap: 6px; flex-wrap: wrap; align-items: center; }

/* Live Printer Cards */
.p-card {
  background: var(--card2);
  border: 1px solid var(--border);
  border-radius: 9px;
  padding: 12px;
  cursor: pointer;
  transition: border-color 0.15s;
}
.p-card:hover { border-color: #44505e; }
.p-card.selected { border-color: var(--accent); box-shadow: 0 0 0 1px var(--accent); }
.p-top { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.p-name { font-weight: 600; font-size: 0.94rem; }
.p-tag {
  font-size: 0.72rem;
  font-weight: 700;
  padding: 2px 7px;
  border-radius: 4px;
  background: #283038;
}
.tag-RUN { background: rgba(0,174,66,0.22); color: #56f08a; border: 1px solid var(--accent); }
.tag-PREP, .tag-PAUSE { background: rgba(210,153,34,0.22); color: #f2cc60; border: 1px solid var(--warn); }
.tag-ERR, .tag-AUTH { background: rgba(229,83,75,0.22); color: #ff8b85; border: 1px solid var(--danger); }
.tag-DONE { background: rgba(56,139,253,0.22); color: #79c0ff; border: 1px solid var(--info); }
.p-bar { height: 7px; background: #21262d; border-radius: 4px; overflow: hidden; margin-bottom: 8px; }
.p-fill { height: 100%; background: var(--accent); transition: width 0.3s; }
.p-meta { font-size: 0.77rem; color: var(--muted); display: flex; justify-content: space-between; margin-top: 3px; }
.p-file { font-size: 0.75rem; color: #a5b0bc; margin-top: 6px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }

.printer-item {
  background: var(--card2);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 12px;
  margin-bottom: 10px;
}
.printer-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; gap: 8px; }
.note-chips { display: flex; gap: 5px; flex-wrap: wrap; margin: 8px 0; }
.note-chip { padding: 3px 8px; font-size: 0.75rem; border-radius: 5px; background: #1d242b; }

#statusBanner {
  display: none;
  padding: 10px 14px;
  border-radius: 8px;
  margin-bottom: 14px;
  font-size: 0.88rem;
}
.status-ok { background: rgba(0,174,66,0.16); border: 1px solid var(--accent); color: #56f08a; }
.status-err { background: rgba(229,83,75,0.16); border: 1px solid var(--danger); color: #ff8b85; }
.hidden { display: none !important; }
.actions { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 10px; margin-top: 14px; }
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1><span class="dot"></span> Bambu OLED Monitor</h1>
    <div class="badges">
      <span class="badge" id="ipBadge">IP: ...</span>
      <span class="badge" id="rssiBadge">Wi-Fi: ...</span>
      <span class="badge" id="heapBadge">RAM: ...</span>
      <span class="badge" id="upBadge">Süre: ...</span>
    </div>
  </header>

  <div id="statusBanner"></div>

  <div class="tabs">
    <button type="button" class="tab-btn active" onclick="openTab('tab-live', this)">📊 Canlı Durum &amp; Ekran</button>
    <button type="button" class="tab-btn" onclick="openTab('tab-sound', this)">🔊 Ses &amp; Melodi Stüdyosu</button>
    <button type="button" class="tab-btn" onclick="openTab('tab-display', this)">🖥️ Ekran, Düğme &amp; Saat</button>
    <button type="button" class="tab-btn" onclick="openTab('tab-network', this)">🖨️ Yazıcılar &amp; Ağ</button>
  </div>

  <!-- TAB 1: LIVE STATUS & REMOTE OLED CONTROL -->
  <div id="tab-live" class="tab-pane active">
    <div class="card">
      <h2>
        <span>Uzaktan OLED Ekran Kontrolü</span>
        <div class="btn-group">
          <button type="button" class="btn-sm" id="btnAutoToggle" onclick="uiCmd({autoMode: !liveState.autoMode})">Auto: ...</button>
          <button type="button" class="btn-sm" id="btnMuteQuick" onclick="soundCmd({action:'mute', muted: !liveState.muted})">Ses: ...</button>
          <button type="button" class="btn-sm" id="btnSleepToggle" onclick="uiCmd({sleep: !liveState.sleeping})">Ekran: ...</button>
        </div>
      </h2>
      <div class="grid">
        <div>
          <label>Aktif OLED Sayfası (Tıkla &amp; Değiştir)</label>
          <div class="btn-group" style="margin-top:4px">
            <button type="button" id="vBtn0" onclick="uiCmd({view:0})">1. Progress</button>
            <button type="button" id="vBtn1" onclick="uiCmd({view:1})">2. Overview</button>
            <button type="button" id="vBtn2" onclick="uiCmd({view:2})">3. Details</button>
            <button type="button" id="vBtn3" onclick="uiCmd({view:3})">4. Info</button>
          </div>
        </div>
        <div>
          <label>OLED Ekrana Anlık Mesaj Gönder (Toast)</label>
          <div style="display:flex;gap:6px">
            <input type="text" id="toastInput" maxlength="21" placeholder="Örn: Merhaba! (max 21 kr)">
            <button type="button" class="btn-info" onclick="sendToast()">Gönder</button>
          </div>
        </div>
      </div>
    </div>

    <div class="card">
      <h2>
        <span>Yazıcılar (OLED'de odaklamak için karta tıklayın)</span>
        <button type="button" class="btn-sm" onclick="fetchStatus()">🔄 Yenile</button>
      </h2>
      <div id="liveGrid" class="grid-3"></div>
    </div>
  </div>

  <!-- TAB 2: SOUND & MELODY STUDIO -->
  <div id="tab-sound" class="tab-pane">
    <div class="card">
      <h2>
        <span>Ses &amp; Buzzer Kontrolleri</span>
        <div class="btn-group">
          <button type="button" class="btn-info btn-sm" onclick="soundCmd({action:'test', sound:'chirp'})">🔊 Açılış Sesi Test</button>
          <button type="button" class="btn-danger btn-sm" onclick="soundCmd({action:'stop'})">⏹ Sesi Durdur</button>
        </div>
      </h2>
      <div class="grid">
        <div class="switch-row">
          <div>
            <span>Buzzer Sesi Açık</span>
            <small>Kapatıldığında bitiş ve hata alarmları çalmaz</small>
          </div>
          <input type="checkbox" class="toggle" id="s_enableSound" onchange="saveLiveSettings()">
        </div>
        <div class="switch-row">
          <div>
            <span>Açılışta Bip Sesi (Startup Chirp)</span>
            <small>Cihaz ilk elektriğe takıldığında kısa bip verir</small>
          </div>
          <input type="checkbox" class="toggle" id="s_startupChirp" onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_buzzerPassive">Buzzer Donanım Tipi</label>
          <select id="s_buzzerPassive" onchange="saveLiveSettings()">
            <option value="0">Aktif Buzzer (Sabit tonlu - Ritimle çalar)</option>
            <option value="1">Pasif Buzzer / Piezo (Gerçek Notalar &amp; Melodi)</option>
          </select>
        </div>
        <div>
          <label for="s_buzzerActiveLow">Sinyal Polaritesi (Tetikleme Yönü)</label>
          <select id="s_buzzerActiveLow" onchange="saveLiveSettings()">
            <option value="0">Normal (HIGH = Ses Verir)</option>
            <option value="1">Ters / Active-LOW (LOW = Ses Verir, 3-pin modüller)</option>
          </select>
        </div>
      </div>
    </div>

    <div class="card">
      <h2>🎵 Baskı Bittiğinde Çalacak Melodi (Finish Sound)</h2>
      <div class="grid">
        <div>
          <label for="s_finishMelody">Bitiş Melodisi Seçimi</label>
          <div style="display:flex;gap:8px">
            <select id="s_finishMelody" onchange="toggleCustomBoxes(); saveLiveSettings()">
              <option value="0">1. Klasik Yükselen (C6 - E6 - G6 / 3 Bip)</option>
              <option value="1">2. Zafer Fanfarı (C6 - E6 - G6 - C7)</option>
              <option value="2">3. Kibar İkili Bip (Kısa &amp; Sade)</option>
              <option value="3">4. Neşeli Arpej (5 Hızlı Nota)</option>
              <option value="4">5. Atölye Uzun Alarm (3 Uzun Güçlü Bip)</option>
              <option value="5">6. Özel Melodi (Aşağıdan Kendin Tasarla)</option>
            </select>
            <button type="button" class="btn-info" style="white-space:nowrap" onclick="testSelectedFinish()">🔊 Test Et</button>
          </div>
        </div>
      </div>

      <div id="customFinishBox" style="margin-top:14px">
        <label for="s_customFinish">Özel Bitiş Melodisi Dizisi (<code>FrekansHz:SüreMs, 0:SusMs, ...</code>)</label>
        <div style="display:flex;gap:8px">
          <input type="text" id="s_customFinish" maxlength="95" placeholder="1047:140,0:70,1319:140,0:70,1568:280" onchange="saveLiveSettings()">
          <button type="button" class="btn-info" style="white-space:nowrap" onclick="playCustomBox('s_customFinish')">▶ Dinle</button>
        </div>
        <div class="note-chips">
          <span style="font-size:0.75rem;color:var(--muted);align-self:center">Nota Ekle:</span>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','523:140')">Do (C5)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','659:140')">Mi (E5)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','784:140')">Sol (G5)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','1047:140')">Do (C6)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','1319:140')">Mi (E6)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','1568:200')">Sol (G6)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','2093:260')">İnce Do (C7)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFinish','0:80')">Sus (80ms)</button>
          <button type="button" class="note-chip btn-danger" onclick="document.getElementById('s_customFinish').value='';saveLiveSettings()">Temizle</button>
        </div>
      </div>
    </div>

    <div class="card">
      <h2>🚨 Hata / Başarısız Baskı Melodisi (Error / Failed Sound)</h2>
      <div class="grid">
        <div>
          <label for="s_failedMelody">Hata Melodisi Seçimi</label>
          <div style="display:flex;gap:8px">
            <select id="s_failedMelody" onchange="toggleCustomBoxes(); saveLiveSettings()">
              <option value="0">1. Klasik İkili Siren (880Hz / 660Hz - 4 Bip)</option>
              <option value="1">2. Acil SOS Mors Ritmi (... --- ...)</option>
              <option value="2">3. Üzgün Alçalan Trombon (G5 - F#5 - F5 - E5)</option>
              <option value="3">4. Hızlı Uyarı Alarmı (6 Seri Bip)</option>
              <option value="4">5. Özel Hata Melodisi (Aşağıdan Kendin Tasarla)</option>
            </select>
            <button type="button" class="btn-info" style="white-space:nowrap" onclick="testSelectedFailed()">🔊 Test Et</button>
          </div>
        </div>
      </div>

      <div id="customFailedBox" style="margin-top:14px">
        <label for="s_customFailed">Özel Hata Melodisi Dizisi (<code>FrekansHz:SüreMs, 0:SusMs, ...</code>)</label>
        <div style="display:flex;gap:8px">
          <input type="text" id="s_customFailed" maxlength="95" placeholder="880:220,0:90,660:220,0:90,880:220,0:90,660:220" onchange="saveLiveSettings()">
          <button type="button" class="btn-info" style="white-space:nowrap" onclick="playCustomBox('s_customFailed')">▶ Dinle</button>
        </div>
        <div class="note-chips">
          <span style="font-size:0.75rem;color:var(--muted);align-self:center">Nota Ekle:</span>
          <button type="button" class="note-chip" onclick="appendNote('s_customFailed','880:200')">La (880Hz)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFailed','660:200')">Mi (660Hz)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFailed','1500:100')">Tiz Bip (1500Hz)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFailed','440:300')">Pes Bip (440Hz)</button>
          <button type="button" class="note-chip" onclick="appendNote('s_customFailed','0:90')">Sus (90ms)</button>
          <button type="button" class="note-chip btn-danger" onclick="document.getElementById('s_customFailed').value='';saveLiveSettings()">Temizle</button>
        </div>
      </div>

      <div class="actions">
        <button type="button" class="btn-primary" onclick="saveLiveSettings(true)">💾 Ses &amp; Melodi Ayarlarını Kaydet</button>
      </div>
    </div>
  </div>

  <!-- TAB 3: DISPLAY, KNOB & TIME SETTINGS -->
  <div id="tab-display" class="tab-pane">
    <div class="card">
      <h2>🖥️ OLED Ekran Görünümü</h2>
      <div class="grid">
        <div>
          <label for="s_oledContrast">Ekran Parlaklığı (Contrast): <b id="contrastVal">255</b></label>
          <input type="range" id="s_oledContrast" min="1" max="255" value="255"
                 oninput="document.getElementById('contrastVal').textContent=this.value"
                 onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_defaultView">Açılışta İlk Gösterilecek Ekran</label>
          <select id="s_defaultView" onchange="saveLiveSettings()">
            <option value="0">Progress (İlerleme Çubuğu &amp; Süre)</option>
            <option value="1">Overview (6'lı Tablo Görünümü)</option>
            <option value="2">Details (Sıcaklıklar, Katman &amp; Dosya Adı)</option>
            <option value="3">Info (Hız, Fan, Filament &amp; Hata Kodu)</option>
          </select>
        </div>
        <div class="switch-row">
          <div>
            <span>Ekranı 180° Döndür (Ters Montaj)</span>
            <small>Kutu veya kablo yönüne göre ekranı baş aşağı çevirir</small>
          </div>
          <input type="checkbox" class="toggle" id="s_oledFlip" onchange="saveLiveSettings()">
        </div>
        <div class="switch-row">
          <div>
            <span>Renkleri Ters Çevir (Invert OLED)</span>
            <small>Beyaz zemin üzerine siyah yazı modu</small>
          </div>
          <input type="checkbox" class="toggle" id="s_oledInvert" onchange="saveLiveSettings()">
        </div>
      </div>
    </div>

    <div class="card">
      <h2>⏱️ Otomatik Geçiş (Carousel) ve Zamanlayıcılar</h2>
      <div class="grid">
        <div class="switch-row">
          <div>
            <span>Otomatik Yazıcı Geçişi (Auto-Cycling)</span>
            <small>Yazıcılar arasında sırayla otomatik dolaşır</small>
          </div>
          <input type="checkbox" class="toggle" id="s_autoCycle" onchange="saveLiveSettings()">
        </div>
        <div class="switch-row">
          <div>
            <span>Bitiş / Hata Anında Yazıcıya Odaklan</span>
            <small>Bir baskı bittiğinde veya hata verdiğinde ekran o yazıcıya atlar</small>
          </div>
          <input type="checkbox" class="toggle" id="s_autoJumpAlert" onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_carouselSec">Yazıcı Geçiş Süresi (Saniye)</label>
          <input type="number" id="s_carouselSec" min="3" max="300" value="10" onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_manualTimeoutSec">Manuel Seçimden Otomatiğe Dönüş (Saniye)</label>
          <input type="number" id="s_manualTimeoutSec" min="5" max="3600" value="45" onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_doneAlertSec">"DONE" (Bitti) Yanıp Sönme Süresi (Saniye)</label>
          <input type="number" id="s_doneAlertSec" min="10" max="7200" value="300" onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_screenSleepSec">Boşta Ekran Uyku Süresi (Sn, 0 = Kapanmaz)</label>
          <input type="number" id="s_screenSleepSec" min="0" max="7200" value="300" onchange="saveLiveSettings()">
        </div>
      </div>
    </div>

    <div class="card">
      <h2>🎛️ Döner Düğme (KY-040 Enkoder) &amp; Saat Dilimi</h2>
      <div class="grid">
        <div class="switch-row">
          <div>
            <span>Düğme Yönünü Ters Çevir</span>
            <small>Saat yönünde çevirince geri gidiyorsa açın</small>
          </div>
          <input type="checkbox" class="toggle" id="s_encReverse" onchange="saveLiveSettings()">
        </div>
        <div>
          <label for="s_encEdges">Düğme Hassasiyeti (Tık Başına Adım)</label>
          <select id="s_encEdges" onchange="saveLiveSettings()">
            <option value="4">4 (Standart KY-040)</option>
            <option value="2">2 (Yarım adım - 1 tık atlıyorsa seçin)</option>
            <option value="1">1 (Çok hassas)</option>
          </select>
        </div>
        <div>
          <label for="tzPreset">Saat Dilimi Hazır Seçim (ETA Bitiş Saati İçin)</label>
          <select id="tzPreset" onchange="if(this.value){document.getElementById('s_timezone').value=this.value;saveLiveSettings();}">
            <option value="">-- Özel / Mevcut --</option>
            <option value="<+03>-3">Türkiye (UTC+3)</option>
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">Avrupa / Almanya (CET/CEST)</option>
            <option value="GMT0BST,M3.5.0/1,M10.5.0">İngiltere (GMT/BST)</option>
            <option value="EST5EDT,M3.2.0,M11.1.0">ABD Doğu (EST/EDT)</option>
            <option value="PST8PDT,M3.2.0,M11.1.0">ABD Batı (PST/PDT)</option>
            <option value="UTC0">UTC</option>
          </select>
        </div>
        <div>
          <label for="s_timezone">POSIX Saat Dilimi Kodu</label>
          <input type="text" id="s_timezone" maxlength="39" placeholder="<+03>-3" onchange="saveLiveSettings()">
        </div>
      </div>
      <div class="actions">
        <button type="button" class="btn-primary" onclick="saveLiveSettings(true)">💾 Ekran &amp; Cihaz Ayarlarını Kaydet</button>
        <button type="button" class="btn-danger" onclick="uiCmd({reboot:true})">🔄 Cihazı Yeniden Başlat</button>
      </div>
    </div>
  </div>

  <!-- TAB 4: PRINTERS, WI-FI & BAMBU CLOUD -->
  <div id="tab-network" class="tab-pane">
    <form id="cfgForm" onsubmit="saveNetworkConfig(event)">
      <div class="card">
        <h2>Wi-Fi Ağı (2.4 GHz)</h2>
        <div class="grid">
          <div>
            <label for="wifiSsid">Wi-Fi Adı (SSID)</label>
            <input type="text" id="wifiSsid" maxlength="63" required placeholder="2.4 GHz Wi-Fi adı">
          </div>
          <div>
            <label for="wifiPass">Wi-Fi Şifresi</label>
            <input type="password" id="wifiPass" maxlength="63" placeholder="Değiştirmemek için boş bırakın">
          </div>
        </div>
      </div>

      <div class="card">
        <h2>
          <span>Yazıcı Listesi (<span id="printerCount">0</span>/8)</span>
          <button type="button" id="addBtn" onclick="addPrinter()">+ Yazıcı Ekle</button>
        </h2>
        <p class="hint">Aynı ağdaki yazıcılar için <b>LAN (Doğrudan IP)</b>, başka yerdeki yazıcılar için <b>Bambu Cloud</b> seçin.</p>
        <div id="printerList"></div>
      </div>

      <div class="card">
        <h2>Bambu Cloud Hesabı (İsteğe Bağlı)</h2>
        <p class="hint">Yalnızca yukarıda <b>Bambu Cloud</b> modunda seçili yazıcı varsa gerekir.</p>
        <div class="grid">
          <div>
            <label for="cloudHost">Cloud MQTT Sunucusu</label>
            <select id="cloudHost">
              <option value="us.mqtt.bambulab.com">Global (us.mqtt.bambulab.com)</option>
              <option value="cn.mqtt.bambulab.com">China (cn.mqtt.bambulab.com)</option>
            </select>
          </div>
          <div>
            <label for="cloudUser">Cloud Kullanıcı ID</label>
            <input type="text" id="cloudUser" maxlength="47" placeholder="u_0000000000">
          </div>
        </div>
        <div style="margin-top:12px">
          <label for="cloudToken">Cloud Erişim Anahtarı (JWT Token)</label>
          <input type="password" id="cloudToken" maxlength="1270" placeholder="eyJ... (mevcudu korumak için boş bırakın)">
          <label style="margin-top:6px;display:inline-flex;align-items:center;gap:6px;cursor:pointer">
            <input type="checkbox" id="clearToken" style="width:auto"> Kayıtlı Cloud Token'ı Sil
          </label>
        </div>
      </div>

      <div class="actions">
        <button type="submit" class="btn-primary" id="saveNetBtn">💾 Ağ &amp; Yazıcıları Kaydet ve Yeniden Başlat</button>
        <button type="button" class="btn-danger hidden" id="resetBtn" onclick="resetDefaults()">⚠️ secrets.h Varsayılanlarına Sıfırla</button>
      </div>
    </form>
  </div>
</div>

<script>
let printers = [];
let liveState = { selectedPrinter: 0, view: 0, autoMode: true, sleeping: false, muted: false };

function openTab(id, btn) {
  document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById(id).classList.add('active');
  btn.classList.add('active');
}

function showMsg(text, ok = true) {
  const el = document.getElementById('statusBanner');
  el.textContent = text;
  el.className = ok ? 'status-ok' : 'status-err';
  el.style.display = 'block';
  clearTimeout(window._msgTimer);
  window._msgTimer = setTimeout(() => { el.style.display = 'none'; }, 4000);
}

function esc(s) {
  return (s || '').replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;');
}

function shortFromName(name) {
  return (name || '').replace(/[^A-Za-z0-9]/g, '').toUpperCase().slice(0, 5) || 'PRN';
}

function fmtMin(m) {
  if (m === undefined || m < 0) return '--';
  if (m < 60) return m + 'dk';
  return Math.floor(m / 60) + 's ' + String(m % 60).padStart(2, '0') + 'dk';
}

function toggleCustomBoxes() {
  const fm = document.getElementById('s_finishMelody').value;
  const em = document.getElementById('s_failedMelody').value;
  document.getElementById('customFinishBox').style.opacity = (fm === '5') ? '1' : '0.75';
  document.getElementById('customFailedBox').style.opacity = (em === '4') ? '1' : '0.75';
}

function appendNote(inputId, pair) {
  const el = document.getElementById(inputId);
  const cur = el.value.trim();
  el.value = cur ? (cur + ',' + pair) : pair;
  saveLiveSettings();
}

async function soundCmd(body) {
  try {
    const r = await fetch('/api/sound', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(body)
    });
    const res = await r.json();
    if (body.action === 'mute') fetchStatus();
    if (!res.ok) showMsg(res.error || 'Ses komutu başarısız', false);
  } catch (e) {
    showMsg('Bağlantı hatası: ' + e.message, false);
  }
}

function testSelectedFinish() {
  const preset = parseInt(document.getElementById('s_finishMelody').value, 10);
  if (preset === 5) {
    playCustomBox('s_customFinish');
  } else {
    soundCmd({
      action: 'test',
      sound: 'finish',
      preset,
      passive: document.getElementById('s_buzzerPassive').value === '1',
      activeLow: document.getElementById('s_buzzerActiveLow').value === '1'
    });
  }
}

function testSelectedFailed() {
  const preset = parseInt(document.getElementById('s_failedMelody').value, 10);
  if (preset === 4) {
    playCustomBox('s_customFailed');
  } else {
    soundCmd({
      action: 'test',
      sound: 'failed',
      preset,
      passive: document.getElementById('s_buzzerPassive').value === '1',
      activeLow: document.getElementById('s_buzzerActiveLow').value === '1'
    });
  }
}

function playCustomBox(id) {
  const pattern = document.getElementById(id).value.trim();
  if (!pattern) return;
  saveLiveSettings();
  soundCmd({
    action: 'custom',
    pattern,
    passive: document.getElementById('s_buzzerPassive').value === '1',
    activeLow: document.getElementById('s_buzzerActiveLow').value === '1'
  });
}

async function uiCmd(body) {
  try {
    await fetch('/api/ui', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(body)
    });
    if (body.reboot) {
      showMsg('Cihaz yeniden başlatılıyor...', true);
      return;
    }
    fetchStatus();
  } catch (e) {
    showMsg('Komut gönderilemedi: ' + e.message, false);
  }
}

function sendToast() {
  const el = document.getElementById('toastInput');
  const txt = el.value.trim();
  if (!txt) return;
  uiCmd({ toast: txt });
  showMsg('Mesaj OLED ekranda gösteriliyor: "' + txt + '"', true);
}

async function fetchStatus() {
  try {
    const r = await fetch('/api/status');
    const d = await r.json();
    liveState = d;

    document.getElementById('ipBadge').textContent = 'IP: ' + (d.ip || 'AP');
    document.getElementById('rssiBadge').textContent = d.wifiRssi ? `Wi-Fi: ${d.wifiRssi} dBm` : 'Wi-Fi: AP Modu';
    document.getElementById('heapBadge').textContent = `Boş RAM: ${Math.round((d.freeHeap||0)/1024)} KB`;
    document.getElementById('upBadge').textContent = `Süre: ${fmtMin(Math.floor((d.uptimeSec||0)/60))}`;

    document.getElementById('btnAutoToggle').textContent = 'Auto Geçiş: ' + (d.autoMode ? 'AÇIK' : 'KAPALI');
    document.getElementById('btnAutoToggle').className = 'btn-sm ' + (d.autoMode ? 'btn-active' : '');
    document.getElementById('btnMuteQuick').textContent = 'Ses: ' + (d.muted ? 'SESSİZ 🔇' : 'AÇIK 🔊');
    document.getElementById('btnMuteQuick').className = 'btn-sm ' + (!d.muted ? 'btn-active' : '');
    document.getElementById('btnSleepToggle').textContent = d.sleeping ? 'Ekranı Uyandır ☀️' : 'Ekranı Uyut 🌙';

    for (let v = 0; v < 4; v++) {
      const b = document.getElementById('vBtn' + v);
      if (b) b.className = (d.view === v) ? 'btn-active' : '';
    }

    const grid = document.getElementById('liveGrid');
    const list = d.printers || [];
    if (list.length === 0) {
      grid.innerHTML = '<p class="hint">Henüz yazıcı eklenmemiş.</p>';
      return;
    }
    grid.innerHTML = list.map((p, i) => {
      const pct = p.percent >= 0 ? p.percent : 0;
      const sel = (i === d.selectedPrinter);
      const nTemp = p.nozzle !== null ? `N:${Math.round(p.nozzle)}/${Math.round(p.nozzleTarget||0)}°C` : 'N:--';
      const bTemp = p.bed !== null ? `B:${Math.round(p.bed)}/${Math.round(p.bedTarget||0)}°C` : 'B:--';
      const layer = (p.totalLayers > 0 && p.layer >= 0) ? `Katman ${p.layer}/${p.totalLayers}` : (p.isCloud ? 'Bambu Cloud' : p.ip);
      return `
      <div class="p-card ${sel ? 'selected' : ''}" onclick="uiCmd({selectPrinter:${i}})">
        <div class="p-top">
          <span class="p-name">${esc(p.name)} ${sel ? '👁️' : ''}</span>
          <span class="p-tag tag-${esc(p.tag)}">${esc(p.tag)}</span>
        </div>
        <div class="p-bar"><div class="p-fill" style="width:${pct}%"></div></div>
        <div class="p-meta">
          <span><b>${p.percent >= 0 ? p.percent + '%' : '--'}</b> (${esc(layer)})</span>
          <span>Kalan: <b>${fmtMin(p.remainingMin)}</b></span>
        </div>
        <div class="p-meta">
          <span>${nTemp} &middot; ${bTemp}</span>
          <span>${esc(p.filament || '')} ${p.fanPct >= 0 ? 'Fan:' + p.fanPct + '%' : ''}</span>
        </div>
        ${p.file ? `<div class="p-file">📄 ${esc(p.file)}</div>` : ''}
      </div>`;
    }).join('');
  } catch (e) {}
}

function renderPrinters() {
  const list = document.getElementById('printerList');
  document.getElementById('printerCount').textContent = printers.length;
  document.getElementById('addBtn').disabled = printers.length >= 8;

  if (printers.length === 0) {
    list.innerHTML = '<p class="hint" style="margin:4px 0">Yazıcı yok. "+ Yazıcı Ekle" butonuna tıklayın.</p>';
    return;
  }

  list.innerHTML = printers.map((p, i) => {
    const isCloud = p.mode === 'cloud';
    return `
    <div class="printer-item">
      <div class="printer-head">
        <span style="font-weight:600">#${i + 1} ${esc(p.name || 'Yazıcı')}</span>
        <div class="btn-group">
          <button type="button" class="btn-sm" onclick="movePrinter(${i},-1)" ${i===0?'disabled':''}>&#9650;</button>
          <button type="button" class="btn-sm" onclick="movePrinter(${i},1)" ${i===printers.length-1?'disabled':''}>&#9660;</button>
          <button type="button" class="btn-sm btn-danger" onclick="removePrinter(${i})">Sil</button>
        </div>
      </div>
      <div class="grid">
        <div>
          <label>Yazıcı Adı (Ekranda ~14 kr)</label>
          <input type="text" value="${esc(p.name)}" maxlength="31" required oninput="printers[${i}].name=this.value">
        </div>
        <div>
          <label>Kısa Ad (Overview tablosu, max 5 kr)</label>
          <input type="text" value="${esc(p.shortName)}" maxlength="5" placeholder="${esc(shortFromName(p.name))}" oninput="printers[${i}].shortName=this.value">
        </div>
        <div>
          <label>Seri Numarası (Serial)</label>
          <input type="text" value="${esc(p.serial)}" maxlength="23" required placeholder="03000A000000001" oninput="printers[${i}].serial=this.value.trim()">
        </div>
        <div>
          <label>Bağlantı Modu</label>
          <select onchange="printers[${i}].mode=this.value;renderPrinters()">
            <option value="lan" ${!isCloud?'selected':''}>LAN (Doğrudan IP)</option>
            <option value="cloud" ${isCloud?'selected':''}>Bambu Cloud</option>
          </select>
        </div>
        <div class="${isCloud?'hidden':''}">
          <label>Yazıcı LAN IP Adresi</label>
          <input type="text" value="${esc(p.ip)}" maxlength="31" ${!isCloud?'required':''} placeholder="192.168.1.101" oninput="printers[${i}].ip=this.value.trim()">
        </div>
        <div class="${isCloud?'hidden':''}">
          <label>LAN Erişim Kodu (8 hane)</label>
          <input type="text" value="${esc(p.accessCode)}" maxlength="15" ${!isCloud?'required':''} placeholder="12345678" oninput="printers[${i}].accessCode=this.value.trim()">
        </div>
      </div>
    </div>`;
  }).join('');
}

function addPrinter() {
  if (printers.length >= 8) return;
  const idx = printers.length + 1;
  printers.push({ name: `Printer ${idx}`, shortName: `PRN${idx}`, mode: 'lan', ip: '', serial: '', accessCode: '' });
  renderPrinters();
}
function removePrinter(i) { printers.splice(i, 1); renderPrinters(); }
function movePrinter(i, dir) {
  const j = i + dir;
  if (j < 0 || j >= printers.length) return;
  const t = printers[i]; printers[i] = printers[j]; printers[j] = t;
  renderPrinters();
}

async function loadAllConfig() {
  try {
    const r = await fetch('/api/config');
    const d = await r.json();
    document.getElementById('wifiSsid').value = d.wifiSsid || '';
    document.getElementById('wifiPass').placeholder = d.wifiPasswordSet ? '•••••••• (kayıtlı, korumak için boş bırakın)' : 'Wi-Fi şifresi';
    document.getElementById('cloudHost').value = d.cloudHost || 'us.mqtt.bambulab.com';
    document.getElementById('cloudUser').value = d.cloudUser || '';
    document.getElementById('cloudToken').placeholder = d.cloudTokenSet ? '•••••••• (kayıtlı, korumak için boş bırakın)' : 'eyJ...';
    document.getElementById('resetBtn').classList.toggle('hidden', !d.hasCustomConfig);

    const s = d.settings || {};
    document.getElementById('s_enableSound').checked = !s.muted;
    document.getElementById('s_startupChirp').checked = s.startupChirp !== false;
    document.getElementById('s_buzzerPassive').value = s.buzzerPassive ? '1' : '0';
    document.getElementById('s_buzzerActiveLow').value = s.buzzerActiveLow ? '1' : '0';
    document.getElementById('s_finishMelody').value = String(s.finishMelody ?? 0);
    document.getElementById('s_failedMelody').value = String(s.failedMelody ?? 0);
    document.getElementById('s_customFinish').value = s.customFinish || '';
    document.getElementById('s_customFailed').value = s.customFailed || '';
    toggleCustomBoxes();

    document.getElementById('s_oledContrast').value = s.oledContrast ?? 255;
    document.getElementById('contrastVal').textContent = s.oledContrast ?? 255;
    document.getElementById('s_oledFlip').checked = !!s.oledFlip;
    document.getElementById('s_oledInvert').checked = !!s.oledInvert;
    document.getElementById('s_defaultView').value = String(s.defaultView ?? 0);
    document.getElementById('s_autoCycle').checked = s.autoCycle !== false;
    document.getElementById('s_autoJumpAlert').checked = s.autoJumpAlert !== false;
    document.getElementById('s_carouselSec').value = s.carouselSec ?? 10;
    document.getElementById('s_manualTimeoutSec').value = s.manualTimeoutSec ?? 45;
    document.getElementById('s_doneAlertSec').value = s.doneAlertSec ?? 300;
    document.getElementById('s_screenSleepSec').value = s.screenSleepSec ?? 300;
    document.getElementById('s_encReverse').checked = !!s.encReverse;
    document.getElementById('s_encEdges').value = String(s.encEdges ?? 4);
    document.getElementById('s_timezone').value = s.timezone || '<+03>-3';

    printers = (d.printers || []).map(p => ({
      name: p.name || '',
      shortName: p.shortName || '',
      ip: p.ip || '',
      serial: p.serial || '',
      accessCode: p.accessCode || '',
      mode: p.ip ? 'lan' : 'cloud'
    }));
    renderPrinters();
  } catch (e) {
    showMsg('Ayarlar yüklenemedi: ' + e.message, false);
  }
}

async function saveLiveSettings(notify = false) {
  const payload = {
    muted: !document.getElementById('s_enableSound').checked,
    startupChirp: document.getElementById('s_startupChirp').checked,
    buzzerPassive: document.getElementById('s_buzzerPassive').value === '1',
    buzzerActiveLow: document.getElementById('s_buzzerActiveLow').value === '1',
    finishMelody: parseInt(document.getElementById('s_finishMelody').value, 10),
    failedMelody: parseInt(document.getElementById('s_failedMelody').value, 10),
    customFinish: document.getElementById('s_customFinish').value.trim(),
    customFailed: document.getElementById('s_customFailed').value.trim(),
    oledContrast: parseInt(document.getElementById('s_oledContrast').value, 10),
    oledFlip: document.getElementById('s_oledFlip').checked,
    oledInvert: document.getElementById('s_oledInvert').checked,
    defaultView: parseInt(document.getElementById('s_defaultView').value, 10),
    autoCycle: document.getElementById('s_autoCycle').checked,
    autoJumpAlert: document.getElementById('s_autoJumpAlert').checked,
    carouselSec: parseInt(document.getElementById('s_carouselSec').value, 10) || 10,
    manualTimeoutSec: parseInt(document.getElementById('s_manualTimeoutSec').value, 10) || 45,
    doneAlertSec: parseInt(document.getElementById('s_doneAlertSec').value, 10) || 300,
    screenSleepSec: parseInt(document.getElementById('s_screenSleepSec').value, 10) || 0,
    encReverse: document.getElementById('s_encReverse').checked,
    encEdges: parseInt(document.getElementById('s_encEdges').value, 10) || 4,
    timezone: document.getElementById('s_timezone').value.trim() || '<+03>-3'
  };
  try {
    const r = await fetch('/api/settings', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    });
    const res = await r.json();
    if (res.ok) {
      if (notify) showMsg('Ayarlar anında uygulandı ve hafızaya kaydedildi!', true);
      fetchStatus();
    } else {
      showMsg('Kaydetme hatası: ' + (res.error || ''), false);
    }
  } catch (e) {
    showMsg('Bağlantı hatası: ' + e.message, false);
  }
}

async function saveNetworkConfig(ev) {
  ev.preventDefault();
  const payload = {
    wifiSsid: document.getElementById('wifiSsid').value.trim(),
    wifiPassword: document.getElementById('wifiPass').value,
    cloudHost: document.getElementById('cloudHost').value,
    cloudUser: document.getElementById('cloudUser').value.trim(),
    cloudToken: document.getElementById('cloudToken').value.trim(),
    clearToken: document.getElementById('clearToken').checked,
    printers: printers.map(p => ({
      name: p.name.trim(),
      shortName: (p.shortName.trim() || shortFromName(p.name)).slice(0, 5),
      ip: p.mode === 'cloud' ? '' : p.ip.trim(),
      serial: p.serial.trim(),
      accessCode: p.mode === 'cloud' ? '' : p.accessCode.trim()
    }))
  };
  const btn = document.getElementById('saveNetBtn');
  btn.disabled = true;
  try {
    const r = await fetch('/api/config', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify(payload)
    });
    const res = await r.json();
    if (res.ok) showMsg('Ağ ve yazıcı ayarları kaydedildi! Cihaz yeniden başlatılıyor...', true);
    else { showMsg('Hata: ' + (res.error || ''), false); btn.disabled = false; }
  } catch (e) {
    showMsg('Bağlantı hatası: ' + e.message, false);
    btn.disabled = false;
  }
}

async function resetDefaults() {
  if (!confirm('Web üzerinden yapılan tüm ayarlar silinip secrets.h varsayılanlarına dönülsün mü?')) return;
  try {
    const r = await fetch('/api/reset', { method: 'POST' });
    const res = await r.json();
    if (res.ok) showMsg('Varsayılan ayarlara dönüldü! Cihaz yeniden başlatılıyor...', true);
  } catch (e) {
    showMsg('Sıfırlama hatası: ' + e.message, false);
  }
}

loadAllConfig();
fetchStatus();
setInterval(fetchStatus, 3000);
</script>
</body>
</html>
)rawliteral";

const char* tagForStatus(const PrinterStatus& s) {
  if (s.link == Link::Offline) return "OFF";
  if (s.link == Link::AuthFailed) return "AUTH";
  if (!s.hasData()) return "...";
  if (s.printError != 0 && s.phase != Phase::Failed) return "ERR";
  switch (s.phase) {
    case Phase::Running: return "RUN";
    case Phase::Paused: return "PAUSE";
    case Phase::Prepare: return "PREP";
    case Phase::Finished: return "DONE";
    case Phase::Failed: return "ERR";
    case Phase::Idle: return "IDLE";
    default: return "...";
  }
}

void handleRoot() {
  g_server.send(200, "text/html; charset=utf-8", kIndexHtml);
}

void handleGetStatus() {
  JsonDocument doc;
  doc["ip"] = state::wifiUp() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["wifiRssi"] = state::wifiUp() ? WiFi.RSSI() : 0;
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["uptimeSec"] = millis() / 1000;
  doc["selectedPrinter"] = ui::selectedPrinter();
  doc["view"] = ui::currentView();
  doc["autoMode"] = ui::autoMode();
  doc["sleeping"] = ui::sleeping();
  doc["muted"] = buzzer::muted();
  doc["visiting"] = poller::visiting();
  doc["warmingUp"] = poller::warmingUp();

  JsonArray arr = doc["printers"].to<JsonArray>();
  for (size_t i = 0; i < config::printerCount(); i++) {
    const PrinterConfig& pc = config::printer(i);
    const PrinterStatus s = state::get(i);
    JsonObject o = arr.add<JsonObject>();
    o["name"] = pc.name;
    o["shortName"] = pc.shortName;
    o["ip"] = pc.ip;
    o["isCloud"] = config::isCloud(i);
    o["tag"] = tagForStatus(s);
    o["percent"] = s.percent;
    o["remainingMin"] = s.remainingMin;
    o["elapsedMin"] = s.elapsedMin;
    o["layer"] = s.layer;
    o["totalLayers"] = s.totalLayers;
    if (!isnan(s.nozzle)) o["nozzle"] = s.nozzle;
    else o["nozzle"] = nullptr;
    if (!isnan(s.nozzleTarget)) o["nozzleTarget"] = s.nozzleTarget;
    else o["nozzleTarget"] = nullptr;
    if (!isnan(s.bed)) o["bed"] = s.bed;
    else o["bed"] = nullptr;
    if (!isnan(s.bedTarget)) o["bedTarget"] = s.bedTarget;
    else o["bedTarget"] = nullptr;
    o["fanPct"] = s.partFanPct;
    o["filament"] = s.filament();
    o["file"] = s.file;
    o["wifiDbm"] = s.wifiDbm;
    o["hmsCount"] = s.hmsCount;
  }

  String out;
  serializeJson(doc, out);
  g_server.send(200, "application/json", out);
}

void handleGetConfig() {
  JsonDocument doc;
  doc["wifiSsid"] = config::wifiSsid();
  doc["wifiPasswordSet"] = strlen(config::wifiPassword()) > 0;
  doc["cloudHost"] = config::cloudHost();
  doc["cloudUser"] = config::cloudUser();
  doc["cloudTokenSet"] = strlen(config::cloudToken()) > 0;
  doc["hasCustomConfig"] = config::hasCustomConfig();
  doc["ip"] = state::wifiUp() ? WiFi.localIP().toString() : WiFi.softAPIP().toString();

  const DeviceSettings& ds = config::settings();
  JsonObject st = doc["settings"].to<JsonObject>();
  st["muted"] = ds.muted;
  st["startupChirp"] = ds.startupChirp;
  st["buzzerPassive"] = ds.buzzerPassive;
  st["buzzerActiveLow"] = ds.buzzerActiveLow;
  st["finishMelody"] = ds.finishMelody;
  st["failedMelody"] = ds.failedMelody;
  st["customFinish"] = ds.customFinish;
  st["customFailed"] = ds.customFailed;
  st["oledContrast"] = ds.oledContrast;
  st["oledFlip"] = ds.oledFlip;
  st["oledInvert"] = ds.oledInvert;
  st["defaultView"] = ds.defaultView;
  st["autoCycle"] = ds.autoCycle;
  st["autoJumpAlert"] = ds.autoJumpAlert;
  st["carouselSec"] = ds.carouselSec;
  st["manualTimeoutSec"] = ds.manualTimeoutSec;
  st["doneAlertSec"] = ds.doneAlertSec;
  st["screenSleepSec"] = ds.screenSleepSec;
  st["encReverse"] = ds.encReverse;
  st["encEdges"] = ds.encEdges;
  st["timezone"] = ds.timezone;

  JsonArray arr = doc["printers"].to<JsonArray>();
  for (size_t i = 0; i < config::printerCount(); i++) {
    const PrinterConfig& pc = config::printer(i);
    JsonObject o = arr.add<JsonObject>();
    o["name"] = pc.name;
    o["shortName"] = pc.shortName;
    o["ip"] = pc.ip;
    o["serial"] = pc.serial;
    o["accessCode"] = pc.accessCode;
  }

  String out;
  serializeJson(doc, out);
  g_server.send(200, "application/json", out);
}

void handlePostSettings() {
  if (!g_server.hasArg("plain")) {
    g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"Empty body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, g_server.arg("plain"))) {
    g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  DeviceSettings& ds = config::settings();
  if (doc["muted"].is<bool>()) ds.muted = doc["muted"].as<bool>();
  if (doc["startupChirp"].is<bool>()) ds.startupChirp = doc["startupChirp"].as<bool>();
  if (doc["buzzerPassive"].is<bool>()) ds.buzzerPassive = doc["buzzerPassive"].as<bool>();
  if (doc["buzzerActiveLow"].is<bool>()) ds.buzzerActiveLow = doc["buzzerActiveLow"].as<bool>();
  if (doc["finishMelody"].is<int>()) ds.finishMelody = (uint8_t)constrain(doc["finishMelody"].as<int>(), 0, 5);
  if (doc["failedMelody"].is<int>()) ds.failedMelody = (uint8_t)constrain(doc["failedMelody"].as<int>(), 0, 4);
  if (doc["customFinish"].is<const char*>())
    strlcpy(ds.customFinish, doc["customFinish"].as<const char*>(), sizeof ds.customFinish);
  if (doc["customFailed"].is<const char*>())
    strlcpy(ds.customFailed, doc["customFailed"].as<const char*>(), sizeof ds.customFailed);

  if (doc["oledContrast"].is<int>()) ds.oledContrast = (uint8_t)constrain(doc["oledContrast"].as<int>(), 1, 255);
  if (doc["oledFlip"].is<bool>()) ds.oledFlip = doc["oledFlip"].as<bool>();
  if (doc["oledInvert"].is<bool>()) ds.oledInvert = doc["oledInvert"].as<bool>();
  if (doc["defaultView"].is<int>()) ds.defaultView = (uint8_t)constrain(doc["defaultView"].as<int>(), 0, 3);
  if (doc["autoCycle"].is<bool>()) {
    ds.autoCycle = doc["autoCycle"].as<bool>();
    ui::setAutoMode(ds.autoCycle);
  }
  if (doc["autoJumpAlert"].is<bool>()) ds.autoJumpAlert = doc["autoJumpAlert"].as<bool>();
  if (doc["carouselSec"].is<int>()) ds.carouselSec = (uint16_t)constrain(doc["carouselSec"].as<int>(), 3, 600);
  if (doc["manualTimeoutSec"].is<int>())
    ds.manualTimeoutSec = (uint16_t)constrain(doc["manualTimeoutSec"].as<int>(), 5, 3600);
  if (doc["doneAlertSec"].is<int>()) ds.doneAlertSec = (uint16_t)constrain(doc["doneAlertSec"].as<int>(), 5, 7200);
  if (doc["screenSleepSec"].is<int>())
    ds.screenSleepSec = (uint16_t)constrain(doc["screenSleepSec"].as<int>(), 0, 7200);

  if (doc["encReverse"].is<bool>()) ds.encReverse = doc["encReverse"].as<bool>();
  if (doc["encEdges"].is<int>()) ds.encEdges = (uint8_t)constrain(doc["encEdges"].as<int>(), 1, 8);
  if (doc["timezone"].is<const char*>()) {
    strlcpy(ds.timezone, doc["timezone"].as<const char*>(), sizeof ds.timezone);
    configTzTime(ds.timezone, "pool.ntp.org", "time.cloudflare.com");
  }

  if (ds.muted) buzzer::stop();
  ui::applyDisplaySettings();
  config::saveSettings();
  g_server.send(200, "application/json", "{\"ok\":true}");
}

void handlePostSound() {
  if (!g_server.hasArg("plain")) {
    g_server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, g_server.arg("plain"))) {
    g_server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  DeviceSettings& ds = config::settings();
  if (doc["passive"].is<bool>()) ds.buzzerPassive = doc["passive"].as<bool>();
  if (doc["activeLow"].is<bool>()) ds.buzzerActiveLow = doc["activeLow"].as<bool>();

  const char* action = doc["action"] | "";
  if (!strcmp(action, "stop")) {
    buzzer::stop();
  } else if (!strcmp(action, "mute")) {
    const bool m = doc["muted"] | !buzzer::muted();
    buzzer::setMuted(m);
    ui::toast(m ? "Sound: OFF" : "Sound: ON");
  } else if (!strcmp(action, "custom")) {
    const char* pat = doc["pattern"] | "";
    buzzer::playCustom(pat);
  } else if (!strcmp(action, "test")) {
    const char* snd = doc["sound"] | "chirp";
    const int preset = doc["preset"] | -1;
    if (!strcmp(snd, "finish")) buzzer::test(buzzer::Sound::Finished, preset);
    else if (!strcmp(snd, "failed")) buzzer::test(buzzer::Sound::Failed, preset);
    else buzzer::test(buzzer::Sound::Chirp, -1);
  }
  g_server.send(200, "application/json", "{\"ok\":true}");
}

void handlePostUi() {
  if (!g_server.hasArg("plain")) {
    g_server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, g_server.arg("plain"))) {
    g_server.send(400, "application/json", "{\"ok\":false}");
    return;
  }

  if (doc["selectPrinter"].is<int>()) ui::selectPrinter(doc["selectPrinter"].as<int>());
  if (doc["view"].is<int>()) ui::setView((uint8_t)doc["view"].as<int>());
  if (doc["autoMode"].is<bool>()) {
    ui::setAutoMode(doc["autoMode"].as<bool>());
    config::saveSettings();
  }
  if (doc["sleep"].is<bool>()) {
    if (doc["sleep"].as<bool>()) ui::sleepNow();
    else ui::wake();
  }
  if (doc["toast"].is<const char*>()) ui::toast(doc["toast"].as<const char*>());
  if (doc["reboot"] | false) g_restartAt = millis() + 400;

  g_server.send(200, "application/json", "{\"ok\":true}");
}

void handlePostConfig() {
  if (!g_server.hasArg("plain")) {
    g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"Empty body\"}");
    return;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, g_server.arg("plain"));
  if (err) {
    g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const char* ssid = doc["wifiSsid"] | "";
  if (!*ssid) {
    g_server.send(400, "application/json", "{\"ok\":false,\"error\":\"Wi-Fi SSID is required\"}");
    return;
  }

  const char* newPass = doc["wifiPassword"] | "";
  const char* pass = (*newPass) ? newPass : config::wifiPassword();

  const char* cHost = doc["cloudHost"] | "us.mqtt.bambulab.com";
  const char* cUser = doc["cloudUser"] | "";
  const bool clearToken = doc["clearToken"] | false;
  const char* newToken = doc["cloudToken"] | "";
  const char* cToken = clearToken ? "" : ((*newToken) ? newToken : config::cloudToken());

  PrinterConfig printers[MAX_PRINTERS] = {};
  size_t count = 0;
  if (doc["printers"].is<JsonArrayConst>()) {
    for (JsonObjectConst p : doc["printers"].as<JsonArrayConst>()) {
      if (count >= MAX_PRINTERS) break;
      const char* name = p["name"] | "";
      const char* sn = p["serial"] | "";
      if (!*name || !*sn) continue;
      strlcpy(printers[count].name, name, sizeof printers[count].name);
      strlcpy(printers[count].shortName, p["shortName"] | "PRN", sizeof printers[count].shortName);
      strlcpy(printers[count].ip, p["ip"] | "", sizeof printers[count].ip);
      strlcpy(printers[count].serial, sn, sizeof printers[count].serial);
      strlcpy(printers[count].accessCode, p["accessCode"] | "", sizeof printers[count].accessCode);
      count++;
    }
  }

  if (!config::save(ssid, pass, cHost, cUser, cToken, printers, count)) {
    g_server.send(500, "application/json", "{\"ok\":false,\"error\":\"NVS write failed\"}");
    return;
  }

  Serial.printf("[web] saved config (%u printers), rebooting...\n", (unsigned)count);
  g_server.send(200, "application/json", "{\"ok\":true}");
  g_restartAt = millis() + 600;
}

void handlePostReset() {
  config::resetToDefaults();
  Serial.println("[web] restored secrets.h defaults, rebooting...");
  g_server.send(200, "application/json", "{\"ok\":true}");
  g_restartAt = millis() + 600;
}
#endif

}  // namespace

namespace web {

void begin() {
#if !DEMO_MODE
  g_server.on("/", HTTP_GET, handleRoot);
  g_server.on("/api/status", HTTP_GET, handleGetStatus);
  g_server.on("/api/config", HTTP_GET, handleGetConfig);
  g_server.on("/api/settings", HTTP_POST, handlePostSettings);
  g_server.on("/api/sound", HTTP_POST, handlePostSound);
  g_server.on("/api/ui", HTTP_POST, handlePostUi);
  g_server.on("/api/config", HTTP_POST, handlePostConfig);
  g_server.on("/api/reset", HTTP_POST, handlePostReset);
  g_server.onNotFound([]() {
    if (g_apActive) {
      g_server.sendHeader("Location", "http://192.168.4.1/", true);
      g_server.send(302, "text/plain", "");
    } else {
      g_server.send(404, "text/plain", "Not found");
    }
  });
  g_server.begin();
  Serial.println("[web] HTTP config server listening on port 80");
#endif
}

void handle() {
#if !DEMO_MODE
  g_server.handleClient();
  if (g_restartAt && (int32_t)(millis() - g_restartAt) >= 0) {
    delay(50);
    ESP.restart();
  }
#endif
}

void setApActive(bool active) { g_apActive = active; }
bool apActive() { return g_apActive; }

}  // namespace web
