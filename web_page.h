#pragma once

// Embedded control page, served from flash (PROGMEM). Multi-bulb UI:
// per-bulb cards with power, brightness, white temperature and RGB color,
// scene bar, pairing, and a tabbed settings dialog (system info, Zigbee
// devices, per-bulb diagnostics, remote controls with action-map editor,
// OTA firmware upload).

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<link rel="icon" href="/icon-192.png">
<link rel="apple-touch-icon" href="/icon-192.png">
<meta name="theme-color" content="#0e1116">
<meta name="apple-mobile-web-app-capable" content="yes">
<link rel="manifest" href="/manifest.json">
<title>esp32-zigbee-bulb</title>
<style>
:root{--bg:#0e1116;--card:#171c24;--card2:#1d242e;--txt:#e8ecf2;--mut:#8b95a5;
--acc:#4da3ff;--ok:#37c26e;--bad:#e5534b;--warn:#e5a44b;--line:#252d3a}
*{box-sizing:border-box;margin:0;padding:0}
[hidden]{display:none!important}
:focus-visible{outline:2px solid var(--acc);outline-offset:2px}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
.masterrow{margin:10px 0 2px}
.masterrow .toggle{width:44px;height:24px}
.masterrow.off{opacity:.45;pointer-events:none}
body{background:var(--bg);color:var(--txt);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;padding:14px;max-width:1100px;margin:auto}
header{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:14px}
h1{font-size:19px;font-weight:650;letter-spacing:.2px}
.spacer{flex:1}
button{font:inherit;color:var(--txt);background:var(--card2);border:1px solid var(--line);border-radius:9px;padding:8px 14px;cursor:pointer;transition:filter .15s}
button:hover{filter:brightness(1.2)}
button.primary{background:var(--acc);border-color:var(--acc);color:#04121f;font-weight:600}
button.danger{color:var(--bad);border-color:#3a2626}
.status{display:flex;gap:8px;align-items:center;color:var(--mut);font-size:13px;cursor:pointer;width:100%;flex-wrap:wrap}
.dot{width:9px;height:9px;border-radius:50%;background:var(--mut);flex:none}
.dot.on{background:var(--ok)}.dot.off{background:var(--bad)}
.sbadge{color:var(--acc);font-size:11px;padding:1px 8px}
.sbadge.warn{color:var(--warn)}
#lights{display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:12px}
#sceneBar{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-bottom:12px;padding:10px;background:var(--card);border:1px solid var(--line);border-radius:12px}
.stitle{color:var(--mut);font-size:12px;text-transform:uppercase;letter-spacing:.5px;margin-right:4px}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--card2);border:1px solid var(--line);border-radius:20px;padding:5px 6px 5px 12px;margin:2px}
.chip.active{border-color:var(--acc);color:var(--acc)}
.chip button.play{background:none;border:none;padding:2px 4px;color:var(--acc);font-weight:700}
.chip button.del{background:none;border:none;padding:2px 6px;color:var(--mut)}
.chip button.del:hover{color:var(--bad)}
button.small{padding:5px 12px;font-size:13px}
.tmrleft{color:var(--acc);font-size:12px;font-variant-numeric:tabular-nums}
.logbox{font:11px/1.5 ui-monospace,Consolas,monospace;background:var(--card2);border:1px solid var(--line);border-radius:10px;padding:8px;height:280px;overflow-y:auto;white-space:pre-wrap;word-break:break-word;color:var(--txt);margin-top:8px}
.tmr{background:none;border:none;color:var(--mut);padding:2px 6px;border-radius:8px;font-size:14px;margin-left:auto}
.tmr:hover{color:var(--acc);background:var(--card2)}
.trow{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;display:flex;flex-direction:column;gap:10px}
.card.on{box-shadow:inset 3px 0 0 var(--acc)}
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
input[type=file]{font:inherit;color:var(--mut);font-size:13px;max-width:100%}
select{font:inherit;background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:8px;padding:6px 8px}
.hex{font-size:12px;color:var(--mut);min-width:58px}
label.klbl{display:flex;justify-content:space-between;color:var(--mut);font-size:12px;margin-top:-4px}
.toggle{position:relative;width:52px;height:28px;flex:none}
.toggle input{opacity:0;width:0;height:0}
.sl{position:absolute;inset:0;background:var(--card2);border:1px solid var(--line);border-radius:20px;transition:.15s;cursor:pointer}
.sl:before{content:"";position:absolute;width:20px;height:20px;border-radius:50%;background:var(--mut);top:3px;left:4px;transition:.15s}
.toggle input:checked+.sl{background:var(--ok);border-color:var(--ok)}
.toggle input:checked+.sl:before{background:#fff;transform:translateX(22px)}
dialog{background:var(--card);color:var(--txt);border:1px solid var(--line);border-radius:14px;padding:18px;max-width:340px;width:90%;max-height:85vh;overflow:auto}
dialog#cfg{max-width:440px}
dialog::backdrop{background:#000a}
dialog h2{font-size:16px;margin-bottom:10px}
dialog label{display:block;font-size:13px;color:var(--mut);margin:8px 0 2px}
dialog input[type=text]{width:100%;font:inherit;background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:9px;padding:8px 10px;margin:0 0 10px}
.drow{display:flex;gap:8px;justify-content:flex-end;margin-top:12px}
.kv{color:var(--mut);font-size:13px;line-height:1.7}
.tabs{display:flex;gap:4px;margin-bottom:12px;flex-wrap:wrap}
.tab{padding:5px 9px;font-size:12px;border-radius:8px;background:none;border:1px solid transparent;color:var(--mut)}
.tab.sel{background:var(--card2);border-color:var(--line);color:var(--txt)}
.rrow{display:flex;align-items:center;gap:8px;padding:4px 0}
.rrow .rname{background:none;border:none;color:var(--txt);font-weight:600;padding:2px 6px;border-radius:6px;cursor:pointer}
.rrow .rname:hover{background:var(--card2)}
.rrow .rdel{background:none;border:none;color:var(--mut);padding:2px 8px;border-radius:8px;cursor:pointer}
.rrow .rdel:hover{color:var(--bad);background:var(--card2)}
.arow{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:4px 0;color:var(--mut);font-size:13px}
.arow span{color:var(--txt)}
.pbar{height:6px;background:var(--card2);border-radius:4px;margin:8px 0;overflow:hidden}
.pfill{height:100%;background:var(--acc);width:0;transition:width .2s}
.empty{grid-column:1/-1;text-align:center;color:var(--mut);padding:40px 0;border:1px dashed var(--line);border-radius:14px}
.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:var(--card2);border:1px solid var(--line);padding:9px 16px;border-radius:10px;font-size:14px;opacity:0;transition:.25s;pointer-events:none;max-width:90%}
.toast.show{opacity:1}
</style>
</head>
<body>
<header>
  <h1>esp32-zigbee-bulb</h1>
  <span class="spacer"></span>
  <button id="installBtn" class="primary" hidden>Installa app</button>
  <button id="pairBtn" class="primary">Aggiungi lampadina</button>
  <button id="setBtn" title="Impostazioni" aria-label="Impostazioni">&#9881;</button>
</header>
<div class="status" id="statusLine">
  <span class="dot" id="sdot"></span><span id="stext">connessione&hellip;</span><span id="sbadges"></span>
</div>
<div class="row masterrow" id="masterRow">
  <span class="stitle">Tutte</span>
  <label class="toggle"><input type="checkbox" id="masterPw" checked><span class="sl"></span></label>
  <span class="tmrleft" id="onCount"></span>
  <button id="masterTimer" class="small" title="Timer: spegni tutte">&#9201;&#65039;</button>
  <span class="tmrleft" id="masterTimerLeft" hidden></span>
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

<dialog id="cfg">
 <h2>Impostazioni</h2>
 <div class="tabs" role="tablist">
  <button class="tab sel" data-t="gen">Generale</button>
  <button class="tab" data-t="net">Rete</button>
  <button class="tab" data-t="diag">Diagnostica</button>
  <button class="tab" data-t="log">Log</button>
  <button class="tab" data-t="rem">Telecomandi</button>
  <button class="tab" data-t="fw">Firmware</button>
 </div>
 <div id="t-gen">
  <label>Hostname mDNS</label><input type="text" id="hostIn" maxlength="31" pattern="[a-z0-9-]+">
  <div class="drow" style="justify-content:flex-start;margin-top:0"><button id="hostApply" class="small">Applica</button></div>
  <div class="kv" id="genInfo"></div>
 </div>
 <div id="t-net" hidden><div class="kv" id="devList"></div></div>
 <div id="t-diag" hidden><div class="kv" id="diagList"></div></div>
 <div id="t-log" hidden>
  <div class="drow" style="justify-content:flex-start">
   <label style="display:flex;align-items:center;gap:6px;margin:0"><input type="checkbox" id="logLive" checked> Live</label>
   <button id="logClear" class="small">Pulisci</button>
   <span class="kv" id="logStat"></span>
  </div>
  <div id="logBox" class="logbox"></div>
  <div class="kv" style="margin-top:6px">Registro eventi in RAM (ultime 100 righe, si azzera al riavvio): pairing, comandi Zigbee, OTA, MQTT e webhook.</div>
 </div>
 <div id="t-rem" hidden>
  <div id="remList"></div>
  <div class="kv" style="margin:8px 0 4px"><b>Mappa azioni</b> (globale, valida per tutti i telecomandi)</div>
  <div id="actMap"></div>
  <div class="drow" style="justify-content:flex-start"><button id="actSave" class="small primary">Salva azioni</button></div>
 </div>
 <div id="t-fw" hidden>
  <div class="kv">Carica un firmware <b>.bin</b> compilato per questo progetto. Al termine il dispositivo si riavvia: la pagina non risponde per qualche decina di secondi, poi si aggiorna da sola.</div>
  <input type="file" id="otaFile" accept=".bin" style="margin:10px 0">
  <label>Token OTA (se richiesto dal server)</label><input type="text" id="otaTok" autocomplete="off">
  <label>MD5 atteso (opzionale)</label><input type="text" id="otaMd5" maxlength="32" autocomplete="off">
  <div class="drow" style="justify-content:flex-start"><button id="otaGo" class="primary">Avvia aggiornamento</button><span class="tmrleft" id="otaPct"></span></div>
  <div class="pbar" id="otaBar" hidden><div class="pfill" id="otaFill"></div></div>
  <div class="kv" id="otaMsg"></div>
 </div>
 <div class="drow"><button id="cfgClose">Chiudi</button></div>
</dialog>
<div class="toast" id="toast"></div>

<script>
'use strict';
const $=id=>document.getElementById(id);
let lights=[],pollTimer=null,modeCache={},lastSig='',masterBusy=0,timerAllCache=0,scenesCache=[],lastScene='';
const ACT_LABELS={none:'Nessuna',all_on:'Tutte on',all_off:'Tutte off',toggle_all:'Toggle tutte',brightness_up:'Luminosità +',brightness_down:'Luminosità −',scene_next:'Scena succ.',scene_prev:'Scena prec.'};
const EV_LABELS={on:'Tasto on',off:'Tasto off',toggle:'Centrale (toggle)',move_up:'Anello su',move_down:'Anello giù',stop:'Rilascio anello',step_up:'Step su',step_down:'Step giù',color_a:'Freccia/dir. A',color_b:'Freccia/dir. B'};

function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('show'),2200)}
async function api(path,opts){const r=await fetch(path,opts);const j=await r.json().catch(()=>({}));
 if(!r.ok){toast(j.error||('Errore '+r.status));throw new Error(j.error||r.status)}return j}

