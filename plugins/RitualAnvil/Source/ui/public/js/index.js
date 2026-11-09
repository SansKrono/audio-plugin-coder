/*
  RitualAnvil: WebView UI controller.

  Binds the approved v1 design (Design/v1-test.html) to the JUCE relays:
    WebSliderRelay       mix_ready, input_gain, hpf, transient_attack,
                         transient_sustain, eq_kick, eq_box, eq_snare,
                         eq_presence, eq_air, punch, heat, width, ceiling
    WebToggleButtonRelay bypass
  and renders the native "meter" event (editor timer, ~30 Hz):
    { in, out:        peak dBFS
      punchGr, limGr: dB of gain reduction (>= 0)
      lat:            plugin latency (samples)
      eff:            { <paramId>: value after the Mix Ready macro } }

  Import order matters: mock-backend must evaluate before the JUCE library,
  which reads window.__JUCE__ at module load.
*/
import "./mock-backend.js";
import * as Juce from "./juce/index.js";

/* ---------- helpers ---------- */
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));

let C = {};
function loadColors() {
  C = {
    bg: css("--bg"), hl: css("--surface-hl"), primary: css("--primary"), ember: css("--ember"),
    danger: css("--danger"), text: css("--text"), dim: css("--text-dim"), border: css("--border"),
    font: css("--font"),
  };
}

function setupHiDPI(canvas) {
  const dpr = window.devicePixelRatio || 1;
  const w = canvas.width, h = canvas.height;
  canvas.style.width = w + "px";
  canvas.style.height = h + "px";
  canvas.width = w * dpr;
  canvas.height = h * dpr;
  const ctx = canvas.getContext("2d");
  ctx.scale(dpr, dpr);
  return { ctx, w, h };
}

/* Latest native meter payload; eff falls back to manual values until the first event */
let meter = { in: -100, out: -100, punchGr: 0, limGr: 0, lat: null, eff: null };
let lastLat = null;
const effOf = (id, manual) => (meter.eff && Number.isFinite(meter.eff[id]) ? meter.eff[id] : manual);

/* Shared drag/wheel/double-click binding for a SliderState */
function bindDrag(canvas, slider, getDefaultNorm, onRender, fine = 0.001, coarse = 0.005) {
  let lastY = 0, dragging = false;
  const set = n => { slider.state.setNormalisedValue(clamp(n, 0, 1)); onRender(); };
  canvas.addEventListener("pointerdown", e => {
    canvas.setPointerCapture(e.pointerId);
    dragging = true; slider.dragging = true; lastY = e.clientY;
    slider.state.sliderDragStarted();
    onRender();
  });
  canvas.addEventListener("pointermove", e => {
    if (!dragging) return;
    const dy = lastY - e.clientY; lastY = e.clientY;
    set(slider.norm + dy * (e.shiftKey ? fine : coarse));
  });
  const end = () => {
    if (!dragging) return;
    dragging = false; slider.dragging = false;
    slider.state.sliderDragEnded();
    onRender();
  };
  canvas.addEventListener("pointerup", end);
  canvas.addEventListener("pointercancel", end);
  canvas.addEventListener("pointerenter", () => { slider.hover = true; onRender(); });
  canvas.addEventListener("pointerleave", () => { slider.hover = false; onRender(); });
  canvas.addEventListener("wheel", e => {
    e.preventDefault();
    slider.state.sliderDragStarted();
    set(slider.norm - Math.sign(e.deltaY) * (e.shiftKey ? 0.005 : 0.025));
    slider.state.sliderDragEnded();
  }, { passive: false });
  canvas.addEventListener("dblclick", () => {
    slider.state.sliderDragStarted();
    set(getDefaultNorm());
    slider.state.sliderDragEnded();
  });
}

/* =====================================================================
 * Rotary knob bound to a JUCE SliderState, with Mix Ready ghost arc
 * ===================================================================== */
class Knob {
  constructor(paramId, { defaultValue, neutral, format, glow }) {
    this.id = paramId;
    this.state = Juce.getSliderState(paramId);
    this.canvas = document.getElementById(paramId + "-knob");
    this.valueEl = document.getElementById(paramId + "-value");
    this.effEl = document.getElementById(paramId + "-eff");
    Object.assign(this, { defaultValue, neutral, format, glow });
    const { ctx, w } = setupHiDPI(this.canvas);
    this.ctx = ctx; this.size = w;
    this.hover = false; this.dragging = false;

    this.state.valueChangedEvent.addListener(() => this.render());
    this.state.propertiesChangedEvent.addListener(() => this.render());
    bindDrag(this.canvas, this, () => this.scaledToNorm(this.defaultValue), () => this.render());
    this.render();
  }

