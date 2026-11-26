/* ============================================================
   THE PRINTING OFFICE — script.js
   Frontend logic: UI, fetch calls, animations, rendering.
   NO DSA logic here. All algorithm data comes from C++ backend.
   ============================================================ */

'use strict';

const API = 'http://localhost:8080';

/* ------------------------------------------------------------------ */
/*  DEPT COLORS (cycled by index)                                       */
/* ------------------------------------------------------------------ */
const DEPT_COLORS = [
  { border: '#58a6ff', bg: 'rgba(88,166,255,0.08)',  badge: 'rgba(88,166,255,0.2)',  text: '#58a6ff'  },
  { border: '#bc8cff', bg: 'rgba(188,140,255,0.08)', badge: 'rgba(188,140,255,0.2)', text: '#bc8cff'  },
  { border: '#3fb950', bg: 'rgba(63,185,80,0.08)',   badge: 'rgba(63,185,80,0.2)',   text: '#3fb950'  },
  { border: '#d29922', bg: 'rgba(210,153,34,0.08)',  badge: 'rgba(210,153,34,0.2)',  text: '#d29922'  },
  { border: '#f85149', bg: 'rgba(248,81,73,0.08)',   badge: 'rgba(248,81,73,0.2)',   text: '#f85149'  },
  { border: '#39c5cf', bg: 'rgba(57,197,207,0.08)',  badge: 'rgba(57,197,207,0.2)',  text: '#39c5cf'  },
  { border: '#ff7eb6', bg: 'rgba(255,126,182,0.08)', badge: 'rgba(255,126,182,0.2)', text: '#ff7eb6'  },
  { border: '#f78166', bg: 'rgba(247,129,102,0.08)', badge: 'rgba(247,129,102,0.2)', text: '#f78166'  },
];

/* ------------------------------------------------------------------ */
/*  STATE                                                                */
/* ------------------------------------------------------------------ */
let deptCounter = 0;
let departments  = []; // [{ id, code, jobs:[{id,time}] }]

// Merge playback state
let mergeSteps    = [];
let mergeTimeline = [];
let mergeStats    = null;
let mergeVerified = false;
let currentStep   = -1;
let isPlaying     = false;
let playTimer     = null;
let speedMs       = 700; // ms per step (default 1×)
let isPaused      = false;

/* ------------------------------------------------------------------ */
/*  INIT                                                                 */
/* ------------------------------------------------------------------ */
document.addEventListener('DOMContentLoaded', () => {
  checkHealth();
  setInterval(checkHealth, 5000);
  loadSampleData(); // start with sample so the page looks alive
});

/* ------------------------------------------------------------------ */
/*  HEALTH CHECK                                                         */
/* ------------------------------------------------------------------ */
async function checkHealth() {
  const dot   = document.getElementById('status-dot');
  const label = document.getElementById('status-label');
  const banner = document.getElementById('offline-banner');
  try {
    const res = await fetch(`${API}/api/health`, { signal: AbortSignal.timeout(3000) });
    if (res.ok) {
      dot.className   = 'status-dot online';
      label.textContent = 'Backend Online';
      banner.classList.add('hidden');
      return;
    }
  } catch (_) {}
  dot.className   = 'status-dot offline';
  label.textContent = 'Backend Offline';
  banner.classList.remove('hidden');
}

/* ------------------------------------------------------------------ */
/*  SAMPLE DATA                                                          */
/* ------------------------------------------------------------------ */
function loadSampleData() {
  departments = [];
  deptCounter = 0;
  document.getElementById('departments-container').innerHTML = '';

  const sample = [
    { code: 'A', jobs: [{ id: 'A2', time: 2 }, { id: 'A7', time: 7 }, { id: 'A9', time: 9 }] },
    { code: 'B', jobs: [{ id: 'B1', time: 1 }, { id: 'B7', time: 7 }] },
    { code: 'C', jobs: [{ id: 'C3', time: 3 }, { id: 'C10', time: 10 }] },
  ];

  sample.forEach(d => {
    const dept = addDepartment();
    const codeInput = document.querySelector(`#dept-${dept.id} .dept-code-input`);
    if (codeInput) { codeInput.value = d.code; dept.code = d.code; }
    d.jobs.forEach(j => {
      const job = addJob(dept.id);
      if (job) {
        job.idInput.value   = j.id;
        job.timeInput.value = j.time;
        // also update internal state
        const deptObj = departments.find(x => x.id === dept.id);
        if (deptObj) {
          const jObj = deptObj.jobs.find(x => x.uid === job.uid);
          if (jObj) { jObj.id = j.id; jObj.time = j.time; }
        }
      }
    });
  });

  refreshAll();
}