async function load(){
 try{
  lights=await api('/api/lights');
  scenesCache=await api('/api/scenes');
  const s=await api('/api/status');
  $('sdot').className='dot '+(s.wifi?'on':'off');
  const heap=(s.free_heap/1024).toFixed(0);
  $('stext').textContent=`${s.bulbs} lampadine · ${s.ip} · ${s.rssi} dBm · heap ${heap} kB`;
  let bh='';
  if(s.pairing_open)bh+='<span class="badge sbadge">rete aperta</span>';
  if(s.ota&&s.ota.pending_verify)bh+='<span class="badge sbadge warn">nuovo fw in verifica</span>';
  $('sbadges').innerHTML=bh;
  timerAllCache=s.timer_all||0;
  const mt=$('masterTimerLeft');
  if(timerAllCache>0){mt.hidden=false;mt.textContent='⏳ '+fmtT(timerAllCache)}
  else mt.hidden=true;
  renderScenes();
  render();
 }catch(e){}
}

function renderScenes(){
 const bar=$('sceneBar');   // Always visible: the save button must be reachable
 bar.hidden=false;          // even with zero scenes.
 const sc=scenesCache;
 if(!sc.length){$('chips').innerHTML='';bar.dataset.sig='';return}
 const sig=sc.map(s=>s.name).join('|');
 if(sig===bar.dataset.sig){paintSceneActive();return}
 bar.dataset.sig=sig;
 const box=$('chips');box.innerHTML='';
 for(const s of sc){
  const chip=document.createElement('span');chip.className='chip';chip.dataset.name=s.name;
  const play=document.createElement('button');play.className='play';play.title='Richiama';play.setAttribute('aria-label','Richiama '+s.name);
  play.innerHTML='&#9654;';
  play.onclick=async()=>{const r=await api('/api/scenes/'+s.name+'/recall',{method:'POST'});lastScene=s.name;paintSceneActive();toast(`Scena «${s.name}»: ${r.applied} lampadine`)};
  const nm=document.createElement('span');nm.textContent=s.name;
  const del=document.createElement('button');del.className='del';del.title='Elimina';del.setAttribute('aria-label','Elimina '+s.name);
  del.innerHTML='&#10005;';
  del.onclick=()=>sceneDeleteDialog(s);
  chip.append(play,nm,del);box.append(chip);
 }
 paintSceneActive();
}

