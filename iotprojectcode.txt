#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <RTClib.h>

/* ---------- WIFI ---------- */
const char* ssid = "realme 11x 5G";
const char* password = "lubhika22";

/* ---------- PINS ---------- */
#define RELAY_BULB   32   // ACTIVE LOW
#define LED_KITCHEN  25
#define LED_BEDROOM  26
#define LED_CORRIDOR 27
#define LED_CHARGER  14
#define UNATTENDED_LIMIT 0  // 2 hours


/* ---------- OBJECTS ---------- */
WebServer server(80);
RTC_DS3231 rtc;


/* ---------- STATES ---------- */
struct DeviceState {
  bool on;
  bool autoOffActive;
  String lastEvent;

   // 🔽 NEW (ADD)
  DateTime turnedOnAt;
  unsigned long onSeconds;
  unsigned long autoOffLimit; // seconds

};
DeviceState bulb     = {false,false,"init",DateTime((uint32_t)0),0, 60};
DeviceState kitchen  = {false,false,"init",DateTime((uint32_t)0),0,120};
DeviceState bedroom  = {false,false,"init",DateTime((uint32_t)0),0,180};
DeviceState corridor = {false,false,"init",DateTime((uint32_t)0),0,240};
DeviceState charger  = {false,false,"init",DateTime((uint32_t)0),0,300};



/* ---------- TIME RULE ---------- */
int nightHour = 21; // 9 PM

/* ---------- HELPERS ---------- */
void applyHardware() {
  digitalWrite(RELAY_BULB, bulb.on ? LOW : HIGH);
  digitalWrite(LED_KITCHEN, kitchen.on);
  digitalWrite(LED_BEDROOM, bedroom.on);
  digitalWrite(LED_CORRIDOR, corridor.on);
  digitalWrite(LED_CHARGER, charger.on);
}

void checkNightRule() {
  DateTime now = rtc.now();
  if (now.hour() >= nightHour && bulb.on && !bulb.autoOffActive) {
    bulb.onSeconds += now.unixtime() - bulb.turnedOnAt.unixtime();
    bulb.on = false;
    bulb.autoOffActive = true;
    bulb.lastEvent = "auto_off_night_rule";
    bulb.turnedOnAt = DateTime(0UL);
    applyHardware();
  }
}

