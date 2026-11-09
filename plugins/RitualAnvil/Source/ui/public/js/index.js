/*
  RitualAnvil: WebView UI controller.

  Binds the approved v2 design (Design/v2-test.html, GlassKit) to the JUCE relays:
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
    primary: css("--gl-color-primary"), primaryDark: css("--gl-color-primary-dark"),
    ember: css("--anvil-ember"), well: css("--anvil-well"), danger: css("--anvil-danger"),
    text: css("--gl-color-text"), dim: css("--gl-color-text-muted"),
    border: css("--gl-border-medium"), borderStrong: css("--gl-border-strong"),
    font: css("--gl-font-family"),
  };
}

/* ---------- glass drawing (v2, GlassKit look; GlassKit has no knob) ---------- */
const A0 = Math.PI * 0.75, A1 = Math.PI * 2.25;
const ang = n => A0 + (A1 - A0) * n;

/** amber → ember along the 270° sweep (conic where supported) */
function arcGradient(ctx, cx, cy) {
  if (ctx.createConicGradient) {
    const g = ctx.createConicGradient(A0, cx, cy);
    g.addColorStop(0, C.primary);
    g.addColorStop(0.75, C.primaryDark);
    g.addColorStop(1, C.primaryDark);
    return g;
  }
  return C.primary;
}

function drawGlassDisc(ctx, cx, cy, r, hover) {
  ctx.save();
  ctx.shadowColor = "rgba(0,0,0,0.35)"; ctx.shadowBlur = 12; ctx.shadowOffsetY = 4;
  const body = ctx.createRadialGradient(cx - r * 0.35, cy - r * 0.45, r * 0.1, cx, cy, r);
  body.addColorStop(0, "rgba(255,255,255,0.22)");
  body.addColorStop(1, "rgba(255,255,255,0.06)");
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.fillStyle = body; ctx.fill();
  ctx.restore();
  ctx.lineWidth = 1.5; ctx.strokeStyle = hover ? C.borderStrong : C.border;
  ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke();
  ctx.lineWidth = 1; ctx.strokeStyle = "rgba(255,255,255,0.45)";          // specular rim, top-left
  ctx.beginPath(); ctx.arc(cx, cy, r - 1.5, Math.PI * 1.05, Math.PI * 1.6); ctx.stroke();
}