  get norm() {
    const n = this.state.getNormalisedValue();
    return Number.isFinite(n) ? clamp(n, 0, 1) : 0;
  }
  get value() { return this.state.getScaledValue(); }

  scaledToNorm(v) {
    const { start, end, skew } = this.state.properties;
    if (end === start) return 0;
    return Math.pow(clamp((v - start) / (end - start), 0, 1), skew);
  }

  render() {
    const { ctx, size: s } = this;
    const norm = this.norm;
    const cx = s / 2, cy = s / 2, r = s / 2 - 6;
    const a0 = Math.PI * 0.75, a1 = Math.PI * 2.25;
    const ang = n => a0 + (a1 - a0) * n;
    const av = ang(norm);
    const originNorm = this.neutral === undefined ? 0 : this.scaledToNorm(this.neutral);
    const effValue = effOf(this.id, this.value);
    const effNorm = this.scaledToNorm(effValue);
    const hasGhost = Math.abs(effNorm - norm) > 0.002;
    ctx.clearRect(0, 0, s, s);

    // heat glow behind the body
    if (this.glow) {
      const g = clamp(this.glow(this), 0, 1);
      if (g > 0.001) {
        const grd = ctx.createRadialGradient(cx, cy, r * 0.3, cx, cy, r + 4);
        grd.addColorStop(0, `rgba(255,90,31,${0.55 * g})`);
        grd.addColorStop(1, "rgba(255,90,31,0)");
        ctx.fillStyle = grd;
        ctx.beginPath(); ctx.arc(cx, cy, r + 4, 0, Math.PI * 2); ctx.fill();
      }
    }

    // body
    ctx.beginPath(); ctx.arc(cx, cy, r - 6, 0, Math.PI * 2);
    ctx.fillStyle = C.hl; ctx.fill();
    ctx.lineWidth = 1; ctx.strokeStyle = C.border; ctx.stroke();

    // track
    ctx.lineCap = "round"; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(cx, cy, r, a0, a1);
    ctx.strokeStyle = this.hover || this.dragging ? C.border : C.bg; ctx.stroke();

    // value arc from origin (bipolar knobs start at neutral)
    if (Math.abs(norm - originNorm) > 0.002) {
      ctx.save();
      if (this.dragging) { ctx.shadowColor = C.primary; ctx.shadowBlur = 8; }
      ctx.beginPath(); ctx.arc(cx, cy, r, ang(Math.min(norm, originNorm)), ang(Math.max(norm, originNorm)));
      ctx.strokeStyle = C.primary; ctx.stroke();
      ctx.restore();
    }
    if (originNorm > 0.001) {
      const ao = ang(originNorm);
      ctx.lineWidth = 1; ctx.strokeStyle = C.dim;
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(ao) * (r + 3), cy + Math.sin(ao) * (r + 3));
      ctx.lineTo(cx + Math.cos(ao) * (r + 5.5), cy + Math.sin(ao) * (r + 5.5));
      ctx.stroke();
    }

    // macro ghost arc: manual → effective
    if (hasGhost) {
      ctx.save();
      ctx.shadowColor = C.ember; ctx.shadowBlur = 6;
      ctx.lineWidth = 3; ctx.lineCap = "butt";
      ctx.beginPath(); ctx.arc(cx, cy, r, ang(Math.min(norm, effNorm)), ang(Math.max(norm, effNorm)));
      ctx.strokeStyle = C.ember; ctx.stroke();
      const ae = ang(effNorm);
      ctx.beginPath(); ctx.arc(cx + Math.cos(ae) * r, cy + Math.sin(ae) * r, 2.5, 0, Math.PI * 2);
      ctx.fillStyle = C.ember; ctx.fill();
      ctx.restore();
    }

