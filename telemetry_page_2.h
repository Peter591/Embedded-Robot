#pragma once
#include <pgmspace.h>
 
// Web UI kept in flash.
// CSV fields: mode, ctrl, irL, irC, irR, rpmL*10, rpmR*10, dist*10, hdg*10, servo, cmdL, cmdR, autoBase, autoTurnSoft, assist
 
static const char INDEX_HTML[] PROGMEM = R"=====(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Robot Telemetry</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Barlow+Condensed:wght@400;600;700;900&display=swap" rel="stylesheet">
<style>
/* colours */
:root {
  --bg:         #0a0c0e;
  --panel:      #0f1215;
  --border:     #1e2530;
  --border-hi:  #2e3d50;
  --accent:     #00e5ff;
  --accent2:    #ff6b35;
  --green:      #1ee36a;
  --red:        #ff3d5a;
  --amber:      #ffb700;
  --dim:        #3a4a5a;
  --text:       #c8d8e8;
  --text-dim:   #5a7080;
  --mono:       'Share Tech Mono', monospace;
  --sans:       'Barlow Condensed', sans-serif;
  --scan-speed: 6s;
}
 
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
 
body {
  background: var(--bg);
  color: var(--text);
  font-family: var(--sans);
  min-height: 100vh;
  overflow-x: hidden;
}
 
/* scanlines */
body::before {
  content: '';
  position: fixed; inset: 0;
  background: repeating-linear-gradient(
    0deg,
    transparent,
    transparent 2px,
    rgba(0,0,0,0.18) 2px,
    rgba(0,0,0,0.18) 4px
  );
  pointer-events: none;
  z-index: 9999;
}
 
/* vignette */
body::after {
  content: '';
  position: fixed; inset: 0;
  background: radial-gradient(ellipse at center, transparent 55%, rgba(0,0,0,0.6) 100%);
  pointer-events: none;
  z-index: 9998;
}
 
/* layout */
.shell {
  max-width: 1100px;
  margin: 0 auto;
  padding: 16px;
  display: grid;
  gap: 10px;
}
 
/* header */
.header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  border: 1px solid var(--border-hi);
  border-left: 4px solid var(--accent);
  background: var(--panel);
  padding: 10px 16px;
  gap: 16px;
  flex-wrap: wrap;
}
.header-title {
  font-family: var(--sans);
  font-weight: 900;
  font-size: 22px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--accent);
  text-shadow: 0 0 18px rgba(0,229,255,0.4);
}
.header-meta {
  font-family: var(--mono);
  font-size: 11px;
  color: var(--text-dim);
  line-height: 1.7;
}
.header-meta span { color: var(--text); }
 
/* status */
.status-row {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 10px;
}
.stat-card {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 12px 14px;
  position: relative;
  overflow: hidden;
}
.stat-card::before {
  content: '';
  position: absolute; top: 0; left: 0; right: 0;
  height: 2px;
  background: var(--accent);
  opacity: 0.3;
}
.stat-label {
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.15em;
  text-transform: uppercase;
  color: var(--text-dim);
  margin-bottom: 6px;
}
.stat-value {
  font-family: var(--mono);
  font-size: 26px;
  font-weight: 400;
  line-height: 1;
  color: var(--text);
}
.stat-value.accent  { color: var(--accent);  text-shadow: 0 0 12px rgba(0,229,255,0.35); }
.stat-value.green   { color: var(--green);   text-shadow: 0 0 12px rgba(30,227,106,0.35); }
.stat-value.amber   { color: var(--amber);   text-shadow: 0 0 12px rgba(255,183,0,0.35); }
.stat-value.red     { color: var(--red);     text-shadow: 0 0 12px rgba(255,61,90,0.35); }
.stat-unit {
  font-family: var(--mono);
  font-size: 11px;
  color: var(--text-dim);
  margin-top: 3px;
}
 
/* ir */
.ir-section {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 12px 14px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
}
.ir-label-row {
  display: flex;
  gap: 24px;
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.12em;
  color: var(--text-dim);
  text-transform: uppercase;
}
.ir-dots {
  display: flex;
  gap: 24px;
  align-items: center;
}
.ir-dot {
  width: 22px; height: 22px;
  border-radius: 50%;
  background: #111;
  border: 1px solid var(--dim);
  transition: background 80ms, box-shadow 80ms;
  position: relative;
}
.ir-dot::after {
  content: '';
  position: absolute;
  inset: 3px;
  border-radius: 50%;
  background: transparent;
  transition: background 80ms, box-shadow 80ms;
}
.ir-dot.on {
  border-color: var(--green);
}
.ir-dot.on::after {
  background: var(--green);
  box-shadow: 0 0 8px 2px rgba(30,227,106,0.6);
}
 
