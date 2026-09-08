#pragma once

// Embedded control page, served from flash (PROGMEM). Multi-bulb UI:
// per-bulb cards with power, brightness, white temperature and RGB color,
// plus pairing, rename/remove and system status.

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="icon" href="data:,">
<title>esp32-zigbee-bulb</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--card2:#1d242e;--txt:#e8ecf2;--mut:#8b95a5;
--acc:#4da3ff;--ok:#37c26e;--bad:#e5534b;--line:#252d3a}
*{box-sizing:border-box;margin:0;padding:0}
[hidden]{display:none!important}
.masterrow{margin:10px 0 2px}
.masterrow .toggle{width:44px;height:24px}
body{background:var(--bg);color:var(--txt);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:14px;max-width:1100px;margin:auto}
header{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:14px}
h1{font-size:19px;font-weight:650;letter-spacing:.2px}
.spacer{flex:1}
button{font:inherit;color:var(--txt);background:var(--card2);border:1px solid var(--line);border-radius:9px;padding:8px 14px;cursor:pointer;transition:filter .15s}
button:hover{filter:brightness(1.2)}
button.primary{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
button.danger{color:var(--bad);border-color:#3a2626}
.status{display:flex;gap:8px;align-items:center;color:var(--mut);font-size:13px;cursor:pointer;width:100%}
.dot{width:9px;height:9px;border-radius:50%;background:var(--mut);flex:none}
.dot.on{background:var(--ok)}.dot.off{background:var(--bad)}
#lights{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:12px}
#sceneBar{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:12px;padding:10px;background:var(--card);border:1px solid var(--line);border-radius:12px}
.stitle{color:var(--mut);font-size:12px;text-transform:uppercase;letter-spacing:.5px;margin-right:4px}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--card2);border:1px solid var(--line);border-radius:20px;padding:5px 6px 5px 12px;margin:2px}
.chip button.play{background:none;border:none;padding:2px 4px;color:var(--acc);font-weight:700}
.chip button.del{background:none;border:none;padding:2px 6px;color:var(--mut)}
.chip button.del:hover{color:var(--bad)}
button.small{padding:5px 12px;font-size:13px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;display:flex;flex-direction:column;gap:10px}
.card.offline{opacity:.55}
.cardtop{display:flex;align-items:center;gap:8px}
.bname{font-weight:600;font-size:16px;cursor:pointer;border:none;background:none;color:var(--txt);padding:2px 4px;border-radius:6px;text-align:left}
.bname:hover{background:var(--card2)}
.badge{font-size:11px;padding:2px 8px;border-radius:20px;border:1px solid var(--line);color:var(--mut)}
.badge.on{color:var(--ok);border-color:var(--ok)}
.rmx{margin-left:auto;background:none;border:none;color:var(--mut);font-size:16px;padding:2px 8px;border-radius:8px}
.rmx:hover{color:var(--bad);background:var(--card2)}
.row{display:flex;align-items:center;gap:10px}
.mode{display:flex;background:var(--card2);border:1px solid var(--line);border-radius:9px;overflow:hidden}
.mode button{border:none;border-radius:0;padding:7px 12px;background:none;color:var(--mut)}
.mode button.sel{background:var(--acc);color:#04121f;font-weight:600}
input[type=range]{flex:1;accent-color:var(--acc)}
input[type=color]{width:46px;height:34px;border:1px solid var(--line);border-radius:9px;background:var(--card2);padding:2px;cursor:pointer}
.hex{font-size:12px;color:var(--mut);min-width:58px}
label.klbl{display:flex;justify-content:space-between;color:var(--mut);font-size:12px;margin-top:-4px}
.toggle{position:relative;width:52px;height:28px;flex:none}
.toggle input{opacity:0;width:0;height:0}
.sl{position:absolute;inset:0;background:var(--card2);border:1px solid var(--line);border-radius:20px;transition:.15s;cursor:pointer}
.sl:before{content:"";position:absolute;width:20px;height:20px;border-radius:50%;background:var(--mut);top:3px;left:4px;transition:.15s}
.toggle input:checked+.sl{background:var(--ok);border-color:var(--ok)}
.toggle input:checked+.sl:before{background:#fff;transform:translateX(22px)}
dialog{background:var(--card);color:var(--txt);border:1px solid var(--line);border-radius:14px;padding:18px;max-width:340px;width:90%}
dialog::backdrop{background:#000a}
dialog h2{font-size:16px;margin-bottom:10px}
dialog input[type=text]{width:100%;font:inherit;background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:9px;padding:8px 10px;margin:6px 0 12px}
.drow{display:flex;gap:8px;justify-content:flex-end;margin-top:12px}
.kv{color:var(--mut);font-size:13px;line-height:1.7}
.empty{grid-column:1/-1;text-align:center;color:var(--mut);padding:40px 0;border:1px dashed var(--line);border-radius:14px}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--card2);border:1px solid var(--line);padding:9px 16px;border-radius:10px;font-size:14px;opacity:0;transition:.25s;pointer-events:none}
.toast.show{opacity:1}
</style>
</head>
<body>
<header>
  <h1>esp32-zigbee-bulb</h1>
  <span class="spacer"></span>
  <button id="pairBtn" class="primary">Aggiungi lampadina</button>
  <button id="setBtn" title="Impostazioni">&#9881;</button>
</header>
<div class="status" id="statusLine">
  <span class="dot" id="sdot"></span><span id="stext">connessione&hellip;</span>
</div>
<div class="row masterrow">
  <span class="stitle">Tutte</span>
  <label class="toggle"><input type="checkbox" id="masterPw" checked><span class="sl"></span></label>
</div>
<div id="sceneBar">
  <span class="stitle">Scene</span>
  <span id="chips"></span>
  <button id="sceneAdd" class="small">+ Salva scena</button>
</div>
<main id="lights"></main>

<dialog id="dlg"><h2 id="dlgTitle"></h2><div id="dlgBody"></div>
  <div class="drow"><button id="dlgCancel">Annulla</button><button id="dlgOk" class="primary">OK</button></div>
</dialog>
<div class="toast" id="toast"></div>

<script>
'use strict';
const $=id=>document.getElementById(id);
let lights=[],pollTimer=null,modeCache={},lastSig='',masterBusy=0;

function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2200)}
async function api(path,opts){const r=await fetch(path,opts);const j=await r.json().catch(()=>({}));
 if(!r.ok){toast(j.error||('Errore '+r.status));throw new Error(j.error||r.status)}return j}