    // pointer (manual position)
    const ri = (r - 6) * 0.35, ro = (r - 6) * 0.72;
    ctx.lineWidth = 2; ctx.lineCap = "round";
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(av) * ri, cy + Math.sin(av) * ri);
    ctx.lineTo(cx + Math.cos(av) * ro, cy + Math.sin(av) * ro);
    ctx.strokeStyle = C.text; ctx.stroke();

    this.valueEl.textContent = this.format(this.value);
    if (this.effEl) {
      this.effEl.textContent = hasGhost ? "→ " + this.format(effValue, true) : "";
      this.effEl.classList.toggle("on", hasGhost);
    }
  }
}

/* =====================================================================
 * Hero dial: Mix Ready
 * ===================================================================== */
class HeroDial {
  constructor(paramId, defaultValue) {
    this.state = Juce.getSliderState(paramId);
    this.canvas = document.getElementById(paramId + "-knob");
    this.defaultValue = defaultValue;
    const { ctx, w } = setupHiDPI(this.canvas);
    this.ctx = ctx; this.size = w;
    this.hover = false; this.dragging = false;
    this.state.valueChangedEvent.addListener(() => this.render());
    this.state.propertiesChangedEvent.addListener(() => this.render());
    bindDrag(this.canvas, this, () => 0, () => this.render(), 0.001, 0.004);
    this.render();
  }
  get norm() {
    const n = this.state.getNormalisedValue();
    return Number.isFinite(n) ? clamp(n, 0, 1) : 0;
  }
  render() {
    const { ctx, size: s } = this;
    const cx = s / 2, cy = s / 2, r = s / 2 - 18;
    const a0 = Math.PI * 0.75, a1 = Math.PI * 2.25;
    const ang = n => a0 + (a1 - a0) * n;
    const t = this.norm;
    ctx.clearRect(0, 0, s, s);

    for (let i = 0; i <= 10; i++) {
      const n = i / 10, a = ang(n), major = i % 5 === 0;
      ctx.strokeStyle = t > 0 && n <= t + 1e-6 ? C.text : C.border;
      ctx.lineWidth = major ? 2 : 1;
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(a) * (r + 9), cy + Math.sin(a) * (r + 9));
      ctx.lineTo(cx + Math.cos(a) * (r + (major ? 15 : 13)), cy + Math.sin(a) * (r + (major ? 15 : 13)));
      ctx.stroke();
    }

    ctx.beginPath(); ctx.arc(cx, cy, r - 14, 0, Math.PI * 2);
    ctx.fillStyle = C.hl; ctx.fill();
    ctx.lineWidth = 1; ctx.strokeStyle = C.border; ctx.stroke();

    ctx.lineCap = "round"; ctx.lineWidth = 10;
    ctx.beginPath(); ctx.arc(cx, cy, r, a0, a1);
    ctx.strokeStyle = this.hover || this.dragging ? C.border : C.bg; ctx.stroke();

    if (t > 0.002) {
      // amber → ember along the sweep, drawn as short segments
      ctx.save();
      ctx.shadowColor = C.ember; ctx.shadowBlur = 4 + 20 * t;
      ctx.lineCap = "butt";
      const steps = Math.max(2, Math.ceil(t * 60));
      for (let i = 0; i < steps; i++) {
        const n0 = (i / steps) * t, n1 = Math.min(((i + 1) / steps) * t + 0.002, t);
        const mix = (n0 + n1) / 2;
        ctx.strokeStyle = `rgb(255,${Math.round(195 - 105 * mix)},${Math.round(31 * mix)})`;
        ctx.beginPath(); ctx.arc(cx, cy, r, ang(n0), ang(n1)); ctx.stroke();
      }
      ctx.restore();
      const ae = ang(t);
      ctx.beginPath(); ctx.arc(cx + Math.cos(ae) * r, cy + Math.sin(ae) * r, 6.5, 0, Math.PI * 2);
      ctx.fillStyle = C.text; ctx.fill();
    }

    const txt = String(Math.round(this.state.getScaledValue()));
    ctx.textBaseline = "alphabetic";
    ctx.font = `700 40px ${C.font}`;
    const tw = ctx.measureText(txt).width;
    ctx.textAlign = "center"; ctx.fillStyle = C.text;
    ctx.fillText(txt, cx - 6, cy + 10);
    ctx.font = `400 14px ${C.font}`; ctx.fillStyle = C.dim; ctx.textAlign = "left";
    ctx.fillText("%", cx - 6 + tw / 2 + 3, cy + 10);
    ctx.font = `600 9px ${C.font}`; ctx.textAlign = "center";
    ctx.fillText(t > 0 ? "FORGING" : "RAW", cx, cy + 30);
  }
}