/* charts */
.chart-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}
@media (max-width: 640px) { .chart-grid { grid-template-columns: 1fr; } }
 
.chart-card {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 12px;
}
.chart-header {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-bottom: 8px;
}
.chart-title {
  font-family: var(--sans);
  font-weight: 700;
  font-size: 13px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--text-dim);
}
.chart-live {
  font-family: var(--mono);
  font-size: 20px;
  color: var(--accent);
  text-shadow: 0 0 10px rgba(0,229,255,0.35);
}
canvas.rpm-chart {
  display: block;
  width: 100%;
  height: 140px;
  background: transparent;
}
 
/* motor bars */
.motor-section {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 12px 14px;
}
.motor-section-title {
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.15em;
  text-transform: uppercase;
  color: var(--text-dim);
  margin-bottom: 10px;
}
.motor-row {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 8px;
}
.motor-row:last-child { margin-bottom: 0; }
.motor-side-label {
  font-family: var(--mono);
  font-size: 11px;
  color: var(--text-dim);
  width: 14px;
  flex-shrink: 0;
}
.motor-bar-track {
  flex: 1;
  height: 14px;
  background: #111;
  border: 1px solid var(--border);
  position: relative;
  overflow: hidden;
}
.motor-bar-center {
  position: absolute;
  top: 0; bottom: 0;
  left: 50%;
  width: 1px;
  background: var(--border-hi);
}
.motor-bar-fill {
  position: absolute;
  top: 1px; bottom: 1px;
  transition: left 80ms, width 80ms, background 80ms;
}
.motor-val {
  font-family: var(--mono);
  font-size: 11px;
  color: var(--text);
  width: 40px;
  text-align: right;
  flex-shrink: 0;
}
 
/* heading */
.odo-section {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 12px 14px;
  display: flex;
  gap: 20px;
  align-items: center;
  flex-wrap: wrap;
}
.compass-wrap {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
}
.compass-label {
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.12em;
  color: var(--text-dim);
  text-transform: uppercase;
}
canvas#compass {
  display: block;
}
.odo-stats {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
 
/* raw packet */
.raw-card {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 10px 14px;
}
.raw-label {
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.12em;
  color: var(--text-dim);
  text-transform: uppercase;
  margin-bottom: 4px;
}
#raw {
  font-family: var(--mono);
  font-size: 12px;
  color: var(--text-dim);
  word-break: break-all;
}
 
/* disconnect banner */
#conn-banner {
  position: fixed;
  top: 0; left: 0; right: 0;
  background: var(--red);
  color: #fff;
  font-family: var(--mono);
  font-size: 12px;
  letter-spacing: 0.1em;
  text-align: center;
  padding: 6px;
  z-index: 10000;
  display: none;
}
#conn-banner.show { display: block; }
 
/* servo */
.servo-track {
  flex: 1;
  min-width: 120px;
  display: flex;
  flex-direction: column;
  gap: 5px;
}
.servo-bar-track {
  height: 10px;
  background: #111;
  border: 1px solid var(--border);
  border-radius: 2px;
  overflow: hidden;
}
.servo-bar-fill {
  height: 100%;
  background: var(--accent2);
  transition: width 80ms;
  border-radius: 1px;
}
 
/* tune bars */
.autotune-section {
  background: var(--panel);
  border: 1px solid var(--border);
  padding: 12px 14px;
}
.autotune-title {
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.15em;
  text-transform: uppercase;
  color: var(--text-dim);
  margin-bottom: 10px;
}
.autotune-row {
  display: flex;
  align-items: center;
  gap: 10px;
  margin-bottom: 8px;
}
.autotune-row:last-child { margin-bottom: 0; }
.autotune-label {
  font-family: var(--mono);
  font-size: 10px;
  letter-spacing: 0.1em;
  color: var(--text-dim);
  width: 90px;
  flex-shrink: 0;
  text-transform: uppercase;
}
.autotune-bar-track {
  flex: 1;
  height: 10px;
  background: #111;
  border: 1px solid var(--border);
  overflow: hidden;
}
.autotune-bar-fill {
  height: 100%;
  background: var(--amber);
  transition: width 150ms;
}
.autotune-val {
  font-family: var(--mono);
  font-size: 12px;
  color: var(--amber);
  width: 36px;
  text-align: right;
  flex-shrink: 0;
}
 
