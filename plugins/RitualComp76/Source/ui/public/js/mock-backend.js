/*
  RitualComp76: browser-preview mock of the JUCE native backend.

  Only active when window.__JUCE__ is undefined (i.e. the page is opened in a
  normal browser). Inside the plugin, JUCE injects the real backend first and
  this file does nothing.

  It mirrors the parameter layout that PluginProcessor registers:
    sliders:    input, output, attack, release
    comboBoxes: ratio, meter_mode
    toggles:    bypass
  and simulates the native "meter" event ({ gr, out }) so the VU can be checked.
*/

if (typeof window.__JUCE__ === "undefined") {
  const SLIDERS = {
    input:   { start: -49, end: 0, skew: 1, interval: 0.1, value: -24, name: "Input",   label: "dB" },
    output:  { start: -49, end: 0, skew: 1, interval: 0.1, value: -24, name: "Output",  label: "dB" },
    attack:  { start: 1,   end: 7, skew: 1, interval: 0.01, value: 3,  name: "Attack",  label: "" },
    release: { start: 1,   end: 7, skew: 1, interval: 0.01, value: 5,  name: "Release", label: "" },
  };
  const COMBOS = {
    ratio:      { choices: ["Out", "4:1", "8:1", "12:1", "20:1", "All"], index: 1, name: "Ratio" },
    meter_mode: { choices: ["GR", "+8", "+4", "Off"],                    index: 0, name: "Meter" },
  };
  const TOGGLES = { bypass: { value: false, name: "Power" } };

  const listeners = new Map();   // eventId -> Map(token -> fn)
  let nextToken = 0;

  const dispatch = (eventId, payload) => {
    const m = listeners.get(eventId);
    if (m) m.forEach(fn => fn(payload));
  };

  const parseId = id => {
    for (const [prefix, kind] of [["__juce__slider", "slider"], ["__juce__comboBox", "combo"], ["__juce__toggle", "toggle"]])
      if (id.startsWith(prefix)) return { kind, name: id.slice(prefix.length) };
    return null;
  };

  const comboNorm = c => (c.choices.length > 1 ? c.index / (c.choices.length - 1) : 0);

  function sendInitial(id, kind, name) {
    if (kind === "slider") {
      const s = SLIDERS[name];
      const { value, ...props } = s;
      dispatch(id, { eventType: "propertiesChanged", ...props, numSteps: Math.round((s.end - s.start) / s.interval) + 1, parameterIndex: -1 });
      dispatch(id, { eventType: "valueChanged", value });
    } else if (kind === "combo") {
      const c = COMBOS[name];
      dispatch(id, { eventType: "propertiesChanged", name: c.name, choices: c.choices, parameterIndex: -1 });
      dispatch(id, { eventType: "valueChanged", value: comboNorm(c) });
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
      __juce__comboBoxes: Object.keys(COMBOS),
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
        if (p.kind === "combo") {
          const c = COMBOS[p.name];
          c.index = Math.round(payload.value * (c.choices.length - 1));
        }
        if (p.kind === "toggle") TOGGLES[p.name].value = payload.value;
      },
    },
    __mock: true,
  };

  /* ---------- simulated meter feed (~30 Hz, like the native timer) ---------- */
  const attackUs = v => 800 * Math.pow(20 / 800, (v - 1) / 6);
  const releaseMs = v => 1100 * Math.pow(50 / 1100, (v - 1) / 6);
  let gr = 0, last = performance.now();

  setInterval(() => {
    const now = performance.now();
    const dt = Math.min(0.1, (now - last) / 1000); last = now;
    const t = now / 1000;

    const kick = Math.exp(-(t % 0.5) / 0.09);
    const snare = Math.exp(-((t + 0.25) % 1.0) / 0.12) * 0.7;
    const sig = -30 + 22 * Math.max(kick, snare) + (Math.random() - 0.5) * 3;

    const inDb = SLIDERS.input.value < -48.5 ? -200 : SLIDERS.input.value;
    const cellIn = sig + inDb + 34;
    const ratioName = COMBOS.ratio.choices[COMBOS.ratio.index];
    const bypass = TOGGLES.bypass.value;

    let target = 0;
    if (!bypass && ratioName !== "Out") {
      const ratio = ratioName === "All" ? 20 : parseFloat(ratioName);
      target = Math.max(0, (cellIn + 6) * (1 - 1 / ratio)) + (ratioName === "All" ? 8 : 0);
    }
    const atk = attackUs(SLIDERS.attack.value) / 1e6 * 40;   // slowed for visibility
    const rel = releaseMs(SLIDERS.release.value) / 1e3;
    gr += (target - gr) * (1 - Math.exp(-dt / (target > gr ? atk : rel)));

    const outDb = SLIDERS.output.value < -48.5 ? -200 : SLIDERS.output.value;
    const out = bypass ? -40 : cellIn - gr + outDb + 22;      // dBu

    dispatch("meter", { gr: bypass ? 0 : gr, out });
  }, 33);

  console.info("[RitualComp76] Mock JUCE backend installed (browser preview).");
}

export {};
