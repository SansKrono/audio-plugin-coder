/*
  Development utility imported by the JUCE frontend library (./index.js).

  - Inside the plugin, JUCE injects window.__JUCE__ before any page script runs.
  - On Android, JUCE delivers its user scripts lazily; evaluate them once here.
  - In a plain browser (design preview), window.__JUCE__ is missing; warn so the
    developer knows a mock backend (../mock-backend.js) must be installed first.
*/
if (typeof window.__JUCE__ === "undefined") {
  console.warn(
    "window.__JUCE__ is not defined: no JUCE native backend detected. " +
      "Load js/mock-backend.js before the JUCE frontend library for browser previews."
  );
} else if (
  typeof window.__JUCE__.getAndroidUserScripts !== "undefined" &&
  typeof window.inAndroidUserScriptEval === "undefined"
) {
  window.inAndroidUserScriptEval = true;
  eval(window.__JUCE__.getAndroidUserScripts());
  delete window.inAndroidUserScriptEval;
}

export {};