/* assist */
.assist-badge {
  display: inline-block;
  font-family: var(--mono);
  font-size: 11px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  padding: 4px 10px;
  border-radius: 3px;
  border: 1px solid var(--dim);
  color: var(--text-dim);
  background: transparent;
  transition: background 100ms, color 100ms, border-color 100ms, box-shadow 100ms;
}
.assist-badge.active {
  background: var(--accent2);
  color: #fff;
  border-color: var(--accent2);
  box-shadow: 0 0 14px rgba(255,107,53,0.5);
}
</style>
</head>
<body>
 
<div id="conn-banner">&#9888; WEBSOCKET DISCONNECTED — RECONNECTING…</div>
 
<div class="shell">
 
  <!-- top bar -->
  <div class="header">
    <div class="header-title">&#x25CF; Robot Telemetry</div>
    <div class="header-meta">
      AP <span>RobotTelemetry</span> &nbsp;/&nbsp; <span>robot1234</span><br>
      ws://<span>192.168.4.1:81</span> &nbsp;|&nbsp; 10 Hz
    </div>
  </div>
 
  <!-- status -->
  <div class="status-row">
    <div class="stat-card">
      <div class="stat-label">Mode</div>
      <div class="stat-value accent" id="mode">—</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Controller</div>
      <div class="stat-value" id="ctrl">—</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Assist</div>
      <div class="assist-badge" id="assistBadge">IDLE</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Left RPM</div>
      <div class="stat-value accent" id="rpmL">—</div>
      <div class="stat-unit">rev / min</div>
    </div>
    <div class="stat-card">
      <div class="stat-label">Right RPM</div>
      <div class="stat-value accent" id="rpmR">—</div>
      <div class="stat-unit">rev / min</div>
    </div>
    <div class="ir-section">
      <div class="ir-label-row">
        <span>L</span><span>C</span><span>R</span>
      </div>
      <div class="ir-dots">
        <div class="ir-dot" id="irL"></div>
        <div class="ir-dot" id="irC"></div>
        <div class="ir-dot" id="irR"></div>
      </div>
      <div class="stat-label" style="margin-top:2px">IR sensors</div>
    </div>
  </div>
 
  <!-- rpm charts -->
  <div class="chart-grid">
    <div class="chart-card">
      <div class="chart-header">
        <div class="chart-title">Left Wheel RPM</div>
        <div class="chart-live" id="rpmL2">—</div>
      </div>
      <canvas id="chartL" class="rpm-chart" width="460" height="140"></canvas>
    </div>
    <div class="chart-card">
      <div class="chart-header">
        <div class="chart-title">Right Wheel RPM</div>
        <div class="chart-live" id="rpmR2">—</div>
      </div>
      <canvas id="chartR" class="rpm-chart" width="460" height="140"></canvas>
    </div>
  </div>
 
  <!-- outputs -->
  <div class="motor-section">
    <div class="motor-section-title">Motor Commands &amp; Servo</div>
    <div class="motor-row">
      <div class="motor-side-label">L</div>
      <div class="motor-bar-track">
        <div class="motor-bar-center"></div>
        <div class="motor-bar-fill" id="barL"></div>
      </div>
      <div class="motor-val" id="valL">0</div>
    </div>
    <div class="motor-row">
      <div class="motor-side-label">R</div>
      <div class="motor-bar-track">
        <div class="motor-bar-center"></div>
        <div class="motor-bar-fill" id="barR"></div>
      </div>
      <div class="motor-val" id="valR">0</div>
    </div>
    <div class="motor-row">
      <div class="motor-side-label" style="color:var(--accent2)">S</div>
      <div class="servo-track">
        <div class="servo-bar-track">
          <div class="servo-bar-fill" id="servoBar"></div>
        </div>
      </div>
      <div class="motor-val" id="servoVal">—°</div>
    </div>
  </div>
 
  <!-- tuning -->
  <div class="autotune-section">
    <div class="autotune-title">Auto Tune — D-pad (hold B) &nbsp;&#x25B2;&#x25BC; Base &nbsp;&#x25C4;&#x25BA; Turn</div>
    <div class="autotune-row">
      <div class="autotune-label">Base Speed</div>
      <div class="autotune-bar-track">
        <div class="autotune-bar-fill" id="autoBaseBar"></div>
      </div>
      <div class="autotune-val" id="autoBaseVal">—</div>
    </div>
    <div class="autotune-row">
      <div class="autotune-label">Turn Soft</div>
      <div class="autotune-bar-track">
        <div class="autotune-bar-fill" id="autoTurnBar"></div>
      </div>
      <div class="autotune-val" id="autoTurnVal">—</div>
    </div>
  </div>
 
  <!-- odometry -->
  <div class="odo-section">
    <div class="compass-wrap">
      <div class="compass-label">Heading</div>
      <canvas id="compass" width="80" height="80"></canvas>
    </div>
    <div class="odo-stats">
      <div class="stat-card" style="min-width:130px">
        <div class="stat-label">Distance</div>
        <div class="stat-value accent" id="dist">—</div>
        <div class="stat-unit">mm</div>
      </div>
      <div class="stat-card" style="min-width:130px">
        <div class="stat-label">Heading</div>
        <div class="stat-value amber" id="hdg">—</div>
        <div class="stat-unit">degrees</div>
      </div>
    </div>
  </div>
 
  <!-- raw -->
  <div class="raw-card">
    <div class="raw-label">Raw packet</div>
    <div id="raw">—</div>
  </div>
 
