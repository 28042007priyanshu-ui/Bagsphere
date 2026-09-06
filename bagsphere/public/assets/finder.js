const root=document.getElementById('finderCard')||document.getElementById('finderRoot');
const id=location.pathname.split('/').filter(Boolean).pop();
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
async function load(){
 const r=await fetch('/api/bag/'+encodeURIComponent(id)); const d=await r.json();
 if(!r.ok){root.innerHTML='<div class="finder-card"><h2>BagSphere tag not found</h2><p class="finder-privacy">This NFC tag may be invalid or no longer registered.</p></div>';return}
 const avatar=d.photo?`<img class="avatar-big" src="${d.photo}" alt="Owner profile photo">`:`<div class="avatar-big">${esc((d.studentName||'B')[0])}</div>`;
 root.innerHTML=`${avatar}<div class="section-label">YOU FOUND A BAG</div><h1>${esc(d.studentName)}</h1><div class="student">Student ID · ${esc(d.studentId)}</div>
 <div class="owner-message">“${esc(d.message||'Thank you for helping me get my bag back. Please call me.') }”</div>
 <a class="call-btn" href="tel:${encodeURIComponent(d.phone)}">☎ Call the owner</a>
 <button id="locBtn" class="location-btn">⌖ Share find location</button><div id="locStatus" class="loc-status"></div>`;
 document.getElementById('locBtn').onclick=shareLocation;
}
async function shareLocation(){
 const btn=document.getElementById('locBtn'),status=document.getElementById('locStatus');
 if(!navigator.geolocation){status.textContent='Location is not supported by this browser.';return}
 btn.disabled=true;btn.textContent='Requesting permission…';status.textContent='Your browser will ask for location permission.';
 navigator.geolocation.getCurrentPosition(async pos=>{
   status.textContent='Location permission approved. Sending…';
   const fd=new URLSearchParams({bag_id:id,latitude:pos.coords.latitude,longitude:pos.coords.longitude});
   const r=await fetch('/api/report',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd});
   const d=await r.json();
   if(r.ok){btn.textContent='✓ Location shared';status.textContent='The owner has received the approximate GPS coordinates.'}
   else{btn.disabled=false;btn.textContent='⌖ Share find location';status.textContent=d.error||'Could not share location.'}
 },err=>{
   btn.disabled=false;btn.textContent='⌖ Share find location';
   status.textContent=err.code===1?'Location permission was denied. You can still call the owner.':'Could not get your location.';
 },{enableHighAccuracy:true,timeout:10000,maximumAge:0});
}
load();