/* ------------------------------------------------------------------ */
/*  RESET                                                                */
/* ------------------------------------------------------------------ */
function resetAll() {
  departments = [];
  deptCounter = 0;
  document.getElementById('departments-container').innerHTML = '';
  document.getElementById('ll-container').innerHTML = '<div class="empty-state">Add departments above to see linked list visualization.</div>';
  updateStatCards();

  // Hide result sections
  ['section-merge', 'section-timeline', 'section-report'].forEach(id => {
    document.getElementById(id).classList.add('hidden');
  });

  stopPlayback();
  mergeSteps    = [];
  mergeTimeline = [];
  mergeStats    = null;
  currentStep   = -1;
  document.getElementById('stat-merge-status').textContent = 'Idle';
  document.getElementById('stat-merge-status').style.color = '';
}

/* ------------------------------------------------------------------ */
/*  ADD DEPARTMENT                                                        */
/* ------------------------------------------------------------------ */
function addDepartment() {
  const id    = ++deptCounter;
  const color = DEPT_COLORS[(id - 1) % DEPT_COLORS.length];
  const dept  = { id, code: `D${id}`, jobs: [], color };
  departments.push(dept);

  const container = document.getElementById('departments-container');
  const row = document.createElement('div');
  row.className  = 'dept-row';
  row.id         = `dept-${id}`;
  row.style.borderColor = color.border + '40';
  row.innerHTML  = `
    <div class="dept-row-header">
      <span class="dept-code-label">Dept Code:</span>
      <input class="dept-code-input" type="text" maxlength="4" value="${dept.code}"
             style="border-color:${color.border}40; color:${color.text}"
             oninput="onDeptCodeChange(${id}, this.value)" />
      <span style="flex:1"></span>
      <button class="btn btn-secondary" onclick="addJob(${id})" style="font-size:12px;padding:6px 12px;">＋ Add Job</button>
      <button class="btn btn-danger" onclick="removeDepartment(${id})" style="font-size:12px;padding:6px 12px;">✕ Remove</button>
    </div>
    <div class="dept-jobs-area" id="jobs-${id}"></div>
    <div class="dept-error" id="error-${id}"></div>
    <div class="mini-ll" id="mini-ll-${id}"></div>
  `;
  container.appendChild(row);
  row.style.animation = 'nodeIn 0.3s ease';

  refreshAll();
  return dept;
}

/* ------------------------------------------------------------------ */
/*  REMOVE DEPARTMENT                                                     */
/* ------------------------------------------------------------------ */
function removeDepartment(id) {
  departments = departments.filter(d => d.id !== id);
  const el = document.getElementById(`dept-${id}`);
  if (el) el.remove();
  refreshAll();
}

/* ------------------------------------------------------------------ */
/*  DEPT CODE CHANGE                                                      */
/* ------------------------------------------------------------------ */
function onDeptCodeChange(id, val) {
  const dept = departments.find(d => d.id === id);
  if (dept) dept.code = val.toUpperCase();
  refreshAll();
}

/* ------------------------------------------------------------------ */
/*  ADD JOB                                                               */
/* ------------------------------------------------------------------ */
let jobCounter = 0;
function addJob(deptId) {
  const dept = departments.find(d => d.id === deptId);
  if (!dept) return null;

  const uid  = ++jobCounter;
  const job  = { uid, id: '', time: '' };
  dept.jobs.push(job);

  const container = document.getElementById(`jobs-${deptId}`);
  const entry     = document.createElement('div');
  entry.className = 'job-entry';
  entry.id        = `job-${uid}`;
  entry.style.borderColor = dept.color.border + '30';
  entry.innerHTML = `
    <div style="display:flex;flex-direction:column;gap:3px">
      <span class="job-entry-label">Job ID</span>
      <input class="id-input" type="text" placeholder="e.g. A2"
             oninput="onJobChange(${deptId}, ${uid}, 'id', this.value)" />
    </div>
    <div style="display:flex;flex-direction:column;gap:3px">
      <span class="job-entry-label">Time</span>
      <input class="time-input" type="number" placeholder="0" min="0"
             oninput="onJobChange(${deptId}, ${uid}, 'time', this.value)" />
    </div>
    <button class="job-remove-btn" onclick="removeJob(${deptId}, ${uid})" title="Remove job">✕</button>
  `;
  container.appendChild(entry);
  entry.style.animation = 'nodeIn 0.3s ease';

  // Return references to inputs for programmatic fill
  const idInput   = entry.querySelector('.id-input');
  const timeInput = entry.querySelector('.time-input');
  return { uid, idInput, timeInput };
}

