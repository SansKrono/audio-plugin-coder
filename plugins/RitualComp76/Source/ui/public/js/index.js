/*
  RitualComp76: WebView UI controller.

  Binds the approved v1 design to the JUCE parameter relays:
    WebSliderRelay      input, output, attack, release
    WebComboBoxRelay    ratio      ["Out","4:1","8:1","12:1","20:1","All"]
                        meter_mode ["GR","+8","+4","Off"]
    WebToggleButtonRelay bypass
  and renders the native "meter" event: { gr: dB of gain reduction (>= 0),
  out: output level in dBu (0 dBFS = +18 dBu) }.

  Import order matters: mock-backend must evaluate before the JUCE library,
  which reads window.__JUCE__ at module load.
*/
import "./mock-backend.js";
import * as Juce from "./juce/index.js";

/* ---------- helpers ---------- */
const css = n => getComputedStyle(document.documentElement).getPropertyValue(n).trim();
const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
const attackUs = v => 800 * Math.pow(20 / 800, (v - 1) / 6);
const releaseMs = v => 1100 * Math.pow(50 / 1100, (v - 1) / 6);
const MUTE_BELOW = -48.5;           // input/output: values below this are "∞" (mute)

let C = {};
function loadColors() {
  C = {
    bg: css("--bg"), hl: css("--surface-hl"), primary: css("--primary"), secondary: css("--secondary"),
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

/* =====================================================================
 * Rotary knob bound to a JUCE SliderState
 * ===================================================================== */
class Knob {
  constructor(paramId, { defaultValue, format }) {
    this.state = Juce.getSliderState(paramId);
    this.canvas = document.getElementById(paramId + "-knob");
    this.valueEl = document.getElementById(paramId + "-value");
    this.defaultValue = defaultValue;
    this.format = format;
    const { ctx, w } = setupHiDPI(this.canvas);
    this.ctx = ctx; this.size = w;
    this.hover = false; this.dragging = false;

    this.state.valueChangedEvent.addListener(() => this.render());
    this.state.propertiesChangedEvent.addListener(() => this.render());
    this.bind();
    this.render();
  }

  get norm() {
    const n = this.state.getNormalisedValue();
    return Number.isFinite(n) ? clamp(n, 0, 1) : 0;
  }

  scaledToNorm(v) {
    const { start, end, skew } = this.state.properties;
    return Math.pow((v - start) / (end - start), skew);
  }

  setNorm(n) {
    this.state.setNormalisedValue(clamp(n, 0, 1));
    this.render();
  }

  bind() {
    const c = this.canvas;
    let lastY = 0;
    c.addEventListener("pointerdown", e => {
      c.setPointerCapture(e.pointerId);
      this.dragging = true; lastY = e.clientY;
      this.state.sliderDragStarted();
      this.render();
    });
    c.addEventListener("pointermove", e => {
      if (!this.dragging) return;
      const dy = lastY - e.clientY; lastY = e.clientY;
      this.setNorm(this.norm + dy * (e.shiftKey ? 0.001 : 0.005));
    });
    const end = () => {
      if (!this.dragging) return;
      this.dragging = false;
      this.state.sliderDragEnded();
      this.render();
    };
    c.addEventListener("pointerup", end);
    c.addEventListener("pointercancel", end);
    c.addEventListener("pointerenter", () => { this.hover = true; this.render(); });
    c.addEventListener("pointerleave", () => { this.hover = false; this.render(); });
    c.addEventListener("wheel", e => {
      e.preventDefault();
      this.state.sliderDragStarted();
      this.setNorm(this.norm - Math.sign(e.deltaY) * (e.shiftKey ? 0.005 : 0.025));
      this.state.sliderDragEnded();
    }, { passive: false });
    c.addEventListener("dblclick", () => {
      this.state.sliderDragStarted();
      this.setNorm(this.scaledToNorm(this.defaultValue));
      this.state.sliderDragEnded();
    });
  }

  render() {
    const { ctx, size: s } = this;
    const norm = this.norm;
    const cx = s / 2, cy = s / 2, r = s / 2 - 6;
    const a0 = Math.PI * 0.75, a1 = Math.PI * 2.25;
    const av = a0 + (a1 - a0) * norm;
    ctx.clearRect(0, 0, s, s);

    ctx.beginPath(); ctx.arc(cx, cy, r - 6, 0, Math.PI * 2);
    ctx.fillStyle = C.hl; ctx.fill();
    ctx.lineWidth = 1; ctx.strokeStyle = C.border; ctx.stroke();

    ctx.lineCap = "round"; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(cx, cy, r, a0, a1);
    ctx.strokeStyle = this.hover || this.dragging ? C.border : C.bg; ctx.stroke();

    if (norm > 0.001) {
      ctx.save();
      if (this.dragging) { ctx.shadowColor = C.primary; ctx.shadowBlur = 8; }
      ctx.beginPath(); ctx.arc(cx, cy, r, a0, av);
      ctx.strokeStyle = C.primary; ctx.stroke();
      ctx.restore();
    }

    const ri = (r - 6) * 0.35, ro = (r - 6) * 0.72;
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(cx + Math.cos(av) * ri, cy + Math.sin(av) * ri);
    ctx.lineTo(cx + Math.cos(av) * ro, cy + Math.sin(av) * ro);
    ctx.strokeStyle = C.text; ctx.stroke();

    this.valueEl.textContent = this.format(this.state.getScaledValue());
  }
}

const fmtAtten = v => (v < MUTE_BELOW ? "∞" : `${Math.round(v)} dB`);
const fmtTime = (v, unit, t) => `${v.toFixed(1)} · ${Math.round(t)} ${unit}`;

/* =====================================================================
 * Ratio buttons ↔ ComboBoxState("ratio")
 * Choices: 0 Out, 1 4:1, 2 8:1, 3 12:1, 4 20:1, 5 All
 * ===================================================================== */
const RATIO_KEYS = ["4", "8", "12", "20"];          // button data-ratio → choice index 1..4

function setupRatio() {
  const state = Juce.getComboBoxState("ratio");
  const root = document.getElementById("ratio-control");
  const btns = [...root.querySelectorAll(".ratio-btn:not(.all)")];
  const allBtn = root.querySelector(".ratio-btn.all");
  const caption = document.getElementById("ratio-value");
  let pressed = new Set(["4"]);

  const indexFromPressed = () =>
    pressed.size === 0 ? 0 : pressed.size >= 2 ? 5 : RATIO_KEYS.indexOf([...pressed][0]) + 1;

  const pressedFromIndex = i =>
    i === 0 ? new Set() : i === 5 ? new Set(RATIO_KEYS) : new Set([RATIO_KEYS[i - 1]]);

  function render() {
    const i = state.getChoiceIndex();
    if (indexFromPressed() !== i) pressed = pressedFromIndex(i);   // host/automation change
    const isAll = i === 5;
    btns.forEach(b => {
      b.classList.toggle("active", !isAll && pressed.has(b.dataset.ratio));
      b.classList.toggle("all-on", isAll);
    });
    allBtn.classList.toggle("all-on", isAll);
    caption.textContent = i === 0 ? "OUT · COLOR ONLY" : isAll ? "ALL BUTTONS IN" : `${RATIO_KEYS[i - 1]}:1`;
    caption.classList.toggle("all", isAll);
  }

  function commit() {
    state.setChoiceIndex(indexFromPressed());
    render();
  }

  btns.forEach(b => b.addEventListener("click", e => {
    const k = b.dataset.ratio;
    if (e.shiftKey) {
      pressed.has(k) ? pressed.delete(k) : pressed.add(k);
    } else if (pressed.size === 1 && pressed.has(k)) {
      pressed.clear();                    // click active → Out
    } else {
      pressed = new Set([k]);             // radio select
    }
    commit();
  }));
  allBtn.addEventListener("click", () => {
    pressed = state.getChoiceIndex() === 5 ? new Set(["4"]) : new Set(RATIO_KEYS);
    commit();
  });

  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();
  return state;
}

/* =====================================================================
 * Meter mode segmented ↔ ComboBoxState("meter_mode")
 * Choices: 0 GR, 1 +8, 2 +4, 3 Off
 * ===================================================================== */
function setupMeterMode() {
  const state = Juce.getComboBoxState("meter_mode");
  const root = document.getElementById("meter_mode-control");
  const btns = [...root.querySelectorAll("button")];
  const canvas = document.getElementById("vu-meter");

  const render = () => {
    const i = state.getChoiceIndex();
    btns.forEach((b, j) => b.classList.toggle("active", j === i));
    canvas.classList.toggle("off", i === 3);
  };
  btns.forEach((b, j) => b.addEventListener("click", () => { state.setChoiceIndex(j); render(); }));
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent.addListener(render);
  render();
  return state;
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
 * VU meter: renders native "meter" events with VU ballistics
 * ===================================================================== */
function setupMeter(meterModeState, bypassState) {
  const vu = setupHiDPI(document.getElementById("vu-meter"));
  const grEl = document.getElementById("gr-readout");
  const VU_MIN = -20, VU_MAX = 3;
  const amp = db => Math.pow(10, db / 20);
  const vuPos = db => clamp((amp(db) - amp(VU_MIN)) / (amp(VU_MAX) - amp(VU_MIN)), 0, 1);
  const SWEEP = 0.82;
  const pivot = { x: vu.w / 2, y: vu.h + 18 };
  const R = vu.h + 4;
  const MODES = ["GR", "+8", "+4", "OFF"];

  let latest = { gr: 0, out: -40 };
  let needle = VU_MIN, last = performance.now();

  window.__JUCE__.backend.addEventListener("meter", e => { if (e) latest = e; });

  function draw(needleDb, label) {
    const { ctx, w, h } = vu;
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = C.bg; ctx.strokeStyle = C.border; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.roundRect(0.5, 0.5, w - 1, h - 1, 4); ctx.fill(); ctx.stroke();

    ctx.save();
    ctx.beginPath(); ctx.roundRect(1, 1, w - 2, h - 2, 4); ctx.clip();

    const ang = p => -Math.PI / 2 - SWEEP + p * SWEEP * 2;
    const scaleR = R - 30;

    ctx.lineWidth = 1.2; ctx.strokeStyle = C.dim;
    ctx.beginPath(); ctx.arc(pivot.x, pivot.y, scaleR, ang(0), ang(vuPos(0))); ctx.stroke();
    ctx.strokeStyle = C.danger; ctx.lineWidth = 3;
    ctx.beginPath(); ctx.arc(pivot.x, pivot.y, scaleR, ang(vuPos(0)), ang(1)); ctx.stroke();

    ctx.font = "9px " + C.font;
    ctx.textAlign = "center"; ctx.textBaseline = "middle";
    [-20, -10, -7, -5, -3, -1, 0, 1, 2, 3].forEach(t => {
      const a = ang(vuPos(t)), hot = t > 0;
      ctx.strokeStyle = hot ? C.danger : C.dim; ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(pivot.x + Math.cos(a) * (scaleR + 2), pivot.y + Math.sin(a) * (scaleR + 2));
      ctx.lineTo(pivot.x + Math.cos(a) * (scaleR + 8), pivot.y + Math.sin(a) * (scaleR + 8));
      ctx.stroke();
      if (t !== -7 && t !== -1 && t !== 1) {
        ctx.fillStyle = hot ? C.danger : C.dim;
        ctx.fillText(t > 0 ? `+${t}` : `${t}`, pivot.x + Math.cos(a) * (scaleR + 17), pivot.y + Math.sin(a) * (scaleR + 17));
      }
    });

    ctx.fillStyle = C.dim; ctx.textAlign = "left"; ctx.fillText(label, 10, 12);
    ctx.textAlign = "right"; ctx.fillText("VU", w - 10, 12);

    const a = ang(vuPos(needleDb));
    ctx.strokeStyle = C.primary; ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(pivot.x + Math.cos(a) * 30, pivot.y + Math.sin(a) * 30);
    ctx.lineTo(pivot.x + Math.cos(a) * (R - 22), pivot.y + Math.sin(a) * (R - 22));
    ctx.stroke();
    ctx.fillStyle = C.hl;
    ctx.beginPath(); ctx.arc(pivot.x, h + 2, 14, Math.PI, 0); ctx.fill();
    ctx.restore();
  }

  function frame(now) {
    const dt = Math.min(0.05, (now - last) / 1000); last = now;
    const mode = meterModeState.getChoiceIndex();
    const bypassed = !!bypassState.getValue();
    let target;
    switch (mode) {
      case 0: target = bypassed ? VU_MIN : -latest.gr; break;
      case 1: target = latest.out - 8; break;
      case 2: target = latest.out - 4; break;
      default: target = VU_MIN;
    }
    target = clamp(target, VU_MIN - 2, VU_MAX + 1);
    needle += (target - needle) * (1 - Math.exp(-dt / 0.3));        // ~300 ms VU ballistics
    draw(needle, MODES[mode] ?? "GR");
    grEl.innerHTML = `<b>GR</b>${(-latest.gr).toFixed(1)} dB`;
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
}

/* =====================================================================
 * Boot
 * ===================================================================== */
function init() {
  loadColors();
  new Knob("input",   { defaultValue: -24, format: fmtAtten });
  new Knob("output",  { defaultValue: -24, format: fmtAtten });
  new Knob("attack",  { defaultValue: 3,   format: v => fmtTime(v, "µs", attackUs(v)) });
  new Knob("release", { defaultValue: 5,   format: v => fmtTime(v, "ms", releaseMs(v)) });
  setupRatio();
  const meterMode = setupMeterMode();
  const bypass = setupPower();
  setupMeter(meterMode, bypass);
  console.log("RitualComp76 WebView UI initialised", window.__JUCE__.__mock ? "(mock backend)" : "(JUCE backend)");
}

if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
else init();