</div>
 
<script>
// chart history
const N = 200;
const histL = new Float32Array(N);
const histR = new Float32Array(N);
let histIdx = 0;
 
// rpm chart
function drawChart(canvasId, arr, color) {
  const c = document.getElementById(canvasId);
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width;
  const h = c.height;
  ctx.clearRect(0, 0, w, h);
 
  // background
  ctx.fillStyle = '#0a0c0e';
  ctx.fillRect(0, 0, w, h);
 
  // grid
  ctx.strokeStyle = '#1a2530';
  ctx.lineWidth = 1;
  for (let i = 1; i < 5; i++) {
    const y = (h * i) / 5;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke();
  }
  // zero line
  ctx.strokeStyle = '#2a3a4a';
  ctx.beginPath(); ctx.moveTo(0, h / 2); ctx.lineTo(w, h / 2); ctx.stroke();
 
  // scale
  let maxV = 1;
  for (let i = 0; i < N; i++) { const a = Math.abs(arr[i]); if (a > maxV) maxV = a; }
  maxV = Math.ceil(maxV / 10) * 10 || 1;
 
  // label
  ctx.fillStyle = '#3a5060';
  ctx.font = '10px Share Tech Mono, monospace';
  ctx.fillText('±' + maxV + ' RPM', 6, 14);
 
  // line + fill
  const grad = ctx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0, color + '55');
  grad.addColorStop(0.5, color + '22');
  grad.addColorStop(1, 'transparent');
 
  ctx.beginPath();
  for (let i = 0; i < N; i++) {
    const di = (histIdx - N + i + N) % N;
    const x = (i / (N - 1)) * w;
    const y = h / 2 - (arr[di] / maxV) * (h * 0.45);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  // fill
  ctx.lineTo(w, h / 2);
  ctx.lineTo(0, h / 2);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();
 
  // outline
  ctx.beginPath();
  for (let i = 0; i < N; i++) {
    const di = (histIdx - N + i + N) % N;
    const x = (i / (N - 1)) * w;
    const y = h / 2 - (arr[di] / maxV) * (h * 0.45);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.5;
  ctx.stroke();
}
 
// heading indicator
function drawCompass(deg) {
  const c = document.getElementById('compass');
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width;
  const h = c.height;
  const cx = w / 2, cy = h / 2, r = (w / 2) - 4;
 
  ctx.clearRect(0, 0, w, h);
 
  // outer ring
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.strokeStyle = '#2e3d50';
  ctx.lineWidth = 1;
  ctx.stroke();
 
  // ticks
  ctx.strokeStyle = '#3a5060';
  ctx.lineWidth = 1;
  for (let i = 0; i < 8; i++) {
    const a = (i / 8) * Math.PI * 2;
    const inner = r - (i % 2 === 0 ? 8 : 4);
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(a) * inner, cy + Math.sin(a) * inner);
    ctx.lineTo(cx + Math.cos(a) * r,     cy + Math.sin(a) * r);
    ctx.stroke();
  }
 
  // north label
  ctx.fillStyle = '#00e5ff';
  ctx.font = 'bold 9px Barlow Condensed, sans-serif';
  ctx.textAlign = 'center';
  ctx.fillText('N', cx, cy - r + 10);
 
  // needle
  const rad = (deg - 90) * Math.PI / 180;
  const tipX = cx + Math.cos(rad) * (r - 6);
  const tipY = cy + Math.sin(rad) * (r - 6);
  const tailX = cx + Math.cos(rad + Math.PI) * (r - 14);
  const tailY = cy + Math.sin(rad + Math.PI) * (r - 14);
 
  ctx.beginPath();
  ctx.moveTo(tipX, tipY);
  ctx.lineTo(tailX, tailY);
  ctx.strokeStyle = '#ffb700';
  ctx.lineWidth = 2;
  ctx.lineCap = 'round';
  ctx.stroke();
 
  // centre dot
  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fillStyle = '#ffb700';
  ctx.fill();
}
 