/* ------------------------------------------------------------------ */
/*  JOB CHANGE                                                            */
/* ------------------------------------------------------------------ */
function onJobChange(deptId, uid, field, value) {
  const dept = departments.find(d => d.id === deptId);
  if (!dept) return;
  const job = dept.jobs.find(j => j.uid === uid);
  if (!job) return;
  job[field] = field === 'time' ? (value === '' ? '' : parseInt(value, 10)) : value;
  refreshAll();
}

/* ------------------------------------------------------------------ */
/*  REMOVE JOB                                                            */
/* ------------------------------------------------------------------ */
function removeJob(deptId, uid) {
  const dept = departments.find(d => d.id === deptId);
  if (!dept) return;
  dept.jobs = dept.jobs.filter(j => j.uid !== uid);
  const el = document.getElementById(`job-${uid}`);
  if (el) el.remove();
  refreshAll();
}

/* ------------------------------------------------------------------ */
/*  VALIDATE JOBS (ascending time per dept)                              */
/* ------------------------------------------------------------------ */
function validateDept(dept) {
  const jobs = dept.jobs.filter(j => j.time !== '' && j.id !== '');
  for (let i = 1; i < jobs.length; i++) {
    if (jobs[i].time < jobs[i - 1].time) {
      return `Job "${jobs[i].id}" (t=${jobs[i].time}) must be ≥ "${jobs[i-1].id}" (t=${jobs[i-1].time})`;
    }
  }
  return null;
}

/* ------------------------------------------------------------------ */
/*  REFRESH ALL (after any change)                                        */
/* ------------------------------------------------------------------ */
function refreshAll() {
  departments.forEach(dept => {
    const err = validateDept(dept);
    const errEl = document.getElementById(`error-${dept.id}`);
    const row   = document.getElementById(`dept-${dept.id}`);
    if (errEl) errEl.textContent = err || '';
    if (row)   row.classList.toggle('has-error', !!err);
    renderMiniLL(dept);
  });
  renderLinkedLists();
  updateStatCards();
}

/* ------------------------------------------------------------------ */
/*  MINI LINKED LIST (in builder)                                         */
/* ------------------------------------------------------------------ */
function renderMiniLL(dept) {
  const el = document.getElementById(`mini-ll-${dept.id}`);
  if (!el) return;
  const validJobs = dept.jobs.filter(j => j.id && j.time !== '');
  if (validJobs.length === 0) { el.innerHTML = ''; return; }

  let html = '';
  validJobs.forEach((j, i) => {
    html += `<div class="mini-node" style="border-color:${dept.color.border}60;color:${dept.color.text}">
               <span class="mini-id">${escHtml(j.id)}</span>
               <span class="mini-t">t=${j.time}</span>
             </div>`;
    html += `<span class="mini-arrow">→</span>`;
  });
  html += `<span class="mini-null">NULL</span>`;
  el.innerHTML = html;
}

