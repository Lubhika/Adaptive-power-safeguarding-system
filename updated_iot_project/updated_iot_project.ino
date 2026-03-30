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
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>An IoT-Based Priority and Time-Aware System for Detecting Unattended Appliance Usage</title>
    <!-- Font Awesome -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <!-- Chart.js -->
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        @media print {
            /* 1. Global Reset & Layout */
            @page { margin: 1cm; size: portrait; }
            body {
                background-color: white !important;
                color: black !important;
                height: auto !important;
                overflow: visible !important;
                font-size: 12pt;
            }
            .app-container {
                display: block !important;
                height: auto !important;
                overflow: visible !important;
            }
            .main-content {
                padding: 0 !important;
                height: auto !important;
                overflow: visible !important;
                margin: 0 !important;
            }

            /* 2. Hide Navigation & UI Controls */
            .sidebar, .toast-container, .theme-btn, .btn-primary {
                display: none !important;
            }
            
            /* 3. Hide Non-Report Sections */
            #dashboard, #reports {
                display: none !important;
            }

            /* 4. Show Report Sections */
            #analytics, #insights {
                display: block !important;
                opacity: 1 !important;
                visibility: visible !important;
                margin-bottom: 2rem;
                page-break-inside: avoid;
            }

            /* 5. Force Light Theme Colors (Override Variables) */
            :root {
                --bg-color: #ffffff !important;
                --sidebar-bg: #ffffff !important;
                --card-bg: #ffffff !important;
                --text-primary: #000000 !important;
                --text-secondary: #333333 !important;
                --border-color: #dddddd !important;
                --shadow: none !important;
            }

            /* 6. Card Styling for Print */
            .card-body, .device-card, .chart-card, .summary-card, .suggestions-panel, .timeline-panel, .export-card {
                background-color: white !important;
                border: 1px solid #ccc !important;
                box-shadow: none !important;
                color: black !important;
                break-inside: avoid;
                margin-bottom: 1rem;
            }
            
            /* 7. Typography */
            h1, h2, h3, h4, h5, h6 {
                color: black !important;
            }
            p, span, div {
                color: #000 !important;
            }

            /* 8. Graph Visibility - Print Handling */
            .chart-container {
                height: 300px !important;
                width: 100% !important;
                display: block !important;
                page-break-inside: avoid;
                position: relative;
            }
            
            /* Hide Canvas in Print, Show Image instead */
            canvas {
                display: none !important;
            }
            #print-chart-img {
                display: block !important;
                width: 100% !important;
                height: 300px !important;
                object-fit: contain;
            }
            
            /* 9. Report Header */
            .main-content::before {
                content: "Energy Safety System - Full Report";
                display: block;
                font-size: 24pt;
                font-weight: bold;
                text-align: center;
                margin-bottom: 2rem;
                border-bottom: 2px solid #000;
                padding-bottom: 1rem;
            }
        }
        :root {
            /* Dark Theme (Default) */
            --bg-color: #0f172a;
            --sidebar-bg: #1e293b;
            --card-bg: #1e293b;
            --text-primary: #f8fafc;
            --text-secondary: #94a3b8;
            --accent-color: #3b82f6;
            --success-color: #10b981;
            --warning-color: #f59e0b;
            --danger-color: #ef4444;
            --border-color: #334155;
            --hover-bg: #334155;
            --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
            --glow: 0 0 15px rgba(59, 130, 246, 0.5);
        }

        body.light-theme {
            /* Light Theme */
            --bg-color: #f1f5f9;
            --sidebar-bg: #ffffff;
            --card-bg: #ffffff;
            --text-primary: #0f172a;
            --text-secondary: #64748b;
            --accent-color: #2563eb;
            --success-color: #059669;
            --warning-color: #d97706;
            --danger-color: #dc2626;
            --border-color: #e2e8f0;
            --hover-bg: #f8fafc;
            --shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05), 0 2px 4px -1px rgba(0, 0, 0, 0.03);
            --glow: 0 0 10px rgba(37, 99, 235, 0.3);
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            transition: background-color 0.3s ease, color 0.3s ease;
        }

        body {
            font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-primary);
            height: 100vh;
            overflow: hidden;
        }

        .app-container {
            display: flex;
            height: 100vh;
            width: 100vw;
            overflow: hidden;
        }

        .main-wrapper {
            flex: 1;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }
        
        /* Hide Print Image on Screen */
        #print-chart-img {
            display: none;
        }

        /* Sidebar */
        .sidebar {
            width: 260px;
            background-color: var(--sidebar-bg);
            border-right: 1px solid var(--border-color);
            display: flex;
            flex-direction: column;
            padding: 1.5rem;
            z-index: 100;
            transition: width 0.3s ease, padding 0.3s ease;
            position: relative;
        }

        .sidebar.collapsed {
            width: 80px;
            padding: 1.5rem 0.8rem;
        }

        .brand {
            display: flex;
            align-items: center;
            gap: 1rem;
            margin-bottom: 3rem;
            justify-content: space-between;
        }

        .sidebar.collapsed .brand {
            flex-direction: column;
            gap: 1.5rem;
        }

        .menu-btn {
            background: none;
            border: none;
            color: var(--text-primary);
            font-size: 1.2rem;
            cursor: pointer;
            padding: 0.5rem;
            border-radius: 8px;
            transition: background 0.2s;
        }

        .menu-btn:hover {
            background: var(--hover-bg);
        }

        .logo-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, var(--accent-color), #8b5cf6);
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.2rem;
            color: white;
            box-shadow: 0 4px 12px rgba(59, 130, 246, 0.3);
            flex-shrink: 0;
        }

        .top-bar {
            background-color: var(--card-bg);
            border-bottom: 1px solid var(--border-color);
            padding: 1rem 2rem;
            display: flex;
            align-items: center;
            z-index: 9;
        }

        .horizontal-title h2 {
            font-size: 1.1rem;
            font-weight: 600;
            color: var(--text-primary);
            margin: 0;
            white-space: nowrap;
            overflow: hidden;
            text-overflow: ellipsis;
        }

        .nav-links {
            list-style: none;
            flex: 1;
        }

        .nav-links li {
            display: flex;
            align-items: center;
            gap: 1rem;
            padding: 0.8rem 1rem;
            border-radius: 8px;
            cursor: pointer;
            margin-bottom: 0.5rem;
            color: var(--text-secondary);
            transition: all 0.2s ease;
            white-space: nowrap;
            overflow: hidden;
        }

        .sidebar.collapsed .nav-links li {
            padding: 0.8rem;
            justify-content: center;
        }

        .sidebar.collapsed .nav-links li span {
            display: none;
        }

        .nav-links li:hover {
            background-color: var(--hover-bg);
            color: var(--text-primary);
        }

        .nav-links li.active {
            background-color: rgba(59, 130, 246, 0.1);
            color: var(--accent-color);
            font-weight: 600;
        }

        .sidebar-footer {
            border-top: 1px solid var(--border-color);
            padding-top: 1.5rem;
        }

        .sidebar.collapsed .sidebar-footer {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 1rem;
        }

        .sidebar.collapsed .status-indicator .text,
        .sidebar.collapsed .last-seen {
            display: none;
        }

        .status-indicator {
            display: flex;
            align-items: center;
            gap: 0.5rem;
            margin-bottom: 0.5rem;
            font-size: 0.9rem;
        }

        .dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background-color: var(--text-secondary);
        }

        .status-indicator.connected .dot {
            background-color: var(--success-color);
            box-shadow: 0 0 8px var(--success-color);
            animation: pulse-dot 2s infinite;
        }

        .status-indicator.disconnected .dot {
            background-color: var(--danger-color);
        }

        @keyframes pulse-dot {
            0% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7); }
            70% { box-shadow: 0 0 0 6px rgba(16, 185, 129, 0); }
            100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0); }
        }

        .last-seen {
            font-size: 0.75rem;
            color: var(--text-secondary);
            margin-bottom: 1rem;
        }

        .theme-btn {
            background: transparent;
            border: 1px solid var(--border-color);
            color: var(--text-primary);
            padding: 0.5rem;
            border-radius: 6px;
            cursor: pointer;
            width: 100%;
            display: flex;
            justify-content: center;
            align-items: center;
        }

        /* Main Content */
        .main-content {
            flex: 1;
            overflow-y: auto;
            padding: 2rem;
            position: relative;
        }

        .page-view {
            display: none;
            animation: fadeIn 0.4s ease;
        }

        .page-view.active {
            display: block;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(10px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .page-header {
            margin-bottom: 2rem;
        }

        .page-header h1 {
            font-size: 1.8rem;
            margin-bottom: 0.5rem;
        }

        .page-header p {
            color: var(--text-secondary);
        }

        /* Dashboard Grid */
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 1.5rem;
        }

        .device-card {
            background-color: var(--card-bg);
            border-radius: 16px;
            padding: 1.5rem;
            box-shadow: var(--shadow);
            border: 1px solid var(--border-color);
            position: relative;
            overflow: hidden;
            transition: transform 0.2s, box-shadow 0.2s;
        }

        .device-card:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
        }

        .device-card.on {
            border-color: var(--accent-color);
            box-shadow: var(--glow);
        }

        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1rem;
        }

        .device-icon {
            width: 48px;
            height: 48px;
            background-color: rgba(59, 130, 246, 0.1);
            border-radius: 12px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.5rem;
            color: var(--accent-color);
        }

        /* Toggle Switch */
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 50px;
            height: 28px;
        }

        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: var(--border-color);
            transition: .4s;
            border-radius: 34px;
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 20px;
            width: 20px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }

        input:checked + .slider {
            background-color: var(--success-color);
        }

        input:checked + .slider:before {
            transform: translateX(22px);
        }

        .card-body {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 1rem;
        }

        .device-info h3 {
            font-size: 1.1rem;
            margin-bottom: 0.25rem;
        }

        .status-text {
            font-size: 0.9rem;
            font-weight: 600;
            color: var(--text-secondary);
        }
)rawliteral" R"rawliteral(
        .auto-off-badge {
            display: inline-block;
            background-color: rgba(239, 68, 68, 0.1);
            color: var(--danger-color);
            font-size: 0.7rem;
            padding: 2px 8px;
            border-radius: 10px;
            margin-top: 0.5rem;
            font-weight: 600;
        }

        /* Circular Timer */
        .timer-ring {
            position: relative;
            width: 80px;
            height: 80px;
        }

        .timer-ring svg {
            width: 100%;
            height: 100%;
            transform: rotate(-90deg);
        }

        .ring-bg {
            fill: none;
            stroke: var(--border-color);
            stroke-width: 6;
        }

        .ring-progress {
            fill: none;
            stroke: var(--accent-color);
            stroke-width: 6;
            stroke-dasharray: 220; /* 2 * PI * 35 */
            stroke-dashoffset: 220;
            transition: stroke-dashoffset 0.5s ease;
        }

        .timer-text {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            font-size: 0.9rem;
            font-weight: 600;
            color: var(--text-primary);
        }

        /* Usage Bar */
        .usage-bar-container {
            margin-top: 1rem;
        }

        .usage-label {
            display: flex;
            justify-content: space-between;
            font-size: 0.8rem;
            color: var(--text-secondary);
            margin-bottom: 0.3rem;
        }

        .progress-bar {
            height: 6px;
            background-color: var(--border-color);
            border-radius: 3px;
            overflow: hidden;
        }

        .progress-fill {
            height: 100%;
            background-color: var(--accent-color);
            width: 0%;
            transition: width 0.5s ease;
        }

        /* Analytics & Other Grids */
        .analytics-grid {
            display: grid;
            grid-template-columns: 2fr 1fr;
            gap: 1.5rem;
        }

        .chart-card, .summary-card, .insights-container, .reports-container {
            background-color: var(--card-bg);
            border-radius: 16px;
            padding: 1.5rem;
            border: 1px solid var(--border-color);
        }

        .chart-container {
            height: 300px;
            margin-top: 1rem;
        }

        .summary-stats {
            display: flex;
            flex-direction: column;
            gap: 1rem;
            margin-top: 1rem;
        }

        .stat-item {
            display: flex;
            align-items: center;
            gap: 1rem;
            padding: 1rem;
            background-color: var(--hover-bg);
            border-radius: 12px;
        }

        .stat-item .icon {
            width: 40px;
            height: 40px;
            border-radius: 10px;
            background-color: rgba(59, 130, 246, 0.1);
            color: var(--accent-color);
            display: flex;
            align-items: center;
            justify-content: center;
        }

        .stat-item .info {
            display: flex;
            flex-direction: column;
        }

        .stat-item .label {
            font-size: 0.8rem;
            color: var(--text-secondary);
        }

        .stat-item .value {
            font-size: 1.1rem;
            font-weight: 700;
        }

        /* Insights */
        .insights-container {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 1.5rem;
        }

        .suggestions-list, .timeline {
            margin-top: 1rem;
            max-height: 400px;
            overflow-y: auto;
        }

        .suggestion-item {
            padding: 0.8rem;
            background-color: rgba(245, 158, 11, 0.1);
            border-left: 3px solid var(--warning-color);
            border-radius: 4px;
            margin-bottom: 0.8rem;
            font-size: 0.9rem;
        }

        .timeline-item {
            display: flex;
            gap: 1rem;
            margin-bottom: 1rem;
            position: relative;
        }

        .timeline-item::before {
            content: "";
            position: absolute;
            left: 4px;
            top: 24px;
            bottom: -16px;
            width: 2px;
            background-color: var(--border-color);
        }

        .timeline-item:last-child::before {
            display: none;
        }

        .timeline-time {
            font-size: 0.8rem;
            color: var(--text-secondary);
            width: 70px;
            flex-shrink: 0;
            padding-top: 0.2rem;
        }

        .timeline-content {
            background-color: var(--hover-bg);
            padding: 0.8rem;
            border-radius: 8px;
            flex: 1;
            font-size: 0.9rem;
        }

        /* Reports */
        .reports-container {
            display: flex;
            gap: 1.5rem;
            flex-wrap: wrap;
        }

        .export-card {
            flex: 1;
            min-width: 250px;
            background-color: var(--hover-bg);
            padding: 2rem;
            border-radius: 12px;
            text-align: center;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 1rem;
        }

        .export-icon {
            font-size: 3rem;
            color: var(--text-secondary);
        }

        .btn-primary {
            background-color: var(--accent-color);
            color: white;
            border: none;
            padding: 0.8rem 1.5rem;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 600;
            transition: background-color 0.2s;
        }

        .btn-primary:hover {
            background-color: #2563eb;
        }

        /* Modal Styles */
        .modal-overlay {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background-color: rgba(0, 0, 0, 0.7);
            backdrop-filter: blur(5px);
            display: none;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            opacity: 0;
            transition: opacity 0.3s ease;
        }

        .modal-overlay.active {
            display: flex;
            opacity: 1;
        }

        .modal-content {
            background-color: var(--card-bg);
            width: 90%;
            max-width: 500px;
            border-radius: 20px;
            padding: 2rem;
            position: relative;
            transform: scale(0.9);
            transition: transform 0.3s cubic-bezier(0.34, 1.56, 0.64, 1);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.4);
        }

        .modal-overlay.active .modal-content {
            transform: scale(1);
        }

        .modal-close {
            position: absolute;
            top: 1.5rem;
            right: 1.5rem;
            background: var(--hover-bg);
            border: none;
            color: var(--text-primary);
            width: 32px;
            height: 32px;
            border-radius: 50%;
            cursor: pointer;
            font-size: 1.2rem;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: background 0.2s;
        }

        .modal-close:hover {
            background-color: var(--border-color);
        }

        .modal-header {
            display: flex;
            align-items: center;
            gap: 1.5rem;
            margin-bottom: 2rem;
        }

        .modal-device-icon {
            width: 60px;
            height: 60px;
            background: linear-gradient(135deg, var(--accent-color), #8b5cf6);
            border-radius: 15px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.8rem;
            color: white;
            box-shadow: 0 8px 16px rgba(59, 130, 246, 0.3);
        }

        .modal-title h2 {
            font-size: 1.5rem;
            margin-bottom: 0.3rem;
        }

        .status-badge {
            font-size: 0.8rem;
            padding: 0.3rem 0.8rem;
            border-radius: 20px;
            background: var(--hover-bg);
            color: var(--text-secondary);
            font-weight: 600;
            text-transform: uppercase;
        }

        .status-badge.on {
            background: rgba(16, 185, 129, 0.1);
            color: var(--success-color);
        }

        .modal-stat-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 1rem;
            margin-bottom: 2rem;
        }

        .modal-stat-card {
            background: var(--hover-bg);
            padding: 1rem;
            border-radius: 12px;
            display: flex;
            flex-direction: column;
            gap: 0.4rem;
        }

        .modal-stat-card i {
            color: var(--accent-color);
            margin-bottom: 0.2rem;
        }

        .modal-stat-card .label {
            font-size: 0.75rem;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }

        .modal-stat-card .value {
            font-size: 1.1rem;
            font-weight: 700;
        }

        .modal-info-section {
            background: rgba(59, 130, 246, 0.05);
            padding: 1.2rem;
            border-radius: 12px;
            border-left: 4px solid var(--accent-color);
        }

        .modal-info-section h3 {
            font-size: 1rem;
            margin-bottom: 0.5rem;
        }

        .modal-info-section p {
            font-size: 0.9rem;
            color: var(--text-secondary);
            line-height: 1.5;
        }

        .modal-footer {
            margin-top: 2rem;
        }

        /* Device Card Clickable Styles */
        .device-card {
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }

        .device-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 12px 24px rgba(0, 0, 0, 0.2);
        }

        .device-card .toggle-switch {
            cursor: default; /* Don't show hand over toggle since card is clickable */
        }

        /* Toast */
        .toast-container {
            position: fixed;
            bottom: 20px;
            right: 20px;
            z-index: 100;
            display: flex;
            flex-direction: column;
            gap: 10px;
        }

        .toast {
            background-color: var(--card-bg);
            border-left: 4px solid var(--accent-color);
            padding: 1rem;
            border-radius: 4px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
            display: flex;
            align-items: center;
            gap: 10px;
            min-width: 300px;
            animation: slideIn 0.3s ease;
        }

        .toast.success { border-color: var(--success-color); }
        .toast.warning { border-color: var(--warning-color); }
        .toast.error { border-color: var(--danger-color); }

        @keyframes slideIn {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }

        /* Responsive */
        @media (max-width: 768px) {
            .app-container {
                flex-direction: column;
            }
            .sidebar {
                width: 100%;
                height: auto;
                border-right: none;
                border-bottom: 1px solid var(--border-color);
                padding: 1rem;
            }
            .sidebar.collapsed {
                width: 100%;
                padding: 1rem;
            }
            .brand {
                margin-bottom: 1rem;
                justify-content: flex-start;
            }
            .nav-links {
                display: flex;
                overflow-x: auto;
                padding-bottom: 0.5rem;
                gap: 0.5rem;
            }
            .sidebar.collapsed .nav-links li span {
                display: inline; /* Always show text on mobile top nav */
            }
            .nav-links li {
                white-space: nowrap;
                margin-bottom: 0;
                padding: 0.5rem 1rem;
            }
            .top-bar {
                padding: 1rem;
            }
            .horizontal-title h2 {
                font-size: 0.9rem;
                white-space: normal;
            }
            .analytics-grid, .insights-container {
                grid-template-columns: 1fr;
            }
            .sidebar-footer {
                display: none;
            }
        }
    </style>
