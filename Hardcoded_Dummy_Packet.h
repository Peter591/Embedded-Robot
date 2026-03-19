<script>
const N = 200;
const histL = new Float32Array(N);
const histR = new Float32Array(N);
let histIdx = 0;

function drawChart(canvasId, arr, color) {
  const c = document.getElementById(canvasId);
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width, h = c.height;
  ctx.clearRect(0, 0, w, h);

  ctx.fillStyle = '#0a0c0e';
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = '#1a2530';
  ctx.lineWidth = 1;
  for (let i = 1; i < 5; i++) {
    const y = (h * i) / 5;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(w, y);
    ctx.stroke();
  }

  ctx.strokeStyle = '#2a3a4a';
  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();

  let maxV = 1;
  for (let i = 0; i < N; i++) {
    const a = Math.abs(arr[i]);
    if (a > maxV) maxV = a;
  }
  maxV = Math.ceil(maxV / 10) * 10 || 1;

  ctx.fillStyle = '#3a5060';
  ctx.font = '10px Share Tech Mono, monospace';
  ctx.fillText('±' + maxV + ' RPM', 6, 14);

  const grad = ctx.createLinearGradient(0, 0, 0, h);
  grad.addColorStop(0, color + '55');
  grad.addColorStop(0.5, color + '22');
  grad.addColorStop(1, 'transparent');

  ctx.beginPath();
  for (let i = 0; i < N; i++) {
    const di = (histIdx - N + i + N) % N;
    const x = (i / (N - 1)) * w;
    const y = h / 2 - (arr[di] / maxV) * (h * 0.45);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.lineTo(w, h / 2);
  ctx.lineTo(0, h / 2);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  ctx.beginPath();
  for (let i = 0; i < N; i++) {
    const di = (histIdx - N + i + N) % N;
    const x = (i / (N - 1)) * w;
    const y = h / 2 - (arr[di] / maxV) * (h * 0.45);
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.strokeStyle = color;
  ctx.lineWidth = 1.5;
  ctx.stroke();
}

function drawCompass(deg) {
  const c = document.getElementById('compass');
  if (!c) return;
  const ctx = c.getContext('2d');
  const w = c.width, h = c.height;
  const cx = w / 2, cy = h / 2, r = (w / 2) - 4;

  ctx.clearRect(0, 0, w, h);

  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.strokeStyle = '#2e3d50';
  ctx.lineWidth = 1;
  ctx.stroke();

  ctx.strokeStyle = '#3a5060';
  ctx.lineWidth = 1;
  for (let i = 0; i < 8; i++) {
    const a = (i / 8) * Math.PI * 2;
    const inner = r - (i % 2 === 0 ? 8 : 4);
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(a) * inner, cy + Math.sin(a) * inner);
    ctx.lineTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r);
    ctx.stroke();
  }

  ctx.fillStyle = '#00e5ff';
  ctx.font = 'bold 9px Barlow Condensed, sans-serif';
  ctx.textAlign = 'center';
  ctx.fillText('N', cx, cy - r + 10);

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

  ctx.beginPath();
  ctx.arc(cx, cy, 3, 0, Math.PI * 2);
  ctx.fillStyle = '#ffb700';
  ctx.fill();
}

function setMotorBar(fillId, valId, cmd) {
  const fill = document.getElementById(fillId);
  const val = document.getElementById(valId);
  if (!fill || !val) return;

  const pct = (Math.abs(cmd) / 1023) * 50;
  const fwd = cmd >= 0;

  fill.style.left = fwd ? '50%' : (50 - pct) + '%';
  fill.style.width = pct + '%';
  fill.style.background = fwd ? '#00e5ff' : '#ff3d5a';
  val.textContent = cmd;
}

function setServoBar(deg, minDeg, maxDeg) {
  const bar = document.getElementById('servoBar');
  const val = document.getElementById('servoVal');
  if (!bar || !val) return;

  const pct = ((deg - minDeg) / (maxDeg - minDeg)) * 100;
  bar.style.width = pct.toFixed(1) + '%';
  val.textContent = deg + '°';
}

function setIr(id, on) {
  const el = document.getElementById(id);
  if (!el) return;
  if (on) el.classList.add('on');
  else el.classList.remove('on');
}

function setCtrl(connected) {
  const el = document.getElementById('ctrl');
  if (!el) return;
  el.textContent = connected ? 'YES' : 'NO';
  el.className = 'stat-value ' + (connected ? 'green' : 'red');
}

function setMode(auto_) {
  const el = document.getElementById('mode');
  if (!el) return;
  el.textContent = auto_ ? 'AUTO' : 'MAN';
  el.className = 'stat-value ' + (auto_ ? 'amber' : 'accent');
}

function applyDummyPacket(s) {
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

  document.getElementById('rpmL').textContent = rL.toFixed(1);
  document.getElementById('rpmR').textContent = rR.toFixed(1);
  document.getElementById('rpmL2').textContent = rL.toFixed(1);
  document.getElementById('rpmR2').textContent = rR.toFixed(1);
  document.getElementById('dist').textContent = dist.toFixed(0);
  document.getElementById('hdg').textContent = hdg.toFixed(1);

  setMotorBar('barL', 'valL', cmdL);
  setMotorBar('barR', 'valR', cmdR);
  setServoBar(servo, 165, 180);

  const basePct = (autoBase / 1023 * 100).toFixed(1);
  const turnPct = (autoTurnSoft / 1023 * 100).toFixed(1);
  document.getElementById('autoBaseBar').style.width = basePct + '%';
  document.getElementById('autoTurnBar').style.width = turnPct + '%';
  document.getElementById('autoBaseVal').textContent = autoBase;
  document.getElementById('autoTurnVal').textContent = autoTurnSoft;

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

  histL[histIdx % N] = rL;
  histR[histIdx % N] = rR;
  histIdx++;

  drawChart('chartL', histL, '#00e5ff');
  drawChart('chartR', histR, '#1ee36a');
  drawCompass(hdg);
}

drawCompass(0);

const dummyData = '1,1,1,0,1,145,138,5230,-125,172,650,610,720,210,1';
applyDummyPacket(dummyData);
</script>