/* ------------------------------------------------------------------ */
/*  LINKED LIST VISUALIZATION                                             */
/* ------------------------------------------------------------------ */
function renderLinkedLists() {
  const container = document.getElementById('ll-container');
  const validDepts = departments.filter(d => d.jobs.some(j => j.id && j.time !== ''));
  if (validDepts.length === 0) {
    container.innerHTML = '<div class="empty-state">Add departments above to see linked list visualization.</div>';
    return;
  }

  let html = '';
  validDepts.forEach(dept => {
    const validJobs = dept.jobs.filter(j => j.id && j.time !== '');
    const c = dept.color;
    html += `<div class="ll-dept-row">
      <span class="ll-dept-label" style="color:${c.text}">${escHtml(dept.code || `D${dept.id}`)}</span>
      <div class="ll-nodes-row">`;
    validJobs.forEach((j, i) => {
      html += `<div class="ll-node" style="border-color:${c.border};background:${c.bg}">
                 <span class="ll-job-id" style="color:${c.text}">${escHtml(j.id)}</span>
                 <span class="ll-time" style="color:${c.text}99">t=${j.time}</span>
                 <span class="ll-dept-badge" style="background:${c.badge};color:${c.text}">${escHtml(dept.code || `D${dept.id}`)}</span>
               </div>`;
      html += `<span class="ll-arrow">→</span>`;
    });
    html += `<span class="ll-null">NULL</span>`;
    html += `</div></div>`;
  });
  container.innerHTML = html;
}

/* ------------------------------------------------------------------ */
/*  STAT CARDS                                                            */
/* ------------------------------------------------------------------ */
function updateStatCards() {
  const validDepts = departments.filter(d => d.jobs.some(j => j.id && j.time !== ''));
  const totalJobs  = validDepts.reduce((sum, d) => sum + d.jobs.filter(j => j.id && j.time !== '').length, 0);
  document.getElementById('stat-depts').textContent = validDepts.length;
  document.getElementById('stat-jobs').textContent  = totalJobs;

  const queueStatus = validDepts.length > 0 ? 'Ready' : 'Idle';
  document.getElementById('stat-queue-status').textContent = queueStatus;
  document.getElementById('stat-queue-status').style.color = validDepts.length > 0 ? 'var(--green)' : '';
}

/* ------------------------------------------------------------------ */
/*  BUILD PAYLOAD                                                          */
/* ------------------------------------------------------------------ */
function buildPayload() {
  const depts = [];
  for (const dept of departments) {
    const validJobs = dept.jobs.filter(j => j.id && j.time !== '');
    if (validJobs.length === 0) continue;
    // Validate
    const err = validateDept(dept);
    if (err) throw new Error(`Dept ${dept.code}: ${err}`);
    depts.push({
      code: dept.code || `D${dept.id}`,
      jobs: validJobs.map(j => ({ id: j.id, time: Number(j.time) }))
    });
  }
  if (depts.length === 0) throw new Error('Add at least one department with jobs.');
  return { departments: depts };
}

/* ------------------------------------------------------------------ */
/*  START MERGE                                                           */
/* ------------------------------------------------------------------ */
async function startMerge() {
  const btn = document.getElementById('start-merge-btn');
  let payload;
  try {
    payload = buildPayload();
  } catch (e) {
    alert(e.message);
    return;
  }

  btn.disabled = true;
  btn.innerHTML = '<span class="spinner"></span> Merging…';
  document.getElementById('stat-merge-status').textContent = 'Running';
  document.getElementById('stat-merge-status').style.color = 'var(--amber)';

  try {
    const res  = await fetch(`${API}/api/merge`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
      signal: AbortSignal.timeout(30000),
    });

    if (!res.ok) {
      const txt = await res.text();
      throw new Error(`Server error ${res.status}: ${txt}`);
    }

    const data = await res.json();
    handleMergeResult(data);
    document.getElementById('stat-merge-status').textContent = 'Complete';
    document.getElementById('stat-merge-status').style.color = 'var(--green)';
  } catch (e) {
    alert(`Merge failed: ${e.message}\n\nMake sure the C++ backend is running on localhost:8080.`);
    document.getElementById('stat-merge-status').textContent = 'Error';
    document.getElementById('stat-merge-status').style.color = 'var(--red)';
  } finally {
    btn.disabled  = false;
    btn.innerHTML = '▶ START MERGE';
  }
}