function drawGroove(ctx, cx, cy, r, w) {
  ctx.lineCap = "round"; ctx.lineWidth = w; ctx.strokeStyle = C.well;
  ctx.beginPath(); ctx.arc(cx, cy, r, A0, A1); ctx.stroke();
  ctx.lineWidth = 1; ctx.strokeStyle = "rgba(255,255,255,0.10)";
  ctx.beginPath(); ctx.arc(cx, cy, r + w / 2, A0, A1); ctx.stroke();
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
    const cx = s / 2, cy = s / 2, rTrack = s / 2 - 5, rDisc = rTrack - 7;
    const originNorm = this.neutral === undefined ? 0 : this.scaledToNorm(this.neutral);
    const effValue = effOf(this.id, this.value);
    const effNorm = this.scaledToNorm(effValue);
    const hasGhost = Math.abs(effNorm - norm) > 0.002;
    ctx.clearRect(0, 0, s, s);

    // heat glow behind the disc
    if (this.glow) {
      const g = clamp(this.glow(this), 0, 1);
      if (g > 0.001) {
        const grd = ctx.createRadialGradient(cx, cy, rDisc * 0.3, cx, cy, rTrack + 4);
        grd.addColorStop(0, `rgba(255,90,31,${0.6 * g})`);
        grd.addColorStop(1, "rgba(255,90,31,0)");
        ctx.fillStyle = grd;
        ctx.beginPath(); ctx.arc(cx, cy, rTrack + 4, 0, Math.PI * 2); ctx.fill();
      }
    }

    drawGroove(ctx, cx, cy, rTrack, 4);

    // value arc from origin (bipolar knobs start at neutral), amber → ember
    if (Math.abs(norm - originNorm) > 0.002) {
      ctx.save();
      if (this.dragging) { ctx.shadowColor = C.primary; ctx.shadowBlur = 10; }
      ctx.lineCap = "round"; ctx.lineWidth = 4; ctx.strokeStyle = arcGradient(ctx, cx, cy);
      ctx.beginPath(); ctx.arc(cx, cy, rTrack, ang(Math.min(norm, originNorm)), ang(Math.max(norm, originNorm))); ctx.stroke();
      ctx.restore();
    }
    if (originNorm > 0.001) {
      const ao = ang(originNorm);
      ctx.lineWidth = 1.5; ctx.lineCap = "round"; ctx.strokeStyle = C.dim;
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(ao) * (rTrack + 3), cy + Math.sin(ao) * (rTrack + 3));
      ctx.lineTo(cx + Math.cos(ao) * (rTrack + 5), cy + Math.sin(ao) * (rTrack + 5));
      ctx.stroke();
    }

    // macro ghost arc: manual → effective
    if (hasGhost) {
      ctx.save();
      ctx.shadowColor = C.ember; ctx.shadowBlur = 8;
      ctx.lineWidth = 4; ctx.lineCap = "butt"; ctx.strokeStyle = C.ember;
      ctx.beginPath(); ctx.arc(cx, cy, rTrack, ang(Math.min(norm, effNorm)), ang(Math.max(norm, effNorm))); ctx.stroke();
      const ae = ang(effNorm);
      ctx.beginPath(); ctx.arc(cx + Math.cos(ae) * rTrack, cy + Math.sin(ae) * rTrack, 3, 0, Math.PI * 2);
      ctx.fillStyle = C.ember; ctx.fill();
      ctx.restore();
    }

    drawGlassDisc(ctx, cx, cy, rDisc, this.hover || this.dragging);

    // pointer (manual position)
    const av = ang(norm);
    ctx.lineWidth = 2.5; ctx.lineCap = "round"; ctx.strokeStyle = C.text;
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(av) * rDisc * 0.30, cy + Math.sin(av) * rDisc * 0.30);
    ctx.lineTo(cx + Math.cos(av) * rDisc * 0.68, cy + Math.sin(av) * rDisc * 0.68);
    ctx.stroke();

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
    const cx = s / 2, cy = s / 2, rTrack = s / 2 - 22, rDisc = rTrack - 16;
    const t = this.norm;
    ctx.clearRect(0, 0, s, s);

    for (let i = 0; i <= 10; i++) {
      const n = i / 10, a = ang(n), major = i % 5 === 0;
      ctx.strokeStyle = t > 0 && n <= t + 1e-6 ? C.text : "rgba(255,255,255,0.25)";
      ctx.lineWidth = major ? 2 : 1; ctx.lineCap = "round";
      const r0 = rTrack + 11, r1 = rTrack + (major ? 18 : 15);
      ctx.beginPath();
      ctx.moveTo(cx + Math.cos(a) * r0, cy + Math.sin(a) * r0);
      ctx.lineTo(cx + Math.cos(a) * r1, cy + Math.sin(a) * r1);
      ctx.stroke();
    }

    drawGroove(ctx, cx, cy, rTrack, 12);
    if (t > 0.002) {
      ctx.save();
      ctx.shadowColor = C.ember; ctx.shadowBlur = 6 + 22 * t;
      ctx.lineCap = "round"; ctx.lineWidth = 12; ctx.strokeStyle = arcGradient(ctx, cx, cy);
      ctx.beginPath(); ctx.arc(cx, cy, rTrack, A0, ang(t)); ctx.stroke();
      ctx.restore();
    }

    drawGlassDisc(ctx, cx, cy, rDisc, this.hover || this.dragging);

    const txt = String(Math.round(this.state.getScaledValue()));
    ctx.textBaseline = "alphabetic";
    ctx.font = `700 44px ${C.font}`;
    const tw = ctx.measureText(txt).width;
    ctx.textAlign = "center"; ctx.fillStyle = C.text;
    ctx.fillText(txt, cx - 7, cy + 12);
    ctx.font = `500 15px ${C.font}`; ctx.fillStyle = C.dim; ctx.textAlign = "left";
    ctx.fillText("%", cx - 7 + tw / 2 + 3, cy + 12);
    ctx.font = `600 10px ${C.font}`; ctx.textAlign = "center";
    ctx.fillStyle = t > 0 ? C.ember : C.dim;
    ctx.fillText(t > 0 ? "FORGING" : "RAW", cx, cy + 32);
  }
}

