const form=document.getElementById('registerForm');
let photoData='';
document.getElementById('photo').addEventListener('change',e=>{
 const file=e.target.files[0]; if(!file)return;
 const reader=new FileReader(); reader.onload=()=>photoData=reader.result; reader.readAsDataURL(file);
});
form.addEventListener('submit',async e=>{
 e.preventDefault();
 const fd=new URLSearchParams(new FormData(form)); fd.set('photo_data',photoData);
 const res=await fetch('/api/register',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:fd});
 const data=await res.json();
 if(!res.ok){alert(data.error||'Could not register bag.');return}
 document.getElementById('tagUrl').textContent=data.url;
 document.getElementById('dashLink').href=data.dashboard;
 document.getElementById('resultModal').classList.remove('hidden');
});
function closeModal(){document.getElementById('resultModal').classList.add('hidden')}
async function copyTag(){try{await navigator.clipboard.writeText(document.getElementById('tagUrl').textContent);event.target.textContent='Copied ✓';}catch(e){alert('Copy failed. Select and copy the URL.')}}

function routeBag(){
 const path=location.pathname;
 if(path.startsWith('/bag/')){
   document.body.innerHTML='<div id="finderRoot"></div>';
   const s=document.createElement('script');s.src='/assets/finder.js';document.body.appendChild(s);
 } else if(path==='/dashboard'){}
}
