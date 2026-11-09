/*
  RitualAnvil: browser-preview mock of the JUCE native backend.

  Only active when window.__JUCE__ is undefined (page opened in a normal
  browser). Inside the plugin JUCE injects the real backend first and this
  file does nothing.

  Mirrors the parameter layout PluginProcessor registers (ids, ranges, skew)
  and simulates the native "meter" event the editor timer sends (~30 Hz):
    { in, out, punchGr, limGr, eff: { <paramId>: effectiveValue, ... } }
  in/out: peak dBFS · punchGr/limGr: dB of gain reduction (>= 0)
  eff: values after the Mix Ready macro (mirror of Source/MacroMapper.h).
*/

if (typeof window.__JUCE__ === "undefined") {
  const HPF_SKEW = Math.log(0.5) / Math.log((Math.sqrt(20 * 100) - 20) / 80);   // centre at geometric mean

  const SLIDERS = {
    mix_ready:         { start: 0,   end: 100,  skew: 1,        interval: 0.1,  value: 0,    name: "Mix Ready", label: "%" },
    input_gain:        { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Input",     label: "dB" },
    hpf:               { start: 20,  end: 100,  skew: HPF_SKEW, interval: 0.1,  value: 40,   name: "HPF",       label: "Hz" },
    transient_attack:  { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Attack",    label: "dB" },
    transient_sustain: { start: -12, end: 6,    skew: 1,        interval: 0.01, value: 0,    name: "Sustain",   label: "dB" },
    eq_kick:           { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Kick",      label: "dB" },
    eq_box:            { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Box",       label: "dB" },
    eq_snare:          { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Snare",     label: "dB" },
    eq_presence:       { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Presence",  label: "dB" },
    eq_air:            { start: -12, end: 12,   skew: 1,        interval: 0.01, value: 0,    name: "Air",       label: "dB" },
    punch:             { start: 0,   end: 100,  skew: 1,        interval: 0.1,  value: 0,    name: "Punch",     label: "%" },
    heat:              { start: 0,   end: 20,   skew: 1,        interval: 0.01, value: 0,    name: "Heat",      label: "dB" },
    width:             { start: 50,  end: 150,  skew: 1,        interval: 0.1,  value: 100,  name: "Width",     label: "%" },
    ceiling:           { start: -1,  end: -0.1, skew: 1,        interval: 0.01, value: -0.3, name: "Ceiling",   label: "dBTP" },
  };
  const TOGGLES = { bypass: { value: false, name: "Bypass" } };

  const listeners = new Map();   // eventId -> Map(token -> fn)
  let nextToken = 0;

  const dispatch = (eventId, payload) => {
    const m = listeners.get(eventId);
    if (m) m.forEach(fn => fn(payload));
  };

  const parseId = id => {
    for (const [prefix, kind] of [["__juce__slider", "slider"], ["__juce__toggle", "toggle"]])
      if (id.startsWith(prefix)) return { kind, name: id.slice(prefix.length) };
    return null;
  };

  function sendInitial(id, kind, name) {
    if (kind === "slider") {
      const s = SLIDERS[name];
      const { value, ...props } = s;
      dispatch(id, { eventType: "propertiesChanged", ...props, numSteps: Math.round((s.end - s.start) / s.interval) + 1, parameterIndex: -1 });
      dispatch(id, { eventType: "valueChanged", value });
    } else if (kind === "toggle") {
      const t = TOGGLES[name];
      dispatch(id, { eventType: "propertiesChanged", name: t.name, parameterIndex: -1 });
      dispatch(id, { eventType: "valueChanged", value: t.value });
    }
  }

  window.__JUCE__ = {
    initialisationData: {
      __juce__platform: [],
      __juce__functions: [],
      __juce__sliders: Object.keys(SLIDERS),
      __juce__toggles: Object.keys(TOGGLES),
      __juce__comboBoxes: [],
    },
    backend: {
      addEventListener(eventId, fn) {
        if (!listeners.has(eventId)) listeners.set(eventId, new Map());
        const token = nextToken++;
        listeners.get(eventId).set(token, fn);
        return [eventId, token];
      },
      removeEventListener([eventId, token]) {
        listeners.get(eventId)?.delete(token);
      },
      emitEvent(eventId, payload) {
        const p = parseId(eventId);
        if (!p) return;
        if (payload.eventType === "requestInitialUpdate") {
          queueMicrotask(() => sendInitial(eventId, p.kind, p.name));
          return;
        }
        if (payload.eventType !== "valueChanged") return;     // drag start/end: no-op
        if (p.kind === "slider") SLIDERS[p.name].value = payload.value;
        if (p.kind === "toggle") TOGGLES[p.name].value = payload.value;
      },
    },
    __mock: true,
  };

  /* ---------- MacroMapper mirror (Source/MacroMapper.h) ---------- */
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  function effective() {
    const s = Object.fromEntries(Object.entries(SLIDERS).map(([k, v]) => [k, v.value]));
    const t = clamp(s.mix_ready / 100, 0, 1);
    const punchBlend = clamp(s.punch / 200 + clamp((t - 0.4) * 0.5 / 0.6, 0, 0.5), 0, 0.5);
    return {
      mix_ready: s.mix_ready,
      input_gain: s.input_gain,
      hpf: Math.max(s.hpf, 20 + 30 * t),
      transient_attack: clamp(s.transient_attack + (t > 0.5 ? (t - 0.5) * 8 : 0), -12, 12),
      transient_sustain: s.transient_sustain,   // not driven by the macro
      eq_kick: clamp(s.eq_kick + 4 * t, -12, 12),
      eq_box: clamp(s.eq_box - 3 * t, -12, 12),
      eq_snare: clamp(s.eq_snare + 3 * t, -12, 12),
      eq_presence: clamp(s.eq_presence + 2 * t, -12, 12),
      eq_air: clamp(s.eq_air + 4 * t, -12, 12),
      punch: punchBlend * 200,
      heat: clamp(s.heat + (t > 0.6 ? (t - 0.6) * 15 : 0), 0, 20),
      width: clamp(s.width + clamp((t - 0.5) * 2, 0, 1) * 20, 50, 150),
      ceiling: s.ceiling,
    };
  }

  /* ---------- simulated meter feed (~30 Hz, like the native timer) ---------- */
  let gP = 0, gL = 0, last = performance.now();

  setInterval(() => {
    const now = performance.now();
    const dt = Math.min(0.1, (now - last) / 1000); last = now;
    const t = now / 1000;
    const e = effective();
    const bypass = TOGGLES.bypass.value;

    // fake drum program: kick on beats, snare on 2 & 4, hats noise
    const kick = Math.exp(-(t % 0.5) / 0.08);
    const snare = Math.exp(-((t + 0.5) % 1.0) / 0.11) * 0.8;
    const env = Math.max(kick, snare, 0.1);
    const inDb = -26 + 24 * env + (Math.random() - 0.5) * 2 + e.input_gain;

    const blend = e.punch / 200;
    const pTarget = Math.max(0, (inDb + 20) * (1 - 1 / 8));
    gP += (pTarget - gP) * (1 - Math.exp(-dt / (pTarget > gP ? 0.003 : 0.15)));
    const eqLift = (e.eq_kick * kick + e.eq_snare * snare + (e.eq_presence + e.eq_air) * 0.3) * 0.35;
    const pre = inDb + eqLift + e.transient_attack * env * 0.3 + blend * 6 + e.heat * 0.35;
    const lTarget = Math.max(0, pre - e.ceiling);
    gL += (lTarget - gL) * (1 - Math.exp(-dt / (lTarget > gL ? 0.0015 : 0.15)));
    const out = bypass ? inDb : Math.min(pre - gL, e.ceiling);

    dispatch("meter", {
      in: inDb,
      out,
      punchGr: bypass || blend <= 0 ? 0 : gP,
      limGr: bypass ? 0 : gL,
      lat: 157,
      eff: e,
    });
  }, 33);

  console.info("[RitualAnvil] Mock JUCE backend installed (browser preview).");
}

export {};
