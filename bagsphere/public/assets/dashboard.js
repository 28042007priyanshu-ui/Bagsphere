const qs=new URLSearchParams(location.search),bag=qs.get('bag'),token=qs.get('token');
const panel=document.getElementById('bagPanel'),reportsEl=document.getElementById('reports'),count=document.getElementById('reportCount');
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function time(ts){return new Date(ts).toLocaleString([], {dateStyle:'medium',timeStyle:'short'})}
async function load(){
 if(!bag||!token){panel.innerHTML='<div class="profile-panel"><h2>Private dashboard</h2><p class="meta">Use the private dashboard link provided after registration.</p></div>';return}
 const r=await fetch('/api/bag/'+encodeURIComponent(bag)); const d=await r.json();
 if(!r.ok||d.ownerToken!==token){panel.innerHTML='<div class="profile-panel"><h2>Access denied</h2><p class="meta">This dashboard link is invalid.</p></div>';return}
 const photo=d.photo?`<img class="profile-photo" src="${d.photo}" alt="Profile photo">`:`<div class="profile-photo" style="display:grid;place-items:center;font-size:30px;font-weight:900;color:#1e6b4d">${esc(d.studentName[0]||'B')}</div>`;
 const profile=document.querySelector('.profile-panel');
 profile.innerHTML=`<span class="section-label">REGISTERED BAG</span><h2>${esc(d.studentName)}</h2>${photo}<div class="profile-meta">
 <div class="meta">Student ID<b>${esc(d.studentId)}</b></div><div class="meta">Phone<b>${esc(d.phone)}</b></div>
 <div class="meta">Finder message<b>${esc(d.message||'No message added.')}</b></div></div>
 <div class="tag-url" id="tagUrl">http://${location.host}/bag/${esc(d.bagId)}</div><button class="copy-small" onclick="copyUrl()">Copy NFC link</button>`;
 count.textContent=d.reports.length;
 reportsEl.innerHTML=d.reports.length?d.reports.map(x=>`<div class="report"><div><b>Finder location received</b><small>${time(x.timestamp)}</small><small>${x.latitude.toFixed(5)}, ${x.longitude.toFixed(5)}</small></div><a class="maps" target="_blank" href="https://www.google.com/maps/search/?api=1&query=${x.latitude},${x.longitude}">Open Maps ↗</a></div>`).join(''):`<div class="empty">No finder locations yet.<br>When someone shares a location, it will appear here.</div>`;
}
function copyUrl(){navigator.clipboard.writeText(document.getElementById('tagUrl').textContent).then(()=>alert('NFC link copied.'))}
load();
setInterval(load,15000);