/* ---------- WEB PAGE (FRONTEND) ---------- */
const char webpage[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
<html lang="en">
  <head>
    <!--
      Adaptive Power Safeguarding System — Web Dashboard
      Frontend-only (HTML/CSS/JS). Designed for hosting from an ESP32 web server.
      Purpose:
        - Show real-time ON/OFF status of multiple appliances
        - Provide manual ON/OFF control via simple REST-style endpoints
        - Visually indicate automatic OFF rules and recent events
      Endpoints (examples expected on ESP32):
        - GET /status                      -> JSON with device states and rule flags
        - GET /device/<slug>/on            -> switch device ON (manual override)
        - GET /device/<slug>/off           -> switch device OFF (manual override)
      Slugs used in this dashboard:
        bulb, kitchen, bedroom, corridor, charger
    -->
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>Adaptive Power Safeguarding System</title>
    <!--
      Design Notes:
        - Professional academic IoT look (light/dark theme switch)
        - Mobile-first responsive layout using CSS grid
        - Smooth status color transitions, clean cards, inline SVG icons
    -->
    <style>
      /* =========================
         THEME & BASE STYLES
         ========================= */
      :root {
        --bg: #0f1220;
        --panel: #171a2b;
        --text: #e8eaf4;
        --muted: #9aa3b2;
        --accent: #4da3ff;
        --accent-2: #9b8cff;
        --success: #27c281;
        --danger: #ff4d4f;
        --off: #7b8190;
        --card: #1e2236;
        --card-border: #2a2f47;
        --badge: #26314a;
        --shadow: 0 12px 28px rgba(0,0,0,0.35);
      }
      .light {
        --bg: #f3f6fb;
        --panel: #ffffff;
        --text: #1f2430;
        --muted: #64748b;
        --accent: #2563eb;
        --accent-2: #7c3aed;
        --success: #16a34a;
        --danger: #dc2626;
        --off: #a3a9b7;
        --card: #ffffff;
        --card-border: #e5e7eb;
        --badge: #eef2ff;
        --shadow: 0 10px 24px rgba(31,41,55,0.12);
      }

      html, body {
        height: 100%;
        margin: 0;
      }
      body {
        font-family: Inter, system-ui, -apple-system, Segoe UI, Roboto, "Helvetica Neue", Arial, "Noto Sans", "Liberation Sans", sans-serif;
        background: var(--bg);
        color: var(--text);
        display: flex;
        flex-direction: column;
        min-height: 100%;
      }

      /* =========================
         HEADER / TOP BAR
         ========================= */
      .topbar {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 12px;
        padding: 18px 20px;
        background: var(--panel);
        box-shadow: var(--shadow);
        position: sticky;
        top: 0;
        z-index: 10;
      }
      .brand {
        display: flex;
        align-items: center;
        gap: 14px;
      }
      .brand-logo {
        width: 36px;
        height: 36px;
        border-radius: 8px;
        background: linear-gradient(135deg, var(--accent), var(--accent-2));
        display: grid;
        place-items: center;
        box-shadow: 0 6px 16px rgba(0,0,0,0.25);
      }
      .brand h1 {
        font-size: 18px;
        margin: 0;
        line-height: 1.2;
        letter-spacing: 0.2px;
      }
      .brand h1 small {
        display: block;
        font-size: 12px;
        color: var(--muted);
        font-weight: 500;
      }

      .controls {
        display: flex;
        align-items: center;
        gap: 10px;
      }
      .pill {
        display: inline-flex;
        align-items: center;
        gap: 8px;
        padding: 8px 12px;
        border-radius: 999px;
        background: var(--badge);
        color: var(--text);
        font-size: 12px;
        border: 1px solid var(--card-border);
      }
      .indicator-dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background: var(--off);
        box-shadow: 0 0 0 2px rgba(0,0,0,0.08) inset;
        transition: background 200ms ease;
      }
      .indicator-dot.online {
        background: var(--success);
      }
      .indicator-dot.offline {
        background: var(--danger);
      }
      .theme-toggle {
        border: 1px solid var(--card-border);
        background: var(--badge);
        color: var(--text);
        padding: 8px 12px;
        border-radius: 10px;
        font-size: 12px;
        cursor: pointer;
      }
      .theme-toggle:active { transform: translateY(1px); }

      /* =========================
         LAYOUT
         ========================= */
      .wrap {
        width: 100%;
        max-width: 1100px;
        margin: 0 auto;
        padding: 22px;
        display: grid;
        gap: 18px;
      }
      .grid {
        display: grid;
        grid-template-columns: repeat(2, minmax(0, 1fr));
        gap: 16px;
      }
      @media (min-width: 900px) {
        .grid {
          grid-template-columns: repeat(3, minmax(0, 1fr));
        }
      }
      @media (max-width: 640px) {
        .grid {
          grid-template-columns: 1fr;
        }
      }

      /* =========================
         DEVICE CARD
         ========================= */
      .card {
        background: var(--card);
        border: 1px solid var(--card-border);
        border-radius: 14px;
        padding: 14px;
        box-shadow: var(--shadow);
        display: grid;
        grid-template-rows: auto auto auto;
        gap: 12px;
        transition: border-color 200ms ease, box-shadow 200ms ease;
      }
      .card:hover {
        border-color: var(--accent);
        box-shadow: 0 16px 32px rgba(0,0,0,0.25);
      }

      .card-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 8px;
      }

      .ident {
        display: flex;
        align-items: center;
        gap: 10px;
        min-width: 0;
      }
      .icon {
        width: 36px;
        height: 36px;
        border-radius: 10px;
        background: linear-gradient(135deg, #22273c, #2c3250);
        display: grid;
        place-items: center;
        border: 1px solid var(--card-border);
      }
      .names { min-width: 0; }
      .device-name {
        font-size: 14px;
        font-weight: 600;
        margin: 0;
        line-height: 1.2;
      }
      .room-name {
        font-size: 12px;
        color: var(--muted);
        margin: 2px 0 0;
      }

      .status-line {
        display: flex;
        align-items: center;
        gap: 8px;
      }
      .status-pill {
        display: inline-flex;
        align-items: center;
        gap: 8px;
        padding: 6px 10px;
        border-radius: 999px;
        font-size: 12px;
        border: 1px solid var(--card-border);
        transition: color 200ms ease, background 200ms ease, border-color 200ms ease;
      }
      .status-pill .dot {
        width: 10px;
        height: 10px;
        border-radius: 50%;
        background: var(--off);
        transition: background 200ms ease, transform 200ms ease;
      }
      .status-pill.on {
        color: var(--success);
        background: color-mix(in oklab, var(--success) 12%, transparent);
        border-color: color-mix(in oklab, var(--success) 35%, var(--card-border));
      }
      .status-pill.on .dot {
        background: var(--success);
        transform: scale(1.05);
      }
      .status-pill.off {
        color: var(--off);
        background: color-mix(in oklab, var(--off) 10%, transparent);
        border-color: color-mix(in oklab, var(--off) 30%, var(--card-border));
      }
      .badge {
        padding: 4px 8px;
        font-size: 11px;
        border-radius: 999px;
        background: var(--badge);
        border: 1px solid var(--card-border);
        color: var(--muted);
      }
      .badge.success {
        color: var(--success);
        border-color: color-mix(in oklab, var(--success) 40%, var(--card-border));
      }
      .badge.warn {
        color: var(--danger);
        border-color: color-mix(in oklab, var(--danger) 40%, var(--card-border));
      }

      /* =========================
         CONTROLS
         ========================= */
      .controls-row {
        display: flex;
        align-items: center;
        gap: 10px;
      }
      .switch {
        position: relative;
        width: 52px;
        height: 28px;
        border-radius: 999px;
        background: #3a3f58;
        border: 1px solid var(--card-border);
        cursor: pointer;
        transition: background 220ms ease, border-color 220ms ease;
      }
      .switch .knob {
        position: absolute;
        top: 50%;
        left: 4px;
        width: 22px;
        height: 22px;
        border-radius: 50%;
        transform: translateY(-50%);
        background: #fff;
        box-shadow: 0 4px 14px rgba(0,0,0,0.25);
        transition: left 200ms ease, background 200ms ease;
      }
      .switch.on {
        background: color-mix(in oklab, var(--success) 35%, transparent);
        border-color: color-mix(in oklab, var(--success) 50%, var(--card-border));
      }
      .switch.on .knob {
        left: 26px;
        background: #eafff5;
      }
      .btn {
        padding: 8px 12px;
        border-radius: 10px;
        border: 1px solid var(--card-border);
        background: var(--badge);
        color: var(--text);
        font-size: 12px;
        cursor: pointer;
        transition: transform 80ms ease, border-color 200ms ease, background 200ms ease;
      }
      .btn:hover {
        border-color: var(--accent);
      }
      .btn:active {
        transform: translateY(1px);
      }

      /* =========================
         MESSAGE FEED
         ========================= */
      .feed {
        background: var(--panel);
        border: 1px solid var(--card-border);
        border-radius: 12px;
        padding: 12px;
        box-shadow: var(--shadow);
      }
      .feed h3 {
        margin: 0 0 8px 0;
        font-size: 14px;
        color: var(--muted);
        font-weight: 600;
      }
      .msg {
        display: flex;
        align-items: center;
        gap: 8px;
        padding: 8px 10px;
        border-radius: 10px;
        border: 1px solid var(--card-border);
        background: var(--badge);
        margin-bottom: 8px;
        font-size: 12px;
      }
      .msg .dot {
        width: 8px;
        height: 8px;
        border-radius: 50%;
        background: var(--off);
      }
      .msg.success .dot { background: var(--success); }
      .msg.warn .dot { background: var(--danger); }
      .msg time {
        margin-left: auto;
        color: var(--muted);
        font-size: 11px;
      }
    </style>
  </head>
  <body class="light">
    <!-- =========================
         TOP BAR
         ========================= -->
    <header class="topbar">
      <div class="brand">
        <div class="brand-logo" aria-hidden="true">
          <!-- Simple inline SVG logo -->
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" aria-hidden="true">
            <path d="M12 2v4M12 18v4M4.93 4.93l2.83 2.83M16.24 16.24l2.83 2.83M2 12h4M18 12h4M4.93 19.07l2.83-2.83M16.24 7.76l2.83-2.83"
                  stroke="#fff" stroke-width="2" stroke-linecap="round"/>
          </svg>
        </div>
        <h1>
          Adaptive Power Safeguarding System
          <small>IoT-based Home Energy Safety Dashboard</small>
        </h1>
      </div>
      <div class="controls">
        <div class="pill" id="net-pill">
          <span class="indicator-dot offline" id="net-dot"></span>
          <span id="net-text">Disconnected</span>
        </div>
        <button class="theme-toggle" id="themeToggle" aria-label="Toggle theme">Switch Theme</button>
      </div>
    </header>

    <main class="wrap">
      <!-- =========================
           APPLIANCE GRID
           ========================= -->
      <section class="grid" id="deviceGrid">
        <!-- Cards injected by JavaScript based on device definitions -->
      </section>

      <!-- =========================
           MESSAGE FEED
           ========================= -->
      <section class="feed" aria-live="polite" aria-atomic="false">
        <h3>Status & Feedback</h3>
        <div id="feed"></div>
      </section>
    </main>
    

    <script>
      /* =========================
         DEVICE DEFINITIONS
         =========================
         Each device has:
           - slug (used in endpoint path)
           - deviceName, roomName
           - icon (inline SVG generator)
      */
      

      const DEVICES = [
        {
          slug: 'bulb',
          deviceName: 'Living Room Bulb',
          roomName: 'Living Room',
          icon: () => `
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
              <path d="M9 18h6M10 22h4M12 2a7 7 0 0 0-7 7c0 2.8 1.65 4.66 3.06 6.1.53.54.94 1.16 1.18 1.86h7.52c.24-.7.65-1.32 1.18-1.86C17.35 13.66 19 11.8 19 9a7 7 0 0 0-7-7Z"
                    stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
            </svg>
          `
        },
        {
          slug: 'kitchen',
          deviceName: 'Kitchen Appliance',
          roomName: 'Kitchen',
          icon: () => `
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
              <rect x="4" y="6" width="16" height="10" rx="2" stroke="currentColor" stroke-width="2"/>
              <circle cx="9" cy="11" r="1" fill="currentColor"/>
              <circle cx="15" cy="11" r="1" fill="currentColor"/>
            </svg>
          `
        },
        {
          slug: 'bedroom',
          deviceName: 'Bedroom Appliance',
          roomName: 'Bedroom',
          icon: () => `
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
              <path d="M6 12h12M6 8h12M7 16h10" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
              <rect x="3" y="6" width="18" height="12" rx="2" stroke="currentColor" stroke-width="2"/>
            </svg>
          `
        },
        {
          slug: 'corridor',
          deviceName: 'Corridor / Staircase Light',
          roomName: 'Corridor',
          icon: () => `
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
              <path d="M12 3v6M8 9h8M10 15h4M9 21h6" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
              <rect x="6" y="9" width="12" height="6" rx="2" stroke="currentColor" stroke-width="2"/>
            </svg>
          `
        },
        {
          slug: 'charger',
          deviceName: 'Charger / Iron Box',
          roomName: 'Utility',
          icon: () => `
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none">
              <rect x="4" y="5" width="12" height="10" rx="2" stroke="currentColor" stroke-width="2"/>
              <path d="M16 9h3a1 1 0 0 1 1 1v4" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
              <path d="M6 21h12" stroke="currentColor" stroke-width="2" stroke-linecap="round"/>
            </svg>
          `
        },
      ];

      /* =========================
         STATE
         =========================
         - deviceStates maps slug -> { on, autoOffActive, lastEvent }
         - manualOverrideUntil maps slug -> timestamp (ms) to show temporary override badge
      */
      const deviceStates = {};
      const manualOverrideUntil = {};
      const turnedOnAt = {};

      let netOnline = false;
      let statusPoller = null;

      /* =========================
         DOM HELPERS
         ========================= */
      const $grid = document.getElementById('deviceGrid');
      const $feed = document.getElementById('feed');
      const $netDot = document.getElementById('net-dot');
      const $netText = document.getElementById('net-text');
      const $themeToggle = document.getElementById('themeToggle');

      function formatTime(d = new Date()) {
        return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
      }
      function addMessage(text, type = 'info') {
        const $m = document.createElement('div');
        $m.className = 'msg' + (type === 'success' ? ' success' : type === 'warn' ? ' warn' : '');
        $m.innerHTML = `
          <span class="dot"></span>
          <span>${text}</span>
          <time>${formatTime()}</time>
        `;
        $feed.prepend($m);
        // Limit feed size
        while ($feed.children.length > 8) $feed.removeChild($feed.lastChild);
      }
      function setConnectivity(online) {
        netOnline = online;
        $netDot.classList.remove('online', 'offline');
        $netDot.classList.add(online ? 'online' : 'offline');

        $netText.textContent = online ? 'Connected' : 'Disconnected';
      }

      /* =========================
         CARD RENDERING
         ========================= */
      function renderCards() {
        const frag = document.createDocumentFragment();
        DEVICES.forEach(dev => {
          const $card = document.createElement('article');
          $card.className = 'card';
          $card.id = `card-${dev.slug}`;

          // Header (icon + names + status + badges)
          const $header = document.createElement('div');
          $header.className = 'card-header';

          const $ident = document.createElement('div');
          $ident.className = 'ident';
          const $icon = document.createElement('div');
          $icon.className = 'icon';
          $icon.innerHTML = dev.icon();
          const $names = document.createElement('div');
          $names.className = 'names';
          $names.innerHTML = `
            <p class="device-name">${dev.deviceName}</p>
            <p class="room-name">${dev.roomName}</p>
          `;
          $ident.appendChild($icon);
          $ident.appendChild($names);

          const $statusLine = document.createElement('div');
          $statusLine.className = 'status-line';
          const $status = document.createElement('span');
          $status.className = 'status-pill off';
          $status.id = `status-${dev.slug}`;
          $status.innerHTML = `<span class="dot"></span><span class="label">OFF</span>`;

          const $badgeAuto = document.createElement('span');
          $badgeAuto.className = 'badge';
          $badgeAuto.id = `badge-auto-${dev.slug}`;
          $badgeAuto.textContent = 'Auto-OFF Active';
          $badgeAuto.style.display = 'none';

          const $badgeOverride = document.createElement('span');
          $badgeOverride.className = 'badge success';
          $badgeOverride.id = `badge-override-${dev.slug}`;
          $badgeOverride.textContent = 'Manual Override';
          $badgeOverride.style.display = 'none';

          $statusLine.appendChild($status);
          $statusLine.appendChild($badgeAuto);
          $statusLine.appendChild($badgeOverride);

          $header.appendChild($ident);
          $header.appendChild($statusLine);

          // Controls row (toggle + buttons)
          const $controls = document.createElement('div');
          $controls.className = 'controls-row';

          const $switch = document.createElement('div');
          $switch.className = 'switch';
          $switch.id = `switch-${dev.slug}`;
          $switch.setAttribute('role', 'switch');
          $switch.setAttribute('aria-checked', 'false');
          $switch.innerHTML = `<div class="knob"></div>`;
          $switch.addEventListener('click', () => {
            const isOn = $switch.classList.contains('on');
            controlDevice(dev.slug, isOn ? 'off' : 'on');
          });

          const $btnOn = document.createElement('button');
          $btnOn.className = 'btn';
          $btnOn.textContent = 'ON';
          $btnOn.addEventListener('click', () => controlDevice(dev.slug, 'on'));

          const $btnOff = document.createElement('button');
          $btnOff.className = 'btn';
          $btnOff.textContent = 'OFF';
          $btnOff.addEventListener('click', () => controlDevice(dev.slug, 'off'));

          $controls.appendChild($switch);
          $controls.appendChild($btnOn);
          $controls.appendChild($btnOff);

          // Assemble card
          $card.appendChild($header);
          $card.appendChild($controls);
          // Timer display
          const $timer = document.createElement('div');
          $timer.className = 'room-name';
          $timer.id = `timer-${dev.slug}`;
          $timer.textContent = 'ON Time: 00:00:00';

          $card.appendChild($timer);

          frag.appendChild($card);
        });
        $grid.innerHTML = '';
        $grid.appendChild(frag);
      }

      /* =========================
         STATUS APPLY / UPDATE
         ========================= */
      function applyStatus(slug, s) {
        const { on, autoOffActive, lastEvent, onSeconds } = s;

        deviceStates[slug] = { on, autoOffActive, lastEvent };
        // Status pill
        const $status = document.getElementById(`status-${slug}`);
        if ($status) {
          $status.classList.toggle('on', !!on);
          $status.classList.toggle('off', !on);
          $status.querySelector('.label').textContent = on ? 'ON' : 'OFF';
        }
        // Switch position
        const $switch = document.getElementById(`switch-${slug}`);
        if ($switch) {
          $switch.classList.toggle('on', !!on);
          $switch.setAttribute('aria-checked', on ? 'true' : 'false');
        }
        // Auto-OFF badge
        const $badgeAuto = document.getElementById(`badge-auto-${slug}`);
        if ($badgeAuto) {
          $badgeAuto.style.display = autoOffActive ? 'inline-flex' : 'none';
        }
        // Manual override badge (timed)
        const $badgeOverride = document.getElementById(`badge-override-${slug}`);
        if ($badgeOverride) {
          const now = Date.now();
          const showOverride = manualOverrideUntil[slug] && manualOverrideUntil[slug] > now;
          $badgeOverride.style.display = showOverride ? 'inline-flex' : 'none';
        }
        // Update ON timer
        
        // Timer display
        const $timer = document.getElementById(`timer-${slug}`);
        if ($timer && typeof s.onSeconds === 'number') {
        const sec = s.onSeconds;
        const h = String(Math.floor(sec / 3600)).padStart(2,'0');
        const m = String(Math.floor((sec % 3600) / 60)).padStart(2,'0');
        const ss = String(sec % 60).padStart(2,'0');
        $timer.textContent = `ON Time: ${h}:${m}:${ss}`;
        }


      }

      /* =========================
         CONTROL ACTIONS
         =========================
         - Sends GET requests to /device/<slug>/<on|off>
         - Shows feedback messages and temporary manual override badge
      */
      async function controlDevice(slug, action) {
        try {
          const res = await fetch(`/device/${slug}/${action}`, { method: 'GET', cache: 'no-store' });
          if (!res.ok) throw new Error('Request failed');
          // Reflect expected state immediately for responsive UX
          const isOn = action === 'on';
          manualOverrideUntil[slug] = Date.now() + 60_000; // show override for 60s
          applyStatus(slug, {
            on: isOn,
            autoOffActive: deviceStates[slug]?.autoOffActive ?? false,
            lastEvent: isOn ? 'manual_on' : 'manual_off'
          });
          addMessage(`Device manually switched ${isOn ? 'ON' : 'OFF'}`, 'success');
        } catch (e) {
          addMessage(`Control failed for ${slug.toUpperCase()} (${action})`, 'warn');
        }
      }

      /* =========================
         POLLING /status
         =========================
         Expected JSON (example):
         {
           "devices": {
             "bulb":    {"on":true,  "autoOffActive":true,  "lastEvent":"auto_off_night_rule"},
             "kitchen": {"on":false, "autoOffActive":false, "lastEvent":"manual_off"},
             ...
           }
         }
         - Updates UI and connectivity indicator
         - Emits feedback when automatic OFF occurs
      */
      async function pollStatusOnce() {
        

        try {
          const res = await fetch('/status', { cache: 'no-store' });
          if (!res.ok) throw new Error('Bad status');
          const data = await res.json();
          setConnectivity(true);
          const devs = data?.devices || {};

          DEVICES.forEach(d => {
            const s = devs[d.slug];
            if (!s) return; // if missing, keep previous
            const prev = deviceStates[d.slug];

            applyStatus(d.slug, s);

            if (!prev || prev.lastEvent !== s.lastEvent) {
              if (s.lastEvent.startsWith("auto_off")) {
                addMessage(`${d.deviceName} automatically switched OFF`, 'warn');
              } else if (s.lastEvent === "manual_on") {
                addMessage(`${d.deviceName} manually switched ON`, 'success');
              } else if (s.lastEvent === "manual_off") {
                addMessage(`${d.deviceName} manually switched OFF`, 'success');
              }


            } else {
              // Initial snapshot message (optional, muted)
            }
          });
        } catch (e) {
          setConnectivity(false);
        }
      }
      function startPolling(){
        if (statusPoller) clearInterval(statusPoller);
        pollStatusOnce();
        statusPoller = setInterval(pollStatusOnce, 2000);
      }
      setTimeout(startPolling, 3000);

      /* =========================
         THEME TOGGLE
         ========================= */
      $themeToggle.addEventListener('click', () => {
        const isLight = document.body.classList.contains('light');
        document.body.classList.toggle('light', !isLight);
      });

      /* =========================
         INIT
         ========================= */
      renderCards();
      startPolling();
    </script>
  </body>
</html>


  )rawliteral";