/* ------------------------------------------------------------------ */
/*  HANDLE MERGE RESULT                                                   */
/* ------------------------------------------------------------------ */
function handleMergeResult(data) {
  mergeSteps    = data.steps || [];
  mergeTimeline = data.mergedTimeline || [];
  mergeStats    = data.stats || {};
  mergeVerified = data.verified || false;

  // Show result sections
  ['section-merge', 'section-timeline', 'section-report'].forEach(id => {
    document.getElementById(id).classList.remove('hidden');
  });

  // Scroll to merge section
  document.getElementById('section-merge').scrollIntoView({ behavior: 'smooth', block: 'start' });

  // Reset playback
  currentStep = -1;
  isPaused    = false;
  stopPlayback();

  // Clear step log
  document.getElementById('step-log').innerHTML = '';
  updateStepCounter(0, mergeSteps.length);
  updateProgressBar(0, mergeSteps.length);
  drawHeapState([]);

  // Render final timeline & report immediately
  renderTimeline(mergeTimeline);
  renderReport(mergeStats, mergeVerified, payload_depts_count());

  // Start playback
  startPlayback();
}

function payload_depts_count() {
  return departments.filter(d => d.jobs.some(j => j.id && j.time !== '')).length;
}

/* ------------------------------------------------------------------ */
/*  PLAYBACK ENGINE                                                        */
/* ------------------------------------------------------------------ */
function startPlayback() {
  isPlaying = true;
  isPaused  = false;
  document.getElementById('btn-pause').textContent = '⏸';
  document.getElementById('btn-pause').classList.remove('active');
  scheduleNextStep();
}

function scheduleNextStep() {
  if (!isPlaying || isPaused) return;
  playTimer = setTimeout(() => {
    if (currentStep < mergeSteps.length - 1) {
      currentStep++;
      playStep(currentStep);
      scheduleNextStep();
    } else {
      isPlaying = false;
      document.getElementById('btn-pause').textContent = '▶';
    }
  }, speedMs);
}

function stopPlayback() {
  clearTimeout(playTimer);
  isPlaying = false;
}

function togglePause() {
  if (!mergeSteps.length) return;
  if (isPaused) {
    isPaused = false;
    isPlaying = true;
    document.getElementById('btn-pause').textContent = '⏸';
    document.getElementById('btn-pause').classList.remove('active');
    scheduleNextStep();
  } else {
    isPaused = true;
    clearTimeout(playTimer);
    document.getElementById('btn-pause').textContent = '▶';
    document.getElementById('btn-pause').classList.add('active');
  }
}

function stepNext() {
  if (currentStep < mergeSteps.length - 1) {
    stopPlayback();
    isPaused = true;
    currentStep++;
    playStep(currentStep);
  }
}

function stepPrev() {
  if (currentStep > 0) {
    stopPlayback();
    isPaused = true;
    currentStep--;
    playStep(currentStep);
  }
}

function resetMerge() {
  stopPlayback();
  currentStep = -1;
  isPaused    = false;
  isPlaying   = false;
  document.getElementById('step-log').innerHTML = '';
  updateStepCounter(0, mergeSteps.length);
  updateProgressBar(0, mergeSteps.length);
  drawHeapState([]);
  document.getElementById('btn-pause').textContent = '▶';
  document.getElementById('btn-pause').classList.remove('active');
}

function updateSpeed(val) {
  // slider: 1–8, map to 2000ms–250ms
  const speeds = [2000, 1400, 1000, 700, 500, 350, 250, 150];
  const labels  = ['0.5×', '0.7×', '1×', '1.4×', '2×', '3×', '4×', '6×'];
  const idx = parseInt(val, 10) - 1;
  speedMs = speeds[idx] || 700;
  document.getElementById('speed-label').textContent = labels[idx] || '1×';
}

/* ------------------------------------------------------------------ */
/*  PLAY A SINGLE STEP                                                    */
/* ------------------------------------------------------------------ */
function playStep(idx) {
  const step = mergeSteps[idx];
  if (!step) return;

  updateStepCounter(idx + 1, mergeSteps.length);
  updateProgressBar(idx + 1, mergeSteps.length);
  appendStepLog(step, idx);
  drawHeapState(step.heapState || []);
}

function updateStepCounter(cur, total) {
  document.getElementById('step-counter').textContent = `Step ${cur} of ${total}`;
}

function updateProgressBar(cur, total) {
  const pct = total ? (cur / total) * 100 : 0;
  document.getElementById('progress-fill').style.width = pct + '%';
}

