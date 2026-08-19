#pragma once
/* ============================================================================
 *  หน้าเว็บ Dashboard ทั้งหมด เก็บไว้ใน PROGMEM (หน่วยความจำ flash)
 *
 *  ทำไมต้องเก็บใน PROGMEM
 *    ESP32 มี RAM ราว 320 KB แต่มี flash ตั้ง 4 MB
 *    ถ้าเก็บ HTML ไว้ใน RAM ธรรมดา จะกินพื้นที่ที่ควรเหลือไว้ให้ WiFi และ MQTT
 *    คำสั่ง PROGMEM สั่งให้เก็บไว้ใน flash แล้วอ่านตอนส่งออกไปเท่านั้น
 *
 *  หน้านี้ไม่เรียกไฟล์จากภายนอกเลย (ไม่มี CDN) เพราะโรงเรือนอาจไม่มีอินเทอร์เน็ต
 *  CSS และ JavaScript จึงฝังมาในไฟล์เดียวทั้งหมด
 * ==========================================================================*/

const char DASHBOARD_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Farm Dashboard</title>
<style>
:root{
  --bg:#0b1220; --bg2:#111c33; --card:rgba(255,255,255,.05);
  --line:rgba(255,255,255,.10); --txt:#e8eef9; --dim:#8ea0bd;
  --acc:#38bdf8; --ok:#22c55e; --off:#475569; --warn:#f59e0b; --bad:#ef4444;
  --temp:#fb7185; --humi:#38bdf8;
}
*{box-sizing:border-box;margin:0;padding:0}
body{
  font-family:'Segoe UI',system-ui,-apple-system,'Sarabun',sans-serif;
  background:radial-gradient(1200px 600px at 20% -10%,#1e3a5f 0%,var(--bg) 55%),var(--bg);
  color:var(--txt);min-height:100vh;padding:18px;
}
.wrap{max-width:1100px;margin:0 auto}

/* ---------- หัวเรื่อง ---------- */
header{
  display:flex;flex-wrap:wrap;align-items:center;gap:14px;
  padding:18px 22px;margin-bottom:18px;border-radius:18px;
  background:linear-gradient(135deg,rgba(56,189,248,.14),rgba(255,255,255,.03));
  border:1px solid var(--line);
}
h1{font-size:1.35rem;font-weight:700;letter-spacing:.3px}
h1 span{color:var(--acc)}
.clock{margin-left:auto;text-align:right;line-height:1.2}
.clock b{font-size:1.7rem;font-variant-numeric:tabular-nums;letter-spacing:1px}
.clock small{color:var(--dim);font-size:.78rem}
.pills{display:flex;gap:8px;flex-wrap:wrap}
.pill{
  font-size:.72rem;padding:5px 11px;border-radius:999px;
  border:1px solid var(--line);background:rgba(0,0,0,.25);color:var(--dim);
}
.pill.on{background:rgba(34,197,94,.16);border-color:rgba(34,197,94,.45);color:#86efac}
.pill.no{background:rgba(239,68,68,.16);border-color:rgba(239,68,68,.45);color:#fca5a5}

/* ---------- การ์ดทั่วไป ---------- */
.grid{display:grid;gap:16px}
.g2{grid-template-columns:repeat(auto-fit,minmax(240px,1fr))}
.g3{grid-template-columns:repeat(auto-fit,minmax(310px,1fr))}
.card{
  background:var(--card);border:1px solid var(--line);border-radius:18px;
  padding:18px 20px;backdrop-filter:blur(6px);
}
.card h3{font-size:.82rem;font-weight:600;color:var(--dim);
  text-transform:uppercase;letter-spacing:1.1px;margin-bottom:12px}

/* ---------- การ์ดค่าเซนเซอร์ ---------- */
.metric{display:flex;align-items:flex-end;gap:10px}
.metric .val{font-size:3.1rem;font-weight:700;line-height:1;font-variant-numeric:tabular-nums}
.metric .unit{font-size:1.1rem;color:var(--dim);padding-bottom:6px}
.t .val{color:var(--temp)} .h .val{color:var(--humi)}
.bar{height:6px;border-radius:99px;background:rgba(255,255,255,.08);margin-top:14px;overflow:hidden}
.bar i{display:block;height:100%;border-radius:99px;transition:width .6s ease}
.t .bar i{background:linear-gradient(90deg,#fbbf24,var(--temp))}
.h .bar i{background:linear-gradient(90deg,#22d3ee,var(--humi))}

/* ---------- การ์ดรีเลย์ ---------- */
.rhead{display:flex;align-items:center;gap:10px;margin-bottom:14px}
.rhead h2{font-size:1.05rem;font-weight:700}
.badge{
  margin-left:auto;font-size:.72rem;font-weight:700;letter-spacing:.6px;
  padding:5px 13px;border-radius:999px;background:var(--off);color:#cbd5e1;
}
.badge.on{background:var(--ok);color:#052e16;box-shadow:0 0 16px rgba(34,197,94,.5)}
.tabs{display:flex;gap:6px;background:rgba(0,0,0,.28);padding:4px;border-radius:12px;margin-bottom:14px}
.tabs button{
  flex:1;padding:8px 4px;font-size:.8rem;font-weight:600;cursor:pointer;
  border:0;border-radius:9px;background:transparent;color:var(--dim);
  transition:.18s;font-family:inherit;
}
.tabs button.sel{background:var(--acc);color:#03202e}
.tabs button:hover:not(.sel){color:var(--txt);background:rgba(255,255,255,.06)}
.pane{display:none;animation:fade .25s ease}
.pane.show{display:block}
@keyframes fade{from{opacity:0;transform:translateY(-4px)}to{opacity:1}}

.row{display:flex;align-items:center;gap:9px;margin-bottom:10px;flex-wrap:wrap}
.row label{font-size:.8rem;color:var(--dim);min-width:74px}
input,select{
  background:rgba(0,0,0,.32);border:1px solid var(--line);border-radius:9px;
  color:var(--txt);padding:8px 10px;font-size:.88rem;font-family:inherit;
}
input:focus,select:focus{outline:0;border-color:var(--acc)}
input[type=number]{width:88px}
input[type=time]{width:118px}
select{flex:1;min-width:96px}

.btn{
  width:100%;padding:11px;margin-top:6px;border:0;border-radius:11px;cursor:pointer;
  font-size:.88rem;font-weight:700;font-family:inherit;transition:.18s;
  background:var(--acc);color:#03202e;
}
.btn:hover{filter:brightness(1.12)}
.btn.gray{background:rgba(255,255,255,.09);color:var(--txt)}
.toggle{display:flex;gap:9px}
.toggle .btn{margin-top:0}
.btn.red{background:var(--bad);color:#fff}
.hint{font-size:.74rem;color:var(--dim);margin-top:9px;line-height:1.5}

/* ---------- ส่วน MQTT ---------- */
.topics{display:grid;gap:7px}
.topic{
  display:flex;align-items:center;gap:10px;font-size:.78rem;
  background:rgba(0,0,0,.28);border:1px solid var(--line);
  border-radius:9px;padding:8px 12px;
}
.topic .k{color:var(--dim);min-width:88px;flex-shrink:0}
.topic .v{font-family:Consolas,monospace;color:#7dd3fc;word-break:break-all}
.topic.cmd .v{color:#fcd34d}
.meta{display:flex;flex-wrap:wrap;gap:18px;font-size:.78rem;color:var(--dim);margin-bottom:14px}
.meta b{color:var(--txt);font-family:Consolas,monospace}

footer{text-align:center;color:var(--dim);font-size:.75rem;padding:22px 0 8px}
.dead{opacity:.45;pointer-events:none}
</style>
</head>
<body>
<div class="wrap">

  <header>
    <div>
      <h1>Smart<span>Farm</span> Dashboard</h1>
      <div class="pills" style="margin-top:9px">
        <span class="pill" id="pWifi">WiFi</span>
        <span class="pill" id="pMqtt">MQTT</span>
        <span class="pill" id="pSens">Sensor</span>
        <span class="pill" id="pIp">IP</span>
      </div>
    </div>
    <div class="clock">
      <b id="clk">--:--:--</b><br>
      <small id="dat">รอเวลาจาก NTP</small>
    </div>
  </header>

  <!-- ---------- ค่าเซนเซอร์ ---------- -->
  <div class="grid g2" style="margin-bottom:16px">
    <div class="card t">
      <h3>อุณหภูมิ</h3>
      <div class="metric"><span class="val" id="vT">--</span><span class="unit">&deg;C</span></div>
      <div class="bar"><i id="bT" style="width:0"></i></div>
    </div>
    <div class="card h">
      <h3>ความชื้นสัมพัทธ์</h3>
      <div class="metric"><span class="val" id="vH">--</span><span class="unit">%RH</span></div>
      <div class="bar"><i id="bH" style="width:0"></i></div>
    </div>
  </div>

  <!-- ---------- รีเลย์ ---------- -->
  <div class="grid g3" id="relays" style="margin-bottom:16px"></div>

  <!-- ---------- MQTT ---------- -->
  <div class="card">
    <h3>MQTT Topic</h3>
    <div class="meta">
      <span>Broker <b id="mHost">-</b></span>
      <span>Client ID <b id="mCid">-</b></span>
      <span>ส่งสำเร็จ <b id="mTx">0</b> ครั้ง</span>
    </div>
    <div class="topics" id="topics"></div>
    <p class="hint">
      แถบสีฟ้าคือ topic ที่บอร์ดส่งข้อมูลออกไป &nbsp;|&nbsp;
      แถบสีเหลืองคือ topic ที่รับคำสั่งเข้ามา ส่งค่า <b>on</b> / <b>off</b> / <b>toggle</b> ได้
    </p>
  </div>

  <footer>ESP32 Devkit V2 &middot; Lab10 &middot; uptime <span id="up">0</span></footer>
</div>

<script>
const MODE = ['manual','schedule','auto'];
const MODE_TH = ['สั่งเอง','ตั้งเวลา','อัตโนมัติ'];
let built = false;

/* สร้างการ์ดรีเลย์ครั้งเดียวตอนโหลดหน้า
   ถ้าสร้างใหม่ทุกครั้งที่อัปเดตข้อมูล ค่าที่ผู้ใช้กำลังพิมพ์อยู่จะถูกล้างทิ้ง */
function buildCards(rs){
  document.getElementById('relays').innerHTML = rs.map(r=>`
   <div class="card" id="c${r.id}">
    <div class="rhead">
      <h2>Relay ${r.id}</h2><span class="badge" id="bd${r.id}">OFF</span>
    </div>

    <div class="tabs" id="tb${r.id}">
      ${MODE.map((m,i)=>`<button onclick="pick(${r.id},${i})">${MODE_TH[i]}</button>`).join('')}
    </div>

    <div class="pane" id="p${r.id}0">
      <div class="toggle">
        <button class="btn" onclick="cmd(${r.id},'on')">เปิด</button>
        <button class="btn gray" onclick="cmd(${r.id},'off')">ปิด</button>
      </div>
      <p class="hint">สั่งงานด้วยมือ ระบบจะตัดให้อัตโนมัติถ้าเปิดค้างเกินเวลาที่ตั้งไว้</p>
    </div>

    <div class="pane" id="p${r.id}1">
      <div class="row"><label>เริ่มเวลา</label>
        <input type="time" id="tm${r.id}" value="06:00"></div>
      <div class="row"><label>นานกี่นาที</label>
        <input type="number" id="rn${r.id}" min="1" max="720" value="15"></div>
      <button class="btn" onclick="save(${r.id})">บันทึกตารางเวลา</button>
      <p class="hint">ทำงานซ้ำทุกวันตามเวลาที่ตั้ง ใช้นาฬิกาจริงจาก NTP</p>
    </div>

    <div class="pane" id="p${r.id}2">
      <div class="row"><label>ดูค่าจาก</label>
        <select id="sc${r.id}"><option value="0">อุณหภูมิ</option><option value="1">ความชื้น</option></select></div>
      <div class="row"><label>เงื่อนไข</label>
        <select id="ab${r.id}"><option value="1">มากกว่า</option><option value="0">น้อยกว่า</option></select>
        <input type="number" id="th${r.id}" step="0.5" value="32"></div>
      <div class="row"><label>ช่วงหน่วง</label>
        <input type="number" id="hy${r.id}" step="0.5" min="0.5" value="1">
        <span class="hint" style="margin:0">กันรีเลย์สั่นรอบจุดตัด</span></div>
      <button class="btn" onclick="save(${r.id})">บันทึกเงื่อนไข</button>
    </div>
   </div>`).join('');
  built = true;
}

/* สลับแท็บโหมด แล้วบันทึกโหมดใหม่ขึ้นบอร์ดทันที */
function pick(id,m){
  showPane(id,m);
  fetch('/api/config',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:`id=${id}&mode=${m}`}).then(load);
}
function showPane(id,m){
  document.querySelectorAll(`#tb${id} button`).forEach((b,i)=>b.classList.toggle('sel',i===m));
  for(let i=0;i<3;i++) document.getElementById(`p${id}${i}`).classList.toggle('show',i===m);
}

function cmd(id,s){
  fetch(`/api/relay?id=${id}&state=${s}`,{method:'POST'}).then(load);
}

function save(id){
  const t = document.getElementById('tm'+id).value.split(':');
  const q = new URLSearchParams({
    id:id,
    onH:t[0], onM:t[1],
    runMin:document.getElementById('rn'+id).value,
    src:document.getElementById('sc'+id).value,
    above:document.getElementById('ab'+id).value,
    thr:document.getElementById('th'+id).value,
    hyst:document.getElementById('hy'+id).value
  });
  fetch('/api/config',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:q.toString()}).then(load);
}

function pill(el,ok,txt){
  el.className = 'pill ' + (ok?'on':'no');
  el.textContent = txt;
}

async function load(){
  let d;
  try{ d = await (await fetch('/api/status')).json(); }
  catch(e){ document.body.classList.add('dead'); return; }
  document.body.classList.remove('dead');

  // ----- นาฬิกาและสถานะ -----
  document.getElementById('clk').textContent = d.synced ? d.time.substr(11) : '--:--:--';
  document.getElementById('dat').textContent = d.synced ? d.time.substr(0,10) : 'รอเวลาจาก NTP';
  pill(document.getElementById('pWifi'), d.wifi, 'WiFi ' + (d.wifi ? d.rssi+' dBm' : 'หลุด'));
  pill(document.getElementById('pMqtt'), d.mqtt, 'MQTT ' + (d.mqtt ? 'ออนไลน์' : 'หลุด'));
  pill(document.getElementById('pSens'), d.sensorOK, 'Sensor ' + (d.sensorOK ? 'ปกติ' : 'ผิดพลาด'));
  document.getElementById('pIp').className = 'pill';
  document.getElementById('pIp').textContent = d.ip;

  // ----- ค่าเซนเซอร์ -----
  document.getElementById('vT').textContent = d.sensorOK ? d.temp.toFixed(1) : '--';
  document.getElementById('vH').textContent = d.sensorOK ? d.humi.toFixed(0) : '--';
  document.getElementById('bT').style.width = d.sensorOK ? Math.min(100,Math.max(0,d.temp/50*100))+'%' : '0';
  document.getElementById('bH').style.width = d.sensorOK ? d.humi+'%' : '0';

  // ----- รีเลย์ -----
  if(!built) {
    buildCards(d.relays);
    d.relays.forEach(r=>{
      document.getElementById('tm'+r.id).value =
        String(r.onH).padStart(2,'0')+':'+String(r.onM).padStart(2,'0');
      document.getElementById('rn'+r.id).value = r.runMin;
      document.getElementById('sc'+r.id).value = r.src;
      document.getElementById('ab'+r.id).value = r.above?1:0;
      document.getElementById('th'+r.id).value = r.thr;
      document.getElementById('hy'+r.id).value = r.hyst;
      showPane(r.id, r.mode);
    });
  }
  d.relays.forEach(r=>{
    const b = document.getElementById('bd'+r.id);
    b.className = 'badge' + (r.on?' on':'');
    // เกิน 99 วินาทีให้แสดงเป็นนาที ไม่งั้นจะขึ้นเลขยาวอย่าง "ON 1800s" อ่านยาก
    const rm = r.remain>99 ? Math.ceil(r.remain/60)+' น.' : r.remain+' วิ';
    b.textContent = r.on ? (r.remain>0 ? 'ON '+rm : 'ON') : 'OFF';
  });

  // ----- MQTT -----
  document.getElementById('mHost').textContent = d.mq.host+':'+d.mq.port;
  document.getElementById('mCid').textContent = d.mq.clientId;
  document.getElementById('mTx').textContent  = d.tx;
  document.getElementById('topics').innerHTML =
    d.mq.pub.map(t=>`<div class="topic"><span class="k">ส่งออก</span><span class="v">${t}</span></div>`).join('')+
    d.mq.sub.map(t=>`<div class="topic cmd"><span class="k">รับคำสั่ง</span><span class="v">${t}</span></div>`).join('');

  // ----- footer -----
  const s=d.uptime, hh=Math.floor(s/3600), mm=Math.floor(s%3600/60);
  document.getElementById('up').textContent = hh+' ชม. '+mm+' นาที';
}

load();
setInterval(load, 2000);
</script>
</body>
</html>
)HTMLPAGE";