function paintSceneActive(){
 if(!lastScene)return;
 const box=$('chips');
 for(const c of box.children)c.classList.toggle('active',c.dataset.name===lastScene);
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
  await api('/api/scenes/'+s.name,{method:'DELETE'});if(lastScene===s.name)lastScene='';refreshSoon();
 },'Elimina');
}

function editing(id,field){const e=modeCache[id];return e&&e.f===field&&Date.now()<e.t}
function markEdit(id,field,ms=1200){modeCache[id]={f:field,t:Date.now()+ms}}

function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/"/g,'&quot;')}

function render(){
 const box=$('lights');
 $('masterRow').classList.toggle('off',!lights.length);
 const on=lights.filter(l=>l.on).length;
 $('onCount').textContent=on>0?on+' accesa'+(on>1?'e':''):'';
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
     <span class="tmrleft" hidden></span>
     <button class="tmr" title="Timer spegni" aria-label="Timer spegni">&#9201;&#65039;</button>
     <button class="rmx" title="Rimuovi" aria-label="Rimuovi">${'&#10005;'}</button>
    </div>
    <div class="row">
     <label class="toggle"><input type="checkbox" class="pw" ${L.on?'checked':''} aria-label="Accendi"><span class="sl"></span></label>
     <input type="range" class="bri" min="1" max="100" value="${L.brightness}" aria-label="Luminosità">
     <span class="hex">${L.brightness}%</span>
    </div>
    <div class="row">
     <div class="mode">
      <button class="mw${L.mode==='white'?' sel':''}">Bianco</button>
      <button class="mc${L.mode==='rgb'?' sel':''}">Colore</button>
     </div>
    </div>
    <div class="row wm"${L.mode==='white'?'':' hidden'}>
     <input type="range" class="kelvin" min="2200" max="4000" step="50" value="${L.kelvin}" aria-label="Temperatura bianco">
    </div>
    <label class="klbl"${L.mode==='white'?'':' hidden'}><span>2200K caldo</span><span>${L.kelvin}K</span><span>4000K freddo</span></label>
    <div class="row cm"${L.mode==='rgb'?'':' hidden'}>
     <input type="color" class="col" value="${L.rgb_hex}" aria-label="Colore">
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
  card.classList.toggle('on',!!L.on);
  const pw=card.querySelector('.pw');
  if(pw.checked!==L.on&&!editing(id,'pw'))pw.checked=L.on;
  const bri=card.querySelector('.bri');
  if(!editing(id,'bri')){if(+bri.value!==L.brightness)bri.value=L.brightness;card.querySelector('.hex').textContent=L.brightness+'%'}
  const kel=card.querySelector('.kelvin');
  if(!editing(id,'kel')){if(+kel.value!==L.kelvin)kel.value=L.kelvin;const lbl=card.querySelectorAll('.klbl span')[1];if(lbl)lbl.textContent=L.kelvin+'K'}
  const col=card.querySelector('.col');
  if(!editing(id,'col')){if(col.value!==L.rgb_hex)col.value=L.rgb_hex;card.querySelector('.chtxt').textContent=L.rgb_hex.toUpperCase()}
  const tleft=card.querySelector('.tmrleft');
  if(L.timer>0){tleft.hidden=false;tleft.textContent='⏳ '+fmtT(L.timer)}
  else tleft.hidden=true;
 }
 // Master switch reflects the aggregate; calm it for a moment after use.
 const master=$('masterPw');
 if(Date.now()>masterBusy)master.checked=lights.some(l=>l.on);
}