/* ------------------------------------------------------------------ */
/*  STEP LOG                                                              */
/* ------------------------------------------------------------------ */
function appendStepLog(step, idx) {
  const log = document.getElementById('step-log');

  // Deactivate previous active
  const prev = log.querySelector('.step-log-entry.active');
  if (prev) prev.classList.remove('active');

  const entry = document.createElement('div');
  entry.className = `step-log-entry type-${step.type} active`;
  entry.dataset.idx = idx;

  const typeNames = { insert:'INSERT', extract:'EXTRACT', heapify_up:'HEAPIFY↑', heapify_down:'HEAPIFY↓', append:'APPEND' };
  entry.innerHTML = `
    <span class="step-type-badge">${typeNames[step.type] || step.type}</span>
    <span>${escHtml(step.explanation || `${step.type}: ${step.jobId}`)}</span>
  `;

  log.appendChild(entry);
  entry.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
}

/* ------------------------------------------------------------------ */
/*  HEAP SVG VISUALIZATION                                                */
/* ------------------------------------------------------------------ */
function drawHeapState(heapNodes) {
  const svg      = document.getElementById('heap-svg');
  const emptyEl  = document.getElementById('heap-empty');
  if (!heapNodes || heapNodes.length === 0) {
    emptyEl.style.display = 'block';
    svg.innerHTML = '';
    return;
  }
  emptyEl.style.display = 'none';

  const svgW = svg.parentElement.clientWidth || 480;
  const nodeR = 32;
  const levelH = 90;
  const maxLevel = Math.floor(Math.log2(heapNodes.length)) + 1;
  const svgH = maxLevel * levelH + nodeR * 2 + 20;
  svg.setAttribute('height', svgH);

  // Compute positions: heap array index → (x,y)
  function pos(i) {
    const level  = Math.floor(Math.log2(i + 1));
    const levelStart = Math.pow(2, level) - 1;
    const posInLevel = i - levelStart;
    const totalInLevel = Math.pow(2, level);
    const x = svgW * (posInLevel + 0.5) / totalInLevel;
    const y = nodeR + level * levelH + 20;
    return { x, y };
  }

  let edgesHTML  = '';
  let nodesHTML  = '';

  // Find dept color lookup
  function getDeptColor(deptCode) {
    const dept = departments.find(d => (d.code || `D${d.id}`) === deptCode);
    if (dept) return dept.color;
    // fallback
    const idx = deptCode ? deptCode.charCodeAt(0) % DEPT_COLORS.length : 0;
    return DEPT_COLORS[idx];
  }

  heapNodes.forEach((node, i) => {
    const { x, y } = pos(i);

    // Draw edge to parent
    if (i > 0) {
      const parentIdx = Math.floor((i - 1) / 2);
      const p = pos(parentIdx);
      edgesHTML += `<line x1="${p.x}" y1="${p.y}" x2="${x}" y2="${y}" class="heap-edge" />`;
    }

    const c = getDeptColor(node.deptCode);
    const isRoot = i === 0;
    const circleClass = isRoot ? 'heap-node-circle root' : 'heap-node-circle';

    nodesHTML += `
      <g>
        <circle cx="${x}" cy="${y}" r="${nodeR}" class="${circleClass}"
          style="stroke:${c.border};fill:${c.bg}" />
        <text x="${x}" y="${y - 12}" class="heap-node-text id-text" style="fill:${c.text}">${escHtml(node.jobId || '')}</text>
        <text x="${x}" y="${y + 2}" class="heap-node-text time-text">t=${node.submitTime ?? ''}</text>
        <text x="${x}" y="${y + 16}" class="heap-node-text dept-text" style="fill:${c.text}">${escHtml(node.deptCode || '')}</text>
      </g>`;
  });

  svg.innerHTML = edgesHTML + nodesHTML;
}

