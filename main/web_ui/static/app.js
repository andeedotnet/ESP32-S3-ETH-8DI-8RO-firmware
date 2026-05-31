const NUM_RELAYS = 8;
const NUM_INPUTS = 8;
let relayStates = new Array(NUM_RELAYS).fill(0);
let inputStates = new Array(NUM_INPUTS).fill(0);

/* ---- Live polling (500 ms) ---- */
let pollTimer = null;

function startPolling() {
  if (pollTimer) return;
  pollState();
  pollTimer = setInterval(pollState, 500);
}

function pollState() {
  fetch('/api/v1/state')
    .then(r => r.json())
    .then(d => {
      if (d.relays) { relayStates = d.relays; updateAllRelayEls(); }
      if (d.inputs) { inputStates = d.inputs; updateAllInputEls(); }
    })
    .catch(() => {});
}

/* ---- Tab routing ---- */
document.getElementById('mainTabs').addEventListener('shown.bs.tab', e => {
  const target = e.target.dataset.bsTarget;
  if (target === '#page-inputs')   { loadInputModes(); }
  if (target === '#page-network')  loadNetwork();
  if (target === '#page-webhooks') loadWebhooks();
  if (target === '#page-ntp')      loadNtp();
  if (target === '#page-system')   loadSystem();
});

/* ---- Relays ---- */
function buildRelayGrid() {
  const grid = document.getElementById('relay-grid');
  grid.innerHTML = '';
  for (let i = 0; i < NUM_RELAYS; i++) {
    const col = document.createElement('div');
    col.className = 'col';
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.id = 'relay-' + i;
    btn.className = 'btn w-100 py-3 ' + (relayStates[i] ? 'btn-success' : 'btn-outline-secondary');
    btn.innerHTML = '<small class="d-block">Relay ' + (i+1) + '</small><strong>' + (relayStates[i] ? 'ON' : 'OFF') + '</strong>';
    btn.onclick = () => toggleRelay(i);
    col.appendChild(btn);
    grid.appendChild(col);
  }
}

function updateAllRelayEls() {
  for (let i = 0; i < NUM_RELAYS; i++) {
    const btn = document.getElementById('relay-' + i);
    if (!btn) continue;
    btn.className = 'btn w-100 py-3 ' + (relayStates[i] ? 'btn-success' : 'btn-outline-secondary');
    btn.querySelector('strong').textContent = relayStates[i] ? 'ON' : 'OFF';
  }
}

function toggleRelay(idx) {
  const newState = relayStates[idx] ? 0 : 1;
  fetch('/api/v1/relays/' + (idx+1), {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({state: newState})
  })
  .then(r => r.json())
  .then(d => { relayStates[idx] = d.state; updateAllRelayEls(); })
  .catch(e => console.error('relay set failed', e));
}

function loadRelays() {
  fetch('/api/v1/relays')
    .then(r => r.json())
    .then(d => { relayStates = d.relays; buildRelayGrid(); })
    .catch(e => console.error('relay load failed', e));
}

/* ---- Inputs UI update (called by SSE) ---- */
function updateAllInputEls() {
  for (let i = 0; i < NUM_INPUTS; i++) {
    const badge = document.getElementById('di-state-' + i);
    if (!badge) continue;
    badge.className = 'badge rounded-pill ' + (inputStates[i] ? 'text-bg-primary' : 'text-bg-secondary');
    badge.textContent = inputStates[i] ? 'HIGH' : 'LOW';
  }
}

/* ---- Input Modes + state combined ---- */
const MODE_LABELS = ['OFF', 'Momentary', 'Latching'];

function loadInputModes() {
  Promise.all([
    fetch('/api/v1/inputs').then(r => r.json()),
    fetch('/api/v1/input_modes').then(r => r.json())
  ])
  .then(([inputData, modeData]) => {
    inputStates = inputData.inputs;
    buildInputTable(modeData.modes);
  })
  .catch(e => console.error('input load failed', e));
}