async function load(){
 try{
  lights=await api('/api/lights');
  const sc=await api('/api/scenes');
  const s=await api('/api/status');
  $('sdot').className='dot '+(s.wifi?'on':'off');
  const heap=(s.free_heap/1024).toFixed(0);
  $('stext').textContent=`${s.bulbs} lampadine · ${s.ip} · ${s.rssi} dBm · heap ${heap} kB`;
  renderScenes(sc);
  render();
 }catch(e){}
}

function renderScenes(sc){
 const bar=$('sceneBar');   // Always visible: the save button must be reachable
 bar.hidden=false;          // even with zero scenes.
 if(!sc.length){$('chips').innerHTML='';bar.dataset.sig='';return}
 const sig=sc.map(s=>s.name).join('|');
 if(sig===bar.dataset.sig)return;
 bar.dataset.sig=sig;
 const box=$('chips');box.innerHTML='';
 for(const s of sc){
  const chip=document.createElement('span');chip.className='chip';
  const play=document.createElement('button');play.className='play';play.title='Richiama';
  play.innerHTML='&#9654;';
  play.onclick=async()=>{const r=await api('/api/scenes/'+s.name+'/recall',{method:'POST'});toast(`Scena «${s.name}»: ${r.applied} lampadine`)};
  const nm=document.createElement('span');nm.textContent=s.name;
  const del=document.createElement('button');del.className='del';del.title='Elimina';
  del.innerHTML='&#10005;';
  del.onclick=()=>sceneDeleteDialog(s);
  chip.append(play,nm,del);box.append(chip);
 }
}