function fmtT(s){
 const h=Math.floor(s/3600),m=Math.floor((s%3600)/60),ss=s%60;
 return h>0?`${h}h ${m}m`:m>0?`${m}m ${ss}s`:`${ss}s`;
}

$('masterPw').onchange=e=>{
 masterBusy=Date.now()+1800;
 api('/api/lights',{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:e.target.checked})}).then(refreshSoon);
};

$('masterTimer').onclick=()=>timerDialog(null,'tutte le lampadine');

function timerDialog(id,label){
 const path=id?`/api/lights/${id}/timer`:'/api/timer';
 const remaining=id?(lights.find(l=>l.id===id)?.timer||0):timerAllCache;
 const set=m=>api(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({seconds:Math.round(m*60)})}).then(()=>{$('dlg').close();refreshSoon()});
 openDialog(`Spegni ${label} tra…`,
  `<div class="trow"><button class="tpre" data-m="10">10 min</button><button class="tpre" data-m="30">30 min</button><button class="tpre" data-m="60">60 min</button></div>
   <div class="row"><input type="number" id="tmMin" min="1" max="1440" style="width:90px"> <span class="kv">min</span> &nbsp;<button id="tmGo" class="primary">Imposta</button></div>
   <div class="drow"${remaining>0?'':' hidden'}><button id="tmCancel" class="danger">Annulla timer (${fmtT(remaining)})</button></div>`,
  ()=>{},'Chiudi');
 document.querySelectorAll('.tpre').forEach(b=>b.onclick=()=>set(+b.dataset.m));
 $('tmGo').onclick=()=>{const v=+$('tmMin').value;if(v>0)set(v)};
 $('tmCancel').onclick=()=>api(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({seconds:0})}).then(()=>{$('dlg').close();refreshSoon()});
}

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

 card.querySelector('.tmr').onclick=()=>timerDialog(id,L().name);
}