function buildInputTable(modes) {
  const tbody = document.getElementById('input-tbody');
  tbody.innerHTML = '';
  for (let i = 0; i < NUM_INPUTS; i++) {
    const cfg = modes[i] || {mode: 0, relay: i+1};
    const state = inputStates[i];
    const tr = document.createElement('tr');

    /* Mode radio buttons */
    const modeBtns = MODE_LABELS.map((label, m) => {
      const id = 'imode-' + i + '-' + m;
      return '<input type="radio" class="btn-check" name="imode-' + i + '" id="' + id + '" value="' + m + '"' +
             (cfg.mode === m ? ' checked' : '') + ' onchange="saveInputMode(' + i + ')">' +
             '<label class="btn btn-sm btn-outline-secondary" for="' + id + '">' + label + '</label>';
    }).join('');

    /* Relay checkboxes (1-8), multi-select via bitmask */
    const rmask = (cfg.relay_mask != null) ? cfg.relay_mask : (1 << i);
    const relayBtns = Array.from({length: 8}, (_, r) => {
      const id = 'irly-' + i + '-' + (r+1);
      return '<input type="checkbox" class="btn-check" id="' + id + '"' +
             ((rmask >> r) & 1 ? ' checked' : '') + ' onchange="saveInputMode(' + i + ')">' +
             '<label class="btn btn-sm btn-outline-primary" for="' + id + '">' + (r+1) + '</label>';
    }).join('');

    tr.innerHTML =
      '<td class="text-nowrap align-middle">DI ' + (i+1) + '</td>' +
      '<td class="align-middle"><span id="di-state-' + i + '" class="badge rounded-pill ' +
        (state ? 'text-bg-primary' : 'text-bg-secondary') + '">' +
        (state ? 'HIGH' : 'LOW') + '</span></td>' +
      '<td><div class="btn-group" role="group">' + modeBtns + '</div></td>' +
      '<td><div class="btn-group" role="group" id="irly-group-' + i + '">' + relayBtns + '</div></td>';
    tbody.appendChild(tr);
    updateRelayGroupVisibility(i, cfg.mode);
  }
}

function updateRelayGroupVisibility(i, mode) {
  const grp = document.getElementById('irly-group-' + i);
  if (grp) grp.style.opacity = (mode === 0) ? '0.3' : '1';
}

function saveInputMode(idx) {
  const modeEl = document.querySelector('input[name="imode-' + idx + '"]:checked');
  const mode = modeEl ? parseInt(modeEl.value) : 0;
  let relay_mask = 0;
  for (let r = 0; r < 8; r++) {
    const cb = document.getElementById('irly-' + idx + '-' + (r+1));
    if (cb && cb.checked) relay_mask |= (1 << r);
  }
  updateRelayGroupVisibility(idx, mode);
  fetch('/api/v1/input_modes/' + (idx+1), {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({mode, relay_mask})
  })
  .then(r => r.json())
  .catch(e => console.error('input mode save failed', e));
}

/* ---- Network ---- */
function loadNetwork() {
  fetch('/api/v1/network')
    .then(r => r.json())
    .then(d => {
      const ns = document.getElementById('net-status');
      ns.innerHTML = '';
      const addBadge = (label, ip, on) => {
        const span = document.createElement('span');
        span.className = 'badge rounded-pill px-3 py-2 ' + (on ? 'text-bg-success' : 'text-bg-danger');
        span.textContent = label + (ip ? ': ' + ip : '') + (on ? '' : ' (disconnected)');
        ns.appendChild(span);
      };
      addBadge('Ethernet', d.eth_ip, d.eth_connected);
      addBadge('WiFi', d.wifi_ip, d.wifi_connected);
      if (d.wifi_ssid) document.getElementById('wifi-ssid').value = d.wifi_ssid;
      if (d.eth_ipcfg) fillIpConfig(d.eth_ipcfg);
    })
    .catch(e => console.error('network load failed', e));
}