</head>
<body>
    <div class="app-container">
        <!-- Sidebar Navigation -->
        <nav class="sidebar" id="sidebar">
            <div class="brand">
                <button id="sidebar-toggle" class="menu-btn">
                    <i class="fas fa-bars"></i>
                </button>
                <div class="logo-icon"><i class="fas fa-bolt"></i></div>
            </div>
            
            <ul class="nav-links">
                <li class="active" data-tab="dashboard">
                    <i class="fas fa-home"></i> <span>Dashboard</span>
                </li>
                <li data-tab="analytics">
                    <i class="fas fa-chart-line"></i> <span>Analytics</span>
                </li>
                <li data-tab="insights">
                    <i class="fas fa-lightbulb"></i> <span>Insights</span>
                </li>
                <li data-tab="reports">
                    <i class="fas fa-file-export"></i> <span>Reports</span>
                </li>
            </ul>

            <div class="sidebar-footer">
                <div class="status-indicator" id="connection-status">
                    <span class="dot"></span> <span class="text">Connecting...</span>
                </div>
                <div class="last-seen" id="last-seen">Last update: --:--:--</div>
                <button id="theme-toggle" class="theme-btn">
                    <i class="fas fa-moon"></i>
                </button>
            </div>
        </nav>

        <!-- Main Wrapper -->
        <div class="main-wrapper">
            <!-- Top Bar -->
            <header class="top-bar">
                <div class="horizontal-title">
                    <h2>An IoT-Based Priority and Time-Aware System for Detecting Unattended Appliance Usage</h2>
                </div>
            </header>

            <!-- Main Content Area -->
            <main class="main-content">
            <!-- Dashboard View -->
            <section id="dashboard" class="page-view active">
                <header class="page-header">
                    <h1>Control Dashboard</h1>
                    <p>Real-time device monitoring and safety controls</p>
                </header>

                <div class="dashboard-grid">
                    <!-- Device Cards will be injected here via JS -->
                </div>
            </section>

            <!-- Analytics View -->
            <section id="analytics" class="page-view">
                <header class="page-header">
                    <h1>Energy Analytics</h1>
                    <p>Power usage tracking and efficiency metrics</p>
                </header>

                <div class="analytics-grid">
                    <div class="chart-card large">
                        <h3><i class="fas fa-bolt"></i> Power Usage Timeline (kWh)</h3>
                        <p style="font-size: 0.8rem; color: var(--text-secondary); margin-bottom: 1rem;">Shows total real-time power consumption (Watts) updated every 2 seconds.</p>
                        <div class="chart-container">
                            <canvas id="powerChart"></canvas>
                        </div>
                    </div>
                    
                    <div class="summary-card">
                        <h3>Daily Summary</h3>
                        <div class="summary-stats">
                            <div class="stat-item">
                                <div class="icon"><i class="fas fa-clock"></i></div>
                                <div class="info">
                                    <span class="label">Total ON Time</span>
                                    <span class="value" id="total-on-time">0h 0m</span>
                                </div>
                            </div>
                            <div class="stat-item">
                                <div class="icon"><i class="fas fa-shield-alt"></i></div>
                                <div class="info">
                                    <span class="label">Safety Triggers</span>
                                    <span class="value" id="safety-triggers">0</span>
                                </div>
                            </div>
                            <div class="stat-item">
                                <div class="icon"><i class="fas fa-leaf"></i></div>
                                <div class="info">
                                    <span class="label">Est. Energy</span>
                                    <span class="value" id="est-energy">0 kWh</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Insights View -->
            <section id="insights" class="page-view">
                <header class="page-header">
                    <h1>Smart Insights</h1>
                    <p>Intelligent suggestions and event history</p>
                </header>

                <div class="insights-container">
                    <div class="suggestions-panel">
                        <h3><i class="fas fa-robot"></i> System Suggestions</h3>
                        <div id="suggestions-list" class="suggestions-list">
                            <!-- Suggestions injected here -->
                            <div class="empty-state">Gathering data for insights...</div>
                        </div>
                    </div>

                    <div class="timeline-panel">
                        <h3><i class="fas fa-history"></i> Event Timeline</h3>
                        <div id="event-timeline" class="timeline">
                            <!-- Timeline items injected here -->
                        </div>
                    </div>
                </div>
            </section>

            <!-- Reports View -->
            <section id="reports" class="page-view">
                <header class="page-header">
                    <h1>Data Reports</h1>
                    <p>Export usage data and safety logs</p>
                </header>

                <div class="reports-container">
                    <div class="export-card">
                        <div class="export-icon"><i class="fas fa-file-csv"></i></div>
                        <h3>Usage Data (CSV)</h3>
                        <p>Detailed log of device usage durations and energy estimates.</p>
                        <button class="btn-primary" onclick="exportCSV()">Download CSV</button>
                    </div>

                    <div class="export-card">
                        <div class="export-icon"><i class="fas fa-file-pdf"></i></div>
                        <h3>Safety Report (PDF)</h3>
                        <p>Summary of safety triggers and efficiency score.</p>
                        <button class="btn-primary" onclick="printReport()">Print / Save PDF</button>
                    </div>
                </div>
            </section>
        </main>
    </div> <!-- main-wrapper end -->
