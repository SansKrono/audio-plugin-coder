/* =============================================================================
   VaporStrings UI runtime — inlined into index.html by tools/build-ui.js.
   Classic script only (webview-008). Depends on: window.Juce (inlined interop),
   window.APC (inlined apc-fit.js), window.VS_META / VS_PRESETS (generated).
   ============================================================================= */
(function () {
  "use strict";

  var Juce = window.Juce;
  var META = window.VS_META;           // id -> param def (from params.json)
  var NATIVE = !!window.VS_NATIVE;     // true inside the plugin, false in a browser

  /* ---------------------------------------------------------------- helpers */
  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }
  function $(sel, root) { return (root || document).querySelector(sel); }
  function $all(sel, root) { return Array.prototype.slice.call((root || document).querySelectorAll(sel)); }
  function skewFor(d) {
    if (d.centre == null) return 1;
    return Math.log(0.5) / Math.log((d.centre - d.min) / (d.max - d.min));
  }
  function nativeFn(name) {
    if (!NATIVE) return function () { return Promise.resolve(null); };
    return Juce.getNativeFunction(name);
  }

  /* ---------------------------------------------------------------- formatting */
  function fmt(id, v) {
    var d = META[id];
    if (!d) return String(v);
    if (d.type === "choice") return d.choices[Math.round(v)] || "";
    if (d.type === "bool") return v ? "ON" : "OFF";
    switch (d.fmt) {
      case "db":     return (v > 0 ? "+" : "") + v.toFixed(1) + " dB";
      case "dbInf":  return v <= d.min + 0.05 ? "-inf dB" : (v > 0 ? "+" : "") + v.toFixed(1) + " dB";
      case "hz":     return v >= 1000 ? (v / 1000).toFixed(v >= 10000 ? 1 : 2) + " kHz" : v >= 100 ? v.toFixed(0) + " Hz" : v >= 1 ? v.toFixed(1) + " Hz" : v.toFixed(2) + " Hz";
      case "ms":     return v >= 1000 ? (v / 1000).toFixed(2) + " s" : (v >= 100 ? v.toFixed(0) : v.toFixed(1)) + " ms";
      case "s":      return v.toFixed(v >= 10 ? 1 : 2) + " s";
      case "pct":    return Math.round(v * 100) + "%";
      case "st":     return (v > 0 ? "+" : "") + v.toFixed(Math.abs(v - Math.round(v)) < 0.005 ? 0 : 2) + " st";
      case "ct":     return (v > 0 ? "+" : "") + v.toFixed(0) + " ct";
      case "bip":    return (v > 0 ? "+" : "") + Math.round(v * 100) + "%";
      case "x":      return v.toFixed(2) + "x";
      case "oct":    return (v > 0 ? "+" : "") + Math.round(v);
      case "voices": return Math.round(v) + (Math.round(v) === 1 ? " voice" : " voices");
      default:       return v.toFixed(2);
    }
  }

  /* ---------------------------------------------------------------- param model
     Wraps the JUCE relay states so every control uses one API:
       get() -> scaled value, norm() -> 0..1, set(scaled), setNorm(n),
       begin()/end() gesture, onChange(fn).
     In a plain browser (no backend) values live locally (mock mode). */
  var params = {};

  function makeParam(id) {
    var d = META[id];
    var listeners = [];
    var p = { id: id, def: d, onChange: function (fn) { listeners.push(fn); } };
    function notify() { for (var i = 0; i < listeners.length; i++) listeners[i](p); }

    if (d.type === "bool") {
      var ts = Juce.getToggleState(id);
      if (!NATIVE) ts.value = !!d.def;
      ts.valueChangedEvent.addListener(notify);
      p.get = function () { return ts.getValue() ? 1 : 0; };
      p.norm = p.get;
      p.set = function (v) { ts.setValue(!!v); notify(); };
      p.setNorm = p.set;
      p.begin = p.end = function () {};
    } else if (d.type === "choice") {
      var cs = Juce.getComboBoxState(id);
      if (!NATIVE) { cs.properties.choices = d.choices.slice(); cs.value = d.choices.length > 1 ? d.def / (d.choices.length - 1) : 0; }
      cs.valueChangedEvent.addListener(notify);
      cs.propertiesChangedEvent.addListener(notify);
      p.count = function () { return (cs.properties.choices && cs.properties.choices.length) || d.choices.length; };
      p.get = function () { return cs.getChoiceIndex(); };
      p.norm = function () { var n = p.count(); return n > 1 ? p.get() / (n - 1) : 0; };
      p.set = function (i) { cs.setChoiceIndex(clamp(Math.round(i), 0, p.count() - 1)); notify(); };
      p.setNorm = function (n) { p.set(n * (p.count() - 1)); };
      p.choiceName = function (i) { var c = cs.properties.choices; return (c && c[i]) || d.choices[i] || ""; };
      p.begin = p.end = function () {};
    } else {
      var ss = Juce.getSliderState(id);
      if (!NATIVE) {
        ss.properties.start = d.min; ss.properties.end = d.max; ss.properties.skew = skewFor(d);
        ss.properties.interval = d.type === "int" ? 1 : 0;
        ss.scaledValue = d.def;
      }
      ss.valueChangedEvent.addListener(notify);
      ss.propertiesChangedEvent.addListener(notify);
      p.get = function () { return ss.getScaledValue(); };
      p.norm = function () { return clamp(ss.getNormalisedValue(), 0, 1); };
      p.setNorm = function (n) { ss.setNormalisedValue(clamp(n, 0, 1)); notify(); };
      p.set = function (v) {
        var pr = ss.properties, span = pr.end - pr.start;
        p.setNorm(span > 0 ? Math.pow(clamp((v - pr.start) / span, 0, 1), pr.skew) : 0);
      };
      p.begin = function () { ss.sliderDragStarted(); };
      p.end = function () { ss.sliderDragEnded(); };
    }
    return p;
  }

  Object.keys(META).forEach(function (id) { params[id] = makeParam(id); });
  window.VS_PARAMS = params; // debugging aid

  /* ---------------------------------------------------------------- knobs */
  function bindKnob(el) {
    var id = el.getAttribute("data-param");
    var p = params[id];
    if (!p) return;
    var d = p.def;
    var dial = $(".apc-knob-dial", el);
    var ptr = $(".apc-knob-pointer", el);
    var lab = $(".apc-ctl-label", el);
    var labelText = lab ? lab.textContent : "";
    var bip = !!d.bipolar;
    var hero = el.classList.contains("hero");
    var showing = false;

    function draw() {
      var n = p.norm();
      var ang = n * 270;
      var mid = bip ? 135 : 0;
      dial.style.setProperty("--s", Math.min(mid, ang) + "deg");
      dial.style.setProperty("--e", Math.max(mid, ang) + "deg");
      dial.style.setProperty("--v", n.toFixed(3));
      ptr.style.transform = "translateX(-50%) rotate(" + (ang - 135) + "deg)";
      el.classList.toggle("is-neg", bip && ang < mid);
      if (hero) el.classList.toggle("is-hot", n > 0.7);
      if (showing && lab) lab.textContent = fmt(id, p.get());
    }
    function showValue(on) {
      showing = on;
      el.classList.toggle("show-value", on);
      if (lab) lab.textContent = on ? fmt(id, p.get()) : labelText;
    }

    var drag = null;
    el.addEventListener("pointerdown", function (e) {
      if (e.button !== 0) return;
      e.preventDefault();
      try { el.setPointerCapture(e.pointerId); } catch (err) {}
      drag = { y: e.clientY, n: p.norm() };
      p.begin();
      el.classList.add("is-active");
      showValue(true);
    });
    el.addEventListener("pointermove", function (e) {
      if (!drag) return;
      var sens = e.shiftKey ? 1200 : 220;
      var plateH = $(".apc-plate").getBoundingClientRect().height / 40; // px per cell
      var dn = (drag.y - e.clientY) / (sens * plateH / 16);
      var nn = clamp(drag.n + dn, 0, 1);
      if (d.type === "int") {
        var steps = d.max - d.min;
        nn = Math.round(nn * steps) / steps;
      }
      p.setNorm(nn);
    });
    function endDrag() {
      if (!drag) return;
      drag = null;
      p.end();
      el.classList.remove("is-active");
      showValue(el.matches(":hover"));
    }
    el.addEventListener("pointerup", endDrag);
    el.addEventListener("pointercancel", endDrag);
    el.addEventListener("pointerenter", function () { showValue(true); });
    el.addEventListener("pointerleave", function () { if (!drag) showValue(false); });
    el.addEventListener("dblclick", function () { p.begin(); p.set(d.def); p.end(); });
    el.addEventListener("wheel", function (e) {
      e.preventDefault();
      var step = d.type === "int" ? 1 / (d.max - d.min) : (e.shiftKey ? 0.002 : 0.01);
      p.begin(); p.setNorm(p.norm() + (e.deltaY < 0 ? step : -step)); p.end();
    }, { passive: false });

    p.onChange(draw);
    draw();
  }

  /* ---------------------------------------------------------------- flips */
  function bindFlip(el) {
    var id = el.getAttribute("data-param");
    var p = params[id];
    if (!p) return;
    var d = p.def;
    var val = $(".apc-flip-value", el);
    var cap = $(".apc-flip-cap", el);

    function draw() {
      if (d.type === "choice") val.textContent = p.choiceName(p.get()).toUpperCase();
      else val.textContent = fmt(id, p.get());
    }
    function step(dir) {
      if (d.type === "choice") {
        var n = p.count();
        p.set((p.get() + dir + n) % n);
      } else {                       // int params shown as flips (octave)
        var span = d.max - d.min + 1;
        var v = Math.round(p.get()) - d.min;
        p.begin(); p.set(((v + dir) % span + span) % span + d.min); p.end();
      }
    }
    cap.addEventListener("click", function (e) { step(e.shiftKey ? -1 : 1); });
    cap.addEventListener("contextmenu", function (e) { e.preventDefault(); step(-1); });
    cap.addEventListener("wheel", function (e) { e.preventDefault(); step(e.deltaY < 0 ? 1 : -1); }, { passive: false });
    p.onChange(draw);
    draw();
  }

  /* ---------------------------------------------------------------- power switches */
  function bindSwitch(el) {
    var id = el.getAttribute("data-param");
    var p = params[id];
    if (!p) return;
    function draw() { el.setAttribute("aria-checked", p.get() ? "true" : "false"); }
    el.addEventListener("click", function () { p.set(p.get() ? 0 : 1); });
    p.onChange(draw);
    draw();
  }

  /* ---------------------------------------------------------------- dependent dimming */
  function linkDim(srcId, targetIds, dimWhenOn) {
    function apply() {
      var on = !!params[srcId].get();
      targetIds.forEach(function (t) {
        var el = document.getElementById("ctl-" + t);
        if (el) el.classList.toggle("is-dim", dimWhenOn ? on : !on);
      });
    }
    params[srcId].onChange(apply);
    apply();
  }

  /* ---------------------------------------------------------------- FX tabs */
  var plate = $(".apc-plate");
  var currentFx = "fx-haze";
  var TAB_POWER = { "fx-slow": "slow_on", "fx-drive": "drive_on", "fx-ens": "ens_on", "fx-wob": "wob_on",
                    "fx-haze": "haze_on", "fx-echo": "echo_on" };

  function selectFx(view) {
    currentFx = view;
    plate.setAttribute("data-fx", view);
    $all(".vs-tab").forEach(function (t) { t.classList.toggle("is-current", t.getAttribute("data-tab") === view); });
    try { localStorage.setItem("vs.fx", view); } catch (e) {}
  }
  function refreshTabLeds() {
    $all(".vs-tab").forEach(function (t) {
      var view = t.getAttribute("data-tab");
      var on;
      if (view === "fx-mod") on = Math.abs(params.lfo1_amt.get()) > 0.001 || Math.abs(params.lfo2_amt.get()) > 0.001;
      else on = !!params[TAB_POWER[view]].get();
      t.classList.toggle("is-on", on);
      t.classList.toggle("is-frozen", view === "fx-haze" && on && !!params.haze_freeze.get());
    });
  }
  $all(".vs-tab").forEach(function (t) {
    t.addEventListener("click", function () { selectFx(t.getAttribute("data-tab")); });
  });
  ["slow_on", "drive_on", "ens_on", "wob_on", "haze_on", "echo_on", "haze_freeze", "lfo1_amt", "lfo2_amt"]
    .forEach(function (id) { params[id].onChange(refreshTabLeds); });

  /* ---------------------------------------------------------------- presets */
  var presetNames = (window.VS_PRESETS || []).map(function (pr) { return pr.name; });
  var presetIndex = 0;
  var nameEl = document.getElementById("ctl-preset-name");
  var menu = document.createElement("div");
  menu.className = "vs-menu";
  plate.appendChild(menu);

  function showPreset(i, name) {
    presetIndex = i;
    nameEl.textContent = name || presetNames[i] || "—";
  }
  function applyPresetLocally(i) {      // browser mock only
    var pr = window.VS_PRESETS[i];
    Object.keys(META).forEach(function (id) {
      var v = pr.values[id] != null ? pr.values[id] : META[id].def;
      params[id].set(v);
    });
  }
  function selectPreset(i) {
    var n = presetNames.length;
    i = ((i % n) + n) % n;
    if (NATIVE) nativeFn("presetSelect")(i).then(function (r) { if (r) showPreset(r.index, r.name); });
    else applyPresetLocally(i);
    showPreset(i);
  }
  function buildMenu() {
    menu.innerHTML = "";
    presetNames.forEach(function (nm, i) {
      var it = document.createElement("div");
      it.textContent = nm;
      if (i === presetIndex) it.className = "sel";
      it.addEventListener("click", function (e) { e.stopPropagation(); selectPreset(i); menu.classList.remove("open"); });
      menu.appendChild(it);
    });
    var r = nameEl.getBoundingClientRect(), pr = plate.getBoundingClientRect();
    var cell = pr.width / 65;
    menu.style.left = ((r.left - pr.left) / cell) + "rem";
    menu.style.top = ((r.bottom - pr.top) / cell + 0.25) + "rem";
  }
  nameEl.addEventListener("click", function (e) {
    e.stopPropagation();
    if (menu.classList.contains("open")) { menu.classList.remove("open"); return; }
    buildMenu();
    menu.classList.add("open");
  });
  document.addEventListener("click", function () { menu.classList.remove("open"); });
  document.getElementById("ctl-preset-prev").addEventListener("click", function () { selectPreset(presetIndex - 1); });
  document.getElementById("ctl-preset-next").addEventListener("click", function () { selectPreset(presetIndex + 1); });

  /* ---------------------------------------------------------------- live meters */
  var meter = { inDb: -90, outDb: -90, env: 0, note: -1, cents: 0, conf: 0, gate: 0, inPk: -90, outPk: -90, inHold: 0, outHold: 0, clip: 0 };
  var ridge = new Float32Array(96);      // horizon waveform history (0..1)
  var ridgeHead = 0;

  function toDb(x) { return x > 1e-5 ? 20 * Math.log10(x) : -100; }
  function onMeters(m) {
    var now = performance.now();
    meter.inDb = toDb(m.in || 0);
    meter.outDb = toDb(m.out || 0);
    meter.env = m.env || 0;
    meter.note = m.note != null ? m.note : -1;
    meter.cents = m.cents || 0;
    meter.conf = m.conf || 0;
    meter.gate = m.gate || 0;
    if (meter.inDb > meter.inPk || now > meter.inHold) { meter.inPk = meter.inDb; meter.inHold = now + 1500; }
    if (meter.outDb > meter.outPk || now > meter.outHold) { meter.outPk = meter.outDb; meter.outHold = now + 1500; }
    if ((m.out || 0) >= 0.999) meter.clip = now + 300;
    ridge[ridgeHead] = clamp((meter.outDb + 60) / 60, 0, 1);
    ridgeHead = (ridgeHead + 1) % ridge.length;
  }
  if (NATIVE) window.__JUCE__.backend.addEventListener("meters", onMeters);
  else {
    // browser preview: a slow fake signal so the visualisers can be judged
    var t0 = performance.now();
    setInterval(function () {
      var t = (performance.now() - t0) / 1000;
      var pluck = Math.exp(-((t * 0.9) % 2.2) * 2.2);
      onMeters({ in: 0.5 * pluck, out: 0.35 * pluck + 0.05 * Math.sin(t * 3) + 0.06, env: pluck,
                 note: (t % 9) < 6 ? 45 + Math.floor((t / 2.2) % 4) * 2 : -1, cents: 8 * Math.sin(t * 2),
                 conf: (t % 9) < 6 ? 0.85 : 0.2, gate: pluck > 0.05 ? 1 : 0 });
    }, 33);
  }

  /* ---------------------------------------------------------------- canvas painters */
  var css = getComputedStyle(document.documentElement);
  function tok(n, fb) { var v = css.getPropertyValue(n).trim(); return v || fb; }
  var C = {
    pink: tok("--accent", "#ff71ce"), cyan: tok("--accent-2", "#01cdfe"), mint: tok("--accent-3", "#05ffa1"),
    violet: tok("--accent-4", "#b967ff"), sun: tok("--sun", "#fffb96"), warn: tok("--warn", "#ff4f6d"),
    ink: tok("--ink", "#f6e9ff"), dim: tok("--ink-dim", "#a58bc7"), rule: tok("--rule", "#5b2a86"),
    inset: tok("--inset", "#12052b"), mono: tok("--font-mono", "monospace"), disp: tok("--font-display", "sans-serif")
  };
  var NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

  var painters = {};
  var clock = 0;

  painters.horizon = function (ctx, w, h) {
    var hz = h * 0.62;                                    // horizon line
    var sky = ctx.createLinearGradient(0, 0, 0, hz);
    sky.addColorStop(0, "#0d0221"); sky.addColorStop(1, "#3b0f4f");
    ctx.fillStyle = sky; ctx.fillRect(0, 0, w, hz);
    // stars
    ctx.fillStyle = "rgba(246,233,255,.55)";
    for (var s = 0; s < 22; s++) { var sx = (s * 97.3) % w, sy = (s * 41.7) % (hz * 0.7); ctx.fillRect(sx, sy, 1, 1); }
    // sun with stripes (tint shifts toward violet as VAPOR rises)
    var vap = params.vapor.norm();
    var sr = h * 0.5, scx = w * 0.5, scy = hz;
    ctx.save();
    ctx.beginPath(); ctx.arc(scx, scy, sr, Math.PI, 2 * Math.PI); ctx.closePath(); ctx.clip();
    var sg = ctx.createLinearGradient(0, scy - sr, 0, scy);
    sg.addColorStop(0, C.sun); sg.addColorStop(0.5, "#ff9a5a"); sg.addColorStop(1, vap > 0.5 ? C.violet : C.pink);
    ctx.fillStyle = sg; ctx.fillRect(scx - sr, scy - sr, sr * 2, sr);
    ctx.fillStyle = "#2a0b3f";
    for (var k = 0; k < 5; k++) { var yy = scy - sr * 0.45 + k * sr * 0.1; ctx.fillRect(scx - sr, yy, sr * 2, 1 + k * 0.9); }
    ctx.restore();
    // glow under the sun
    var gl = ctx.createRadialGradient(scx, scy, sr * 0.6, scx, scy, sr * 1.6);
    gl.addColorStop(0, "rgba(255,113,206,.25)"); gl.addColorStop(1, "rgba(255,113,206,0)");
    ctx.fillStyle = gl; ctx.fillRect(0, 0, w, hz);
    // waveform ridge (output history) on the horizon
    ctx.beginPath();
    var n = ridge.length;
    for (var i = 0; i < n; i++) {
      var v = ridge[(ridgeHead + i) % n];
      var x = (i / (n - 1)) * w;
      var y = hz - v * hz * 0.55 * (0.6 + 0.4 * Math.sin(i * 0.9 + 1.3) * Math.sin(i * 0.31));
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.lineTo(w, hz); ctx.lineTo(0, hz); ctx.closePath();
    ctx.fillStyle = "rgba(13,2,33,.85)"; ctx.fill();
    ctx.shadowColor = C.pink; ctx.shadowBlur = 8; ctx.strokeStyle = C.pink; ctx.lineWidth = 1.5; ctx.stroke();
    ctx.shadowBlur = 0;
    // floor + perspective grid scrolling toward viewer (slows with VAPOR)
    ctx.fillStyle = "#0d0221"; ctx.fillRect(0, hz, w, h - hz);
    ctx.strokeStyle = "rgba(1,205,254,.55)"; ctx.lineWidth = 1;
    var vp = w * 0.5;
    for (var g = -12; g <= 12; g++) {
      ctx.beginPath(); ctx.moveTo(vp + g * w * 0.012, hz); ctx.lineTo(vp + g * w * 0.11, h); ctx.stroke();
    }
    var phase = (clock * (0.35 - 0.25 * vap)) % 1;
    for (var r = 0; r < 7; r++) {
      var tt = (r + phase) / 7;
      var yy2 = hz + (h - hz) * tt * tt;
      ctx.globalAlpha = 0.25 + 0.75 * tt;
      ctx.beginPath(); ctx.moveTo(0, yy2); ctx.lineTo(w, yy2); ctx.stroke();
    }
    ctx.globalAlpha = 1;
    ctx.strokeStyle = C.pink; ctx.beginPath(); ctx.moveTo(0, hz + 0.5); ctx.lineTo(w, hz + 0.5); ctx.stroke();
  };

  painters.tracking = function (ctx, w, h) {
    ctx.fillStyle = C.inset; ctx.fillRect(0, 0, w, h);
    var voiced = meter.note >= 0 && meter.gate > 0;
    var chord = meter.gate > 0 && meter.conf < 0.35;
    var noteTxt = "— —";
    if (meter.note >= 0) noteTxt = NOTE_NAMES[((meter.note % 12) + 12) % 12] + (Math.floor(meter.note / 12) - 1);
    // note
    ctx.textBaseline = "middle"; ctx.textAlign = "left";
    ctx.font = "italic 900 " + Math.round(h * 0.55) + "px " + C.disp;
    ctx.fillStyle = voiced ? C.ink : C.dim;
    if (voiced) { ctx.shadowColor = C.pink; ctx.shadowBlur = 10; }
    ctx.fillText(noteTxt, w * 0.05, h * 0.46);
    ctx.shadowBlur = 0;
    // cents needle
    var nx = w * 0.42, nw = w * 0.52, ny = h * 0.3;
    ctx.strokeStyle = C.rule; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(nx, ny); ctx.lineTo(nx + nw, ny); ctx.stroke();
    for (var i = 0; i <= 10; i++) {
      var tx = nx + nw * i / 10;
      ctx.beginPath(); ctx.moveTo(tx, ny - (i % 5 === 0 ? 4 : 2)); ctx.lineTo(tx, ny + (i % 5 === 0 ? 4 : 2)); ctx.stroke();
    }
    if (voiced) {
      var cx = nx + nw * (0.5 + clamp(meter.cents, -50, 50) / 100);
      ctx.strokeStyle = Math.abs(meter.cents) < 5 ? C.mint : C.cyan; ctx.lineWidth = 2;
      ctx.beginPath(); ctx.moveTo(cx, ny - 7); ctx.lineTo(cx, ny + 7); ctx.stroke();
    }
    ctx.font = Math.round(h * 0.16) + "px " + C.mono; ctx.fillStyle = C.dim; ctx.textAlign = "right";
    ctx.fillText(voiced ? (meter.cents > 0 ? "+" : "") + Math.round(meter.cents) + " ct" : "", nx + nw, ny + h * 0.18);
    // confidence bar
    var by = h * 0.76, bh = h * 0.1;
    ctx.fillStyle = "rgba(91,42,134,.4)"; ctx.fillRect(nx, by, nw, bh);
    ctx.fillStyle = meter.conf > 0.6 ? C.mint : meter.conf > 0.35 ? "#ffb35a" : C.warn;
    ctx.fillRect(nx, by, nw * clamp(meter.conf, 0, 1), bh);
    ctx.textAlign = "left"; ctx.font = Math.round(h * 0.15) + "px " + C.mono;
    if (chord) { ctx.fillStyle = C.warn; ctx.fillText("CHORD? · MONO SYNTH", w * 0.05, h * 0.86); }
    else { ctx.fillStyle = C.dim; ctx.fillText("CONF", w * 0.05, h * 0.86); }
  };

  painters["env-curve"] = function (ctx, w, h) {
    ctx.fillStyle = C.inset; ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "rgba(1,205,254,.1)"; ctx.lineWidth = 1;
    for (var gx = 0; gx < w; gx += w / 12) { ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, h); ctx.stroke(); }
    var mode = params.env_mode.get();
    var pad = 6, x0 = pad, x1 = w - pad, y0 = h - pad, y1 = pad, H = y0 - y1, W = x1 - x0;
    var a = params.env_attack.norm(), dcy = params.env_decay.norm(), su = params.env_sustain.get(), re = params.env_release.norm();
    var pts = [];
    if (mode === 0) {                 // Follow: a plucked-string envelope trace
      for (var i = 0; i <= 60; i++) { var t = i / 60; pts.push([t, t < 0.04 ? t / 0.04 : Math.exp(-(t - 0.04) * 4.5) * (1 - 0.12 * Math.sin(t * 40) * t)]); }
    } else if (mode === 1) {          // ADSR
      var tot = a * 0.25 + dcy * 0.25 + 0.2 + re * 0.3;
      var ta = a * 0.25 / tot, td = dcy * 0.25 / tot, ts = 0.2 / tot;
      pts = [[0, 0], [ta, 1], [ta + td, su], [ta + td + ts, su], [1, 0]];
    } else {                          // Swell: slow attack ramp riding the follower
      var sw = 0.15 + a * 0.6;
      for (var j = 0; j <= 60; j++) { var u = j / 60; var ramp = clamp(u / sw, 0, 1); pts.push([u, ramp * ramp * (3 - 2 * ramp) * Math.exp(-Math.max(0, u - sw) * 1.6)]); }
    }
    ctx.beginPath();
    pts.forEach(function (p, k) { var X = x0 + p[0] * W, Y = y0 - p[1] * H; if (k === 0) ctx.moveTo(X, Y); else ctx.lineTo(X, Y); });
    ctx.strokeStyle = C.pink; ctx.lineWidth = 1.6; ctx.shadowColor = C.pink; ctx.shadowBlur = 6; ctx.stroke(); ctx.shadowBlur = 0;
    ctx.lineTo(x1, y0); ctx.lineTo(x0, y0); ctx.closePath(); ctx.fillStyle = "rgba(255,113,206,.08)"; ctx.fill();
    // live envelope dot
    var lv = clamp(meter.env, 0, 1);
    ctx.fillStyle = C.cyan; ctx.shadowColor = C.cyan; ctx.shadowBlur = 8;
    ctx.beginPath(); ctx.arc(x1 - 4, y0 - lv * H, 3, 0, 7); ctx.fill(); ctx.shadowBlur = 0;
    ctx.font = Math.round(h * 0.17) + "px " + C.mono; ctx.fillStyle = C.dim; ctx.textAlign = "left"; ctx.textBaseline = "top";
    ctx.fillText(params.env_mode.choiceName(mode).toUpperCase(), x0 + 2, y1);
  };

  painters["fx-scope"] = function (ctx, w, h) {
    ctx.fillStyle = C.inset; ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = "rgba(1,205,254,.1)"; ctx.lineWidth = 1;
    for (var gx = 0; gx <= 8; gx++) { ctx.beginPath(); ctx.moveTo(gx * w / 8, 0); ctx.lineTo(gx * w / 8, h); ctx.stroke(); }
    for (var gy = 0; gy <= 6; gy++) { ctx.beginPath(); ctx.moveTo(0, gy * h / 6); ctx.lineTo(w, gy * h / 6); ctx.stroke(); }
    var mid = h / 2;
    ctx.lineWidth = 1.6; ctx.strokeStyle = C.pink; ctx.shadowColor = C.pink; ctx.shadowBlur = 6;
    var i, x, y, t;
    function line(fn, color) {
      ctx.strokeStyle = color || C.pink; ctx.shadowColor = color || C.pink;
      ctx.beginPath();
      for (var k = 0; k <= 100; k++) { var u = k / 100; var yy = fn(u); if (k === 0) ctx.moveTo(u * w, yy); else ctx.lineTo(u * w, yy); }
      ctx.stroke();
    }
    switch (currentFx) {
      case "fx-slow": {
        var st = params.slow_pitch.get(), dr = params.slow_drift.get();
        line(function (u) { return mid - Math.sin(u * 24) * h * 0.18; }, C.dim);
        var ratio = Math.pow(2, st / 12);
        line(function (u) { return mid - Math.sin(u * 24 * ratio + Math.sin(u * 3 + clock) * dr * 2) * h * 0.18; });
        ctx.shadowBlur = 0; ctx.fillStyle = C.ink; ctx.font = Math.round(h * 0.12) + "px " + C.mono; ctx.textAlign = "center";
        ctx.fillText((st > 0 ? "+" : "") + st.toFixed(1) + " st", w / 2, h * 0.9);
        break;
      }
      case "fx-drive": {
        var amt = params.drive_amt.get(), mode = params.drive_mode.get(), g = 1 + amt * 12;
        var shape = function (v) {
          if (mode === 0) return Math.tanh(v * g) / Math.tanh(g);
          if (mode === 1) { var q = v * g + 0.2 * amt; return (q / (1 + Math.abs(q))) / (g / (1 + g)); }
          if (mode === 2) return clamp(v * g * 1.5, -1, 1);
          var bits = Math.round(16 - amt * 13), lv = Math.pow(2, bits - 1); return Math.round(v * lv) / lv;
        };
        ctx.strokeStyle = C.rule; ctx.shadowBlur = 0; ctx.beginPath(); ctx.moveTo(0, h); ctx.lineTo(w, 0); ctx.stroke();
        line(function (u) { return mid - clamp(shape(u * 2 - 1), -1, 1) * h * 0.42; });
        break;
      }
      case "fx-ens": {
        var rate = params.ens_rate.get(), dep = params.ens_depth.get();
        var cols = [C.pink, C.violet, C.cyan];
        for (i = 0; i < 3; i++) {
          (function (ph, col) {
            line(function (u) { return mid - (Math.sin(u * 6.283 * 2 + clock * rate * 2 + ph) * 0.8 + Math.sin(u * 6.283 * 19 + ph) * 0.2) * dep * h * 0.38; }, col);
          })(i * 2.094, cols[i]);
        }
        break;
      }
      case "fx-wob": {
        var wow = params.wob_wow.get(), fl = params.wob_flutter.get(), age = params.wob_age.get();
        line(function (u) { return mid - (Math.sin(u * 5 + clock * 0.7) * wow * 0.6 + Math.sin(u * 70 + clock * 9) * fl * 0.15) * h * 0.4; });
        ctx.shadowBlur = 0; ctx.fillStyle = "rgba(246,233,255," + (0.12 * age) + ")";
        for (i = 0; i < 120 * age; i++) ctx.fillRect(Math.random() * w, Math.random() * h, 1, 1);
        break;
      }
      case "fx-haze": {
        var dec = params.haze_decay.norm(), size = params.haze_size.get(), frz = !!params.haze_freeze.get(), rev = params.haze_reverse.get();
        var tail = ctx.createLinearGradient(0, 0, w, 0);
        tail.addColorStop(0, "rgba(255,113,206,.5)"); tail.addColorStop(1, frz ? "rgba(1,205,254,.5)" : "rgba(1,205,254,.05)");
        ctx.fillStyle = tail; ctx.beginPath(); ctx.moveTo(0, h);
        for (i = 0; i <= 100; i++) {
          t = i / 100;
          var env = frz ? 0.7 : Math.exp(-t * (6 - dec * 5.4));
          if (rev > 0) env = env * (1 - rev) + rev * Math.min(1, t * 2.5) * Math.exp(-Math.max(0, t - 0.4) * 4);
          var jitter = 0.08 * Math.sin(t * 90 + clock * 3) * size;
          ctx.lineTo(t * w, h - (env * (0.75 + jitter)) * h * 0.85);
        }
        ctx.lineTo(w, h); ctx.closePath(); ctx.shadowBlur = 0; ctx.fill();
        break;
      }
      case "fx-echo": {
        var fb = params.echo_fb.get(), tone = params.echo_tone.get();
        ctx.shadowBlur = 6;
        for (i = 0; i < 9; i++) {
          var amp = Math.pow(fb, i) * (i === 0 ? 1 : 0.9);
          if (amp < 0.02) break;
          x = (0.06 + i * 0.11) * w;
          ctx.strokeStyle = i === 0 ? C.pink : (tone > 0.5 ? C.cyan : C.violet);
          ctx.lineWidth = 3;
          ctx.beginPath(); ctx.moveTo(x, h * 0.92); ctx.lineTo(x, h * 0.92 - amp * h * 0.8); ctx.stroke();
        }
        break;
      }
      case "fx-mod": {
        function lfo(shape, u) {
          var ph = u - Math.floor(u);
          switch (shape) {
            case 0: return Math.sin(ph * 6.283);
            case 1: return 1 - 4 * Math.abs(ph - 0.5);
            case 2: return 2 * ph - 1;
            case 3: return ph < 0.5 ? 1 : -1;
            default: return Math.sin(Math.floor(u * 4) * 12.9898) * 0.9;
          }
        }
        var s1 = params.lfo1_shape.get(), a1 = params.lfo1_amt.get(), r1 = params.lfo1_rate.norm();
        var s2 = params.lfo2_shape.get(), a2 = params.lfo2_amt.get(), r2 = params.lfo2_rate.norm();
        line(function (u) { return h * 0.27 - lfo(s1, u * (1 + r1 * 5) + clock * 0.2) * h * 0.2 * (0.15 + Math.abs(a1)); }, C.pink);
        line(function (u) { return h * 0.73 - lfo(s2, u * (1 + r2 * 5) + clock * 0.2) * h * 0.2 * (0.15 + Math.abs(a2)); }, C.cyan);
        break;
      }
    }
    ctx.shadowBlur = 0;
    ctx.font = Math.round(h * 0.09) + "px " + C.mono; ctx.fillStyle = C.dim; ctx.textAlign = "left"; ctx.textBaseline = "top";
    ctx.fillText(currentFx.replace("fx-", "").toUpperCase(), 5, 4);
  };

  painters["io-meters"] = function (ctx, w, h) {
    ctx.fillStyle = C.inset; ctx.fillRect(0, 0, w, h);
    var rows = [["IN", meter.inDb, meter.inPk], ["OUT", meter.outDb, meter.outPk]];
    var lw = w * 0.2, bx = lw, bw = w - lw - 6, bh = h * 0.26;
    ctx.font = Math.round(h * 0.2) + "px " + C.mono; ctx.textBaseline = "middle"; ctx.textAlign = "left";
    rows.forEach(function (r, i) {
      var y = h * (0.3 + i * 0.4) - bh / 2;
      ctx.fillStyle = C.dim; ctx.fillText(r[0], 4, y + bh / 2);
      ctx.fillStyle = "rgba(91,42,134,.35)"; ctx.fillRect(bx, y, bw, bh);
      var f = clamp((r[1] + 60) / 60, 0, 1), pk = clamp((r[2] + 60) / 60, 0, 1);
      var g = ctx.createLinearGradient(bx, 0, bx + bw, 0);
      g.addColorStop(0, C.cyan); g.addColorStop(0.75, C.pink); g.addColorStop(1, C.warn);
      ctx.fillStyle = g; ctx.fillRect(bx, y, bw * f, bh);
      ctx.fillStyle = C.ink; ctx.fillRect(bx + bw * pk - 1, y, 2, bh);
      if (i === 1 && performance.now() < meter.clip) { ctx.fillStyle = C.warn; ctx.fillRect(bx + bw - 4, y, 4, bh); }
    });
  };

  /* expose painters to apc-fit.js (canvas[data-fit] -> window[fit_<id>]) */
  var fitters = [];
  Object.keys(painters).forEach(function (id) {
    window["fit_" + id.replace(/-/g, "_")] = painters[id];
  });

  /* ---------------------------------------------------------------- boot */
  function boot() {
    $all(".apc-knob[data-param]").forEach(bindKnob);
    $all(".apc-flip[data-param]").forEach(bindFlip);
    $all(".apc-psw[data-param]").forEach(bindSwitch);
    linkDim("echo_sync", ["echo_time"], true);
    linkDim("echo_sync", ["echo_div"], false);

    var saved = null;
    try { saved = localStorage.getItem("vs.fx"); } catch (e) {}
    selectFx(saved && /^fx-(slow|drive|ens|wob|haze|echo|mod)$/.test(saved) ? saved : "fx-haze");
    refreshTabLeds();

    // canvases: fit at device resolution and animate at ~30 fps
    $all("canvas[data-fit]").forEach(function (cv) {
      var fn = window[cv.getAttribute("data-fit")];
      if (typeof fn === "function") fitters.push(window.APC.fitCanvas(cv, fn));
    });
    var last = 0;
    function frame(ts) {
      requestAnimationFrame(frame);
      if (document.hidden || ts - last < 32) return;
      clock += (ts - last) / 1000 * (last ? 1 : 0);
      last = ts;
      for (var i = 0; i < fitters.length; i++) fitters[i].refit();
    }
    requestAnimationFrame(frame);

    // corner grip
    $all(".apc-grip").forEach(function (g) { window.APC.attachGrip(g); });

    // current preset name from the backend
    if (NATIVE) nativeFn("presetCurrent")().then(function (r) { if (r) showPreset(r.index, r.name); });
    else showPreset(0);
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", boot);
  else boot();
})();