/* ---------- ROUTES ---------- */
void handleRoot() {
  server.send(200, "text/html", webpage); // your HTML stays here
}

void handleStatus() {
  unsigned long now = rtc.now().unixtime();

  String json = "{ \"devices\": {";

  json += "\"bulb\":{";
  json += "\"on\":" + String(bulb.on ? "true" : "false") + ",";
  json += "\"autoOffActive\":" + String(bulb.autoOffActive ? "true" : "false") + ",";
  json += "\"lastEvent\":\"" + bulb.lastEvent + "\",";
  //json += "\"onSeconds\":" + String(bulb.onSeconds) ;
  unsigned long bulbOnTime =
  bulb.on
    ? bulb.onSeconds + (now - bulb.turnedOnAt.unixtime())
    : bulb.onSeconds;
  json += "\"onSeconds\":" + String(bulbOnTime);
  json += "},";

  json += "\"kitchen\":{";
  json += "\"on\":" + String(kitchen.on ? "true" : "false") + ",";
  json += "\"autoOffActive\":" + String(kitchen.autoOffActive ? "true" : "false") + ",";
  json += "\"lastEvent\":\"" + kitchen.lastEvent + "\",";
  //json += "\"onSeconds\":" + String(kitchen.onSeconds) ;
  unsigned long kitchenOnTime =
  kitchen.on
    ? kitchen.onSeconds + (now - kitchen.turnedOnAt.unixtime())
    : kitchen.onSeconds;
  json += "\"onSeconds\":" + String(kitchenOnTime);
  json += "},";

  json += "\"bedroom\":{";
  json += "\"on\":" + String(bedroom.on ? "true" : "false") + ",";
  json += "\"autoOffActive\":" + String(bedroom.autoOffActive ? "true" : "false") + ",";
  json += "\"lastEvent\":\"" + bedroom.lastEvent + "\",";
  //json += "\"onSeconds\":" + String(bedroom.onSeconds) ;
  unsigned long bedroomOnTime =
  bedroom.on
    ? bedroom.onSeconds + (now - bedroom.turnedOnAt.unixtime())
    : bedroom.onSeconds;
  json += "\"onSeconds\":" + String(bedroomOnTime);
  json += "},";

  json += "\"corridor\":{";
  json += "\"on\":" + String(corridor.on ? "true" : "false") + ",";
  json += "\"autoOffActive\":" + String(corridor.autoOffActive ? "true" : "false") + ",";
  json += "\"lastEvent\":\"" + corridor.lastEvent + "\",";
  //json += "\"onSeconds\":" + String(corridor.onSeconds) ;
  unsigned long corridorOnTime =
  corridor.on
    ? corridor.onSeconds + (now - corridor.turnedOnAt.unixtime())
    : corridor.onSeconds;
  json += "\"onSeconds\":" + String(corridorOnTime);
  json += "},";

  json += "\"charger\":{";
  json += "\"on\":" + String(charger.on ? "true" : "false") + ",";
  json += "\"autoOffActive\":" + String(charger.autoOffActive ? "true" : "false") + ",";
  json += "\"lastEvent\":\"" + charger.lastEvent + "\",";
  //json += "\"onSeconds\":" + String(charger.onSeconds) ;
  unsigned long chargerOnTime =
  charger.on
    ? charger.onSeconds + (now - charger.turnedOnAt.unixtime())
    : charger.onSeconds;
  json += "\"onSeconds\":" + String(chargerOnTime);
  json += "}";


  json += "} }";

  server.send(200, "application/json", json);
}

