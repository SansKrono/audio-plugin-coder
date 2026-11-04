#!/usr/bin/env node
/* =============================================================================
   VaporStrings UI builder.

   Inputs (sources of truth):
     Design/v1-ui-map.json        approved, hash-locked layout contract
     Source/ui/params.json        parameter table + factory presets
     Source/ui/src/theme.css      component styling ("Neon Night")
     Source/ui/src/app.js         UI runtime
     templates/webview/apc-ui/*   APC kit (core css, components, apc-fit.js)
     _tools/JUCE/.../webview-interop/dist/index.js   JUCE frontend library

   Outputs:
     Source/ui/public/index.html      production page, ALL CSS/JS inline (webview-008)
     Source/ui/public/test-local.html identical copy for browser testing
     Design/index.html                WebView production copy (naming conventions)
     Source/ParameterIDs.hpp          C++ parameter IDs + definition table

   Usage (from repo root):  node plugins/vaporstrings/tools/build-ui.js
   ============================================================================= */
'use strict';
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

const pluginDir = path.resolve(__dirname, '..');
const repoRoot = path.resolve(pluginDir, '..', '..');
const rd = p => fs.readFileSync(p, 'utf8');

const mapFile = path.join(pluginDir, 'Design', 'v1-ui-map.json');
const map = JSON.parse(rd(mapFile));
const table = JSON.parse(rd(path.join(pluginDir, 'Source', 'ui', 'params.json')));
const params = table.params;

/* ---- guard: design lock -------------------------------------------------- */
const status = JSON.parse(rd(path.join(pluginDir, 'status.json')).replace(/^﻿/, ''));
const lock = status.design_lock && status.design_lock.sha256;
if (lock) {
  const h = crypto.createHash('sha256').update(fs.readFileSync(mapFile)).digest('hex');
  if (h !== lock) { console.error(`ui-map hash ${h} != design_lock ${lock} — map changed after approval`); process.exit(1); }
}

/* ---- guard: params <-> map coverage ------------------------------------- */
const ids = new Set(params.map(p => p.id));
const mapParams = map.controls.filter(c => c.param).map(c => c.param);
const missingCtl = [...ids].filter(id => !mapParams.includes(id));
const unknown = mapParams.filter(id => !ids.has(id));
if (missingCtl.length || unknown.length) {
  console.error('param/map mismatch', { missingCtl, unknown });
  process.exit(1);
}
if (params.length !== 82) { console.error(`expected 82 params, got ${params.length}`); process.exit(1); }