function refreshSoon(){clearTimeout(pollTimer);pollTimer=setTimeout(load,400)}

function patchMode(id,mode){
 api('/api/lights/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},
  body:JSON.stringify({mode})}
 ).then(refreshSoon);
}

// --- dialogs ---------------------------------------------------------------
let dlgAction=null;
function openDialog(title,bodyHtml,onOk,okLabel='OK'){$('dlgTitle').textContent=title;$('dlgBody').innerHTML=bodyHtml;$('dlgOk').textContent=okLabel;dlgAction=onOk;$('dlgCancel').textContent='Annulla';$('dlg').showModal()}
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

// --- Settings dialog (tabbed) ------------------------------------------------
function switchTab(t){
 document.querySelectorAll('#cfg .tab').forEach(b=>b.classList.toggle('sel',b.dataset.t===t));
  for(const k of ['gen','net','diag','log','rem','fw'])$('t-'+k).hidden=k!==t;
}
document.querySelectorAll('#cfg .tab').forEach(b=>b.onclick=()=>switchTab(b.dataset.t));
$('cfgClose').onclick=()=>$('cfg').close();
$('hostApply').onclick=async()=>{
 const v=$('hostIn').value.trim().toLowerCase();
 if(!v)return;
 await api('/api/hostname',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({hostname:v})});
 toast('Hostname: '+v+'.local');
};