function fillIpConfig(cfg) {
  document.getElementById('ip-dhcp').checked   = !!cfg.dhcp;
  document.getElementById('ip-static').checked = !cfg.dhcp;
  document.getElementById('ip-addr').value = cfg.ip      || '';
  document.getElementById('ip-mask').value = cfg.netmask || '';
  document.getElementById('ip-gw').value   = cfg.gateway || '';
  document.getElementById('ip-dns').value  = cfg.dns     || '';
  onIpModeChange();
}

function onIpModeChange() {
  const dhcp = document.getElementById('ip-dhcp').checked;
  document.getElementById('ip-static-fields').style.opacity = dhcp ? '0.4' : '1';
  ['ip-addr', 'ip-mask', 'ip-gw', 'ip-dns'].forEach(id => {
    document.getElementById(id).disabled = dhcp;
  });
}

function saveIpConfig() {
  const dhcp = document.getElementById('ip-dhcp').checked;
  const st = document.getElementById('ip-status');
  const body = {
    dhcp,
    ip:      document.getElementById('ip-addr').value.trim(),
    netmask: document.getElementById('ip-mask').value.trim(),
    gateway: document.getElementById('ip-gw').value.trim(),
    dns:     document.getElementById('ip-dns').value.trim()
  };
  if (!dhcp && (!body.ip || !body.netmask)) {
    st.textContent = 'Static mode needs IP and subnet mask.'; st.className = 'small mt-2 text-danger'; return;
  }
  st.textContent = 'Saving...'; st.className = 'small mt-2 text-secondary';
  fetch('/api/v1/network/ip', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(body)
  })
  .then(r => r.json())
  .then(() => { st.textContent = 'Saved. Reboot the device to apply.'; st.className = 'small mt-2 text-success'; })
  .catch(e => { st.textContent = 'Error: ' + e; st.className = 'small mt-2 text-danger'; });
}

/* ---- System ---- */
function fmtUptime(s) {
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60), sec = s % 60;
  return (d ? d + 'd ' : '') + (h ? h + 'h ' : '') + (m ? m + 'm ' : '') + sec + 's';
}

function loadSystem() {
  fetch('/api/v1/system')
    .then(r => r.json())
    .then(d => {
      const rows = [
        ['Firmware version', d.fw_version],
        ['Firmware built',   d.fw_built],
        ['ESP-IDF version',  d.idf_version],
        ['Uptime',           fmtUptime(d.uptime_s)],
        ['Free heap',        (d.free_heap / 1024).toFixed(1) + ' KB'],
        ['Min free heap',    (d.min_free_heap / 1024).toFixed(1) + ' KB'],
        ['Largest block',    (d.largest_block / 1024).toFixed(1) + ' KB'],
        ['Last reset reason', d.reset_reason]
      ];
      document.getElementById('system-tbody').innerHTML = rows.map(
        ([k, v]) => '<tr><td class="text-secondary" style="width:45%">' + k + '</td><td><code>' + escHtml(String(v)) + '</code></td></tr>'
      ).join('');
    })
    .catch(e => console.error('system load failed', e));
}

function saveWifi() {
  const ssid = document.getElementById('wifi-ssid').value.trim();
  const pass = document.getElementById('wifi-pass').value;
  const st = document.getElementById('wifi-status');
  if (!ssid) { st.textContent = 'SSID required.'; st.className = 'small mt-2 text-danger'; return; }
  st.textContent = 'Saving...'; st.className = 'small mt-2 text-secondary';
  fetch('/api/v1/network/wifi', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({ssid, password: pass})
  })
  .then(r => r.json())
  .then(() => { st.textContent = 'Saved. Board is connecting...'; st.className = 'small mt-2 text-success'; })
  .catch(e => { st.textContent = 'Error: ' + e; st.className = 'small mt-2 text-danger'; });
}

/* ---- Webhooks ---- */
const TRIGGERS = ['rising', 'falling', 'change'];

function loadWebhooks() {
  fetch('/api/v1/webhooks')
    .then(r => r.json())
    .then(d => buildWebhookTable(d.webhooks))
    .catch(e => console.error('webhook load failed', e));
}

