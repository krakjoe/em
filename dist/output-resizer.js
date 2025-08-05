// output-resizer.js: Makes the output panel resizable via the drag handle

(function() {
    const panel = document.querySelector('.output-panel');
    const resizer = document.querySelector('.output-resizer');
    if (!panel || !resizer) return;

    let startY = 0;
    let startHeight = 0;
    let dragging = false;

    resizer.addEventListener('mousedown', function(e) {
        dragging = true;
        startY = e.clientY;
        startHeight = panel.offsetHeight;
        document.body.style.cursor = 'ns-resize';
        document.body.style.userSelect = 'none';
    });

    function onMouseMove(e) {
        if (!dragging) return;
        const dy = e.clientY - startY;
        let newHeight = startHeight - dy;
        // Clamp to min/max
        newHeight = Math.max(80, Math.min(window.innerHeight * 0.8, newHeight));
        panel.style.height = newHeight + 'px';
        panel.style.maxHeight = window.innerHeight * 0.8 + 'px';
        panel.style.minHeight = '80px';
    }

    function onMouseUp() {
        if (!dragging) return;
        dragging = false;
        document.body.style.cursor = '';
        document.body.style.userSelect = '';
    }

    window.addEventListener('mousemove', onMouseMove);
    window.addEventListener('mouseup', onMouseUp);
})();