$('setBtn').onclick=async()=>{
 try{
  const [s,devs,ls,rems,acts]=await Promise.all([
   api('/api/status'),
   api('/api/devices').catch(()=>[]),
   api('/api/lights?debug=1').catch(()=>[]),
   api('/api/remotes').catch(()=>[]),
   api('/api/remotes/actions').catch(()=>({}))
  ]);
  let devRows='';
  for(const d of devs){
   if(!d.ieee)continue;
   const tipo=d.name?'lampadina registrata':d.type==='router'?'lampadina (alimentata)':d.type==='end-device'?'dispositivo a batteria (pulsante/telecomando)':d.type==='bound'?'dispositivo agganciato':'controller';
   const nome=d.name?`<b>${esc(d.name)}</b>`:`Dispositivo ${d.short}`;
   devRows+=`<div>• ${nome} &nbsp;<span class="kv">${tipo} · ${d.short} · ${d.ieee.slice(0,8)}…</span></div>`;
  }
  $('devList').innerHTML=devRows||'(elenco in aggiornamento, riprova tra 30 s)';
  let diagRows='';
  for(const L of ls){
   if(!L.debug)continue;
   const rssi=L.debug.rssi?` · ${L.debug.rssi} dBm`:'';
   const seen=L.debug.last_seen_s<0?'mai vista':'vista '+L.debug.last_seen_s+'s fa';
   const fail=L.debug.cmd_failed>0?` · ${L.debug.cmd_failed} err (${esc(L.debug.last_fail||'?')})`:'';
   diagRows+=`<div>• <b>${esc(L.name)}</b> <span class="kv">· LQI ${L.debug.lqi}${rssi} · ${seen} · ${L.debug.cmd_sent} comandi${fail}</span></div>`;
  }
  $('diagList').innerHTML=diagRows||'(nessuna lampadina registrata)';
  let remRows='';
  for(const r of rems){
   const visto=r.last_seen_s<0?'mai premuto':esc(r.last_event||'?')+' · '+r.last_seen_s+'s fa';
   remRows+=`<div class="rrow"><button class="rname" data-id="${r.id}" data-name="${esc(r.name)}" title="Rinomina">${esc(r.name)}</button>
    <span class="kv">${visto}</span>
    <button class="rdel" data-id="${r.id}" data-name="${esc(r.name)}" title="Dimentica" aria-label="Dimentica telecomando">&#10005;</button></div>`;
  }
  $('remList').innerHTML=remRows||'(premi un pulsante del telecomando per associarlo)';
  $('remList').querySelectorAll('.rname').forEach(b=>b.onclick=()=>remoteRenameDialog(b.dataset.id,b.dataset.name));
  $('remList').querySelectorAll('.rdel').forEach(b=>b.onclick=()=>remoteDeleteDialog(b.dataset.id,b.dataset.name));
  fillActionMap(acts);
  $('genInfo').innerHTML=`Indirizzo: ${s.ip}<br>Rete: accedi a http://${esc(s.hostname)}.local/<br>Uptime: ${Math.floor(s.uptime_s/3600)}h ${Math.floor(s.uptime_s%3600/60)}m<br>Heap libero: ${(s.free_heap/1024).toFixed(0)} kB<br>RSSI Wi-Fi: ${s.rssi} dBm<br>Versione: ${s.version}`;
  $('hostIn').value=s.hostname;
  $('otaMsg').textContent='';
  switchTab('gen');
  $('cfg').showModal();
 }catch(e){}
};