/* ---------- formatters (second arg: short form for the effective readout) ---------- */
const sgn = v => { const r = Math.abs(v) < 0.05 ? 0 : v; return (r > 0 ? "+" : "") + r.toFixed(1); };   // no "-0.0" from host round-trips
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
    ctx.fillStyle = C.well;
    ctx.beginPath(); ctx.roundRect(0, 0, w, h, 12); ctx.fill();
    const fx = f => (Math.log10(f / 20) / 3) * w;
    const RANGE = 15, dy = db => h / 2 - (db / RANGE) * (h / 2 - 6);

    ctx.lineWidth = 1; ctx.strokeStyle = "rgba(255,255,255,0.06)";
    [100, 1000, 10000].forEach(f => { ctx.beginPath(); ctx.moveTo(fx(f) + 0.5, 0); ctx.lineTo(fx(f) + 0.5, h); ctx.stroke(); });
    ctx.strokeStyle = "rgba(255,255,255,0.14)";
    ctx.beginPath(); ctx.moveTo(8, dy(0) + 0.5); ctx.lineTo(w - 8, dy(0) + 0.5); ctx.stroke();
    ctx.fillStyle = C.dim; ctx.font = `9px ${C.font}`; ctx.textAlign = "left"; ctx.textBaseline = "top";
    [[100, "100"], [1000, "1k"], [10000, "10k"]].forEach(([f, l]) => ctx.fillText(l, fx(f) + 4, 5));

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
    const stroke = (pts, color, width = 2) => { trace(pts); ctx.strokeStyle = color; ctx.lineWidth = width; ctx.stroke(); };

    const manualPts = path(manual);
    if (macroOn) {
      const effPts = path(eff);
      trace(effPts); ctx.lineTo(w, dy(0)); ctx.lineTo(0, dy(0)); ctx.closePath();
      ctx.fillStyle = "rgba(255,90,31,0.12)"; ctx.fill();
      stroke(manualPts, "rgba(255,195,0,0.55)", 1.5);
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

    ctx.fillStyle = C.dim; ctx.font = `9px ${C.font}`; ctx.textAlign = "center"; ctx.textBaseline = "middle";
    [0, -3, -6, -12, -24, -36, -48].forEach(db => ctx.fillText(String(db), w / 2, y(db)));

    const fill = ctx.createLinearGradient(0, bot, 0, top);
    fill.addColorStop(0, C.primary); fill.addColorStop(1, C.primaryDark);
    [[mIn, hIn], [mOut, hOut]].forEach(([lvl, hold], i) => {
      const x = xs[i];
      ctx.fillStyle = C.well;
      ctx.beginPath(); ctx.roundRect(x, top, bw, mh, 8); ctx.fill();
      if (lvl > M_MIN) {
        const yl = y(lvl), y3 = y(-3), yb = Math.max(yl, y3);
        ctx.fillStyle = fill;
        ctx.beginPath(); ctx.roundRect(x + 3, yb, bw - 6, Math.max(0, bot - 3 - yb), 5); ctx.fill();
        if (yl < y3) { ctx.fillStyle = C.danger; ctx.fillRect(x + 3, yl, bw - 6, y3 - yl); }
      }
      if (hold > M_MIN) { ctx.fillStyle = C.text; ctx.fillRect(x + 3, y(hold) - 0.5, bw - 6, 1); }
    });

    const yc = y(ceilingKnob.value);
    ctx.strokeStyle = C.danger; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(xs[1] - 3, yc + 0.5); ctx.lineTo(xs[1] + bw + 3, yc + 0.5); ctx.stroke();
  }

  function drawGR(m, grDb, maxDb, color) {
    const { ctx, w, h } = m;
    ctx.clearRect(0, 0, w, h);
    const bw = 8, x = 2, top = 2, bot = h - 2;
    ctx.fillStyle = C.well;
    ctx.beginPath(); ctx.roundRect(x, top, bw, bot - top, 4); ctx.fill();
    const frac = clamp(grDb / maxDb, 0, 1);
    if (frac > 0.001) {
      ctx.fillStyle = color;
      ctx.beginPath(); ctx.roundRect(x + 2, top + 2, bw - 4, (bot - top - 4) * frac, 2); ctx.fill();
    }
    ctx.fillStyle = C.dim; ctx.font = `8px ${C.font}`; ctx.textAlign = "left"; ctx.textBaseline = "middle";
    [0, maxDb / 2, maxDb].forEach(d => ctx.fillText(d === 0 ? "0" : `-${d}`, x + bw + 4, top + 4 + (bot - top - 8) * (d / maxDb)));
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
    limRO.textContent = `LIM GR ${(-gL).toFixed(1)} dB`;
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

/* =====================================================================
 * Living forge: the aurora's ember glow follows effective Heat + Mix Ready.
 * Only on a >= 2 % move: changing the page background re-renders every
 * backdrop-filter above it.
 * ===================================================================== */
let lastForge = -1;
function updateForge() {
  const g = clamp(0.5 * effOf("heat", 0) / 20 + 0.5 * effOf("mix_ready", 0) / 100, 0, 1);
  if (Math.abs(g - lastForge) < 0.02) return;
  lastForge = g;
  document.body.style.setProperty("--gl-bg-warm-glow", `rgba(255, 90, 31, ${(0.10 + 0.22 * g).toFixed(3)})`);
  document.body.style.setProperty("--gl-bg-aurora-1", `rgba(255, 90, 31, ${(0.20 + 0.14 * g).toFixed(3)})`);
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
      document.getElementById("footer-info").textContent = `LAT ${e.lat} smp`;
    }
    Object.values(knobs).forEach(k => {
      if (!prev || prev[k.id] !== e.eff?.[k.id]) k.render();
    });
    drawEQ();
    updateForge();
  });

  console.log("RitualAnvil WebView UI initialised", window.__JUCE__.__mock ? "(mock backend)" : "(JUCE backend)");
}

if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
else init();
