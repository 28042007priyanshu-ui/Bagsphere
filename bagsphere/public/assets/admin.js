const state = { token: localStorage.getItem('bagsphere_admin_token') || '' };

const loginPanel = document.getElementById('loginPanel');
const adminPanel = document.getElementById('adminPanel');
const loginForm = document.getElementById('loginForm');
const loginError = document.getElementById('loginError');
const tbody = document.getElementById('bagsTableBody');
const emptyState = document.getElementById('emptyState');
const statBags = document.getElementById('statBags');
const statReports = document.getElementById('statReports');
const searchBox = document.getElementById('searchBox');

let allBags = [];

function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function fmtTime(ts) {
  if (!ts) return 'No reports yet';
  return new Date(ts).toLocaleString([], { dateStyle: 'medium', timeStyle: 'short' });
}

function showAdmin() { loginPanel.classList.add('hidden'); adminPanel.classList.remove('hidden'); loadBags(); }
function showLogin() { adminPanel.classList.add('hidden'); loginPanel.classList.remove('hidden'); }

async function loadBags() {
  const r = await fetch('/api/admin/bags?token=' + encodeURIComponent(state.token));
  if (r.status === 401) { forceLogout(); return; }
  const d = await r.json();
  allBags = d.bags || [];
  statBags.textContent = d.totalBags || 0;
  statReports.textContent = d.totalReports || 0;
  render(allBags);
}

function render(list) {
  if (!list.length) { tbody.innerHTML = ''; emptyState.classList.remove('hidden'); return; }
  emptyState.classList.add('hidden');
  tbody.innerHTML = list.map(b => `<tr>
    <td><code>${esc(b.bagId)}</code></td>
    <td>${esc(b.studentName)}</td>
    <td>${esc(b.studentId)}</td>
    <td>${esc(b.phone)}</td>
    <td>${b.reportCount}</td>
    <td>${esc(fmtTime(b.latestReport))}</td>
    <td class="admin-row-actions">
      <button onclick="viewBag('${esc(b.bagId)}')">View</button>
      <button class="admin-danger" onclick="deleteBag('${esc(b.bagId)}')">Delete</button>
    </td>
  </tr>`).join('');
}

searchBox.addEventListener('input', () => {
  const q = searchBox.value.toLowerCase().trim();
  if (!q) { render(allBags); return; }
  render(allBags.filter(b => [b.bagId, b.studentName, b.studentId, b.phone].some(v => String(v || '').toLowerCase().includes(q))));
});

async function viewBag(id) {
  const r = await fetch('/api/admin/bag/' + encodeURIComponent(id) + '?token=' + encodeURIComponent(state.token));
  if (r.status === 401) { forceLogout(); return; }
  const d = await r.json();
  if (!r.ok) { alert(d.error || 'Could not load bag.'); return; }

  const publicUrl = location.origin + '/bag/' + d.bagId;
  const dashUrl = location.origin + '/dashboard?bag=' + encodeURIComponent(d.bagId) + '&token=' + encodeURIComponent(d.ownerToken);
  const detail = document.getElementById('detailBody');
  detail.innerHTML = `<span class="section-label">BAG DETAIL</span><h2>${esc(d.studentName)}</h2>
    <div class="profile-meta">
      <div class="meta">Bag ID<b>${esc(d.bagId)}</b></div>
      <div class="meta">Student ID<b>${esc(d.studentId)}</b></div>
      <div class="meta">Phone<b>${esc(d.phone)}</b></div>
      <div class="meta">Message<b>${esc(d.message || '—')}</b></div>
    </div>
    <div class="tag-url">${esc(publicUrl)}</div>
    <a class="btn btn-primary full" target="_blank" href="${dashUrl}">Open owner dashboard ↗</a>
    <div class="admin-reports-list">${
      d.reports.length
        ? d.reports.map(x => `<div class="report"><div><b>Finder location</b><small>${esc(new Date(x.timestamp).toLocaleString())}</small><small>${x.latitude.toFixed(5)}, ${x.longitude.toFixed(5)}</small></div><a class="maps" target="_blank" href="https://www.google.com/maps/search/?api=1&query=${x.latitude},${x.longitude}">Open Maps ↗</a></div>`).join('')
        : '<div class="empty">No finder reports yet.</div>'
    }</div>`;
  document.getElementById('detailModal').classList.remove('hidden');
}
function closeDetail() { document.getElementById('detailModal').classList.add('hidden'); }

async function deleteBag(id) {
  if (!confirm('Delete bag ' + id + ' and all its finder reports? This cannot be undone.')) return;
  const fd = new URLSearchParams({ token: state.token, bag_id: id });
  const r = await fetch('/api/admin/delete', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: fd });
  if (r.status === 401) { forceLogout(); return; }
  const d = await r.json();
  if (!r.ok) { alert(d.error || 'Could not delete bag.'); return; }
  loadBags();
}

function forceLogout() {
  localStorage.removeItem('bagsphere_admin_token');
  state.token = '';
  showLogin();
}

loginForm.addEventListener('submit', async e => {
  e.preventDefault();
  loginError.textContent = '';
  const password = document.getElementById('adminPassword').value;
  const fd = new URLSearchParams({ password });
  const r = await fetch('/api/admin/login', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: fd });
  const d = await r.json();
  if (!r.ok) { loginError.textContent = d.error || 'Incorrect password.'; return; }
  state.token = d.token;
  localStorage.setItem('bagsphere_admin_token', d.token);
  document.getElementById('adminPassword').value = '';
  showAdmin();
});

document.getElementById('logoutBtn').addEventListener('click', async () => {
  await fetch('/api/admin/logout', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: new URLSearchParams({ token: state.token }) });
  forceLogout();
});

document.getElementById('refreshBtn').addEventListener('click', loadBags);

if (state.token) showAdmin(); else showLogin();