function buildWebhookTable(webhooks) {
  const tbody = document.getElementById('wh-tbody');
  tbody.innerHTML = '';
  for (let i = 0; i < NUM_INPUTS; i++) {
    const wh = webhooks[i] || {input: i+1, url: '', trigger: 'rising', enabled: true};
    const tr = document.createElement('tr');
    const enChecked = wh.enabled !== false;
    tr.innerHTML =
      '<td class="text-nowrap align-middle">DI ' + (i+1) + '</td>' +
      '<td class="align-middle">' +
        '<div class="btn-group btn-group-sm" role="group">' +
          '<input type="radio" class="btn-check" name="wh-en-' + i + '" id="wh-en-on-' + i + '" value="1"' + (enChecked ? ' checked' : '') + '>' +
          '<label class="btn btn-outline-success" for="wh-en-on-' + i + '">ON</label>' +
          '<input type="radio" class="btn-check" name="wh-en-' + i + '" id="wh-en-off-' + i + '" value="0"' + (!enChecked ? ' checked' : '') + '>' +
          '<label class="btn btn-outline-danger" for="wh-en-off-' + i + '">OFF</label>' +
        '</div>' +
      '</td>' +
      '<td><input type="text" class="form-control form-control-sm" id="wh-url-' + i + '" value="' + escHtml(wh.url) + '" placeholder="http://..."></td>' +
      '<td><select class="form-select form-select-sm" id="wh-trig-' + i + '">' +
        TRIGGERS.map(t => '<option value="' + t + '"' + (t === wh.trigger ? ' selected' : '') + '>' + t + '</option>').join('') +
      '</select></td>' +
      '<td><button class="btn btn-sm btn-dark text-nowrap" onclick="saveWebhook(' + i + ')">Save</button></td>';
    tbody.appendChild(tr);
  }
}

function saveWebhook(idx) {
  const url     = document.getElementById('wh-url-' + idx).value.trim();
  const trig    = document.getElementById('wh-trig-' + idx).value;
  const enEl    = document.querySelector('input[name="wh-en-' + idx + '"]:checked');
  const enabled = enEl ? enEl.value === '1' : true;
  const st      = document.getElementById('wh-status');
  fetch('/api/v1/webhooks/' + (idx+1), {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({url, trigger: trig, enabled})
  })
  .then(r => r.json())
  .then(() => { st.textContent = 'DI' + (idx+1) + ' saved.'; st.className = 'small mt-2 text-success'; })
  .catch(e => { st.textContent = 'Error: ' + e; st.className = 'small mt-2 text-danger'; });
}

/* ---- NTP ---- */
function loadNtp() {
  fetch('/api/v1/ntp')
    .then(r => r.json())
    .then(d => {
      document.getElementById('ntp-server').value = d.server || 'pool.ntp.org';
      document.getElementById('ntp-tz').value     = d.tz     || 'UTC0';
      const badge = document.getElementById('ntp-status-badge');
      badge.className = 'badge rounded-pill ' + (d.synced ? 'text-bg-success' : 'text-bg-warning');
      badge.textContent = d.synced ? ('Synced: ' + d.time) : 'Not synced';
    })
    .catch(e => console.error('ntp load failed', e));
}

function saveNtp() {
  const server = document.getElementById('ntp-server').value.trim();
  const tz     = document.getElementById('ntp-tz').value.trim();
  const st     = document.getElementById('ntp-save-status');
  if (!server) { st.textContent = 'Server required.'; st.className = 'small mt-2 text-danger'; return; }
  st.textContent = 'Saving...'; st.className = 'small mt-2 text-secondary';
  fetch('/api/v1/ntp', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({server, tz})
  })
  .then(r => r.json())
  .then(() => {
    st.textContent = 'Saved. Syncing...'; st.className = 'small mt-2 text-success';
    setTimeout(loadNtp, 3000);
  })
  .catch(e => { st.textContent = 'Error: ' + e; st.className = 'small mt-2 text-danger'; });
}

function escHtml(s) {
  return s.replace(/&/g,'&amp;').replace(/"/g,'&quot;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

loadRelays();
startPolling();