function remoteRenameDialog(id,old){
 openDialog('Rinomina telecomando',`<input type="text" id="rnRem" maxlength="20" value="${esc(old)}">`,async()=>{
  const v=$('rnRem').value.trim();if(!v||v===old)return false;
  await api('/api/remotes/'+id,{method:'PATCH',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:v})});
  toast('Telecomando rinominato');$('setBtn').onclick();
 },'Salva');
 setTimeout(()=>$('rnRem').focus(),50);
}

function remoteDeleteDialog(id,name){
 openDialog('Dimenticare «'+name+'»?','Il telecomando va riassociato (reset di fabbrica) per tornare nel sistema.',async()=>{
  await api('/api/remotes/'+id,{method:'DELETE'});
  toast('Telecomando dimenticato');$('setBtn').onclick();
 },'Dimentica');
}

function fillActionMap(acts){
 let m='';
 for(const ev in acts){
  const cur=acts[ev];
  let opts='';
  for(const a in ACT_LABELS)opts+=`<option value="${a}"${a===cur?' selected':''}>${ACT_LABELS[a]}</option>`;
  m+=`<label class="arow"><span>${EV_LABELS[ev]||ev}</span><select data-ev="${ev}">${opts}</select></label>`;
 }
 $('actMap').innerHTML=m;
}