/* ---------- formatters (second arg: short form for the effective readout) ---------- */
const sgn = v => (v > 0.0005 ? "+" : "") + v.toFixed(1);
const fmtDb   = (v, short) => (short ? sgn(v) : `${sgn(v)} dB`);
const fmtHz   = (v, short) => (short ? `${Math.round(v)}` : `${Math.round(v)} Hz`);
const fmtPct  = (v, short) => (short ? `${Math.round(v)}` : `${Math.round(v)} %`);
const fmtHeat = (v, short) => (short ? v.toFixed(1) : `${v.toFixed(1)} dB`);
const fmtCeil = v => v.toFixed(1);

/* =====================================================================
 * EQ curve: manual (amber) vs effective (ember)
 * ===================================================================== */
const EQ_BANDS = [
  { id: "eq_kick", f: 60, q: 1.0 },
  { id: "eq_box", f: 200, q: 1.0 },
  { id: "eq_snare", f: 3000, q: 1.0 },
  { id: "eq_presence", f: 8000, q: 0.707 },
  { id: "eq_air", f: 12000, q: 0.5 },
];
const FS = 48000;

function biquadMagDb(b, f) {
  const w = 2 * Math.PI * f / FS;
  const c1 = Math.cos(w), s1 = Math.sin(w), c2 = Math.cos(2 * w), s2 = Math.sin(2 * w);
  const nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -(b.b1 * s1 + b.b2 * s2);
  const dr = 1 + b.a1 * c1 + b.a2 * c2, di = -(b.a1 * s1 + b.a2 * s2);
  return 10 * Math.log10((nr * nr + ni * ni) / (dr * dr + di * di));
}
function peakCoeffs(f0, q, gainDb) {
  const A = Math.pow(10, gainDb / 40), w = 2 * Math.PI * f0 / FS, al = Math.sin(w) / (2 * q), a0 = 1 + al / A;
  return { b0: (1 + al * A) / a0, b1: -2 * Math.cos(w) / a0, b2: (1 - al * A) / a0, a1: -2 * Math.cos(w) / a0, a2: (1 - al / A) / a0 };
}
function hpfCoeffs(f0) {
  const w = 2 * Math.PI * f0 / FS, al = Math.sin(w) / (2 * 0.7071), c = Math.cos(w), a0 = 1 + al;
  return { b0: (1 + c) / 2 / a0, b1: -(1 + c) / a0, b2: (1 + c) / 2 / a0, a1: -2 * c / a0, a2: (1 - al) / a0 };
}

function setupEQCurve(knobs, mixState) {
  const eqc = setupHiDPI(document.getElementById("eq-curve"));
  let lastKey = "";

  const valuesFrom = get => {
    const v = { hpf: get("hpf") };
    EQ_BANDS.forEach(b => (v[b.id] = get(b.id)));
    return v;
  };
  const responseDb = (vals, f) => {
    let db = biquadMagDb(hpfCoeffs(vals.hpf), f);
    EQ_BANDS.forEach(b => { if (Math.abs(vals[b.id]) > 0.01) db += biquadMagDb(peakCoeffs(b.f, b.q, vals[b.id]), f); });
    return db;
  };

  return function draw() {
    const manual = valuesFrom(id => knobs[id].value);
    const eff = valuesFrom(id => effOf(id, knobs[id].value));
    const macroOn = mixState.getScaledValue() > 0;
    const key = JSON.stringify([manual, eff, macroOn]);
    if (key === lastKey) return;
    lastKey = key;

    const { ctx, w, h } = eqc;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = C.bg;
    ctx.beginPath(); ctx.roundRect(0, 0, w, h, 3); ctx.fill();
    const fx = f => (Math.log10(f / 20) / 3) * w;
    const RANGE = 15, dy = db => h / 2 - (db / RANGE) * (h / 2 - 4);

    ctx.lineWidth = 1; ctx.strokeStyle = "rgba(58,58,69,0.6)";
    [100, 1000, 10000].forEach(f => { ctx.beginPath(); ctx.moveTo(fx(f) + 0.5, 0); ctx.lineTo(fx(f) + 0.5, h); ctx.stroke(); });
    ctx.strokeStyle = C.border;
    ctx.beginPath(); ctx.moveTo(0, dy(0) + 0.5); ctx.lineTo(w, dy(0) + 0.5); ctx.stroke();
    ctx.fillStyle = C.dim; ctx.font = `8px ${C.font}`; ctx.textAlign = "left"; ctx.textBaseline = "top";
    [[100, "100"], [1000, "1k"], [10000, "10k"]].forEach(([f, l]) => ctx.fillText(l, fx(f) + 3, 3));

    const N = 160;
    const path = vals => {
      const pts = [];
      for (let i = 0; i <= N; i++) {
        const f = 20 * Math.pow(1000, i / N);
        pts.push([fx(f), dy(clamp(responseDb(vals, f), -RANGE, RANGE))]);
      }
      return pts;
    };
    const trace = pts => { ctx.beginPath(); pts.forEach(([x, y], i) => (i ? ctx.lineTo(x, y) : ctx.moveTo(x, y))); };
    const stroke = (pts, color) => { trace(pts); ctx.strokeStyle = color; ctx.lineWidth = 1.5; ctx.stroke(); };

    const manualPts = path(manual);
    if (macroOn) {
      const effPts = path(eff);
      trace(effPts); ctx.lineTo(w, dy(0)); ctx.lineTo(0, dy(0)); ctx.closePath();
      ctx.fillStyle = "rgba(255,90,31,0.10)"; ctx.fill();
      stroke(manualPts, "rgba(255,195,0,0.55)");
      stroke(effPts, C.ember);
    } else {
      stroke(manualPts, C.primary);
    }
  };
}