/* ------------------------------------------------------------------ */
/*  TIMELINE RENDERING                                                     */
/* ------------------------------------------------------------------ */
function renderTimeline(timeline) {
  const container = document.getElementById('timeline-container');
  if (!timeline || timeline.length === 0) {
    container.innerHTML = '<div class="empty-state">No merged jobs.</div>';
    return;
  }

  // Find duplicate timestamps
  const timeCounts = {};
  timeline.forEach(j => { timeCounts[j.time] = (timeCounts[j.time] || 0) + 1; });

  let html = '';
  timeline.forEach((job, i) => {
    const isTie = timeCounts[job.time] > 1;
    const dept  = departments.find(d => (d.code || `D${d.id}`) === job.dept);
    const c     = dept ? dept.color : DEPT_COLORS[job.dept ? job.dept.charCodeAt(0) % DEPT_COLORS.length : 0];

    html += `<div class="timeline-node${isTie ? ' tie-node' : ''}"
                  style="border-color:${isTie ? '#d29922' : c.border};background:${c.bg}">
               ${isTie ? '<span class="tl-tie">⚡ tie</span>' : ''}
               <span class="tl-id" style="color:${c.text}">${escHtml(job.id || '')}</span>
               <span class="tl-time">t=${job.time}</span>
               <span class="tl-dept" style="background:${c.badge};color:${c.text}">${escHtml(job.dept || '')}</span>
             </div>`;
    if (i < timeline.length - 1) html += '<span class="timeline-arrow">→</span>';
  });
  container.innerHTML = html;
}

/* ------------------------------------------------------------------ */
/*  REPORT RENDERING                                                       */
/* ------------------------------------------------------------------ */
function renderReport(stats, verified, deptCount) {
  if (!stats) return;

  const reportStats = [
    { label: 'Total Depts',    value: stats.totalDepts       ?? deptCount },
    { label: 'Total Jobs',     value: stats.totalJobs        ?? '—' },
    { label: 'Output Jobs',    value: stats.totalOutput      ?? '—' },
    { label: 'Heap Insertions',value: stats.heapInsertions   ?? '—' },
    { label: 'Heap Extractions',value: stats.heapExtractions ?? '—' },
    { label: 'Comparisons',    value: stats.heapComparisons  ?? '—' },
    { label: 'Duplicate Timestamps', value: stats.duplicateTimestamps ?? '—' },
    { label: 'Empty Queues',   value: stats.emptyQueues      ?? '—' },
    { label: 'Exec Time (ms)', value: typeof stats.executionTimeMs === 'number' ? stats.executionTimeMs.toFixed(3) : '—' },
  ];

  const grid = document.getElementById('report-stats');
  grid.innerHTML = reportStats.map(s => `
    <div class="report-stat">
      <div class="report-stat-value">${s.value}</div>
      <div class="report-stat-label">${s.label}</div>
    </div>
  `).join('');

  // Verification banner
  const banner = document.getElementById('verification-banner');
  const inJobs  = stats.totalJobs ?? '?';
  const outJobs = stats.totalOutput ?? '?';
  if (verified) {
    banner.className   = 'verification-banner verified';
    banner.innerHTML   = `✅ VERIFIED — ${inJobs} jobs in = ${outJobs} jobs out`;
  } else {
    banner.className   = 'verification-banner failed';
    banner.innerHTML   = `❌ MISMATCH — ${inJobs} jobs in ≠ ${outJobs} jobs out`;
  }
  banner.style.display = 'flex';
}

/* ------------------------------------------------------------------ */
/*  TEST CASES                                                             */
/* ------------------------------------------------------------------ */
async function runTest(n) {
  const container = document.getElementById('test-results');
  const cardId    = `test-card-${n}`;
  let card = document.getElementById(cardId);
  if (!card) {
    card = document.createElement('div');
    card.id = cardId;
    container.appendChild(card);
  }

  const testNames = {
    1: 'TC1 — Basic 3-dept merge',
    2: 'TC2 — Tie-breaking across depts',
    3: 'TC3 — Empty dept queues',
    4: 'TC4 — Unequal queue sizes',
  };

  card.className = 'test-card running';
  card.innerHTML = `
    <span class="test-badge running"><span class="spinner" style="width:10px;height:10px;border-width:1.5px"></span> Running</span>
    <div class="test-info">
      <div class="test-title">${testNames[n] || `Test ${n}`}</div>
      <div class="test-detail">Calling backend…</div>
    </div>`;

  const t0 = performance.now();
  try {
    const res = await fetch(`${API}/api/test`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ testCase: n }),
      signal: AbortSignal.timeout(15000),
    });
    const data = await res.json();
    const ms   = (performance.now() - t0).toFixed(1);
    renderTestCard(card, n, data, ms, testNames[n]);
  } catch (e) {
    card.className = 'test-card fail';
    card.innerHTML = `
      <span class="test-badge fail">FAIL</span>
      <div class="test-info">
        <div class="test-title">${testNames[n] || `Test ${n}`}</div>
        <div class="test-detail" style="color:var(--red)">${escHtml(e.message)}</div>
      </div>`;
  }
}

