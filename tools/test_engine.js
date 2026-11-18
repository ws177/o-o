// test_engine.js — 无头跑 o-o.html 里的表情引擎，检查数值健全性
// 用法: node test_engine.js
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const HTML = path.join(__dirname, '..', 'preview', 'o-o.html');
const src = fs.readFileSync(HTML, 'utf8');
const m = src.match(/<script>([\s\S]*?)<\/script>/);
if (!m) { console.error('找不到 script'); process.exit(1); }
const code = m[1];

// ── 假的 canvas / DOM ──
// 关键：任何绘制调用如果传进来 NaN，都记一笔——绘制层的错也能被抓到
const drawNaNs = {};
let curCall = '';
const ctxStub = new Proxy({}, {
  get(t, k) {
    if (k === 'canvas') return { width: 1600, height: 900 };
    return (...a) => {
      for (const v of a) {
        if (typeof v === 'number' && Number.isNaN(v)) {
          drawNaNs[curCall + '.' + String(k)] = (drawNaNs[curCall + '.' + String(k)] || 0) + 1;
        }
      }
      return { addColorStop() {} };
    };
  },
  set() { return true; },
});

function elStub(tag) {
  const base = {
    tagName: tag, style: {}, dataset: {}, children: [], innerHTML: '', textContent: '', className: '',
    classList: { add() {}, remove() {}, toggle() {}, contains() { return false; } },
    appendChild() {}, addEventListener() {}, removeEventListener() {},
    querySelectorAll() { return []; }, setAttribute() {},
    getContext() { return ctxStub; }, width: 1600, height: 900,
    get parentNode() { return elStub('div'); },
  };
  return base;
}

const listeners = {};
const sandbox = {
  console,
  Math, Date, JSON, parseInt, parseFloat, isNaN, setTimeout, clearTimeout, fetch: () => Promise.resolve(),
  performance: { now: () => 0 },
  requestAnimationFrame: () => 0,
  location: { search: '' },
  EventSource: function () { this.onmessage = null; this.onerror = null; },
  window: {
    innerWidth: 1600, innerHeight: 900, devicePixelRatio: 1,
    addEventListener(t, f) { (listeners[t] = listeners[t] || []).push(f); },
    __oo: null,
  },
  document: {
    getElementById(id) { return elStub(id); },
    createElement(tag) { return elStub(tag); },
    addEventListener(t, f) { (listeners[t] = listeners[t] || []).push(f); },
    querySelectorAll() { return []; },
  },
};
sandbox.window.window = sandbox.window;
sandbox.globalThis = sandbox;

vm.createContext(sandbox);
try {
  vm.runInContext(code, sandbox, { filename: 'o-o.html' });
} catch (e) {
  console.error('脚本执行失败:', e.message);
  process.exit(1);
}

const oo = sandbox.window.__oo;
if (!oo) { console.error('没有拿到 window.__oo 钩子'); process.exit(1); }

const KEYS = ['mOmega', 'mBlob', 'mSmile', 'eyeX', 'eyeY', 'tilt', 'shakeX', 'dim', 'blinkLid', 'anger', 'happy', 'surprise'];

const stat = {};
const nan = {};
for (const k of KEYS) {
  stat[k] = { min: Infinity, max: -Infinity };
  nan[k] = 0;
}
const eyeStat = { t1: [Infinity, -Infinity], t2: [Infinity, -Infinity], ang: [Infinity, -Infinity], sc: [Infinity, -Infinity] };
const idleSeen = new Set();

function sample() {
  const S = oo.S;
  for (const k of KEYS) {
    const v = S[k];
    if (typeof v !== 'number' || Number.isNaN(v)) { nan[k]++; continue; }
    if (v < stat[k].min) stat[k].min = v;
    if (v > stat[k].max) stat[k].max = v;
  }
  for (const o of [S.eL, S.eR]) {
    if (!o) continue;
    for (const p of ['t1', 't2', 'ang', 'sc']) {
      const v = o[p];
      if (typeof v !== 'number' || Number.isNaN(v)) { nan['e.' + p] = (nan['e.' + p] || 0) + 1; continue; }
      const es = eyeStat[p];
      if (v < es[0]) es[0] = v;
      if (v > es[1]) es[1] = v;
    }
  }
  if (S.idle && S.idle.kind) idleSeen.add(S.idle.kind);
}

const DT = 1 / 60;
let frame = 0;
function advance(n) {
  for (let i = 0; i < n; i++) {
    oo.update(DT);
    sample();
    curCall = 'update';
    try { oo.drawFrame(); } catch (e) { console.log('drawFrame 抛错:', e.message); process.exit(1); }
    frame++;
  }
}

// 空闲 20 秒，看它自己会不会动
advance(1200);

// 依次戳一轮
const seq = [['eye', 300, 450], ['head', 800, 100], ['cheek', 200, 600], ['chin', 800, 620]];
for (const [kind, x, y] of seq) {
  oo.poke(kind, x, y);
  advance(90);
}

// 再空闲 10 秒
advance(600);

// 持续按住：眼睛 2s → 怒气该上去；额头 2s → 开心该上去
let holdBad = 0;
oo.S.anger = 0; oo.S.happy = 0;
oo.S.hold.active = true; oo.S.hold.region = 'eye';
advance(120);
const angerHold = oo.S.anger;
if (angerHold < 0.4) { console.log('警告: 按住眼睛 2s 怒气只有 ' + angerHold.toFixed(2)); holdBad++; }

oo.S.hold.region = 'head';
oo.S.happy = 0;
advance(120);
const happyHold = oo.S.happy;
if (happyHold < 0.4) { console.log('警告: 按住额头 2s 开心只有 ' + happyHold.toFixed(2)); holdBad++; }
oo.S.hold.active = false;
console.log('\n持续触摸: 按住眼睛 2s 怒气=' + angerHold.toFixed(2) +
            '  按住额头 2s 开心=' + happyHold.toFixed(2));

function fmt(v) { return Number.isFinite(v) ? v.toFixed(3) : String(v); }

console.log('总帧数:', frame);
console.log('\n== 数值范围 ==');
console.log('名称      最小      最大     NaN');
for (const k of KEYS) {
  console.log('%s %s %s %s',
    k.padEnd(8), fmt(stat[k].min).padStart(9), fmt(stat[k].max).padStart(9), String(nan[k] || 0).padStart(5));
}
console.log('\n== 眼睛形状 ==');
for (const p of ['t1', 't2', 'ang', 'sc']) {
  console.log('%s  %s .. %s   NaN=%d', p.padEnd(4), fmt(eyeStat[p][0]), fmt(eyeStat[p][1]), nan['e.' + p] || 0);
}
console.log('\n== 空闲动作出现过 ==');
console.log([...idleSeen].join(', ') || '(一个都没出现)');

let bad = 0;
for (const k in nan) if (nan[k] > 0) { console.log('NaN 出现在', k); bad++; }
for (const k in drawNaNs) { console.log('绘制收到 NaN:', k, 'x' + drawNaNs[k]); bad++; }
bad += holdBad;
if (stat.mOmega && !(stat.mOmega.max > 0.05)) { console.log('警告: 嘴的 omega 权重从没起来'); bad++; }
if (!(stat.tilt.max > 0.002 || stat.tilt.min < -0.002)) { console.log('警告: 倾斜没有任何变化'); bad++; }
if (idleSeen.size < 3) { console.log('警告: 空闲动作太少 (' + idleSeen.size + ' 种)'); bad++; }
console.log(bad === 0 ? '\n结果: OK' : '\n结果: 有 ' + bad + ' 个问题');
process.exit(bad === 0 ? 0 : 1);