/* =====================================================================
 * Power ↔ ToggleState("bypass")   (power ON == bypass false)
 * ===================================================================== */
function setupPower() {
  const state = Juce.getToggleState("bypass");
  const el = document.getElementById("bypass-control");
  const ui = document.getElementById("plugin-ui");
  const render = () => {
    const bypassed = !!state.getValue();
    el.classList.toggle("on", !bypassed);
    ui.classList.toggle("bypassed", bypassed);
  };
  el.addEventListener("click", () => { state.setValue(!state.getValue()); render(); });
  state.valueChangedEvent.addListener(render);
  render();
  return state;
}

/* =====================================================================
 * Meters: IN/OUT peak bars, Punch + limiter GR bars, footer readout
 * ===================================================================== */
function setupMeters(ceilingKnob) {
  const io = setupHiDPI(document.getElementById("io-meters"));
  const pgr = setupHiDPI(document.getElementById("punch-gr"));
  const lgr = setupHiDPI(document.getElementById("limiter-gr"));
  const inRO = document.getElementById("in-readout");
  const outRO = document.getElementById("out-readout");
  const limRO = document.getElementById("lim-readout");
  const M_MIN = -48;
  const mPos = db => clamp((db - M_MIN) / -M_MIN, 0, 1);

  let mIn = -100, mOut = -100, hIn = -100, hOut = -100, hInT = 0, hOutT = 0, gP = 0, gL = 0;
  let last = performance.now();
  const fall = (cur, target, dt, rate) => (target > cur ? target : Math.max(target, cur - rate * dt));

  function drawIO() {
    const { ctx, w, h } = io;
    ctx.clearRect(0, 0, w, h);
    const bw = 16, top = 4, bot = h - 4, mh = bot - top;
    const xs = [4, w - 4 - bw];
    const y = db => bot - mPos(db) * mh;

    ctx.fillStyle = C.dim; ctx.font = `8px ${C.font}`; ctx.textAlign = "center"; ctx.textBaseline = "middle";
    [0, -3, -6, -12, -24, -36, -48].forEach(db => ctx.fillText(String(db), w / 2, y(db)));

    [[mIn, hIn], [mOut, hOut]].forEach(([lvl, hold], i) => {
      const x = xs[i];
      ctx.fillStyle = C.bg;
      ctx.beginPath(); ctx.roundRect(x, top, bw, mh, 2); ctx.fill();
      if (lvl > M_MIN) {
        const yl = y(lvl), y3 = y(-3);
        ctx.fillStyle = C.primary;
        ctx.fillRect(x + 2, Math.max(yl, y3), bw - 4, bot - Math.max(yl, y3) - 2);
        if (yl < y3) { ctx.fillStyle = C.danger; ctx.fillRect(x + 2, yl, bw - 4, y3 - yl); }
      }
      if (hold > M_MIN) { ctx.fillStyle = C.text; ctx.fillRect(x + 2, y(hold) - 0.5, bw - 4, 1); }
    });

    const yc = y(ceilingKnob.value);
    ctx.strokeStyle = C.danger; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(xs[1] - 3, yc + 0.5); ctx.lineTo(xs[1] + bw + 3, yc + 0.5); ctx.stroke();
  }

  function drawGR(m, grDb, maxDb, color) {
    const { ctx, w, h } = m;
    ctx.clearRect(0, 0, w, h);
    const bw = 8, x = 2, top = 2, bot = h - 2;
    ctx.fillStyle = C.bg;
    ctx.beginPath(); ctx.roundRect(x, top, bw, bot - top, 2); ctx.fill();
    const frac = clamp(grDb / maxDb, 0, 1);
    if (frac > 0.001) {
      ctx.fillStyle = color;
      ctx.fillRect(x + 1.5, top + 1, bw - 3, (bot - top - 2) * frac);
    }
    ctx.fillStyle = C.dim; ctx.font = `8px ${C.font}`; ctx.textAlign = "left"; ctx.textBaseline = "middle";
    [0, maxDb / 2, maxDb].forEach(d => ctx.fillText(d === 0 ? "0" : `-${d}`, x + bw + 3, top + 4 + (bot - top - 8) * (d / maxDb)));
  }

  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000); last = now;
    mIn = fall(mIn, meter.in, dt, 24);
    mOut = fall(mOut, meter.out, dt, 24);
    if (mIn >= hIn || now - hInT > 1200) { hIn = mIn; hInT = now; }
    if (mOut >= hOut || now - hOutT > 1200) { hOut = mOut; hOutT = now; }
    gP = fall(gP, meter.punchGr, dt, 40);
    gL = fall(gL, meter.limGr, dt, 40);

    drawIO();
    drawGR(pgr, gP, 20, C.ember);
    drawGR(lgr, gL, 12, C.danger);
    const fmtLvl = v => (v <= M_MIN ? "-inf" : v.toFixed(1));
    inRO.textContent = fmtLvl(hIn);
    outRO.textContent = fmtLvl(hOut);
    limRO.innerHTML = `<b>LIM GR</b>${(-gL).toFixed(1)} dB`;
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

