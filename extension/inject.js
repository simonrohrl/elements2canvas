// This script runs at document_start on every page
(function() {
  function init() {
    // Create the target element
    const target = document.createElement('div');
    target.id = 'dom-tracing-target';
    target.style.cssText = `
      position: fixed;
      top: 10px;
      right: 10px;
      width: 50px;
      height: 50px;
      z-index: 2147483647;
      border: 2px solid black;
    `;
    document.body.appendChild(target);

    let count = 0;

    // Use a large Prime Number as an increment.
    // This ensures colors look "unique" and different every frame immediately,
    // rather than just slowly turning from black to dark blue.
    const JUMP = 132497;
    const MAX_COLOR = 16777216; // 2^24 (total possible 24-bit colors)

    function changeColor() {
      // Re-insert if removed
      if (!document.body.contains(target)) {
        document.body.appendChild(target);
      }

      // 1. Increment the counter
      count = (count + JUMP) % MAX_COLOR;

      // 2. Use Bitwise Shifts to extract R, G, and B
      // This is the fastest way to handle color math in JS
      const r = (count >> 16) & 0xff;
      const g = (count >> 8) & 0xff;
      const b = count & 0xff;

      // 3. Apply via Template Literal
      target.style.backgroundColor = `rgb(${r},${g},${b})`;

      requestAnimationFrame(changeColor);
    }

    requestAnimationFrame(changeColor);
  }

  // Wait for body to exist
  if (document.body) {
    init();
  } else {
    document.addEventListener('DOMContentLoaded', init);
  }
})();