</div> <!-- app-container end -->

    <!-- Toast Container -->
    <div id="toast-container" class="toast-container"></div>

    <!-- Device Details Modal -->
    <div id="device-modal" class="modal-overlay">
        <div class="modal-content">
            <button class="modal-close" id="modal-close">&times;</button>
            <div class="modal-header">
                <div id="modal-icon" class="modal-device-icon">
                    <i class="fas fa-bolt"></i>
                </div>
                <div class="modal-title">
                    <h2 id="modal-device-name">Device Name</h2>
                    <span id="modal-device-status" class="status-badge">OFF</span>
                </div>
            </div>
            <div class="modal-body">
                <div class="modal-stat-grid">
                    <div class="modal-stat-card">
                        <i class="fas fa-clock"></i>
                        <span class="label">Session Time</span>
                        <span id="modal-on-time" class="value">00:00:00</span>
                    </div>
                    <div class="modal-stat-card">
                        <i class="fas fa-plug"></i>
                        <span class="label">Power Rating</span>
                        <span id="modal-wattage" class="value">0W</span>
                    </div>
                    <div class="modal-stat-card">
                        <i class="fas fa-leaf"></i>
                        <span class="label">Est. Energy</span>
                        <span id="modal-energy" class="value">0.00 kWh</span>
                    </div>
                    <div class="modal-stat-card" id="modal-auto-off-card">
                        <i class="fas fa-shield-alt"></i>
                        <span class="label">Safety Status</span>
                        <span id="modal-safety-reason" class="value">Secure</span>
                    </div>
                </div>
                
                <div class="modal-info-section">
                    <h3>Safety Configuration</h3>
                    <p id="modal-config-info">This device will automatically turn off after reaching its safety time limit to prevent unattended usage.</p>
                </div>
            </div>
            <div class="modal-footer">
                <button class="btn-primary" style="width: 100%;" id="modal-action-btn">Close Details</button>
            </div>
        </div>
    </div>
)rawliteral" R"rawliteral(
    <script>
        // Configuration & Constants
        const DEVICES = {
            bulb: { name: "Main Bulb", icon: "fa-lightbulb", watts: 15, limit: 300, type: "relay" },
            kitchen: { name: "Kitchen Light", icon: "fa-utensils", watts: 12, limit: 60, type: "led" },
            bedroom: { name: "Bedroom Light", icon: "fa-bed", watts: 10, limit: 240, type: "led" },
            corridor: { name: "Corridor", icon: "fa-walking", watts: 8, limit: 180, type: "led" },
            charger: { name: "Phone Charger", icon: "fa-plug", watts: 20, limit: 120, type: "led" }
        };

        const API_URL = ""; // Relative path for ESP32 hosting
        const POLL_INTERVAL = 2000; // 2 seconds

        // State
        let deviceState = {};
        let powerHistory = [];
        let sessionEvents = []; // Store events for CSV export
        let totalEnergy = 0;
        let lastUpdate = 0;
        let isConnected = false;
        let activeDeviceSlug = null; // Track which device is open in modal

        // DOM Elements
        const dashboardGrid = document.querySelector('.dashboard-grid');
        const suggestionsList = document.getElementById('suggestions-list');
        const eventTimeline = document.getElementById('event-timeline');
        const connectionStatus = document.getElementById('connection-status');
        const lastSeenEl = document.getElementById('last-seen');
        const totalOnTimeEl = document.getElementById('total-on-time');
        const safetyTriggersEl = document.getElementById('safety-triggers');
        const estEnergyEl = document.getElementById('est-energy');
        
        // Modal Elements
        const deviceModal = document.getElementById('device-modal');
        const modalClose = document.getElementById('modal-close');
        const modalActionBtn = document.getElementById('modal-action-btn');

        // Audio Context for Beep
        let audioCtx = null;

        function playBeep() {
            try {
                if (!audioCtx) {
                    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                }
                
                const now = audioCtx.currentTime;
                
                const playNote = (freq, startTime, duration, volume = 0.2) => {
                    const osc = audioCtx.createOscillator();
                    const gain = audioCtx.createGain();
                    
                    // Triangle wave is softer than sawtooth/square but clearer than sine
                    osc.type = 'triangle'; 
                    osc.frequency.setValueAtTime(freq, startTime);
                    
                    gain.gain.setValueAtTime(0, startTime);
                    gain.gain.linearRampToValueAtTime(volume, startTime + 0.02); 
                    gain.gain.exponentialRampToValueAtTime(0.01, startTime + duration);
                    
                    osc.connect(gain);
                    gain.connect(audioCtx.destination);
                    
                    osc.start(startTime);
                    osc.stop(startTime + duration);
                };

                // Modern Melodic Chime (Three quick ascending notes)
                playNote(659.25, now, 0.3, 0.15);      // E5
                playNote(880.00, now + 0.15, 0.3, 0.15); // A5
                playNote(1046.50, now + 0.3, 0.5, 0.2);  // C6 (Higher, clear finish)
            } catch (e) {
                console.log("Audio play failed:", e);
            }
        }

        // Initialization
        document.addEventListener('DOMContentLoaded', () => {
            // Sidebar Toggle Logic
            const sidebar = document.getElementById('sidebar');
            const sidebarToggle = document.getElementById('sidebar-toggle');
            
            sidebarToggle.addEventListener('click', () => {
                sidebar.classList.toggle('collapsed');
            });

            // Modal Close Logic
            modalClose.addEventListener('click', closeModal);
            modalActionBtn.addEventListener('click', closeModal);
            deviceModal.addEventListener('click', (e) => {
                if (e.target === deviceModal) closeModal();
            });

            // User gesture to unlock audio context (needed for mobile)
            document.addEventListener('click', () => {
                if (audioCtx && audioCtx.state === 'suspended') {
                    audioCtx.resume();
                }
            }, { once: true });

            initDashboard();
            initChart();
            setupNavigation();
            setupTheme();
            
            // Start Polling
            fetchStatus();
            setInterval(fetchStatus, POLL_INTERVAL);
        });

        // --- UI Setup ---

        function initDashboard() {
            dashboardGrid.innerHTML = '';
            Object.keys(DEVICES).forEach(slug => {
                const device = DEVICES[slug];
                const card = document.createElement('div');
                card.className = 'device-card';
                card.id = `card-${slug}`;
                card.onclick = (e) => {
                    // Prevent modal if clicking the toggle switch
                    if (e.target.closest('.toggle-switch')) return;
                    openModal(slug);
                };
                card.innerHTML = `
                    <div class="card-header">
                        <div class="device-icon"><i class="fas ${device.icon}"></i></div>
                        <label class="toggle-switch">
                            <input type="checkbox" onchange="toggleDevice('${slug}', this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="card-body">
                        <div class="device-info">
                            <h3>${device.name}</h3>
                            <div class="status-text" id="status-${slug}">OFF</div>
                            <div class="auto-off-badge" id="badge-${slug}">AUTO-OFF</div>
                        </div>
                        <div class="timer-ring">
                            <svg viewBox="0 0 80 80">
                                <circle class="ring-bg" cx="40" cy="40" r="35"></circle>
                                <circle class="ring-progress" id="ring-${slug}" cx="40" cy="40" r="35"></circle>
                            </svg>
                            <div class="timer-text" id="timer-${slug}">00:00</div>
                        </div>
                    </div>
                    <div class="usage-bar-container">
                        <div class="usage-label">
                            <span>Daily Usage</span>
                            <span id="pct-${slug}">0%</span>
                        </div>
                        <div class="progress-bar">
                            <div class="progress-fill" id="bar-${slug}"></div>
                        </div>
                    </div>
                `;
                dashboardGrid.appendChild(card);
            });
        }

        function initChart() {
            const ctx = document.getElementById('powerChart').getContext('2d');
            Chart.defaults.color = '#94a3b8';
            Chart.defaults.borderColor = '#334155';
            
            powerChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [{
                        label: 'Real-time Power (Watts)',
                        data: [],
                        borderColor: '#3b82f6',
                        backgroundColor: 'rgba(59, 130, 246, 0.1)',
                        borderWidth: 2,
                        fill: true,
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    scales: {
                        x: { display: false }, // Hide x labels for clean look
                        y: { beginAtZero: true }
                    },
                    animation: { duration: 0 } // Disable animation for smooth realtime updates
                }
            });
        }

        function setupNavigation() {
            const tabs = document.querySelectorAll('.nav-links li');
            const views = document.querySelectorAll('.page-view');

            tabs.forEach(tab => {
                tab.addEventListener('click', () => {
                    // Remove active class
                    tabs.forEach(t => t.classList.remove('active'));
                    views.forEach(v => v.classList.remove('active'));

                    // Add active class
                    tab.classList.add('active');
                    const target = tab.getAttribute('data-tab');
                    document.getElementById(target).classList.add('active');
                });
            });
        }

        function formatDuration(seconds) {
            const h = Math.floor(seconds / 3600);
            const m = Math.floor((seconds % 3600) / 60);
            const s = Math.floor(seconds % 60);
            return `${h.toString().padStart(2, '0')}:${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}`;
        }

        function openModal(slug) {
            activeDeviceSlug = slug;
            const device = DEVICES[slug];
            const state = deviceState[slug] || { wasOn: false };
            
            // Initial content setup
            document.getElementById('modal-device-name').textContent = device.name;
            document.getElementById('modal-icon').innerHTML = `<i class="fas ${device.icon}"></i>`;
            document.getElementById('modal-wattage').textContent = `${device.watts}W`;
            
            // Show modal
            deviceModal.classList.add('active');
            
            // Initial update
            // We need current state from the last fetch, but updateDeviceCard will handle updates
            // if we trigger a fetch or just wait for the next poll. 
            // For immediate feel, we'll try to use local state if available.
        }

        function closeModal() {
            deviceModal.classList.remove('active');
            activeDeviceSlug = null;
        }

        function updateModalContent(slug, state, config) {
            const statusEl = document.getElementById('modal-device-status');
            const onTimeEl = document.getElementById('modal-on-time');
            const energyEl = document.getElementById('modal-energy');
            const safetyEl = document.getElementById('modal-safety-reason');
            const safetyCard = document.getElementById('modal-auto-off-card');

            // Status Badge
            statusEl.textContent = state.on ? "Active" : "OFF";
            statusEl.className = `status-badge ${state.on ? 'on' : ''}`;

            // Time & Energy
            const seconds = state.onSeconds || 0;
            onTimeEl.textContent = formatDuration(seconds);
            
            const kwh = (seconds * config.watts) / 3600000;
            energyEl.textContent = `${kwh.toFixed(4)} kWh`;

            // Safety Status
            if (state.autoOffActive) {
                safetyEl.textContent = state.lastEvent || "Auto-OFF";
                safetyEl.style.color = "var(--danger-color)";
                safetyCard.style.background = "rgba(239, 68, 68, 0.1)";
            } else {
                safetyEl.textContent = state.on ? "Monitoring" : "Secure";
                safetyEl.style.color = state.on ? "var(--accent-color)" : "var(--success-color)";
                safetyCard.style.background = "var(--hover-bg)";
            }
        }

        function setupTheme() {
            const toggle = document.getElementById('theme-toggle');
            const body = document.body;
            const icon = toggle.querySelector('i');

            toggle.addEventListener('click', () => {
                body.classList.toggle('light-theme');
                if (body.classList.contains('light-theme')) {
                    icon.classList.remove('fa-moon');
                    icon.classList.add('fa-sun');
                } else {
                    icon.classList.remove('fa-sun');
                    icon.classList.add('fa-moon');
                }
            });
        }

        // --- Logic & API ---

        async function fetchStatus() {
            try {
                const response = await fetch(`${API_URL}/status`);
                if (!response.ok) throw new Error("API Error");
                const data = await response.json();
                
                updateConnectionStatus(true);
                // Handle structure { devices: { ... } } or just { ... }
                const deviceData = data.devices || data;
                processData(deviceData);
            } catch (e) {
                console.error("Fetch failed", e);
                updateConnectionStatus(false);
            }
        }

        function updateConnectionStatus(connected) {
            isConnected = connected;
            const dot = connectionStatus.querySelector('.dot');
            const text = connectionStatus.querySelector('.text');
            
            if (connected) {
                connectionStatus.classList.add('connected');
                connectionStatus.classList.remove('disconnected');
                text.textContent = "Connected";
                
                const now = new Date();
                lastSeenEl.textContent = `Last update: ${now.toLocaleTimeString()}`;
            } else {
                connectionStatus.classList.remove('connected');
                connectionStatus.classList.add('disconnected');
                text.textContent = "Disconnected";
            }
        }

        function toggleDevice(slug, state) {
            const action = state ? 'on' : 'off';
            fetch(`${API_URL}/device/${slug}/${action}`)
                .then(res => {
                    if (res.ok) {
                        showToast(`${DEVICES[slug].name} turned ${action.toUpperCase()}`, 'success');
                        fetchStatus(); // Refresh immediately
                    }
                })
                .catch(err => showToast("Command failed", 'error'));
        }

        function processData(data) {
            let currentTotalWatts = 0;
            let dailyOnSeconds = 0;
            let safetyCount = 0;

            Object.keys(DEVICES).forEach(slug => {
                if (!data[slug]) return;

                const state = data[slug];
                const config = DEVICES[slug];
                
                // Update Card UI
                updateDeviceCard(slug, state, config);

                // Stats
                if (state.on) {
                    currentTotalWatts += config.watts;
                }
                dailyOnSeconds += (state.onSeconds || 0); 
                
                // Accumulate local daily stats
                if (!deviceState[slug]) deviceState[slug] = { totalSeconds: 0 };
                
                // If device is ON, add polling interval to total
                if (state.on) {
                     // rough estimation
                    deviceState[slug].totalSeconds += (POLL_INTERVAL / 1000);
                }
                
                // Check for Auto-OFF events
                if (state.autoOffActive && !deviceState[slug].wasAutoOff) {
                    const eventMsg = "Auto-OFF triggered (" + state.lastEvent + ")";
                    addTimelineEvent(slug, eventMsg, "warning");
                    showToast(`${config.name} Auto-OFF triggered!`, 'warning');
                    playBeep(); // Play sound on auto-off
                    safetyCount++;
                }
                deviceState[slug].wasAutoOff = state.autoOffActive;
                
                // Check for manual events (naive check)
                if (state.on && !deviceState[slug].wasOn) {
                    addTimelineEvent(slug, "Turned ON", "success");
                } else if (!state.on && deviceState[slug].wasOn && !state.autoOffActive) {
                    addTimelineEvent(slug, "Turned OFF", "info");
                }
                deviceState[slug].wasOn = state.on;
            });

            // Update Global Stats
            updateGlobalStats(dailyOnSeconds, currentTotalWatts, safetyCount);
            
            // Update Suggestions
            updateSuggestions(data);
        }

        function updateDeviceCard(slug, state, config) {
            const card = document.getElementById(`card-${slug}`);
            const statusText = document.getElementById(`status-${slug}`);
            const badge = document.getElementById(`badge-${slug}`);
            const checkbox = card.querySelector('input');
            const ring = document.getElementById(`ring-${slug}`);
            const timerText = document.getElementById(`timer-${slug}`);
            const bar = document.getElementById(`bar-${slug}`);
            const pct = document.getElementById(`pct-${slug}`);

            // ON/OFF State
            if (state.on) {
                card.classList.add('on');
                statusText.textContent = "ON - Active";
                statusText.style.color = "var(--accent-color)";
                checkbox.checked = true;
            } else {
                card.classList.remove('on');
                statusText.textContent = "OFF";
                statusText.style.color = "var(--text-secondary)";
                checkbox.checked = false;
            }

            // Update Modal if open for this device
            if (activeDeviceSlug === slug) {
                updateModalContent(slug, state, config);
            }

            // Auto-off Badge
            if (state.autoOffActive) {
                badge.style.display = "inline-block";
                badge.textContent = state.lastEvent.includes('night') ? "NIGHT RULE" : "AUTO-OFF";
            } else {
                badge.style.display = "none";
            }

            // Timer Ring
            const limit = config.limit;
            const current = state.onSeconds || 0;
            const percentage = Math.min(current / limit, 1);
            
            // Stroke Dash: 2 * PI * 35 ≈ 220
            const offset = 220 - (220 * percentage);
            ring.style.strokeDashoffset = offset;
            
            // Color change near limit
            if (percentage > 0.8) ring.style.stroke = "var(--danger-color)";
            else ring.style.stroke = "var(--accent-color)";

            // Timer Text
            const mins = Math.floor(current / 60).toString().padStart(2, '0');
            const secs = (current % 60).toString().padStart(2, '0');
            timerText.textContent = `${mins}:${secs}`;

            // Usage Bar (Daily)
            const totalSec = deviceState[slug] ? deviceState[slug].totalSeconds : 0;
            const dailyLimit = 3600; // 1 hour example
            const dailyPct = Math.min((totalSec / dailyLimit) * 100, 100);
            bar.style.width = `${dailyPct}%`;
            pct.textContent = `${Math.floor(dailyPct)}%`;
        }

        function updateGlobalStats(dailySeconds, currentWatts, safetyCount) {
            // Add to chart
            const now = new Date();
            const timeLabel = now.getHours() + ':' + now.getMinutes() + ':' + now.getSeconds();
            
            if (powerChart.data.labels.length > 20) {
                powerChart.data.labels.shift();
                powerChart.data.datasets[0].data.shift();
            }
            powerChart.data.labels.push(timeLabel);
            powerChart.data.datasets[0].data.push(currentWatts);
            powerChart.update();

            // Summary Text
            const intervalHours = (POLL_INTERVAL / 1000) / 3600;
            const energyIncrement = (currentWatts * intervalHours) / 1000; // kWh
            totalEnergy += energyIncrement;
            
            estEnergyEl.textContent = totalEnergy.toFixed(5) + " kWh";
            
            // Total ON Time (Accumulated from all devices)
            let totalSecsAll = 0;
            Object.values(deviceState).forEach(s => totalSecsAll += s.totalSeconds);
            const hrs = Math.floor(totalSecsAll / 3600);
            const mins = Math.floor((totalSecsAll % 3600) / 60);
            totalOnTimeEl.textContent = `${hrs}h ${mins}m`;
            
            safetyTriggersEl.textContent = safetyCount;
        }

        function addTimelineEvent(slug, event, type) {
            const item = document.createElement('div');
            item.className = 'timeline-item';
            const time = new Date().toLocaleTimeString();
            
            // Add to session events for CSV
            sessionEvents.push({
                time: new Date().toISOString(),
                device: DEVICES[slug].name,
                event: event
            });

            item.innerHTML = `
                <div class="timeline-time">${time}</div>
                <div class="timeline-content">
                    <strong>${DEVICES[slug].name}</strong>: ${event}
                </div>
            `;
            eventTimeline.prepend(item);
            
            // Keep list short
            if (eventTimeline.children.length > 10) {
                eventTimeline.removeChild(eventTimeline.lastChild);
            }
        }

        function updateSuggestions(data) {
            suggestionsList.innerHTML = '';
            let hasSuggestions = false;

            Object.keys(DEVICES).forEach(slug => {
                const state = data[slug];
                const config = DEVICES[slug];
                
                // Rule 1: High usage
                if (state.onSeconds > config.limit * 0.8) {
                     addSuggestion(`${config.name} is running long. Consider turning off to save energy.`, 'warning');
                     hasSuggestions = true;
                }
                
                // Rule 2: Auto Off
                if (state.autoOffActive) {
                    addSuggestion(`${config.name} was auto-stopped. Check if you need it.`, 'info');
                    hasSuggestions = true;
                }
            });

            if (!hasSuggestions) {
                suggestionsList.innerHTML = '<div class="empty-state">System running efficiently. No actions needed.</div>';
            }
        }

        function addSuggestion(text, type) {
            const div = document.createElement('div');
            div.className = 'suggestion-item';
            div.textContent = text;
            suggestionsList.appendChild(div);
        }

        function showToast(message, type = 'info') {
            const toast = document.createElement('div');
            toast.className = `toast ${type}`;
            toast.innerHTML = `
                <i class="fas ${type === 'success' ? 'fa-check-circle' : type === 'warning' ? 'fa-exclamation-triangle' : 'fa-info-circle'}"></i>
                <span>${message}</span>
            `;
            
            const container = document.getElementById('toast-container');
            container.appendChild(toast);
            
            setTimeout(() => {
                toast.style.opacity = '0';
                setTimeout(() => toast.remove(), 300);
            }, 3000);
        }

        function exportCSV() {
            if (sessionEvents.length === 0) {
                showToast("No events to export yet!", "warning");
                return;
            }

            let csvContent = "data:text/csv;charset=utf-8,";
            csvContent += "Time,Device,Event\n";
            
            sessionEvents.forEach(e => {
                csvContent += `${e.time},${e.device},${e.event}\n`;
            });
            
            const encodedUri = encodeURI(csvContent);
            const link = document.createElement("a");
            link.setAttribute("href", encodedUri);
            link.setAttribute("download", "energy_report.csv");
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }

        function printReport() {
            const container = document.querySelector('.chart-container');
            const canvas = document.getElementById('powerChart');
            
            // 1. Create Image Element if needed (hidden on screen by CSS)
            let img = document.getElementById('print-chart-img');
            if (!img) {
                img = document.createElement('img');
                img.id = 'print-chart-img';
                container.appendChild(img);
            }

            // 2. Save Original Styles
            const originalYColor = powerChart.options.scales.y.ticks.color;
            const originalGridColor = powerChart.options.scales.y.grid.color;
            const originalBorderColor = powerChart.data.datasets[0].borderColor;
            const originalBgColor = powerChart.data.datasets[0].backgroundColor;

            // 3. Apply Print Styles (High Contrast)
            powerChart.options.scales.y.ticks.color = '#000000';
            powerChart.options.scales.y.grid.color = '#cccccc';
            powerChart.data.datasets[0].borderColor = '#000000';
            powerChart.data.datasets[0].backgroundColor = 'rgba(0, 0, 0, 0.1)';
            powerChart.update();

            // 4. Generate Image & Print
            // We give the chart a moment to re-render with new colors
            setTimeout(() => {
                // Generate high-res image
                img.src = powerChart.toBase64Image();
                
                // Note: CSS @media print handles the swapping:
                // canvas -> display: none
                // #print-chart-img -> display: block
                
                // Wait for image to load into DOM
                setTimeout(() => {
                    window.print();
                    
                    // 5. Revert Styles (after print dialog opens/closes)
                    // We revert immediately because print dialog pauses JS execution in most browsers,
                    // or we revert after a delay.
                    powerChart.options.scales.y.ticks.color = originalYColor;
                    powerChart.options.scales.y.grid.color = originalGridColor;
                    powerChart.data.datasets[0].borderColor = originalBorderColor;
                    powerChart.data.datasets[0].backgroundColor = originalBgColor;
                    powerChart.update();
                }, 500);
            }, 300);
        }
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
  d.lastEvent = "auto_off_timeout";
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