/* =====================================================================
 * Boot
 * ===================================================================== */
function init() {
  loadColors();

  const knobs = {};
  const mk = (id, opts) => (knobs[id] = new Knob(id, opts));
  mk("input_gain",        { defaultValue: 0,    neutral: 0,   format: fmtDb });
  mk("hpf",               { defaultValue: 40,                 format: fmtHz });
  mk("transient_attack",  { defaultValue: 0,    neutral: 0,   format: fmtDb });
  mk("transient_sustain", { defaultValue: 0,    neutral: 0,   format: fmtDb });
  EQ_BANDS.forEach(b => mk(b.id, { defaultValue: 0, neutral: 0, format: fmtDb }));
  mk("punch",             { defaultValue: 0,                  format: fmtPct });
  mk("heat",              { defaultValue: 0,                  format: fmtHeat, glow: k => effOf("heat", k.value) / 20 });
  mk("width",             { defaultValue: 100,  neutral: 100, format: fmtPct });
  mk("ceiling",           { defaultValue: -0.3,               format: fmtCeil });

  const hero = new HeroDial("mix_ready", 0);
  const drawEQ = setupEQCurve(knobs, hero.state);
  Object.values(knobs).forEach(k => k.state.valueChangedEvent.addListener(drawEQ));
  drawEQ();

  setupPower();
  setupMeters(knobs.ceiling);

  // Native meter event: levels + effective values → repaint knobs whose ghost changed
  window.__JUCE__.backend.addEventListener("meter", e => {
    if (!e) return;
    const prev = meter.eff;
    meter = e;
    if (Number.isFinite(e.lat) && e.lat !== lastLat) {
      lastLat = e.lat;
      document.getElementById("footer-info").textContent = `RITUALANVIL v0.3.0 · LAT ${e.lat} smp`;
    }
    Object.values(knobs).forEach(k => {
      if (!prev || prev[k.id] !== e.eff?.[k.id]) k.render();
    });
    drawEQ();
  });

  console.log("RitualAnvil WebView UI initialised", window.__JUCE__.__mock ? "(mock backend)" : "(JUCE backend)");
}

if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
else init();
