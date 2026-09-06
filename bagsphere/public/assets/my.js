const myForm = document.getElementById('myForm');
const myError = document.getElementById('myError');

myForm.addEventListener('submit', async e => {
  e.preventDefault();
  myError.textContent = '';
  const fd = new FormData(myForm);
  const bag = (fd.get('bag') || '').trim();
  const token = (fd.get('token') || '').trim();
  if (!bag || !token) { myError.textContent = 'Enter both the Bag ID and owner token.'; return; }

  const submitBtn = myForm.querySelector('button[type="submit"]');
  submitBtn.disabled = true;
  try {
    const r = await fetch('/api/bag/' + encodeURIComponent(bag));
    const d = await r.json();
    if (!r.ok) { myError.textContent = 'Bag not found. Check the Bag ID and try again.'; return; }
    if (d.ownerToken !== token) { myError.textContent = 'That owner token does not match this bag.'; return; }
    location.href = '/dashboard?bag=' + encodeURIComponent(bag) + '&token=' + encodeURIComponent(token);
  } catch (err) {
    myError.textContent = 'Could not reach the server. Try again.';
  } finally {
    submitBtn.disabled = false;
  }
});
