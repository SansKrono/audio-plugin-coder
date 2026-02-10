
import * as Juce from "./juce/index.js";

// Parameter State
const sliders = {
    "input_trim": { value: 0.0, min: -24.0, max: 24.0, elementId: "input_trim-control" },
    "mix_ready":  { value: 0.0, min: 0.0, max: 100.0, elementId: "mix_ready-control" },
    "output_trim":{ value: 0.0, min: -24.0, max: 24.0, elementId: "output_trim-control" }
};

const toggles = {
    "bypass": { value: false, elementId: "bypass-control" }
};

// --- Initialization ---
document.addEventListener("DOMContentLoaded", () => {
    
    // Bind Sliders
    for (const [paramId, config] of Object.entries(sliders)) {
        const element = document.getElementById(config.elementId);
        
        if (element) {
            // attach slider listener provided by JUCE
            // The JUCE getSliderState returns an object we can attach a listener to.
            // When C++ param changes, listener fires.
            // When UI changes, we call setNormalizedValue (or similar, handled by integration).
            
            const state = Juce.getSliderState(paramId);
            
            // Initial update
            updateSliderUI(paramId, state.getNormalisedValue());
            
            // Listener for C++ updates
            state.valueChangedEvent.addListener(() => {
                updateSliderUI(paramId, state.getNormalisedValue());
            });
            
            // UI Interaction (Simple Drag Mockup for now, real implementation needs full drag handling)
            // For production, you'd implement a real drag handler here converting pixels to normalized values.
            // Here we just map a click for demonstration or simple integration.
            // In a real app, integrate a library or write custom drag logic.
            
            setupKnobInteraction(element, state);
        }
    }

    // Bind Toggles
    for (const [paramId, config] of Object.entries(toggles)) {
        const element = document.getElementById(config.elementId);
        if (element) {
            const state = Juce.getToggleState(paramId);
            updateToggleUI(paramId, state.getValue());
            
            state.valueChangedEvent.addListener(() => {
                updateToggleUI(paramId, state.getValue());
            });
            
            element.addEventListener('click', () => {
                state.setValue(!state.getValue());
            });
        }
    }
});

// --- UI Updates ---
function updateSliderUI(paramId, normValue) {
    // This function updates the visual rotation/position based on normalized value (0..1)
    const config = sliders[paramId];
    const el = document.getElementById(config.elementId);
    
    // Calculate display value
    const range = config.max - config.min;
    const displayValue = config.min + (range * normValue);
    
    // Update Text
    const label = el.querySelector(".knob-label-value"); // If exists
    // Note: v1-test.html structure might differ slightly, let's adapt generically
    
    // Update SVG Arc
    const arc = el.querySelector(".value-arc");
    if (arc) {
        // Simple mapping: stroke-dashoffset. 
        // Logic depends on specific SVG setup in v1-test.html.
        // Assuming standard setup:
        // Full circle usually. 
        // For mix_ready: 
        // stroke-dasharray: 176, offset ranges 176 (empty) to 0 (full) or specific range.
        // Let's assume standard behavior for now.
        
        // This logic needs to match the CSS/SVG setup exactly to look right.
        // Ideally we read total length.
        const length = arc.getTotalLength();
        // If length is ~176.
        const offset = length - (length * normValue);
        arc.style.strokeDashoffset = offset;
    }
    
    // Fader
    const fill = el.querySelector(".fader-fill");
    if (fill) {
        fill.style.height = `${normValue * 100}%`;
    }
    
    // Output text
    // Assuming there's a div for value text. If not, we might insert one.
    // v1-test.html has <div style="font-size... ">0.0 dB</div>
    // We can target specific children if needed, or add IDs in HTML generation.
}

function updateToggleUI(paramId, value) {
    const config = toggles[paramId];
    const el = document.getElementById(config.elementId);
    if (value) {
        el.classList.add("active");
    } else {
        el.classList.remove("active");
    }
}

// --- Interaction (Basic) ---
function setupKnobInteraction(element, juceState) {
    let isDragging = false;
    let startY = 0;
    let startVal = 0;
    
    element.addEventListener('mousedown', (e) => {
        isDragging = true;
        startY = e.clientY;
        startVal = juceState.getNormalisedValue();
        document.body.style.cursor = 'ns-resize';
    });
    
    document.addEventListener('mousemove', (e) => {
        if (!isDragging) return;
        const deltaY = startY - e.clientY;
        const sensitivity = 0.005; // Adjust as needed
        let newVal = startVal + (deltaY * sensitivity);
        newVal = Math.max(0, Math.min(1, newVal));
        juceState.setNormalisedValue(newVal);
    });
    
    document.addEventListener('mouseup', () => {
        isDragging = false;
        document.body.style.cursor = 'default';
    });
}
