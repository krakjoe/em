// browser.js: Logic for the in-IDE browser tab

// Exported: create a browser tab object for openTabs
window.createBrowserTab = function(url = '/') {
    return {
        type: 'browser',
        name: 'Browser',
        url: url,
        history: [url],
        historyIndex: 0,
        // The browser tab does not have a file path
        path: null,
        unsaved: false
    };
};

// Exported: render the browser tab content into the editor panel
// Render the browser tab UI inside a dedicated, empty container (e.g., .browser-tab-container)
// The container should be created and managed by the tab switching logic (see main.js)
window.renderBrowserTab = function(tab, container) {
    const browserTabTemplate = document.getElementById('browserTabTemplate');
    if (!browserTabTemplate) return;

    const tabContent = browserTabTemplate.content.cloneNode(true);
    const browserTab = tabContent.querySelector('.browser-tab');
    // Do not set width/height/flex on container; let parent control layout
    browserTab.style.width = '100%';
    browserTab.style.height = '100%';
    browserTab.style.display = 'flex';
    browserTab.style.flexDirection = 'column';
    browserTab.style.flex = '1 1 auto';
    browserTab.style.overflow = 'hidden';
    browserTab.style.minHeight = '0';

    const toolbar = browserTab.querySelector('.browser-toolbar');
    const urlInput = toolbar.querySelector('.browser-url');
    const goBtn = toolbar.querySelector('.browser-go');
    const backBtn = toolbar.querySelector('.browser-back');
    const fwdBtn = toolbar.querySelector('.browser-forward');
    const refreshBtn = toolbar.querySelector('.browser-refresh');
    const menuBtn = toolbar.querySelector('.browser-menu');
    const contextMenu = toolbar.querySelector('.browser-context-menu');
    const contentDiv = browserTab.querySelector('.browser-content');

    // --- CONTEXT MENU (ellipsis/menu button, static markup) ---
    let lastHtmlSource = '';
    // Patch setIframeContent to remember last HTML
    const originalSetIframeContent = setIframeContent;
    setIframeContent = function(html) {
        lastHtmlSource = html;
        originalSetIframeContent(html);
    };

    // Show/hide and position the static context menu
    function showContextMenu() {
        // Position menu below the button, clamped to viewport
        const rect = menuBtn.getBoundingClientRect();
        // Temporarily show to measure size
        contextMenu.style.display = 'block';
        contextMenu.style.visibility = 'hidden';
        contextMenu.style.left = '0px';
        contextMenu.style.top = '0px';
        const menuWidth = contextMenu.offsetWidth;
        const menuHeight = contextMenu.offsetHeight;
        // Default position: below button
        let left = rect.left;
        let top = rect.bottom + window.scrollY;
        // Clamp right edge
        if (left + menuWidth > window.innerWidth) {
            left = window.innerWidth - menuWidth - 8;
        }
        // Clamp bottom edge
        if (top + menuHeight > window.innerHeight + window.scrollY) {
            top = rect.top + window.scrollY - menuHeight;
        }
        // Never negative
        left = Math.max(8, left);
        top = Math.max(8, top);
        contextMenu.style.left = left + 'px';
        contextMenu.style.top = top + 'px';
        contextMenu.style.zIndex = '10000';
        contextMenu.style.visibility = 'visible';
    }
    function hideContextMenu() {
        contextMenu.style.display = 'none';
    }
    menuBtn.addEventListener('click', function(e) {
        e.preventDefault();
        showContextMenu();
    });
    // Hide menu on click outside
    document.addEventListener('mousedown', function(e) {
        if (contextMenu.style.display === 'block' && !contextMenu.contains(e.target) && e.target !== menuBtn) {
            hideContextMenu();
        }
    });
    // Menu actions
    contextMenu.querySelectorAll('.browser-context-menu-item').forEach(item => {
        item.addEventListener('click', function(e) {
            const action = item.getAttribute('data-action');
            hideContextMenu();
            if (action === 'refresh') {
                refreshBtn.click();
            } else if (action === 'view-source') {
                // Open untitled tab with HTML source using the same pattern as loadDemo
                if (window.openTabs && typeof window.switchTabView === 'function') {
                    // Generate a unique name for the untitled source tab
                    let baseName = 'untitled-source.html';
                    let name = baseName;
                    let counter = 1;
                    while (window.openTabs.some(t => !t.path && t.name === name)) {
                        name = baseName.replace('.html', `-${counter}.html`);
                        counter++;
                    }
                    const newTab = {
                        path: null,
                        name,
                        content: lastHtmlSource,
                        unsaved: false
                    };
                    window.openTabs.push(newTab);
                    window.switchTabView(newTab);
                } else {
                    alert('Tab API not available');
                }
            }
        });
    });

    // Set initial URL
    urlInput.value = tab.history[tab.historyIndex];

    function updateNavButtons() {
        backBtn.disabled = tab.historyIndex <= 0;
        fwdBtn.disabled = tab.historyIndex >= tab.history.length - 1;
    }

    // --- IFRAME BROWSER RENDERING ---
    // Create the iframe for browser content
    const iframe = document.createElement('iframe');
    iframe.className = 'browser-iframe';
    iframe.style.width = '100%';
    iframe.style.flex = '1 1 auto';
    iframe.style.minHeight = '0';
    iframe.style.border = 'none';
    iframe.setAttribute('sandbox',
        'allow-scripts allow-forms allow-same-origin');
            // allow scripts/styles, but not navigation outside

    // Helper to write HTML to the iframe
    function setIframeContent(html) {
        iframe.onload = function() {
            const doc = iframe.contentDocument;

            doc.open();
            doc.write(html);
            doc.close();
            // Instrument links and forms
            instrumentIframe(doc);
        };
    }

    // Instrument links and forms in the iframe to reroute through dispatch
    function instrumentIframe(doc) {
        // Helper: is a URL relative/local (should be rerouted)?
        function isRelativeUrl(url) {
            return url && !/^(?:[a-z]+:)?\/\//i.test(url) && !url.startsWith('data:') && !url.startsWith('mailto:');
        }
        // Intercept all <a> clicks
        Array.from(doc.querySelectorAll('a[href]')).forEach(a => {
            a.addEventListener('click', function(e) {
                const href = a.getAttribute('href');
                if (isRelativeUrl(href)) {
                    e.preventDefault();
                    navigate(href);
                }
            });
        });

        // Intercept all <link rel="stylesheet"> requests and reroute through dispatch (only relative)
        Array.from(doc.querySelectorAll('link[rel="stylesheet"][href]')).forEach(link => {
            const href = link.getAttribute('href');
            if (isRelativeUrl(href) && typeof Module !== 'undefined' && typeof Module.dispatch === 'function') {
                try {
                    const css = Module.dispatch('GET', href, 'text/css', null);
                    const style = doc.createElement('style');
                    style.textContent = css;
                    link.parentNode.replaceChild(style, link);
                } catch (err) {
                    // If error, remove the link and show error in console
                    link.parentNode.removeChild(link);
                    console.error('Failed to load stylesheet:', href, err);
                }
            }
        });

        // Intercept all <script src> requests and reroute through dispatch (only relative)
        Array.from(doc.querySelectorAll('script[src]')).forEach(script => {
            const src = script.getAttribute('src');
            if (isRelativeUrl(src) && typeof Module !== 'undefined' && typeof Module.dispatch === 'function') {
                try {
                    const js = Module.dispatch('GET', src, 'application/javascript', null);
                    const inlineScript = doc.createElement('script');
                    inlineScript.textContent = js;
                    script.parentNode.replaceChild(inlineScript, script);
                } catch (err) {
                    script.parentNode.removeChild(script);
                    console.error('Failed to load script:', src, err);
                }
            }
        });

        // Intercept all <img src>, <source srcset>, <video src>, <audio src>, <track src>, <iframe src>, <object data>, <embed src> (only relative)
        // Helper for src/data attributes
        function rerouteElementSrc(el, attr, mime) {
            const url = el.getAttribute(attr);
            if (isRelativeUrl(url) && typeof Module !== 'undefined' && typeof Module.dispatch === 'function') {
                try {
                    const data = Module.dispatch('GET', url, mime, null);
                    // For images/media, convert to data URL if possible
                    if (mime.startsWith('image/') || mime.startsWith('video/') || mime.startsWith('audio/')) {
                        // Try to base64 encode if not already
                        // Assume Module.dispatch returns raw binary or base64 string
                        // For now, just set src to data: URI
                        el.setAttribute(attr, `data:${mime};base64,${data}`);
                    } else {
                        // For iframe/object/embed, just set srcdoc/data if possible
                        if (el.tagName === 'IFRAME') {
                            el.removeAttribute(attr);
                            el.setAttribute('srcdoc', data);
                        } else if (el.tagName === 'OBJECT') {
                            el.removeAttribute(attr);
                            el.innerHTML = data;
                        } else {
                            // fallback: set src to data URI
                            el.setAttribute(attr, `data:${mime};base64,${data}`);
                        }
                    }
                } catch (err) {
                    el.setAttribute(attr, '');
                    console.error('Failed to load resource:', url, err);
                }
            }
        }

        // Images
        Array.from(doc.querySelectorAll('img[src]')).forEach(img => rerouteElementSrc(img, 'src', 'image/png'));
        // Video
        Array.from(doc.querySelectorAll('video[src]')).forEach(video => rerouteElementSrc(video, 'src', 'video/mp4'));
        // Audio
        Array.from(doc.querySelectorAll('audio[src]')).forEach(audio => rerouteElementSrc(audio, 'src', 'audio/mpeg'));
        // Source (for <picture>, <video>, <audio>)
        Array.from(doc.querySelectorAll('source[src]')).forEach(source => {
            // Try to guess mime from type attribute or parent
            let mime = source.getAttribute('type') || 'application/octet-stream';
            rerouteElementSrc(source, 'src', mime);
        });
        // Track (subtitles/captions)
        Array.from(doc.querySelectorAll('track[src]')).forEach(track => rerouteElementSrc(track, 'src', 'text/vtt'));
        // Iframe
        Array.from(doc.querySelectorAll('iframe[src]')).forEach(iframe => rerouteElementSrc(iframe, 'src', 'text/html'));
        // Object
        Array.from(doc.querySelectorAll('object[data]')).forEach(obj => rerouteElementSrc(obj, 'data', 'application/octet-stream'));
        // Embed
        Array.from(doc.querySelectorAll('embed[src]')).forEach(embed => rerouteElementSrc(embed, 'src', 'application/octet-stream'));

        // Intercept all <form> submissions
        // ---
        // FORM SUBMISSION HANDLING
        // Note: For GET and application/x-www-form-urlencoded POST, we serialize form data as a query string.
        // For multipart/form-data (file uploads), FormData cannot be directly serialized to a multipart string in JS without XHR/fetch or a custom serializer.
        // If/when backend SAPI supports uploads, this can be improved to support multipart POST by serializing FormData as multipart and passing correct headers.
        // For now, only URL-encoded forms are fully supported; multipart forms may require further JS work.
        // ---
        Array.from(doc.querySelectorAll('form')).forEach(form => {
            form.addEventListener('submit', function(e) {
                e.preventDefault();
                const action = form.getAttribute('action') || urlInput.value;
                const method = (form.getAttribute('method') || 'GET').toUpperCase();
                let formData = new FormData(form);
                let params = new URLSearchParams();
                for (const [key, value] of formData.entries()) {
                    params.append(key, value);
                }
                let targetUrl = action;
                if (method === 'GET') {
                    targetUrl += (targetUrl.includes('?') ? '&' : '?') + params.toString();
                    navigate(targetUrl);
                } else {
                    // For POST, send params as body
                    navigate(targetUrl, true, params);
                }
            });
        });
    }

    // Navigation logic
    function navigate(toUrl, addToHistory = true, postData = null) {
        urlInput.value = toUrl;
        iframe.srcdoc = '<em>Loading...</em>';
        if (typeof Module !== 'undefined' && typeof Module.dispatch === 'function') {
            try {
                let raw;
                if (postData) {
                    // POST
                    raw = Module.dispatch('POST', toUrl,
                        'application/x-www-form-urlencoded', postData.toString());
                } else {
                    // GET
                    raw = Module.dispatch('GET', toUrl, 'text/html', null);
                }
                let headers = {}, body = raw;
                if (typeof raw === 'string') {
                    const split = raw.split(/\r?\n\r?\n/);
                    if (split.length > 1) {
                        const headerLines = split[0].split(/\r?\n/);
                        headerLines.forEach(line => {
                            const idx = line.indexOf(':');
                            if (idx > 0) {
                                const key = line.slice(0, idx).trim().toLowerCase();
                                const value = line.slice(idx + 1).trim();
                                headers[key] = value;
                            }
                        });
                        body = split.slice(1).join('\n\n');
                    }
                }
                setIframeContent(body);
            } catch (err) {
                iframe.srcdoc = '<span style="color:red">Error: ' + (err && err.message ? err.message : err) + '</span>';
            }
        } else {
            iframe.srcdoc = '<span style="color:red">PHP runtime not loaded</span>';
        }
        if (addToHistory) {
            tab.history = tab.history.slice(0, tab.historyIndex + 1);
            tab.history.push(toUrl);
            tab.historyIndex = tab.history.length - 1;
        }
        updateNavButtons();
    }

    goBtn.addEventListener('click', () => navigate(urlInput.value));
    urlInput.addEventListener('keydown', e => {
        if (e.key === 'Enter') navigate(urlInput.value);
    });
    backBtn.addEventListener('click', () => {
        if (tab.historyIndex > 0) {
            tab.historyIndex--;
            navigate(tab.history[tab.historyIndex], false);
        }
    });
    fwdBtn.addEventListener('click', () => {
        if (tab.historyIndex < tab.history.length - 1) {
            tab.historyIndex++;
            navigate(tab.history[tab.historyIndex], false);
        }
    });
    refreshBtn.addEventListener('click', () => navigate(tab.history[tab.historyIndex], false));

    // Initial navigation
    navigate(tab.history[tab.historyIndex], false);

    // Add toolbar and iframe to the browser tab
    contentDiv.innerHTML = '';
    contentDiv.appendChild(iframe);
    container.appendChild(browserTab);
};
