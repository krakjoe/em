// output-resizer.js: Makes the output panel resizable via the drag handle

(function() {
    const panel = document.querySelector('.output-panel');
    const resizer = document.querySelector('.output-resizer');
    if (!panel || !resizer) return;

    let startY = 0;
    let startHeight = 0;
    let dragging = false;

    // Cache these functions to avoid creating new ones on each call
    const onMouseMove = function(e) {
        if (!dragging) return;
        
        // Prevent default to avoid text selection and other issues
        e.preventDefault();
        
        const dy = e.clientY - startY;
        let newHeight = startHeight - dy;
        
        // Calculate bounds once
        const minHeight = 80;
        const maxHeight = window.innerHeight * 0.8;
        
        // Clamp to min/max
        newHeight = Math.max(minHeight, Math.min(maxHeight, newHeight));
        
        // Use requestAnimationFrame for smooth updates
        requestAnimationFrame(() => {
            panel.style.height = newHeight + 'px';
            panel.style.maxHeight = maxHeight + 'px';
            panel.style.minHeight = minHeight + 'px';
        });
    };

    const onMouseUp = function(e) {
        if (!dragging) return;
        
        e.preventDefault();
        dragging = false;
        
        // Reset cursor and selection immediately
        document.body.style.cursor = '';
        document.body.style.userSelect = '';
        
        // Remove event listeners to prevent memory leaks and ensure clean state
        document.removeEventListener('mousemove', onMouseMove);
        document.removeEventListener('mouseup', onMouseUp);
        
        // Also remove from window as fallback
        window.removeEventListener('mousemove', onMouseMove);
        window.removeEventListener('mouseup', onMouseUp);
    };

    resizer.addEventListener('mousedown', function(e) {
        // Prevent default to avoid text selection
        e.preventDefault();
        
        // Only handle left mouse button
        if (e.button !== 0) return;
        
        dragging = true;
        startY = e.clientY;
        startHeight = panel.offsetHeight;
        
        // Set cursor styles
        document.body.style.cursor = 'ns-resize';
        document.body.style.userSelect = 'none';
        
        // Add event listeners when dragging starts
        document.addEventListener('mousemove', onMouseMove, { passive: false });
        document.addEventListener('mouseup', onMouseUp, { passive: false });
        
        // Add to window as well for better coverage
        window.addEventListener('mousemove', onMouseMove, { passive: false });
        window.addEventListener('mouseup', onMouseUp, { passive: false });
    });

    // Handle edge cases where mouse leaves the window
    window.addEventListener('blur', function() {
        if (dragging) {
            dragging = false;
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
            
            // Clean up listeners
            document.removeEventListener('mousemove', onMouseMove);
            document.removeEventListener('mouseup', onMouseUp);
            window.removeEventListener('mousemove', onMouseMove);
            window.removeEventListener('mouseup', onMouseUp);
        }
    });

    // Handle mouse leaving the document
    document.addEventListener('mouseleave', function(e) {
        // Only trigger if mouse actually leaves the document bounds
        if (dragging && (e.clientY <= 0 || e.clientX <= 0 || 
            e.clientX >= document.documentElement.clientWidth || 
            e.clientY >= document.documentElement.clientHeight)) {
            
            dragging = false;
            document.body.style.cursor = '';
            document.body.style.userSelect = '';
            
            // Clean up listeners
            document.removeEventListener('mousemove', onMouseMove);
            document.removeEventListener('mouseup', onMouseUp);
            window.removeEventListener('mousemove', onMouseMove);
            window.removeEventListener('mouseup', onMouseUp);
        }
    });
})();