/* ---- HTML ----------------------------------------------------------------- */
const esc = s => String(s == null ? '' : s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;');
const cell = c => `left:${c.x}rem;top:${c.y}rem;width:${c.w}rem;height:${c.h}rem;`;

function sectionTag(title) {
  if (!title) return '';
  const m = String(title).match(/^([A-Z0-9 /&-]+?)\s+(\S+)$/);
  return m ? `${esc(m[1])}<b>${esc(m[2])}</b>` : esc(title);
}

const parts = [];
for (const s of map.sections || [])
  parts.push(`<div class="apc-cell apc-section" id="sec-${esc(s.id)}" style="${cell(s.cell)}"><span class="apc-sec-tag">${sectionTag(s.title)}</span></div>`);
for (const c of map.canvases || [])
  parts.push(`<div class="apc-cell apc-well" id="cv-${esc(c.id)}" style="${cell(c.cell)}"><canvas data-fit="fit_${c.id.replace(/-/g, '_')}"></canvas></div>`);

for (const ctl of map.controls) {
  const st = cell(ctl.cell);
  const idA = ` id="ctl-${esc(ctl.id)}"`;
  const pA = ctl.param ? ` data-param="${esc(ctl.param)}"` : '';
  const pageA = (ctl.view && ctl.component !== 'chip') ? ` data-page="${esc(ctl.view)}"` : '';
  const lab = ctl.label ? `<div class="apc-ctl-label">${esc(ctl.label)}</div>` : '';
  switch (ctl.component) {
    case 'knob':
      parts.push(`<div class="apc-cell apc-knob${ctl.kind === 'hero' ? ' hero' : ''}"${idA}${pA}${pageA} style="${st}"><div class="apc-knob-dial"><div class="apc-knob-pointer"></div></div>${lab}</div>`);
      break;
    case 'flip':
      parts.push(`<div class="apc-cell apc-flip${ctl.cell.h <= 2 ? ' inline' : ''}"${idA}${pA}${pageA} style="${st}"><button class="apc-flip-cap"><span class="apc-flip-value"></span></button>${lab}</div>`);
      break;
    case 'psw':
      parts.push(`<button class="apc-cell apc-psw"${idA}${pA}${pageA} aria-checked="false" style="${st}"><span class="led"></span><span>${esc(ctl.label)}</span></button>`);
      break;
    case 'chip':
      if (ctl.view) parts.push(`<button class="apc-cell apc-chip vs-tab"${idA} data-tab="${esc(ctl.view)}" style="${st}"><span class="led"></span>${esc(ctl.label)}</button>`);
      else parts.push(`<button class="apc-cell apc-chip"${idA} style="${st}">${esc(ctl.label)}</button>`);
      break;
    case 'screen':
      parts.push(`<div class="apc-cell apc-screen"${idA} style="${st}">—</div>`);
      break;
    case 'label':
      parts.push(`<div class="apc-cell"${idA} style="${st}">${esc(ctl.label)}</div>`);
      break;
    default:
      throw new Error(`unsupported component ${ctl.component} (${ctl.id})`);
  }
}
parts.push('<div class="apc-grip" id="ctl-grip" title="Drag to resize"></div>');

const kitDir = path.join(repoRoot, 'templates', 'webview', 'apc-ui');
const L = map.layout;
const rootVars = [`--cols: ${L.plate.cols};`, `--rows: ${L.plate.rows};`, `--apc-letterbox: var(${L.letterbox_fill || '--surface'});`]
  .concat(Object.entries(map.tokens || {}).map(([k, v]) => `${k}: ${v};`));

const css = [
  rd(path.join(kitDir, 'apc-core.css')),
  rd(path.join(kitDir, 'apc-components.css')),
  `:root {\n  ${rootVars.join('\n  ')}\n}`,
  rd(path.join(pluginDir, 'Source', 'ui', 'src', 'theme.css')),
].join('\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/\n{3,}/g, '\n\n');

/* JUCE interop: strip ESM export, expose as window.Juce (webview-008) */
const interopPath = path.join(repoRoot, '_tools', 'JUCE', 'modules', 'juce_gui_extra', 'native', 'typescript', 'webview-interop', 'dist', 'index.js');
let interop = rd(interopPath)
  .replace(/\/\/# sourceMappingURL=.*$/m, '')
  .replace(/export\s*\{[\s\S]*?\};?\s*$/m, '');
if (/^\s*(import|export)\b/m.test(interop)) throw new Error('interop still contains import/export');
interop = `(function () {\n${interop}\nwindow.Juce = { getSliderState: getSliderState, getToggleState: getToggleState, getComboBoxState: getComboBoxState, getNativeFunction: getNativeFunction, getBackendResourceAddress: getBackendResourceAddress, ControlParameterIndexUpdater: ControlParameterIndexUpdater };\n})();`;

const meta = {};
for (const p of params) {
  const m = { type: p.type, def: p.def, name: p.name };
  if (p.type === 'choice') m.choices = p.choices;
  else if (p.type !== 'bool') { m.min = p.min; m.max = p.max; m.fmt = p.fmt; if (p.centre != null) m.centre = p.centre; if (p.bipolar) m.bipolar = true; }
  meta[p.id] = m;
}

const js = [
  '/* webview-008: the ENTIRE UI runtime lives in this single inline script. */',
  'window.VS_NATIVE = typeof window.__JUCE__ !== "undefined";',
  interop,
  '/* bridge for apc-fit.js (expects window.__juce__.invoke) */',
  'if (window.VS_NATIVE) window.__juce__ = { invoke: function (name) { var a = Array.prototype.slice.call(arguments, 1); return window.Juce.getNativeFunction(name).apply(null, a); } };',
  rd(path.join(kitDir, 'apc-fit.js')),
  `window.VS_META = ${JSON.stringify(meta)};`,
  `window.VS_PRESETS = ${JSON.stringify(table.presets)};`,
  rd(path.join(pluginDir, 'Source', 'ui', 'src', 'app.js')),
].join('\n');

const html = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>VaporStrings</title>
<!-- GENERATED by plugins/vaporstrings/tools/build-ui.js from Design/v1-ui-map.json — do not hand-edit.
     webview-008/011: all CSS and JavaScript are inline; no external files. -->
<style>
${css}
</style>
</head>
<body>
<div class="apc-plate" data-fx="fx-haze">
${parts.join('\n')}
</div>
<script>
${js}
</script>
</body>
</html>
`;

const pub = path.join(pluginDir, 'Source', 'ui', 'public');
fs.mkdirSync(pub, { recursive: true });
fs.writeFileSync(path.join(pub, 'index.html'), html);
fs.writeFileSync(path.join(pub, 'test-local.html'), html);
fs.writeFileSync(path.join(pluginDir, 'Design', 'index.html'), html);

/* ---- C++ ParameterIDs.hpp -------------------------------------------------- */
const typeName = { float: 'Float', int: 'Int', bool: 'Bool', choice: 'Choice' };
const num = v => { const s = String(v); return /[.e]/.test(s) ? s + 'f' : s + '.0f'; };
const hpp = `// GENERATED by tools/build-ui.js from Source/ui/params.json — do not hand-edit.
#pragma once

namespace ParameterIDs
{
${params.map(p => `    inline constexpr const char* ${p.id} = "${p.id}";`).join('\n')}
}

namespace ParamTable
{
    enum class Type { Float, Int, Bool, Choice };

    struct Def
    {
        const char* id;
        const char* name;
        Type type;
        float min, max, def;
        float centre;          // skew centre (0 = linear)
        float step;            // interval (0 = continuous)
        const char* choices;   // '|' separated, Choice only
    };

    inline constexpr Def defs[] =
    {
${params.map(p => {
  const isC = p.type === 'choice', isB = p.type === 'bool';
  const min = isB || isC ? 0 : p.min, max = isB ? 1 : isC ? p.choices.length - 1 : p.max;
  const step = p.type === 'int' ? 1 : (p.step || 0);
  return `        { "${p.id}", "${p.name}", Type::${typeName[p.type]}, ${num(min)}, ${num(max)}, ${num(p.def)}, ${num(p.centre || 0)}, ${num(step)}, ${isC ? `"${p.choices.join('|')}"` : 'nullptr'} },`;
}).join('\n')}
    };

    inline constexpr int numParams = (int) (sizeof (defs) / sizeof (defs[0]));

    struct PresetValue { const char* id; float value; };
    struct Preset { const char* name; const PresetValue* values; int numValues; };

${table.presets.map((pr, i) => {
  const vals = Object.entries(pr.values);
  return `    inline constexpr PresetValue preset${i}Values[] = { ${vals.length ? vals.map(([k, v]) => `{ "${k}", ${num(v)} }`).join(', ') : '{ nullptr, 0.0f }'} };`;
}).join('\n')}

    inline constexpr Preset presets[] =
    {
${table.presets.map((pr, i) => `        { "${pr.name}", preset${i}Values, ${Object.keys(pr.values).length} },`).join('\n')}
    };

    inline constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));
}
`;
fs.writeFileSync(path.join(pluginDir, 'Source', 'ParameterIDs.hpp'), hpp);

console.log(`built index.html (${(html.length / 1024).toFixed(1)} KB), test-local.html, Design/index.html, ParameterIDs.hpp (${params.length} params, ${table.presets.length} presets)`);
