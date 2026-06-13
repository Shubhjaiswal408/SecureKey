// =============================================================
//  portal_html.h  —  WiFi vault-manager web page (served from flash)
//
//  Kept in a .h (NOT a .ino) on purpose: Arduino's .ino auto-prototype
//  generator mis-parses the JavaScript "function foo(){...}" patterns
//  inside this raw string and emits bogus C++ prototypes. Header files
//  are not scanned by that generator, so the page lives here.
// =============================================================
#pragma once

static const char PORTAL_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>SecureKey</title><style>
:root{--bg:#0b0e13;--card:#161b24;--card2:#1c222d;--line:#272e3a;--txt:#eef1f6;--mut:#8a93a4;--accent:#4d9fff;--ok:#36d67a;--err:#ff5f6d}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:radial-gradient(1000px 500px at 50% -10%,#15243b,#0b0e13) fixed;color:var(--txt);font-family:-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:20px;margin:0}.mut{color:var(--mut);font-size:13px}
input,textarea{width:100%;background:#0c0f15;color:var(--txt);border:1px solid var(--line);border-radius:10px;padding:11px;font-size:15px;font-family:inherit;outline:none}
input:focus,textarea:focus{border-color:var(--accent)}
button{border:0;border-radius:10px;padding:11px 14px;font-size:14px;font-weight:700;cursor:pointer;background:linear-gradient(140deg,#4d9fff,#2d77df);color:#04101f}
button.ghost{background:#0c0f15;color:var(--txt);border:1px solid var(--line)}
button.danger{background:#2a1416;color:var(--err);border:1px solid #4a2226}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:12px}
.row{display:flex;gap:8px;align-items:center}
.head{display:flex;justify-content:space-between;align-items:center;margin:6px 0 14px}
.bar{display:flex;gap:8px;margin-bottom:12px}.bar input{flex:1}
.entry .t{font-size:16px;font-weight:700}.entry .u{color:var(--mut);font-size:13px;margin-top:2px}
.pw{font-family:ui-monospace,Consolas,monospace;background:#0c0f15;border:1px solid var(--line);border-radius:8px;padding:8px;margin-top:8px;display:flex;justify-content:space-between;align-items:center;gap:8px}
.pw span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.acts{display:flex;gap:8px;margin-top:10px}.acts button{flex:1;padding:9px}
.mini{padding:6px 9px;font-size:12px;border-radius:7px}
.overlay{position:fixed;inset:0;background:rgba(5,8,13,.85);display:flex;align-items:center;justify-content:center;padding:18px;z-index:9}
.modal{background:var(--card2);border:1px solid var(--line);border-radius:16px;padding:18px;width:100%;max-width:420px}
label{display:block;font-size:12px;color:var(--mut);margin:10px 2px 5px;font-weight:600}
.gate input{letter-spacing:8px;text-align:center;font-size:24px}
.hidden{display:none}.toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#1c222d;border:1px solid var(--line);border-radius:10px;padding:10px 16px;font-size:13px;z-index:20}
</style></head><body>

<div id="gate" class="overlay"><div class="modal">
 <h1>SecureKey</h1><p class="mut">Enter the 6-digit code shown on your device.</p>
 <input id="gcode" class="gate" inputmode="numeric" maxlength="6" placeholder="------">
 <button style="width:100%;margin-top:14px" onclick="unlock()">Unlock</button>
 <div id="gerr" class="mut" style="color:var(--err);margin-top:8px"></div>
</div></div>

<div id="app" class="wrap hidden">
 <div class="head"><h1>SecureKey</h1>
  <div class="row"><button class="ghost mini" onclick="doExport()">Export</button>
  <button class="mini" onclick="openAdd()">+ Add</button></div></div>
 <div class="bar"><input id="q" placeholder="Search..." oninput="render()">
  <button class="ghost" onclick="openImport()">Import</button></div>
 <div id="list"></div>
 <p class="mut" style="text-align:center;margin-top:18px">Nothing leaves this WiFi. Turn it off on the device when done.</p>
</div>

<div id="modal" class="overlay hidden"><div class="modal">
 <h1 id="mtitle">Add</h1><input type="hidden" id="mid">
 <label>Title</label><input id="mt" placeholder="YouTube">
 <label>Username</label><input id="mu" placeholder="you@example.com">
 <label>Password</label>
 <div class="row"><input id="mp"><button class="ghost mini" onclick="genInto()">Gen</button></div>
 <label>URL</label><input id="murl" placeholder="youtube.com">
 <div class="acts"><button class="ghost" onclick="closeModal()">Cancel</button><button onclick="save()">Save</button></div>
</div></div>

<div id="imp" class="overlay hidden"><div class="modal">
 <h1>Import</h1><p class="mut">One per line: title, username, password, url. Chrome/Google CSV auto-detected. Lines missing a username or password are skipped.</p>
 <textarea id="ibulk" style="min-height:140px;white-space:pre;font-family:ui-monospace,monospace;font-size:13px" placeholder="YouTube,me@x.com,pass,youtube.com"></textarea>
 <div class="acts"><button class="ghost" onclick="closeImport()">Cancel</button><button onclick="doImport()">Import</button></div>
</div></div>

<div id="confirm" class="overlay hidden"><div class="modal">
 <h1 id="cfTitle">Delete?</h1><p class="mut" id="cfMsg"></p>
 <div class="acts"><button class="ghost" onclick="cfNo()">Cancel</button><button class="danger" onclick="cfYes()">Delete</button></div>
</div></div>

<div id="toast" class="toast hidden"></div>

<script>
var CODE="",DATA=[];
function $(i){return document.getElementById(i)}
function toast(m){var t=$("toast");t.textContent=m;t.classList.remove("hidden");setTimeout(function(){t.classList.add("hidden")},1800)}
function api(p){return fetch(p+(p.indexOf("?")<0?"?":"&")+"code="+encodeURIComponent(CODE))}
function post(p,o){o.code=CODE;return fetch(p,{method:"POST",body:new URLSearchParams(o)})}
function esc(s){return(s||"").replace(/[&<>"]/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;","\"":"&quot;"}[c]})}

function unlock(){CODE=$("gcode").value.trim();load(true)}
function load(first){api("/list").then(function(r){if(!r.ok)throw 0;return r.json()}).then(function(d){
  DATA=d;$("gate").classList.add("hidden");$("app").classList.remove("hidden");render();
}).catch(function(){if(first)$("gerr").textContent="Wrong code or not connected."})}

function render(){
  var q=$("q").value.toLowerCase(),L=$("list");L.innerHTML="";var n=0;
  DATA.filter(function(e){return !q||(e.title+e.user+e.url).toLowerCase().indexOf(q)>=0}).forEach(function(e){
    n++;var c=document.createElement("div");c.className="card entry";
    c.innerHTML='<div class="t">'+esc(e.title)+'</div>'+
      (e.user?'<div class="u">'+esc(e.user)+'</div>':'')+
      (e.url?'<div class="u">'+esc(e.url)+'</div>':'')+
      '<div class="pw"><span data-pw="0">&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;</span>'+
      '<span style="flex:0 0 auto"><button class="ghost mini" onclick="rev(this)">Show</button> '+
      '<button class="ghost mini" onclick="cp(this)">Copy</button></span></div>'+
      '<div class="acts"><button class="ghost" onclick="openEdit('+e.id+')">Edit</button>'+
      '<button class="danger" onclick="del('+e.id+')">Delete</button></div>';
    c._pw=e.pass;L.appendChild(c);
  });
  if(!n)L.innerHTML='<p class="mut" style="text-align:center">No entries.</p>';
}
function rev(b){var c=b.closest(".entry"),s=c.querySelector("[data-pw]");
  if(s.getAttribute("data-pw")=="0"){s.textContent=c._pw;s.setAttribute("data-pw","1");b.textContent="Hide"}
  else{s.innerHTML="&bull;&bull;&bull;&bull;&bull;&bull;&bull;&bull;";s.setAttribute("data-pw","0");b.textContent="Show"}}
function fallbackCopy(t){
  var ta=document.createElement("textarea");ta.value=t;
  ta.style.position="fixed";ta.style.top="0";ta.style.left="0";ta.style.opacity="0";
  document.body.appendChild(ta);ta.focus();ta.select();
  ta.setSelectionRange(0,t.length);
  var ok=false;try{ok=document.execCommand("copy")}catch(e){}
  document.body.removeChild(ta);
  toast(ok?"Password copied":"Copy blocked — long-press to copy")}
function copyText(t){
  // clipboard API only works on https/localhost; the portal is plain http,
  // so fall back to a hidden textarea + execCommand on phones.
  if(window.isSecureContext&&navigator.clipboard&&navigator.clipboard.writeText){
    navigator.clipboard.writeText(t).then(function(){toast("Password copied")},function(){fallbackCopy(t)});
  }else{fallbackCopy(t)}}
function cp(b){copyText(b.closest(".entry")._pw)}

function openAdd(){$("mtitle").textContent="Add";$("mid").value="";$("mt").value="";$("mu").value="";$("mp").value="";$("murl").value="";$("modal").classList.remove("hidden")}
function openEdit(id){var e=DATA.find(function(x){return x.id==id});if(!e)return;
  $("mtitle").textContent="Edit";$("mid").value=e.id;$("mt").value=e.title;$("mu").value=e.user;$("mp").value=e.pass;$("murl").value=e.url;$("modal").classList.remove("hidden")}
function closeModal(){$("modal").classList.add("hidden")}
function save(){
  var id=$("mid").value;
  var t=$("mt").value.trim(),u=$("mu").value.trim(),pw=$("mp").value,url=$("murl").value;
  // Require title + username + password so half-empty entries can't be saved.
  if(!t){toast("Title required");$("mt").focus();return}
  if(!u){toast("Username required");$("mu").focus();return}
  if(!pw){toast("Password required");$("mp").focus();return}
  var o={title:t,user:u,pass:pw,url:url};
  var p;if(id){o.id=id;p=post("/edit",o)}else{p=post("/save",o)}
  p.then(function(){closeModal();toast("Saved");setTimeout(load,400)})}

// Custom confirm — native confirm() is blocked in captive-portal browsers.
var cfCb=null;
function confirmBox(msg,cb){$("cfMsg").textContent=msg;cfCb=cb;$("confirm").classList.remove("hidden")}
function cfNo(){$("confirm").classList.add("hidden");cfCb=null}
function cfYes(){$("confirm").classList.add("hidden");var c=cfCb;cfCb=null;if(c)c()}
function del(id){confirmBox("Delete this entry? This cannot be undone.",function(){
  post("/delete",{id:id}).then(function(){toast("Deleted");setTimeout(load,400)})})}

function genPw(){var L=16,ch="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789!@#$%&*?",a=new Uint32Array(L);
  crypto.getRandomValues(a);var s="";for(var i=0;i<L;i++)s+=ch[a[i]%ch.length];return s}
function genInto(){$("mp").value=genPw();$("mp").type="text"}

function openImport(){$("ibulk").value="";$("imp").classList.remove("hidden")}
function closeImport(){$("imp").classList.add("hidden")}
function doImport(){post("/save",{bulk:$("ibulk").value}).then(function(){closeImport();toast("Imported");setTimeout(load,600)})}
function doExport(){window.location="/export?code="+encodeURIComponent(CODE)}
$("gcode").addEventListener("keydown",function(e){if(e.key=="Enter")unlock()});
</script></body></html>)HTML";