$('actSave').onclick=async()=>{
 const body={};
 $('actMap').querySelectorAll('select').forEach(s=>{body[s.dataset.ev]=s.value});
 try{
  const r=await api('/api/remotes/actions',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  toast('Mappa azioni salvata');fillActionMap(r.actions);
 }catch(e){}
};

// --- OTA upload ---------------------------------------------------------------
$('otaGo').onclick=()=>{
 const f=$('otaFile').files[0];
 if(!f){toast('Scegli un file .bin');return}
 if(!f.name.toLowerCase().endsWith('.bin')){toast('Il file deve essere .bin');return}
 if(!confirm('Avviare l\'aggiornamento? Al termine il dispositivo si riavvia.'))return;
 const fd=new FormData();fd.append('update',f);
 const x=new XMLHttpRequest();x.open('POST','/api/ota');
 const tok=$('otaTok').value.trim(),md5=$('otaMd5').value.trim();
 if(tok)x.setRequestHeader('X-OTA-TOKEN',tok);
 if(md5)x.setRequestHeader('X-OTA-MD5',md5);
 $('otaBar').hidden=false;$('otaFill').style.width='0';$('otaPct').textContent='';
 $('otaGo').disabled=true;
 x.upload.onprogress=e=>{if(e.lengthComputable){const p=Math.round(e.loaded/e.total*100);$('otaFill').style.width=p+'%';$('otaPct').textContent=p+'%'}};
 x.onload=()=>{
  $('otaGo').disabled=false;
  let j={};try{j=JSON.parse(x.responseText)}catch(_){}
  if(x.status===200&&j.ok){
   $('otaFill').style.width='100%';
   $('otaMsg').textContent='Firmware caricato ('+(j.size||'')+' byte): il dispositivo si riavvia. Se il nuovo firmware non parte, torna automaticamente alla versione precedente.';
   toast('Firmware caricato, riavvio…');
  }else{
   $('otaBar').hidden=true;
   $('otaMsg').textContent='Errore: '+(j.error||('HTTP '+x.status));
  }
 };
 x.onerror=()=>{$('otaGo').disabled=false;$('otaBar').hidden=true;$('otaMsg').textContent='Errore di rete durante l\'upload'};
 x.send(fd);
};

// --- Debug log tab ------------------------------------------------------------
let logSince=0;
const logTs=ms=>{const s=Math.floor(ms/1000);return String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0')};
async function pollLog(){
 if($('t-log').hidden||!$('logLive').checked)return;
 try{
  const box=$('logBox');
  for(let guard=0;guard<8;guard++){
   const j=await api('/api/logs?since='+logSince);
   if(!j.lines.length)break;
   const stick=box.scrollTop+box.clientHeight>=box.scrollHeight-30;
   for(const l of j.lines){
    const d=document.createElement('div');
    d.textContent=logTs(l.t)+'  '+l.m;
    box.appendChild(d);
    logSince=l.i;
   }
   if(stick)box.scrollTop=box.scrollHeight;
   $('logStat').textContent=j.lines.length+' righe nuove';
   if(j.lines.length<24)break;
  }
 }catch(e){}
}
$('logClear').onclick=async()=>{
 logSince=0;$('logBox').textContent='';$('logStat').textContent='';
 await api('/api/logs',{method:'DELETE'});
};
setInterval(pollLog,2000);

$('statusLine').onclick=()=>$('setBtn').onclick();

load();
setInterval(load,2500);

// Installable app: show the button when the browser offers the prompt.
let installEvt=null;
window.addEventListener('beforeinstallprompt',e=>{e.preventDefault();installEvt=e;$('installBtn').hidden=false});
$('installBtn').onclick=async()=>{if(!installEvt)return;installEvt.prompt();installEvt=null;$('installBtn').hidden=true};
if(navigator.serviceWorker&&!window.navigator.standalone)navigator.serviceWorker.register('/sw.js').catch(()=>{});
</script>
</body>
</html>)rawliteral";

static const char MANIFEST_JSON[] PROGMEM = R"rawliteral({
"name": "esp32-zigbee-bulb",
"short_name": "Bulb",
"description": "Controllo lampadine Zigbee IKEA",
"start_url": "/",
"scope": "/",
"display": "standalone",
"background_color": "#0e1116",
"theme_color": "#0e1116",
"icons": [
  {"src": "/icon-192.png", "sizes": "192x192", "type": "image/png"},
  {"src": "/icon-512.png", "sizes": "512x512", "type": "image/png", "purpose": "maskable"}
]
})rawliteral";

// Network-first service worker: keeps the UI always fresh, makes the app
// installable and caches the static shell as a fallback. Bump V when the
// page changes so installed PWAs pick up the new shell.
static const char SW_JS[] PROGMEM = R"rawliteral(const V='sw-v3';
const SHELL=['/','/icon-192.png','/icon-512.png','/manifest.json'];
self.addEventListener('install',e=>{e.waitUntil(caches.open(V).then(c=>c.addAll(SHELL)).then(()=>self.skipWaiting()))});
self.addEventListener('activate',e=>{e.waitUntil(caches.keys().then(ks=>Promise.all(ks.filter(k=>k!==V).map(k=>caches.delete(k)))).then(()=>self.clients.claim()))});
self.addEventListener('fetch',e=>{
  if(e.request.method!=='GET'||!e.request.url.startsWith(self.location.origin))return;
  e.respondWith(caches.open(V).then(c=>c.match(e.request).then(hit=>fetch(e.request).then(r=>{
    if(r.ok&&(e.request.headers.get('accept')||'').includes('text/html')||SHELL.some(p=>e.request.url.endsWith(p)))c.put(e.request,r.clone());
    return r;
  }).catch(()=>hit||Response.error()))));
});
)rawliteral";