$('sceneAdd').onclick=()=>{
 openDialog('Salva scena',
  `<label>Nome (a-z, cifre, - _)</label><input type="text" id="scName" maxlength="20" pattern="[a-z0-9_-]+">
   <div class="kv">Registra lo stato attuale di tutte le lampadine.</div>`,
  async()=>{
   const v=$('scName').value.trim().toLowerCase();
   if(!v)return false;
   await api('/api/scenes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:v})});
   toast('Scena «'+v+'» salvata');refreshSoon();
  },'Salva');
 setTimeout(()=>$('scName').focus(),50);
};

function sceneDeleteDialog(s){
 openDialog('Eliminare la scena «'+s.name+'»?','',async()=>{
  await api('/api/scenes/'+s.name,{method:'DELETE'});refreshSoon();
 },'Elimina');
}

function editing(id,field){const e=modeCache[id];return e&&e.f===field&&Date.now()<e.t}
function markEdit(id,field,ms=1200){modeCache[id]={f:field,t:Date.now()+ms}}

function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/"/g,'&quot;')}

function render(){
 const box=$('lights');
 if(!lights.length){box.innerHTML='<div class="empty">Nessuna lampadina collegata.<br>Premi &laquo;Aggiungi lampadina&raquo; e riaccendi la lampadina 6 volte per associarla.</div>';lastSig='empty';return}

 // Rebuild the DOM only when the structure changes (ids, names, online
 // state, mode). Otherwise update values in place, skipping fields the
 // user is currently interacting with, so controls never disappear
 // under a drag or a click.
 const sig=lights.map(L=>`${L.id}|${L.name}|${L.online}|${L.mode}`).join(';');
 if(sig!==lastSig){
  let h='';
  for(const L of lights){
   h+=`<section class="card${L.online?'':' offline'}" data-id="${L.id}">
    <div class="cardtop">
     <button class="bname" title="Rinomina">${esc(L.name)}</button>
     <span class="badge${L.online?' on':''}">${L.online?'online':'offline'}</span>
     <button class="rmx" title="Rimuovi">&#10005;</button>
    </div>
    <div class="row">
     <label class="toggle"><input type="checkbox" class="pw" ${L.on?'checked':''}><span class="sl"></span></label>
     <input type="range" class="bri" min="1" max="100" value="${L.brightness}">
     <span class="hex">${L.brightness}%</span>
    </div>
    <div class="row">
     <div class="mode">
      <button class="mw${L.mode==='white'?' sel':''}">Bianco</button>
      <button class="mc${L.mode==='rgb'?' sel':''}">Colore</button>
     </div>
    </div>
    <div class="row wm"${L.mode==='white'?'':' hidden'}>
     <input type="range" class="kelvin" min="2200" max="4000" step="50" value="${L.kelvin}">
    </div>
    <label class="klbl"${L.mode==='white'?'':' hidden'}><span>2200K caldo</span><span>${L.kelvin}K</span><span>4000K freddo</span></label>
    <div class="row cm"${L.mode==='rgb'?'':' hidden'}>
     <input type="color" class="col" value="${L.rgb_hex}">
     <span class="hex chtxt">${L.rgb_hex.toUpperCase()}</span>
    </div>
   </section>`;
  }
  box.innerHTML=h;
  box.querySelectorAll('.card').forEach(bindCard);
  lastSig=sig;
 }

 for(const L of lights){
  const card=box.querySelector(`.card[data-id="${L.id}"]`);
  if(!card)continue;
  const id=L.id;
  const pw=card.querySelector('.pw');
  if(pw.checked!==L.on&&!editing(id,'pw'))pw.checked=L.on;
  const bri=card.querySelector('.bri');
  if(!editing(id,'bri')){if(+bri.value!==L.brightness)bri.value=L.brightness;card.querySelector('.hex').textContent=L.brightness+'%'}
  const kel=card.querySelector('.kelvin');
  if(!editing(id,'kel')){if(+kel.value!==L.kelvin)kel.value=L.kelvin;const lbl=card.querySelectorAll('.klbl span')[1];if(lbl)lbl.textContent=L.kelvin+'K'}
  const col=card.querySelector('.col');
  if(!editing(id,'col')){if(col.value!==L.rgb_hex)col.value=L.rgb_hex;card.querySelector('.chtxt').textContent=L.rgb_hex.toUpperCase()}
 }
 // Master switch reflects the aggregate; calm it for a moment after use.
 const master=$('masterPw');
 if(Date.now()>masterBusy)master.checked=lights.some(l=>l.on);
}

$('masterPw').onchange=e=>{
 masterBusy=Date.now()+1800;
 api('/api/lights',{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:e.target.checked})}).then(refreshSoon);
};

