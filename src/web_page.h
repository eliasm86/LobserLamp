#pragma once
#include <pgmspace.h>

// ---------------------------------------------------------------- shared styles
const char APP_CSS[] PROGMEM = R"rawliteral(
:root{--bg:#121316;--card:#1d1f24;--line:#2c2f36;--text:#eceef2;--mute:#9097a3;--accent:#ff3b30;--bad:#ff6b5e;--good:#3ddc84}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text)}
main{max-width:460px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:8px 0 16px}
h1{font-size:1.4rem;margin:0;flex:1}
a{color:inherit}
.icon{display:flex;align-items:center;justify-content:center;width:36px;height:36px;border-radius:50%;color:var(--mute);text-decoration:none}
.icon:hover{background:var(--card);color:var(--text)}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:12px}
.label{display:flex;justify-content:space-between;color:var(--mute);font-size:.85rem;margin-bottom:10px}
.power{width:64px;height:36px;flex:none;border-radius:18px;border:none;background:var(--line);position:relative;cursor:pointer;transition:.2s}
.power::after{content:"";position:absolute;top:4px;left:4px;width:28px;height:28px;border-radius:50%;background:#fff;box-shadow:0 0 0 1px rgba(0,0,0,.25),0 1px 3px rgba(0,0,0,.35);transition:.2s}
.power.on{background:var(--accent)} .power.on::after{left:32px}
.power.claw.on{background:var(--claw,#ffb000)}
.claw-ctl{display:flex;align-items:center;gap:10px;margin-top:10px}
.claw-ctl input[type=range]{flex:1}
.claw-ctl .val{color:var(--mute);font-size:.85rem;min-width:3em;text-align:right}
.claw-box+.claw-box{margin-top:16px;padding-top:16px;border-top:1px solid var(--line)}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px}
.row+.row{margin-top:12px}
input[type=color]{width:48px;height:32px;border:none;background:none;padding:0;cursor:pointer}
input[type=range]{width:100%;accent-color:var(--accent)}
.modes{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.mode{padding:12px 6px;border-radius:10px;border:1px solid var(--line);background:transparent;color:var(--text);font-size:.95rem;cursor:pointer}
.mode.sel{background:var(--accent);border-color:var(--accent);color:var(--onaccent,#fff)}
select,input[type=text],input[type=password],input[type=number]{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:9px 10px;font-size:.95rem;font-family:inherit}
.field{display:block;margin-top:12px} .field:first-of-type{margin-top:0}
.field>span{display:block;color:var(--mute);font-size:.85rem;margin-bottom:6px}
.field input,.field select{width:100%}
.field .row select{width:auto;flex:1;min-width:0}
.btn{padding:10px 16px;border-radius:10px;border:1px solid var(--line);background:transparent;color:var(--text);font-size:.95rem;cursor:pointer}
.btn.primary{width:100%;padding:14px;background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
.btn:disabled{opacity:.5;cursor:default}
.nets{display:flex;flex-direction:column;gap:6px;margin-top:10px}
.net{display:flex;justify-content:space-between;padding:10px 12px;border-radius:8px;border:1px solid var(--line);background:transparent;color:var(--text);font-size:.95rem;cursor:pointer;text-align:left}
.net span{color:var(--mute);font-size:.85rem}
.hint{color:var(--mute);font-size:.8rem;margin-top:10px;min-height:1em}
.msg{margin:10px 0 4px;font-size:.9rem;min-height:1.2em} .msg.bad{color:var(--bad)} .msg.good{color:var(--good)}
.anim{margin-top:16px} .hidden{display:none!important}
footer{color:var(--mute);font-size:.75rem;text-align:center;margin-top:8px}
)rawliteral";

#define GEAR_SVG "<svg width=\"22\" height=\"22\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><circle cx=\"12\" cy=\"12\" r=\"3\"/><path d=\"M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09a1.65 1.65 0 0 0-1-1.51 1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09a1.65 1.65 0 0 0 1.51-1 1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33h0a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51h0a1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82v0a1.65 1.65 0 0 0 1.51 1H21a2 2 0 1 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z\"/></svg>"

// ---------------------------------------------------------------- lamp controls
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LobserLamp</title>
<link rel="stylesheet" href="/app.css">
</head>
<body>
<main>
  <header><h1>LobserLamp</h1><a class="icon" href="/setup" aria-label="Setup" title="Setup">)rawliteral" GEAR_SVG R"rawliteral(</a></header>

  <div class="card hidden" id="printerCard">
    <div class="label"><span>Printer status lights</span><span id="sceneMsg"></span></div>
    <div id="printerState"></div>
    <label class="field" style="margin-top:12px"><span>When the printer is</span><select id="pstate"></select></label>
    <div class="row" style="margin-top:12px"><span class="hint" id="ptestMsg" style="margin:0"></span><button class="btn" id="ptest">Test</button></div>
    <div class="hint">Everything below is saved for the chosen status. Choosing a status or changing a setting shows it on the lamp for 15 s, then the lamp goes back to the printer's real status. Body effect Progress lights up as many LEDs as the % done.</div>
  </div>

  <div class="card">
    <div class="row" style="margin-bottom:12px"><span>Body</span><button id="power" class="power" aria-label="Body on/off"></button></div>
    <div class="modes" id="modes"></div>
    <div class="claw-ctl"><input type="color" id="color" aria-label="Color"><input type="range" id="bri" min="1" max="255" aria-label="Brightness"><span class="val" id="briVal"></span></div>
    <div class="hint" id="modeHint"></div>
    <div class="anim" id="anim">
      <div class="label"><span>Speed</span><span id="speedVal"></span></div>
      <input type="range" id="speed" min="1" max="100">
      <div class="label" style="margin-top:14px"><span>Tail</span><span id="tailVal"></span></div>
      <input type="range" id="tail" min="0" max="100">
    </div>
    <div class="anim" id="pulse">
      <div class="label"><span>Pulse speed</span><span id="pspeedVal"></span></div>
      <input type="range" id="pspeed" min="1" max="100">
      <div class="label" style="margin-top:14px"><span>Min brightness</span><span id="pminVal"></span></div>
      <input type="range" id="pmin" min="0" max="100">
      <div class="label" style="margin-top:14px"><span>Max brightness</span><span id="pmaxVal"></span></div>
      <input type="range" id="pmax" min="0" max="100">
    </div>
    <div class="anim" id="candle">
      <div class="label"><span>Flicker speed</span><span id="cspeedVal"></span></div>
      <input type="range" id="cspeed" min="1" max="100">
    </div>
  </div>

  <div class="card">
    <div class="row"><span>Top of head</span><button id="head" class="power claw" aria-label="Top of head light"></button></div>
    <div class="claw-ctl"><input type="color" id="headColor"><input type="range" id="headBri" min="1" max="255"><span class="val" id="headBriVal"></span></div>
    <div class="claw-box" style="margin-top:10px">
      <div class="row"><span>Pulse</span><button id="hpulse" class="power claw" aria-label="Pulse top of head"></button></div>
      <div id="hpulseCtl" style="margin-top:10px">
        <div class="row"><span>Sync with claws</span><button id="hsync" class="power claw" aria-label="Sync head pulse with the claws"></button></div>
        <div class="hint" id="hsyncHint"></div>
        <div id="hpownCtl">
          <div class="label"><span>Pulse speed</span><span id="hpspeedVal"></span></div>
          <input type="range" id="hpspeed" min="1" max="100">
          <div class="label"><span>Fade</span><span id="hpfadeVal"></span></div>
          <input type="range" id="hpfade" min="0" max="100">
        </div>
      </div>
    </div>
    <div class="hint" id="headHint"></div>
  </div>

  <div class="card">
    <div class="label"><span>Claw lights</span></div>
    <div id="claws"></div>
    <div class="claw-box">
      <div class="row"><span>Pulse both</span><button id="cpulse" class="power claw" aria-label="Pulse both claws"></button></div>
      <div id="cpulseCtl" style="margin-top:10px">
        <div class="label"><span>Pulse speed</span><span id="cpspeedVal"></span></div>
        <input type="range" id="cpspeed" min="1" max="100">
        <div class="label"><span>Fade</span><span id="cpfadeVal"></span></div>
        <input type="range" id="cpfade" min="0" max="100">
      </div>
    </div>
    <div class="claw-box">
      <div class="row"><span>Toggle 1 &harr; 2</span><button id="ctoggle" class="power claw" aria-label="Toggle between the claws"></button></div>
      <div id="ctoggleCtl" style="margin-top:10px">
        <div class="label"><span>Toggle speed</span><span id="ctspeedVal"></span></div>
        <input type="range" id="ctspeed" min="1" max="100">
        <div class="label"><span>Fade</span><span id="ctfadeVal"></span></div>
        <input type="range" id="ctfade" min="0" max="100">
      </div>
    </div>
    <div class="hint">Not affected by power or effects. Choose which LEDs they are in <a href="/setup">Setup</a>.</div>
  </div>

  <footer id="status"></footer>
</main>
<script>
const MODES={Solid:"Steady color on all LEDs",Wander:"A dot travels from one end to the other and back",Pulse:"Everything except top of head and claws slowly breathes",Candle:"Everything except top of head and claws flickers like a candle",Progress:"Lights up as many LEDs as the print's % done, in this color"};
const SLIDERS=[["bri","brightness"],["speed","speed"],["tail","tail"],["pspeed","pulse_speed"],["pmin","pulse_min"],["pmax","pulse_max"],["cspeed","candle_speed"],["cpspeed","claw_pulse_speed"],["cpfade","claw_pulse_fade"],["ctspeed","claw_toggle_speed"],["ctfade","claw_toggle_fade"],["hpspeed","head_pulse_speed"],["hpfade","head_pulse_fade"]];
const $=id=>document.getElementById(id);
let st={},busy=0,timer={};

const hex=c=>"#"+[c.r,c.g,c.b].map(v=>v.toString(16).padStart(2,"0")).join("");
const rgb=h=>({r:parseInt(h.slice(1,3),16),g:parseInt(h.slice(3,5),16),b:parseInt(h.slice(5,7),16)});

Object.keys(MODES).forEach(m=>{const b=document.createElement("button");b.className="mode";b.textContent=m;b.dataset.m=m;
  b.onclick=()=>send({state:"ON",effect:m});$("modes").appendChild(b);});

$("power").onclick=()=>send({state:st.state==="ON"?"OFF":"ON"});
const CLAWS=["claw1","claw2"];
CLAWS.forEach((k,i)=>{
  const d=document.createElement("div");d.className="claw-box";
  d.innerHTML=`<div class="row"><span>Claw ${i+1}</span><button id="${k}" class="power claw" aria-label="Claw ${i+1}"></button></div>
    <div class="claw-ctl"><input type="color" id="${k}Color"><input type="range" id="${k}Bri" min="1" max="255"><span class="val" id="${k}BriVal"></span></div>`;
  $("claws").appendChild(d);
  $(k).onclick=()=>send({[k]:{state:st[k]?.state==="ON"?"OFF":"ON"}});
  // While "Pulse both" or "Toggle" is on, color/brightness only change that pattern and don't switch the claw on
  const on=()=>st.claw_pulse==="ON"||st.claw_toggle==="ON"?{}:{state:"ON"};
  $(k+"Color").oninput=e=>throttled(k+"c",{[k]:{...on(),color:rgb(e.target.value)}});
  $(k+"Bri").oninput=e=>{$(k+"BriVal").textContent=label("bri",+e.target.value);throttled(k+"b",{[k]:{...on(),brightness:+e.target.value}});};
});
$("cpulse").onclick=()=>send({claw_pulse:st.claw_pulse==="ON"?"OFF":"ON"});
$("ctoggle").onclick=()=>send({claw_toggle:st.claw_toggle==="ON"?"OFF":"ON"});
// [LED index key, controls disabled while no LED is chosen]
const LEDCTL=[["head_led",["head","headColor","headBri","hpulse"]],...CLAWS.map(k=>[k+"_led",[k,k+"Color",k+"Bri"]])];
$("color").oninput=e=>throttled("color",{state:"ON",color:rgb(e.target.value)});
SLIDERS.forEach(([id,key])=>{
  $(id).oninput=e=>{$(id+"Val").textContent=label(id,+e.target.value);throttled(id,{[key]:+e.target.value});};});
$("head").onclick=()=>send({head:{state:st.head?.state==="ON"?"OFF":"ON"}});
// While head pulse is on, color/brightness only change the pulse and don't switch the plain light on
const headOn=()=>st.head_pulse==="ON"?{}:{state:"ON"};
$("headColor").oninput=e=>throttled("headc",{head:{...headOn(),color:rgb(e.target.value)}});
$("headBri").oninput=e=>{$("headBriVal").textContent=label("bri",+e.target.value);throttled("headb",{head:{...headOn(),brightness:+e.target.value}});};
$("hpulse").onclick=()=>send({head_pulse:st.head_pulse==="ON"?"OFF":"ON"});
$("hsync").onclick=()=>send({head_sync:st.head_sync==="ON"?"OFF":"ON"});

function label(id,v){return id==="bri"?Math.round(v/2.55)+"%":(id==="pmin"||id==="pmax")?v+"%":(id==="ctfade"||id==="hpfade"||id==="cpfade")?(v?v+"%":"Off"):v;}
function throttled(k,body){clearTimeout(timer[k]);timer[k]=setTimeout(()=>send(body),60);}

// look = printer state whose look the controls edit (printer status mode), null = the lamp's own settings
let look=null;
const stateUrl=()=>"/api/state"+(look?"?look="+encodeURIComponent(look):"");
async function send(body){
  busy++;
  try{const r=await fetch(stateUrl(),{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body)});
    const j=await r.json();if(!r.ok){$("sceneMsg").textContent=j.error||"Failed";return;}render(j);}
  catch(e){$("status").textContent="Connection lost";}
  finally{busy--;}
}
async function poll(){
  if(busy||document.activeElement?.type==="range")return;
  try{render(await (await fetch(stateUrl())).json());}catch(e){$("status").textContent="Connection lost";}
}
function render(s){
  if((s.look||null)!==look)return;  // reply for a status that is no longer chosen
  // Printer status mode: start with the look of the printer's current status
  if(s.printer&&!look){look=s.printer.state;if(pcfg)$("pstate").value=look;poll();return;}
  if(!s.printer&&look){look=null;poll();return;}  // printer mode was turned off
  st=s;const c=hex(s.color);
  $("power").classList.toggle("on",s.state==="ON");
  CLAWS.forEach(k=>{const c=s[k];if(!c)return;const h=hex(c.color);
    $(k).classList.toggle("on",c.state==="ON");$(k).style.setProperty("--claw",h);$(k+"Bri").style.accentColor=h;
    if(document.activeElement!==$(k+"Color"))$(k+"Color").value=h;
    if(document.activeElement!==$(k+"Bri")){$(k+"Bri").value=c.brightness;$(k+"BriVal").textContent=label("bri",c.brightness);}});
  document.documentElement.style.setProperty("--accent",s.state==="ON"?c:"#555");
  document.documentElement.style.setProperty("--onaccent",s.state==="ON"&&(.299*s.color.r+.587*s.color.g+.114*s.color.b)>160?"#111":"#fff");
  document.querySelectorAll(".mode").forEach(b=>b.classList.toggle("sel",b.dataset.m===s.effect));
  if(document.activeElement!==$("color"))$("color").value=c;
  for(const [id,key] of SLIDERS){
    if(document.activeElement!==$(id)){$(id).value=s[key];$(id+"Val").textContent=label(id,s[key]);}}
  for(const [key,ctl] of LEDCTL)ctl.forEach(id=>{$(id).disabled=s[key]<0;$(id).style.opacity=s[key]<0?.4:1;});
  $("headHint").innerHTML=s.head_led<0?'No LED chosen yet. Pick one in <a href="/setup">Setup</a>.':"Own color and brightness. Not affected by power or effects.";
  const hd=s.head,hh=hex(hd.color);
  $("head").classList.toggle("on",hd.state==="ON");$("head").style.setProperty("--claw",hh);$("headBri").style.accentColor=hh;
  if(document.activeElement!==$("headColor"))$("headColor").value=hh;
  if(document.activeElement!==$("headBri")){$("headBri").value=hd.brightness;$("headBriVal").textContent=label("bri",hd.brightness);}
  $("cpulse").classList.toggle("on",s.claw_pulse==="ON");
  $("cpulseCtl").classList.toggle("hidden",s.claw_pulse!=="ON");
  $("cpspeed").style.accentColor=s.claw1?hex(s.claw1.color):"";
  $("cpfade").style.accentColor=s.claw1?hex(s.claw1.color):"";
  $("ctoggle").classList.toggle("on",s.claw_toggle==="ON");
  $("ctoggleCtl").classList.toggle("hidden",s.claw_toggle!=="ON");
  $("ctspeed").style.accentColor=s.claw1?hex(s.claw1.color):"";
  $("ctfade").style.accentColor=s.claw1?hex(s.claw1.color):"";
  $("hpulse").classList.toggle("on",s.head_pulse==="ON");$("hpulse").style.setProperty("--claw",hh);
  $("hpulseCtl").classList.toggle("hidden",s.head_pulse!=="ON");
  $("hpspeed").style.accentColor=hh;$("hpfade").style.accentColor=hh;
  // Synced and a claw pattern is running: the head follows the claws, its own speed/fade aren't used
  const clawFx=s.claw_pulse==="ON"?"claw pulse":s.claw_toggle==="ON"?"claw toggle (on with claw 1)":"";
  const synced=s.head_sync==="ON"&&clawFx;
  $("hsync").classList.toggle("on",s.head_sync==="ON");$("hsync").style.setProperty("--claw",hh);
  $("hpownCtl").classList.toggle("hidden",!!synced);
  $("hsyncHint").textContent=s.head_sync!=="ON"?"":synced?"Following "+clawFx+".":"No claw pattern running, using its own speed and fade.";
  $("anim").classList.toggle("hidden",s.effect!=="Wander");
  $("pulse").classList.toggle("hidden",s.effect!=="Pulse");
  $("candle").classList.toggle("hidden",s.effect!=="Candle");
  $("modeHint").textContent=MODES[s.effect]||"";
  $("status").textContent="Home Assistant: "+s.mqtt;
  showPrinter(s.printer);
}

// ---- Printer status lights (printer status mode only): the controls on this page edit the
// look of the status chosen here instead of the lamp's own settings
let pcfg=null,pload=false;
const esc=s=>String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
const stateLabel=k=>pcfg?.states.find(x=>x.key===k)?.label||k;
async function loadPrinter(){
  pload=true;
  try{pcfg=await (await fetch("/api/printer")).json();}catch(e){pload=false;return;}
  $("pstate").innerHTML=pcfg.states.map(x=>`<option value="${x.key}">${esc(x.label)}</option>`).join("");
  $("pstate").value=look;
}
async function test(key){
  try{const r=await fetch("/api/printer",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({test:key})});
    if(!r.ok)$("sceneMsg").textContent=(await r.json()).error||"Failed";}catch(e){}
  poll();
}
$("pstate").onchange=()=>{look=$("pstate").value;test(look);};  // show the chosen status while editing it
$("ptest").onclick=()=>test(st.printer?.test?"":look);
function showPrinter(p){
  $("printerCard").classList.toggle("hidden",!p);
  $("modes").querySelector('[data-m="Progress"]').classList.toggle("hidden",!look);
  if(!p)return;
  if(!pcfg){if(!pload)loadPrinter();return;}
  let h=p.connected?`Printer: <b>${esc(p.state_label)}</b>`:"Printer: <b>not connected</b>";
  if(p.connected&&(p.state==="RUNNING"||p.state==="PAUSE"||p.state==="PREPARE")){
    h+=`, ${p.percent}%`;if(p.remaining_min!=null)h+=`, ${p.remaining_min} min left`;
    if(p.layer!=null&&p.total_layers)h+=`, layer ${p.layer}/${p.total_layers}`;}
  $("printerState").innerHTML=h;
  $("ptest").textContent=p.test?"Stop test":"Test";
  $("ptestMsg").textContent=p.test?`Testing ${stateLabel(p.test)}, ${p.test_left} s left`:"";
  // Count down while a test runs
  clearTimeout(timer.pt);if(p.test)timer.pt=setTimeout(poll,1000);
}
poll();setInterval(poll,3000);
</script>
</body>
</html>
)rawliteral";

// ---------------------------------------------------------------- setup page
const char SETUP_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LobserLamp setup</title>
<link rel="stylesheet" href="/app.css">
</head>
<body>
<main>
  <header><a class="icon" href="/" aria-label="Back to lamp" title="Back to lamp">&#8592;</a><h1>Setup</h1></header>

  <div class="card">
    <div class="label"><span>Status</span></div>
    <div id="netStatus">Loading…</div>
  </div>

  <div class="card">
    <div class="label"><span>Mode</span></div>
    <label class="field"><span>What drives the lamp</span><select id="mode">
      <option value="lamp">Lamp Mode</option>
      <option value="printer">Printer status</option>
    </select></label>
    <div class="hint" id="modeHint"></div>
  </div>

  <div class="card" id="bambuCard">
    <div class="label"><span>Bambu Lab printer</span></div>
    <label class="field"><span>Printer IP address</span><input type="text" id="bhost" placeholder="e.g. 192.168.1.50" autocapitalize="none" spellcheck="false"></label>
    <label class="field"><span>Serial number</span><input type="text" id="bserial" autocapitalize="characters" spellcheck="false"></label>
    <label class="field"><span>Access code</span><input type="text" id="bcode" autocomplete="off" autocapitalize="none" spellcheck="false"></label>
    <div class="hint">All three are on the printer's screen: the IP address and access code under Settings &rarr; Network (WLAN), the serial number under Settings &rarr; Device. Works with all current models (X1, P1, A1, H2). If the lamp can't read the printer, turn on LAN only mode and Developer mode on the printer.</div>
  </div>

  <div class="card">
    <div class="label"><span>WiFi</span><button class="btn" id="scan" style="padding:4px 10px">Scan</button></div>
    <div class="nets" id="nets"></div>
    <label class="field"><span>Network name</span><input type="text" id="ssid" autocomplete="off" autocapitalize="none" spellcheck="false"></label>
    <label class="field"><span>Password</span><input type="password" id="pass" autocomplete="new-password"></label>
  </div>

  <div class="card">
    <div class="label"><span>Device</span></div>
    <label class="field"><span>Name (shown in Home Assistant)</span><input type="text" id="name"></label>
    <label class="field"><span>Hostname</span><input type="text" id="host" autocapitalize="none" spellcheck="false"></label>
    <div class="hint" id="hostHint"></div>
  </div>

  <div class="card">
    <div class="label"><span>Home Assistant (MQTT)</span></div>
    <label class="field"><span>Server, empty = off</span><input type="text" id="mhost" placeholder="e.g. homeassistant.local" autocapitalize="none" spellcheck="false"></label>
    <label class="field"><span>Port</span><input type="number" id="mport" min="1" max="65535"></label>
    <label class="field"><span>User</span><input type="text" id="muser" autocapitalize="none" spellcheck="false"></label>
    <label class="field"><span>Password</span><input type="password" id="mpass" autocomplete="new-password"></label>
  </div>

  <div class="card">
    <div class="label"><span>LED strip</span></div>
    <label class="field"><span>Data pin</span><select id="pin"></select></label>
    <label class="field"><span>Number of LEDs (including claws and top of head)</span><input type="number" id="count" min="1"></label>
    <label class="field"><span>Color order</span><select id="order"></select></label>
    <div class="hint">WS2812B strips are usually GRB. If red and green are swapped, try RGB.</div>
    <div class="label" style="margin-top:14px"><span>Max brightness (protects the power supply)</span><span id="maxbriVal"></span></div>
    <input type="range" id="maxbri" min="1" max="255">
  </div>

  <div class="card">
    <div class="label"><span>LED roles</span><span id="rolesMsg"></span></div>
    <div class="field"><span>Top of head</span><div class="row"><select id="head_led" aria-label="Top of head LED"></select><button class="btn" id="head_ledFlash">Flash</button></div></div>
    <div class="field"><span>Claw 1</span><div class="row"><select id="claw1_led" aria-label="Claw 1 LED"></select><button class="btn" id="claw1_ledFlash">Flash</button></div></div>
    <div class="field"><span>Claw 2</span><div class="row"><select id="claw2_led" aria-label="Claw 2 LED"></select><button class="btn" id="claw2_ledFlash">Flash</button></div></div>
    <div class="hint">Applied right away. Flash blinks the chosen LED 3 times so you can see which one it is. The effects use all other LEDs. Count LEDs from the start of the strip.</div>
  </div>

  <div class="msg" id="msg"></div>
  <button class="btn primary" id="save">Save and restart</button>
</main>
<script>
const $=id=>document.getElementById(id);
const ROLES=["head_led","claw1_led","claw2_led"];
let cfg={};
const esc=s=>String(s).replace(/[&<>"]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;"}[c]));
const pct=v=>Math.round(v/2.55)+"%";

async function load(){
  const [c,s]=await Promise.all([fetch("/api/setup").then(r=>r.json()),fetch("/api/state").then(r=>r.json())]);
  cfg=c;
  $("ssid").value=c.wifi_ssid;
  $("pass").value="";$("pass").placeholder=c.wifi_pass_set?"Unchanged":"";
  $("name").value=c.device_name;$("host").value=c.hostname;hostHint();
  $("mhost").value=c.mqtt_host;$("mport").value=c.mqtt_port;$("muser").value=c.mqtt_user;
  $("mpass").value="";$("mpass").placeholder=c.mqtt_pass_set?"Unchanged":"";
  $("pin").innerHTML=c.pins.map(p=>`<option value="${p}">GPIO ${p}</option>`).join("");$("pin").value=c.led_pin;
  $("count").max=c.max_leds;$("count").value=c.led_count;
  $("order").innerHTML=c.orders.map(o=>`<option>${o}</option>`).join("");$("order").value=c.led_order;
  $("maxbri").value=c.max_brightness;$("maxbriVal").textContent=pct(c.max_brightness);
  $("mode").value=c.mode;modeChanged();
  $("bhost").value=c.bambu_host;$("bserial").value=c.bambu_serial;
  $("bcode").value="";$("bcode").placeholder=c.bambu_code_set?"Saved, leave empty to keep":"";
  for(const k of ROLES){
    $(k).innerHTML='<option value="-1">None</option>'+Array.from({length:s.led_count},(_,i)=>`<option value="${i}">LED ${i+1}</option>`).join("");
    $(k).value=s[k];flashState(k);}
  showStatus(c.status);
  if(c.status.ap&&!c.status.connected)scan();  // setting up through the hotspot: list networks right away
}

function showStatus(n){
  let h=n.connected?`Connected to <b>${esc(n.ssid)}</b>, IP ${n.ip}`:(n.ssid?`Not connected to <b>${esc(n.ssid)}</b>`:"No WiFi set up yet");
  if(n.ap)h+=`<div class="hint">Setup hotspot <b>${esc(n.ap_ssid)}</b> is on (${n.ap_ip}).${n.connected?" It turns off shortly.":""}</div>`;
  $("netStatus").innerHTML=h;
}
setInterval(async()=>{try{showStatus((await (await fetch("/api/setup")).json()).status);}catch(e){}},5000);

const MODE_HINTS={lamp:"Stand-alone lamp, controlled from the lamp page and Home Assistant.",
  printer:"Each printer status gets its own lamp settings, set on the lamp page (after saving)."};
function modeChanged(){const m=$("mode").value;$("modeHint").textContent=MODE_HINTS[m];$("bambuCard").classList.toggle("hidden",m!=="printer");}
$("mode").onchange=modeChanged;


function hostHint(){const h=$("host").value.trim().toLowerCase();$("hostHint").textContent=h?`The lamp will be at http://${h}.local`:"";}
$("host").oninput=hostHint;
$("maxbri").oninput=e=>$("maxbriVal").textContent=pct(+e.target.value);

async function scan(){
  $("scan").disabled=true;$("nets").innerHTML='<div class="hint">Scanning…</div>';
  try{
    for(let i=0;i<20;i++){
      const r=await (await fetch("/api/scan")).json();
      if(r.networks){
        r.networks.sort((a,b)=>b.rssi-a.rssi);
        $("nets").innerHTML=r.networks.length?"":'<div class="hint">No networks found</div>';
        for(const n of r.networks){
          const b=document.createElement("button");b.className="net";
          const bars=n.rssi>-60?"▂▄▆█":n.rssi>-70?"▂▄▆":n.rssi>-80?"▂▄":"▂";
          b.innerHTML=`${esc(n.ssid)}<span>${n.secure?"🔒 ":""}${bars}</span>`;
          b.onclick=()=>{$("ssid").value=n.ssid;$("nets").innerHTML="";$("pass").focus();};
          $("nets").appendChild(b);}
        break;}
      await new Promise(r=>setTimeout(r,1000));}
  }catch(e){$("nets").innerHTML='<div class="hint">Scan failed</div>';}
  $("scan").disabled=false;
}
$("scan").onclick=scan;

function msg(t,cls){$("msg").textContent=t;$("msg").className="msg "+(cls||"");}

$("save").onclick=async()=>{
  const b={wifi_ssid:$("ssid").value.trim(),device_name:$("name").value.trim(),hostname:$("host").value.trim(),
    mqtt_host:$("mhost").value.trim(),mqtt_port:+$("mport").value,mqtt_user:$("muser").value.trim(),
    led_pin:+$("pin").value,led_count:+$("count").value,led_order:$("order").value,max_brightness:+$("maxbri").value,
    mode:$("mode").value,bambu_host:$("bhost").value.trim(),bambu_serial:$("bserial").value.trim()};
  // Empty access code field keeps the saved one
  if($("bcode").value.trim())b.bambu_code=$("bcode").value.trim();
  if(b.mode==="printer"&&(!b.bambu_host||!b.bambu_serial||(!b.bambu_code&&!cfg.bambu_code_set))){
    msg("Fill in the printer's IP address, serial number and access code","bad");return;}
  // Empty password field keeps the saved one, unless the network/server changed
  if($("pass").value||b.wifi_ssid!==cfg.wifi_ssid)b.wifi_pass=$("pass").value;
  if($("mpass").value||b.mqtt_host!==cfg.mqtt_host||b.mqtt_user!==cfg.mqtt_user)b.mqtt_pass=$("mpass").value;
  if(!b.wifi_ssid){msg("Enter a WiFi network name","bad");return;}
  $("save").disabled=true;msg("Saving…");
  try{
    const r=await fetch("/api/setup",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(b)});
    const j=await r.json();
    if(!r.ok){msg(j.error||"Could not save","bad");$("save").disabled=false;return;}
  }catch(e){msg("Connection lost","bad");$("save").disabled=false;return;}
  const host=(b.hostname||cfg.hostname).toLowerCase();
  msg(`Saved. Restarting and joining "${b.wifi_ssid}"… Then open http://${host}.local (or the lamp's IP from your router). If it can't connect, the setup hotspot comes back.`,"good");
  // If we're still on the same network, reload when the lamp is back
  setTimeout(()=>{const t=setInterval(async()=>{try{await fetch("/api/setup");clearInterval(t);location.reload();}catch(e){}},2000);},5000);
};

function flashState(k){$(k+"Flash").disabled=+$(k).value<0;}
for(const k of ROLES)$(k+"Flash").onclick=()=>fetch("/api/identify",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({led:+$(k).value})});
for(const k of ROLES)$(k).onchange=async e=>{
  flashState(k);
  $("rolesMsg").textContent="Saving…";
  try{await fetch("/api/state",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({[k]:+e.target.value})});
    $("rolesMsg").textContent="Saved";}catch(err){$("rolesMsg").textContent="Failed";}
  setTimeout(()=>$("rolesMsg").textContent="",1500);
};

load().catch(()=>msg("Could not load settings","bad"));
</script>
</body>
</html>
)rawliteral";