function renderTestCard(card, n, data, ms, name) {
  const pass = data.pass;
  card.className = `test-card ${pass ? 'pass' : 'fail'}`;

  const exp    = (data.expected || []).map(j => j.id || j).join(', ');
  const act    = (data.actual || []).map(j => j.id || j).join(', ');
  const stats  = data.stats ? `Insertions: ${data.stats.heapInsertions}, Extractions: ${data.stats.heapExtractions}, Comparisons: ${data.stats.heapComparisons}` : '';

  card.innerHTML = `
    <span class="test-badge ${pass ? 'pass' : 'fail'}">${pass ? '✅ PASS' : '❌ FAIL'}</span>
    <div class="test-info">
      <div class="test-title">${escHtml(name || `Test ${n}`)}</div>
      <div class="test-detail">
        Expected: ${escHtml(exp || '—')}<br/>
        Actual: ${escHtml(act || '—')}<br/>
        ${stats ? escHtml(stats) : ''}
      </div>
    </div>
    <span class="test-timing">${ms}ms</span>`;
}

async function runStressTest() {
  const container = document.getElementById('test-results');
  const cardId    = 'test-card-stress';
  let card = document.getElementById(cardId);
  if (!card) {
    card = document.createElement('div');
    card.id = cardId;
    container.appendChild(card);
  }

  card.className = 'test-card running';
  card.innerHTML = `
    <span class="test-badge running"><span class="spinner" style="width:10px;height:10px;border-width:1.5px"></span> Running</span>
    <div class="test-info">
      <div class="test-title">🔥 Stress Test (100K jobs)</div>
      <div class="test-detail">200 depts × 500 jobs = 100,000 total — calling backend…</div>
    </div>`;

  const t0 = performance.now();
  try {
    const res = await fetch(`${API}/api/test/stress`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({}),
      signal: AbortSignal.timeout(60000),
    });
    const data = await res.json();
    const ms   = (performance.now() - t0).toFixed(1);
    const pass = data.pass;

    card.className = `test-card ${pass ? 'pass' : 'fail'}`;
    card.innerHTML = `
      <span class="test-badge ${pass ? 'pass' : 'fail'}">${pass ? '✅ PASS' : '❌ FAIL'}</span>
      <div class="test-info">
        <div class="test-title">🔥 Stress Test (100K jobs)</div>
        <div class="test-detail">200 depts × 500 jobs = 100,000 total — ${pass ? 'PASS' : 'FAIL'} in ${data.stats?.executionTimeMs?.toFixed(2) ?? ms}ms<br/>
        Output jobs: ${data.stats?.totalOutput ?? '?'} | Insertions: ${data.stats?.heapInsertions ?? '?'} | Comparisons: ${data.stats?.heapComparisons ?? '?'}</div>
      </div>
      <span class="test-timing">${ms}ms</span>`;
  } catch (e) {
    card.className = 'test-card fail';
    card.innerHTML = `
      <span class="test-badge fail">FAIL</span>
      <div class="test-info">
        <div class="test-title">🔥 Stress Test (100K jobs)</div>
        <div class="test-detail" style="color:var(--red)">${escHtml(e.message)}</div>
      </div>`;
  }
}

/* ------------------------------------------------------------------ */
/*  DEMO / FAQ                                                             */
/* ------------------------------------------------------------------ */
function toggleDemo() {
  const panel  = document.getElementById('demo-panel');
  const btn    = document.getElementById('demo-toggle');
  const hidden = panel.classList.toggle('hidden');
  btn.textContent = hidden ? '▼ Expand' : '▲ Collapse';
}

function toggleFaq(btn) {
  const item   = btn.closest('.faq-item');
  const answer = item.querySelector('.faq-answer');
  const isOpen = btn.classList.toggle('open');
  answer.classList.toggle('open', isOpen);
}

/* ------------------------------------------------------------------ */
/*  HELPER                                                                */
/* ------------------------------------------------------------------ */
function escHtml(str) {
  if (str == null) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}