function bindCard(card){
 const id=card.dataset.id;
 const L=()=>lights.find(x=>x.id===id);

 card.querySelector('.bname').onclick=()=>renameDialog(L());
 card.querySelector('.rmx').onclick=()=>removeDialog(L());
 card.querySelector('.pw').onchange=e=>{markEdit(id,'pw');api('/api/lights/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:e.target.checked})}).then(refreshSoon)};

 const bri=card.querySelector('.bri');
 bri.oninput=()=>{markEdit(id,'bri');card.querySelector('.hex').textContent=bri.value+'%'};
 bri.onchange=()=>api('/api/lights/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({brightness:+bri.value})}).then(refreshSoon);

 card.querySelector('.mw').onclick=()=>patchMode(id,'white');
 card.querySelector('.mc').onclick=()=>patchMode(id,'rgb');

 const kel=card.querySelector('.kelvin');
 kel.oninput=()=>{markEdit(id,'kel');card.querySelectorAll('.klbl span')[1].textContent=kel.value+'K'};
 kel.onchange=()=>api('/api/lights/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({kelvin:+kel.value})}).then(refreshSoon);

 const col=card.querySelector('.col');
 col.oninput=()=>{markEdit(id,'col');card.querySelector('.chtxt').textContent=col.value.toUpperCase()};
 col.onchange=()=>api('/api/lights/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({rgb_hex:col.value})}).then(refreshSoon);
}

function refreshSoon(){clearTimeout(pollTimer);pollTimer=setTimeout(load,400)}

function patchMode(id,mode){
 api('/api/lights/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({mode})}
 ).then(refreshSoon);
}

// --- dialogs ---------------------------------------------------------------
let dlgAction=null;
function openDialog(title,bodyHtml,onOk,okLabel='OK'){$('dlgTitle').textContent=title;$('dlgBody').innerHTML=bodyHtml;$('dlgOk').textContent=okLabel;dlgAction=onOk;$('dlg').showModal()}
$('dlgCancel').onclick=()=>$('dlg').close();
$('dlgOk').onclick=()=>{if(dlgAction&&dlgAction()===false)return;$('dlg').close()};

function renameDialog(L){
 openDialog('Rinomina lampadina',`<input type="text" id="rnIn" maxlength="20" value="${esc(L.name)}">`,()=>{
  const v=$('rnIn').value.trim();if(!v||v===L.name)return;
  api('/api/lights/'+L.id,{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:v})}).then(load);
 });
 setTimeout(()=>$('rnIn').focus(),50);
}

function removeDialog(L){
 openDialog('Rimuovere «'+L.name+'»?','La lampadina verrà dissociata dalla rete Zigbee.',async()=>{
  await api('/api/lights/'+L.id,{method:'DELETE'});load();
 },'Rimuovi');
}

$('pairBtn').onclick=async()=>{
 try{
  await api('/api/pairing',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({seconds:180})});
  countdown(180);
  toast('Rete aperta: riaccendi la lampadina 6 volte (finisce accesa)');
 }catch(e){}
};

function countdown(s){
 const btn=$('pairBtn');btn.disabled=true;
 const h=setInterval(()=>{
  if(s--<=0){clearInterval(h);btn.disabled=false;btn.textContent='Aggiungi lampadina';return}
  btn.textContent='Rete aperta… '+s+' s';
 },1000);
}

$('setBtn').onclick=async()=>{
 const s=await api('/api/status');
 openDialog('Impostazioni',
  `<label>Hostname mDNS</label><input type="text" id="hostIn" maxlength="31" value="${esc(s.hostname)}" pattern="[a-z0-9-]+">
   <div class="kv">Indirizzo: ${s.ip}<br>Rete: accedi a http://${s.hostname}.local/<br>Uptime: ${Math.floor(s.uptime_s/3600)}h ${Math.floor(s.uptime_s%3600/60)}m<br>Heap libero: ${(s.free_heap/1024).toFixed(0)} kB<br>RSSI Wi-Fi: ${s.rssi} dBm<br>Versione: ${s.version}</div>`,
  async()=>{
   const v=$('hostIn').value.trim().toLowerCase();
   if(v&&v!==s.hostname){await api('/api/hostname',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({hostname:v})});toast('Hostname: '+v+'.local')}
  });
};

$('statusLine').onclick=()=>$('setBtn').onclick();

load();
setInterval(load,2500);
</script>
</body>
</html>)rawliteral";