void handleDevice() {
  String path = server.uri();   // e.g. /device/bulb/on

  if (!path.startsWith("/device/")) {
    server.send(404, "text/plain", "Not Found");
    return;
  }

  path.replace("/device/", "");

  int slashIndex = path.indexOf('/');
  if (slashIndex < 0) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  String dev = path.substring(0, slashIndex);
  String act = path.substring(slashIndex + 1);
  bool on = (act == "on");

  DeviceState* d = nullptr;

  if (dev == "bulb") d = &bulb;
  else if (dev == "kitchen") d = &kitchen;
  else if (dev == "bedroom") d = &bedroom;
  else if (dev == "corridor") d = &corridor;
  else if (dev == "charger") d = &charger;

  if (d == nullptr) {
    server.send(404, "text/plain", "Device Not Found");
    return;
  }

  DateTime now = rtc.now();
d->on = on;
d->autoOffActive = false;

if (on) {
  d->onSeconds = 0; 
  d->turnedOnAt = now;
  d->autoOffActive = false;
  d->lastEvent = "manual_on";

} else {
  if (d->turnedOnAt.unixtime() > 0) {
    d->onSeconds += now.unixtime() - d->turnedOnAt.unixtime();
  }
  d->turnedOnAt = DateTime(0UL);
  d->lastEvent = "manual_off";
}


  applyHardware();
  server.send(200, "text/plain", "OK");
}

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(9600);

  pinMode(RELAY_BULB, OUTPUT);
  pinMode(LED_KITCHEN, OUTPUT);
  pinMode(LED_BEDROOM, OUTPUT);
  pinMode(LED_CORRIDOR, OUTPUT);
  pinMode(LED_CHARGER, OUTPUT);

  digitalWrite(RELAY_BULB, HIGH);

  Wire.begin(21, 22);
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1);
  }
  rtc.begin();
  bulb.autoOffLimit = 300;      // 5 minutes
  kitchen.autoOffLimit = 60;  // 1 minute
  bedroom.autoOffLimit = 240;  // 4 minutes
  corridor.autoOffLimit = 180; // 3 minutes
  charger.autoOffLimit = 120;  // 2 minutes


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);

  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.onNotFound(handleDevice);
  delay(1000);   // allow WiFi stack to stabilize

  server.begin();
  

}
void checkAutoOff(DeviceState &d) {
  if (!d.on) return;
  if (d.autoOffActive) return;
  if (d.turnedOnAt.unixtime() == 0) return;

  DateTime now = rtc.now();
  unsigned long elapsed =
    now.unixtime() - d.turnedOnAt.unixtime();

  // ⚠ Unattended limit FIRST
if (UNATTENDED_LIMIT > 0 && elapsed >= UNATTENDED_LIMIT) {
  d.onSeconds += elapsed;
  d.autoOffActive = true;
  d.on = false;
  d.lastEvent = "auto_off_unattended";
  d.turnedOnAt = DateTime((uint32_t)0);
  applyHardware();
}

// ⏱ Normal Auto OFF
else if (elapsed >= d.autoOffLimit) {
  d.onSeconds += elapsed;
  d.autoOffActive = true;
  d.on = false;
  d.lastEvent = "auto_off_timer";
  d.turnedOnAt = DateTime((uint32_t)0);
  applyHardware();
}


}



/* ---------- LOOP ---------- */
void loop() {
  server.handleClient();
  checkNightRule();
  checkAutoOff(bulb);
  checkAutoOff(kitchen);
  checkAutoOff(bedroom);
  checkAutoOff(corridor);
  checkAutoOff(charger);
  
}
