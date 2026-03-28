const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Dino Control</title>
  <link rel="icon" type="image/png" href="/icon.png">
  <link rel="apple-touch-icon" href="/icon.png">
  <style>
    * { box-sizing: border-box; }
    body { font-family: -apple-system, sans-serif; background: #121212; color: #e0e0e0; margin: 0; padding: 5px; }
    .hidden { display: none !important; }
    .container { max-width: 100%; margin: auto; background: #1e1e1e; padding: 10px; border-radius: 10px; }
    h2 { font-size: 1.2rem; margin: 10px 0; color: #3498db; text-align: center; }
    table { width: 100%; border-collapse: collapse; table-layout: fixed; }
    th { font-size: 0.65rem; padding: 8px 2px; border-bottom: 2px solid #333; color: #888; text-transform: uppercase; }
    td { padding: 10px 2px; border-bottom: 2px solid #333; vertical-align: top; }
    td:first-child { 
    vertical-align: middle; 
    text-align: center; 
    font-size: 1.1rem; 
    background: rgba(52, 152, 219, 0.1);
  }
    th:nth-child(1), td:nth-child(1) { width: 30px; text-align: center; } 
    th:nth-child(4), td:nth-child(4) { width: 40px; text-align: center; }
    .relay-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; margin-bottom: 8px; }
    .relay-item { display: flex; flex-direction: column; align-items: center; font-size: 0.6rem; color: #666; }
    input[type="checkbox"] { width: 18px; height: 18px; accent-color: #3498db; margin: 2px 0; }
    input[type="number"], select { background: #2a2a2a; color: #fff; border: 1px solid #444; padding: 8px; border-radius: 6px; width: 100%; font-size: 0.9rem; text-align: center; }
    .sound-fullwidth { margin-top: 10px; background: #252525; padding: 8px; border-radius: 8px; border: 1px dashed #444; display: flex; align-items: center; gap: 10px; }
    .sound-fullwidth span { font-size: 0.75rem; color: #3498db; white-space: nowrap; font-weight: bold; }
    .btn { width: 100%; padding: 12px; margin-top: 8px; border: none; border-radius: 8px; font-size: 0.9rem; font-weight: bold; cursor: pointer; }
    .add { background: #3498db; color: white; }
    .save { background: #27ae60; color: white; margin-top: 15px; }
    .del-btn { background: #c0392b; color: white; width: 32px; height: 32px; border-radius: 6px; border: none; margin-top: 20px; }
    .control-panel { background: #252525; padding: 15px; border-radius: 12px; margin: 10px 0; border: 1px solid #333; }
    .volume-wrapper { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; }
    .btn-vol { width: 55px; height: 40px; border: none; border-radius: 20px; color: #fff; font-weight: bold; font-size: 1.2rem; }
    .note { width: 100%;  margin-top: 15px;  text-align: center;  color: #7f8c8d;  font-size: 0.85rem;  line-height: 1.4; }
	.note h5 {margin: 0;  font-weight: normal;}
    .note b {color: #e67e22;}
  </style>
</head><body>
  <div class="container">
    <h2>Dino Control</h2>
    <div class="control-panel" style="padding-bottom: 5px;">
      <span style="font-size: 0.95rem; color: #3498db; font-weight: bold;">🔄 Запуск программы каждые N минут:</span>
      <div style="width: 120px; margin: 10px auto 0 auto;"> 
        <select id="repeat-interval" style="padding: 5px; font-size: 0.8rem;">
          <option value="0">ВЫКЛ</option>
          <option value="1">1 мин</option>
          <option value="5">5 мин</option>
          <option value="15">15 мин</option>
          <option value="30">30 мин</option>
          <option value="60">60 мин</option>
        </select>
	    </div>
    </div>
    <table id="seqTable">
      <thead>
        <tr>
          <th>№</th>
          <th colspan="2">Настройка шага<br><small>(Выходы / Режим / Трек)</small></th>
          <th>Удалить Шаг</th>
        </tr>
      </thead>
      <tbody></tbody>
    </table>

    <button class="btn add" onclick="addRow()">+ ДОБАВИТЬ ШАГ</button>

    <div class="control-panel">
    <h2><small>Настроить Громкость*</small></h2>
      <div class="volume-wrapper">
        <button class="btn-vol" style="background:#c0392b" onclick="changeVol(-1)">-</button>
        <div style="text-align:center"><b id="vol-val" style="font-size:1.4rem">10</b><br><small>ГРОМКОСТЬ</small></div>
        <button class="btn-vol" style="background:#3498db" onclick="changeVol(1)">+</button>
      </div>
    </div>
    <div class="control-panel">
      <h2>Прослушать трек</h2>
      <div style="display:flex; gap:8px;">
        <select id="track-num" style="flex:1; padding: 5px; border-radius: 6px; border: 1px solid #ccc; font-size: 11px;"></select>
        <button onclick="playTest()" style="flex:2; background:#27ae60; color:white; border:none; border-radius:6px; font-weight:bold">ПРОСЛУШАТЬ</button>
      </div>
    </div>
    <button class="btn save" onclick="saveData()">СОХРАНИТЬ И ЗАПУСТИТЬ</button>
    <div class="note">
      <h5>* Громкость обновится в памяти устройства <b>только</b> после нажатия кнопки «Сохранить и запустить»</h5>
    </div>
  </div>
  
  <script>
      let lastSaveTime = 0;

      fetch('/get-data')
      .then(r => r.json())
      .then(d => {
      if (d.vol !== undefined) {
        const volEl = document.getElementById('vol-val');
        if (volEl) volEl.innerText = d.vol;
      }
      if (d.interval !== undefined) {
        const intEl = document.getElementById('repeat-interval');
        if (intEl) intEl.value = d.interval;
      }
      const steps = d.steps || (Array.isArray(d) ? d : []);
      const tbody = document.querySelector("#seqTable tbody");
      tbody.innerHTML = ""; 
      if (steps.length > 0) {
        steps.forEach(s => addRow(s));
      } else {
        addRow(); 
      }
      })
      .catch(() => addRow());
        
      const select = document.getElementById('track-num');
      for (let i = 1; i <= 20; i++) {
        let opt = document.createElement('option');
        opt.value = i;
        opt.innerHTML = `Трек ${i}`;
        select.appendChild(opt);
      }

      function changeVol(delta) {
        const volEl = document.getElementById('vol-val');
        let current = parseInt(volEl.innerText) || 0;
        current += delta;
        if (current < 0) current = 0;
        if (current > 30) current = 30;
        volEl.innerText = current;
      }

      function playTest() { fetch(`/playtest?track=${document.getElementById('track-num').value}`); }

      function renumberSteps() {
        document.querySelectorAll("#seqTable tbody tr").forEach((row, index) => {
          row.cells[0].innerText = index + 1;
        });
      }

      function toggleMode(el) {
        const row = el.closest('tr');
        const isPin = el.value === 'pin';
        
        row.querySelector('.time-input').classList.toggle('hidden', isPin);
        row.querySelector('.pin-input').classList.toggle('hidden', !isPin);
        
        row.querySelectorAll('.relay-grid input').forEach(cb => {
          cb.disabled = isPin;
          if (isPin) cb.checked = false;
        });
        
        const sInput = row.querySelector('.sound-input');
        sInput.disabled = isPin;
        if (isPin) sInput.value = "0";
      }

      function addRow(data = {}) {
        const tbody = document.querySelector("#seqTable tbody");
        const row = tbody.insertRow();
        
        const isPin = data.t === undefined || (data.in && data.in > 0); 
        const p = data.p || [0,0,0,0,0,0,0,0];
        const t = data.t || 1000;
        const inP = data.in || (isPin ? 1 : 0);
        const s = data.s || 0; 

        let grid = '<div class="relay-grid">';
        for(let i=0; i<8; i++) {
          grid += `<div class="relay-item">${i+1}<input type="checkbox" ${p[i] && !isPin ?'checked':''} ${isPin?'disabled':''}></div>`;
        }
        grid += '</div>';

        row.innerHTML = `
          <td style="font-weight:bold; color:#3498db; vertical-align:middle; text-align:center; font-size:1.1rem;">${tbody.rows.length}</td>
          <td colspan="2" style="padding: 10px 5px;">
            <div style="display:flex; gap:10px; align-items: flex-start;">
              <div style="flex:1">${grid}</div>
              <div style="width:115px">
                <select class="type-select" onchange="toggleMode(this)">
                  <option value="pin" ${isPin?'selected':''}>🔌Вход</option>
                  <option value="time" ${!isPin?'selected':''}>⏳Время</option>
                </select>
                <div style="margin-top:5px";>
                  <input type="number" step="0.1" class="time-input ${isPin?'hidden':''}" value="${t/1000}">
                  <select class="pin-input ${!isPin?'hidden':''}">
                    ${[1,2,3,4,5,6,7,8].map(i => {const label = (i === 1) ? `PIR ${i}` : `IN ${i}`;
                    return `<option value="${i}" ${inP == i ? 'selected' : ''}>${label}</option>`;}).join('')}
                  </select>
                </div>
              </div>
            </div>
            <div class="sound-fullwidth">
              <span>🔊 ТРЕК №:</span>
              <select class="sound-input" ${isPin?'disabled':''} style="flex: 1; padding: 5px; font-size: 11px;">
                <option value="0" ${s == 0 || isPin ? 'selected' : ''}>--OFF--</option>
                ${[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20].map(i => 
                  `<option value="${i}" ${i == s && !isPin ? 'selected' : ''}>Трек ${i}</option>`
                ).join('')}
              </select>
            </div>
          </td>
          <td style="text-align:center; vertical-align:middle;">
            <button class="del-btn" onclick="if(confirm('Хотите ли вы удалить шаг?')) { this.parentElement.parentElement.remove(); renumberSteps(); }">✕</button>
          </td>
        `;
        renumberSteps();
      }

      function saveData() {
        const now = Date.now();
        if (now - lastSaveTime < 1250) {
          console.log("Too many save requests!");
          return; 
        }
        lastSaveTime = now;
        
        const config = [];
        document.querySelectorAll("#seqTable tbody tr").forEach(r => {
          const type = r.querySelector('.type-select').value;
          const isPin = type === 'pin';
          const pins = Array.from(r.querySelectorAll('.relay-grid input[type="checkbox"]')).map(c => c.checked ? 1 : 0);
          const soundNum = parseInt(r.querySelector('.sound-input').value) || 0;
          let step = { p: pins, s: soundNum, t: 0, in: 0 };
          if (type === 'time') {
            step.t = Math.round(parseFloat(r.querySelector('.time-input').value) * 1000);
          } else {
            step.in = parseInt(r.querySelector('.pin-input').value);
            step.p = [0,0,0,0,0,0,0,0];
            step.s = 0;
          }
          config.push(step);
        });
        const currentVol = parseInt(document.getElementById('vol-val').innerText) || 0;
        const repeatInterval = parseInt(document.getElementById('repeat-interval').value) || 0;
        fetch('/save', {
          method: 'POST',
          headers: {'Content-Type': 'application/json'},
          body: JSON.stringify({ 
            vol: currentVol, 
            interval: repeatInterval,
            steps: config 
          })
        }).then(res => { if(res.ok) alert("Сохранено!"); });
      }
  </script>
</body></html>
)rawliteral";

const char login_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <link rel="icon" type="image/png" href="/icon.png">
  <link rel="apple-touch-icon" href="/icon.png">
  <style>
    * { box-sizing: border-box; }
    body { font-family: sans-serif; background: #121212; color: #fff; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }
    form { background: #1e1e1e; padding: 30px; border-radius: 15px; width: 90%; max-width: 320px; text-align: center; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
    h2 { color: #3498db; margin-bottom: 20px; }
    input { width: 100%; padding: 15px; margin: 10px 0; background: #333; border: 1px solid #444; color: white; border-radius: 10px; font-size: 1.1rem; text-align: center; }
    button { width: 100%; padding: 15px; background: #27ae60; border: none; color: white; border-radius: 10px; font-size: 1.1rem; font-weight: bold; cursor: pointer; margin-top: 10px; }
    .error-msg { color: #e74c3c; font-size: 0.9rem; margin-top: 10px; display: none; }
  </style>
</head><body>
  <form action="/login" method="POST">
    <h2>Доступ ограничен</h2>
    <input type="password" name="pass" placeholder="Введите пароль" autofocus>
    <div id="err" class="error-msg">Неверный пароль!</div>
    <button type="submit">ВОЙТИ</button>
  </form>
  <script>
    if(window.location.search.includes('error')) document.getElementById('err').style.display='block';
  </script>
</body></html>
)rawliteral";

const char devlogin_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <style>
    * { box-sizing: border-box; }
    body { font-family: sans-serif; background: #121212; color: #fff; display: flex; align-items: center; justify-content: center; height: 100vh; margin: 0; }
    form { background: #1e1e1e; padding: 30px; border-radius: 15px; width: 90%; max-width: 320px; text-align: center; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
    h2 { color: #3498db; margin-bottom: 20px; }
    input { width: 100%; padding: 15px; margin: 10px 0; background: #333; border: 1px solid #444; color: white; border-radius: 10px; font-size: 1.1rem; text-align: center; }
    button { width: 100%; padding: 15px; background: #27ae60; border: none; color: white; border-radius: 10px; font-size: 1.1rem; font-weight: bold; cursor: pointer; margin-top: 10px; }
    .error-msg { color: #e74c3c; font-size: 0.9rem; margin-top: 10px; display: none; }
  </style>
</head>
<body>
  <form action="/devlogin" method="POST">
      <h2>devtools</h2>
      <input type="password" name="pass" placeholder="Введите пароль" autofocus>
      <div id="err" class="error-msg" style="display:none;">Неверный пароль!</div>
      <button type="submit">ВОЙТИ</button>
    </form>
    <script>
      if(window.location.search.includes('error')) {document.getElementById('err').style.display = 'block';}
    </script>
</body></html>
)rawliteral";

const char devtools_html[] PROGMEM = R"rawliteral(<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <style>
    * { box-sizing: border-box; }
    body { font-family: sans-serif; background: #121212; color: #fff; display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; padding: 20px; }
    .container { width: 100%; max-width: 320px; text-align: center; }
    h1 { color: #555; font-size: 1.1rem; margin-bottom: 10px; text-transform: uppercase; letter-spacing: 2px; }
    .data-display { 
      background: #1e1e1e; padding: 15px; border-radius: 15px; margin-bottom: 20px; 
      border: 1px dashed #444; font-family: monospace; font-size: 0.85rem; color: #27ae60;
      text-align: left; word-break: break-all;
    }
    .data-line { display: flex; justify-content: space-between; margin-bottom: 4px; border-bottom: 1px solid #2a2a2a; padding-bottom: 2px; }
    .data-key { color: #888; }
    form { background: #1e1e1e; padding: 30px; border-radius: 15px; width: 100%; text-align: center; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
    h2 { color: #3498db; margin-bottom: 20px; margin-top: 0; font-size: 1.4rem; }
    .toggle-container { display: flex; flex-direction: column; align-items: center; margin: 20px 0; }
    .label-text { margin-bottom: 12px; color: #bbb; font-size: 0.9rem; letter-spacing: 0.5px; }
    .switch { position: relative; display: inline-block; width: 54px; height: 28px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #333; transition: .3s; border-radius: 34px; border: 1px solid #444; }
    .slider:before { position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px; background-color: #888; transition: .3s; border-radius: 50%; }
    input:checked + .slider { background-color: #e74c3c; border-color: #e74c3c; }
    input:checked + .slider:before { transform: translateX(26px); background-color: #fff; }
    button { width: 100%; padding: 15px; background: #3498db; border: none; color: white; border-radius: 10px; font-size: 1.1rem; font-weight: bold; cursor: pointer; margin-top: 10px; transition: 0.3s; }
    button:disabled { opacity: 0.6; cursor: not-allowed; }
  </style>
</head>
<body>

<div class="container">
  <h1>devtools</h1>
  <div id="dev-data" class="data-display">Загрузка...</div>
  
  <form id="dev-form">
    <h2>Статус</h2>
    <div class="toggle-container">
      <span class="label-text" id="status-text">Разблокировано</span>
      <label class="switch">
        <input type="checkbox" id="lock-toggle">
        <span class="slider"></span>
      </label>
    </div>
    <button type="submit" id="submit-btn">Применить</button>
  </form>
  <small>⚠️ВНИМАНИЕ: При нажатии на «Применить» устройство будет перезагруженно!</small>
</div>

<script>
  const display = document.getElementById('dev-data');
  const lockToggle = document.getElementById('lock-toggle');
  const statusText = document.getElementById('status-text');
  const submitBtn = document.getElementById('submit-btn');

  function updateVisualStatus() {
    statusText.textContent = lockToggle.checked ? 'Заблокировано' : 'Разблокировано';
    statusText.style.color = lockToggle.checked ? '#e74c3c' : '#bbb';
  }

  async function loadDevData() {
    try {
      const response = await fetch('/get-dev-data');
      if (!response.ok) throw new Error();
      const data = await response.json(); // Парсим JSON
      lockToggle.checked = (data.blocked == 1);
      updateVisualStatus();
      let html = '';
      for (let key in data) {
        if (key !== 'blocked') {
          html += `<div class="data-line"><span class="data-key">${key}:</span><span>${data[key]}</span></div>`;
        }
      }
      display.innerHTML = html || 'Нет данных';
    } catch (error) {
      display.textContent = 'Ошибка загрузки';
      display.style.color = '#e74c3c';
    }
  }
  lockToggle.addEventListener('change', updateVisualStatus);
  document.getElementById('dev-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    submitBtn.disabled = true;
    const isLocked = lockToggle.checked;
    submitBtn.textContent = 'Сохранение...';
    try {
      const response = await fetch('/save-status', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ blocked: isLocked ? 1 : 0 })
      });
      submitBtn.style.background = '#27ae60';
      submitBtn.textContent = 'Готово!';  
    } catch (error) {
      submitBtn.style.background = '#27ae60';
      submitBtn.textContent = 'Готово!';
    } 
  });

  loadDevData();
</script>
</body>
</html>)rawliteral";