// motor bar
function setMotorBar(fillId, valId, cmd) {
  const fill = document.getElementById(fillId);
  const val = document.getElementById(valId);
  if (!fill || !val) return;
  // zero is middle of the track
  const pct = (Math.abs(cmd) / 1023) * 50;
  const fwd = cmd >= 0;
  fill.style.left       = fwd ? '50%' : (50 - pct) + '%';
  fill.style.width      = pct + '%';
  fill.style.background = fwd ? '#00e5ff' : '#ff3d5a';
  val.textContent = cmd;
}
 
// servo bar
function setServoBar(deg, minDeg, maxDeg) {
  const bar = document.getElementById('servoBar');
  const val = document.getElementById('servoVal');
  if (!bar || !val) return;
  const pct = ((deg - minDeg) / (maxDeg - minDeg)) * 100;
  bar.style.width = pct.toFixed(1) + '%';
  val.textContent = deg + '°';
}
 
// ir indicator
function setIr(id, on) {
  const el = document.getElementById(id);
  if (!el) return;
  on ? el.classList.add('on') : el.classList.remove('on');
}
 
// controller status
function setCtrl(connected) {
  const el = document.getElementById('ctrl');
  if (!el) return;
  el.textContent = connected ? 'YES' : 'NO';
  el.className = 'stat-value ' + (connected ? 'green' : 'red');
}
 
// mode label
function setMode(auto_) {
  const el = document.getElementById('mode');
  if (!el) return;
  el.textContent = auto_ ? 'AUTO' : 'MAN';
  el.className = 'stat-value ' + (auto_ ? 'amber' : 'accent');
}
 
// websocket
let ws;
function connect() {
  const banner = document.getElementById('conn-banner');
  ws = new WebSocket('ws://' + location.hostname + ':81/');
 
  ws.onopen = () => { banner.classList.remove('show'); };
  ws.onclose = () => { banner.classList.add('show'); setTimeout(connect, 800); };
 
  ws.onmessage = (ev) => {
    const s = ('' + ev.data).trim();
    document.getElementById('raw').textContent = s;
 
    const f = s.split(',');
    if (f.length !== 15) return;
 
    const [mode, ctrl, irL, irC, irR, rpmL10, rpmR10,
           dist10, hdg10, servo, cmdL, cmdR,
           autoBase, autoTurnSoft, assist] = f.map(Number);
 
    const rL = rpmL10 / 10;
    const rR = rpmR10 / 10;
    const dist = dist10 / 10;
    const hdg = hdg10 / 10;
 
    setMode(mode === 1);
    setCtrl(ctrl === 1);
    setIr('irL', irL === 1);
    setIr('irC', irC === 1);
    setIr('irR', irR === 1);
 
    document.getElementById('rpmL').textContent  = rL.toFixed(1);
    document.getElementById('rpmR').textContent  = rR.toFixed(1);
    document.getElementById('rpmL2').textContent = rL.toFixed(1);
    document.getElementById('rpmR2').textContent = rR.toFixed(1);
    document.getElementById('dist').textContent  = dist.toFixed(0);
    document.getElementById('hdg').textContent   = hdg.toFixed(1);
 
    setMotorBar('barL', 'valL', cmdL);
    setMotorBar('barR', 'valR', cmdR);
    setServoBar(servo, 165, 180);
 
    // tuning bars
    const basePct = (autoBase / 1023 * 100).toFixed(1);
    const turnPct = (autoTurnSoft / 1023 * 100).toFixed(1);
    document.getElementById('autoBaseBar').style.width = basePct + '%';
    document.getElementById('autoTurnBar').style.width = turnPct + '%';
    document.getElementById('autoBaseVal').textContent = autoBase;
    document.getElementById('autoTurnVal').textContent = autoTurnSoft;
 
    // assist state
    const badge = document.getElementById('assistBadge');
    if (badge) {
      if (assist === 1) {
        badge.textContent = 'ACTIVE';
        badge.classList.add('active');
      } else {
        badge.textContent = 'IDLE';
        badge.classList.remove('active');
      }
    }
 
    // update history
    histL[histIdx % N] = rL;
    histR[histIdx % N] = rR;
    histIdx++;
 
    drawChart('chartL', histL, '#00e5ff');
    drawChart('chartR', histR, '#1ee36a');
    drawCompass(hdg);
  };
}
 
// initial draw
drawCompass(0);
connect();
</script>
</body>
</html>
)=====";