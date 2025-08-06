const browserUUID = crypto.randomUUID();
const workerScope = window.location.pathname.endsWith('/') ?
    window.location.pathname :
    window.location.pathname.substring(
        0, 
        window.location.pathname.lastIndexOf('/') + 1
);

const workerUrl = workerScope + 'worker.js';

function findContentType(headers, fallback) {
    if (!headers) {
        return fallback;
    }

    if (headers["Content-Type"]) {
        return headers["Content-Type"];
    }

    if (headers["content-type"]) {
        return headers["content-type"];
    }

    return fallback;
}

function findRequestPath(uri) {
    // Get the base path (e.g., '/em/')
    const basePath = window.location.pathname.endsWith('/')
        ? window.location.pathname
        : window.location.pathname.substring(
            0, window.location.pathname.lastIndexOf('/') + 1);

    // Only strip basePath if uri starts with it (and basePath is not '/')
    if (basePath !== '/' && uri.startsWith(basePath)) {
        const stripped = uri.slice(basePath.length - 1);
        // Special case: if stripped is empty, use '/'
        return stripped === '' ? '/' : stripped;
    }
    return uri;
}

navigator.serviceWorker.addEventListener('message', function(event) {
    if (event.data.type == 'CLIENT_AUTH') {
        navigator.serviceWorker.controller.postMessage({
            type: 'CLIENT_IDENT',
            uuid: browserUUID
        });

        if (event.data.queued) {
            const url = new URL(event.data.queued.url, window.location.url);
            const uri = url.pathname + url.search;
            const body = event.data.queued.body ?
                new Uint8Array(event.data.queued.body) : null;

            const response = Module.dispatch(
                event.data.queued.method,
                findRequestPath(uri),
                findContentType(event.data.queued.headers,
                    'application/x-em-dispatch'),
                body,
            );

            navigator.serviceWorker.controller.postMessage({
                type: 'dispatch-response',
                id: event.data.queued.id,
                uuid: browserUUID,
                response: response
            });
        }
    } else if (event.data.type === 'CLIENT_REDIRECT') {
        window.location.href = event.data.url;
    } else if (event.data.type === 'dispatch-request') {
        const url = new URL(event.data.url, window.location.url);

        if (url.origin != window.location.origin) {
            return;
        }

        const uri = url.pathname + url.search;
        const body = event.data.body ?
                new Uint8Array(event.data.body) : null;

        const response = Module.dispatch(
            event.data.method,
            findRequestPath(uri),
            findContentType(event.data.headers,
                'application/x-em-dispatch'),
            body,
        );

        navigator.serviceWorker.controller.postMessage({
            type: 'dispatch-response',
            id: event.data.id,
            ident: browserUUID,
            response: response});
    }
});

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

window.renderBrowserTab = async function(tab, container) {
    if (typeof Module === 'undefined' || !Module.dispatch) {
        throw new Error('Module not ready - cannot create browser tab');
    }

    const browserTabTemplate = document.getElementById('browserTabTemplate');
    if (!browserTabTemplate) {
        return;
    }

    const tabContent = browserTabTemplate.content.cloneNode(true);
    const browserTab = tabContent.querySelector('.browser-tab');

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
    const contentDiv = browserTab.querySelector('.browser-content');

    // Set initial URL
    urlInput.value = tab.history[tab.historyIndex];

    function updateNavButtons() {
        backBtn.disabled = tab.historyIndex <= 0;
        fwdBtn.disabled = tab.historyIndex >= tab.history.length - 1;
    }

    // Create the iframe for browser content
    const iframe = document.createElement('iframe');
    iframe.className = 'browser-iframe';
    iframe.style.width = '100%';
    iframe.style.flex = '1 1 auto';
    iframe.style.minHeight = '0';
    iframe.style.border = 'none';
    iframe.setAttribute('sandbox',
        'allow-scripts allow-downloads allow-forms allow-same-origin');
            // allow scripts/styles, but not navigation outside
    // Listen for navigation events in the iframe and update nav bar/history
    iframe.addEventListener('load', function() {
        try {
            const newUrl =
                iframe.contentWindow.location.pathname +
                iframe.contentWindow.location.search +
                iframe.contentWindow.location.hash;
            if (newUrl && newUrl !== tab.history[tab.historyIndex]) {
                urlInput.value = newUrl;
                tab.history = tab.history.slice(0, tab.historyIndex + 1);
                tab.history.push(newUrl);
                tab.historyIndex = tab.history.length - 1;
                updateNavButtons();
            }

            // Inject navigation guard for virtual webroot
            const basePath = window.location.pathname.endsWith('/')
                ? window.location.pathname
                : window.location.pathname.substring(
                    0, window.location.pathname.lastIndexOf('/') + 1);
            if (iframe.contentWindow && iframe.contentDocument) {
                const doc = iframe.contentDocument;
                doc.addEventListener('click', function(e) {
                let a = e.target;
                while (a && a.tagName !== 'A') a = a.parentElement;
                    if (a && a.tagName === 'A' && a.hasAttribute('href')) {
                        let href = a.getAttribute('href');
                        // Only rewrite absolute paths not under basePath
                        if (href &&
                            href.startsWith('/') &&
                            basePath !== '/' &&
                            !href.startsWith(basePath)) {
                            const newHref = basePath.replace(/\/$/, '') + href;
                            a.setAttribute('href', newHref);
                            e.preventDefault();
                            
                            navigate(newHref);
                        }
                    }
                }, true);
            }
        } catch (e) {}
    });

    // Compute the base path (virtual webroot) for this deployment
    const basePath = window.location.pathname.endsWith('/')
        ? window.location.pathname
        : window.location.pathname.substring(
            0, window.location.pathname.lastIndexOf('/') + 1);

    async function navigate(toUrl, addToHistory = true) {
        let url = toUrl.startsWith("/") ? toUrl : "/" + toUrl;
        // Prepend basePath if not already present
        if (basePath !== '/' && !url.startsWith(basePath)) {
            url = basePath.replace(/\/$/, '') + url;
        }
        urlInput.value = url;
        iframe.src = url;
        if (addToHistory) {
            tab.history = tab.history.slice(0, tab.historyIndex + 1);
            tab.history.push(url);
            tab.historyIndex = tab.history.length - 1;
        }
        updateNavButtons();
    }

    goBtn.addEventListener('click',
        () => navigate(urlInput.value));
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
    refreshBtn.addEventListener('click',
        () => navigate(tab.history[tab.historyIndex], false));

    // Initial navigation
    navigate(tab.history[tab.historyIndex], false);

    // Add toolbar and iframe to the browser tab
    contentDiv.innerHTML = '';
    contentDiv.appendChild(iframe);
    container.appendChild(browserTab);
};

// At the very end of IDE loading, after everything is ready:
window.addEventListener('load', async () => {
    /* cleanup from previous loads */
    if ('serviceWorker' in navigator) {
        const registrations = await
            navigator.serviceWorker.getRegistrations(workerScope);
        for (const registration of registrations) {
            await registration.unregister();
        }
    }

    if (typeof Module !== 'undefined' && Module.dispatch) {
        const registration = await navigator.serviceWorker.register(
            workerUrl, { scope: workerScope });

        // Force SW to take control immediately
        if (registration.waiting) {
            registration.waiting.postMessage({ type: 'SKIP_WAITING' });
        }

        await navigator.serviceWorker.ready;

        // Force SW to claim control of this page
        if (registration.active) {
            registration.active.postMessage({ type: 'CLIENTS_CLAIM' });
        }

        // Wait for control
        if (!navigator.serviceWorker.controller) {
            await new Promise(resolve => {
                navigator.serviceWorker.addEventListener(
                    'controllerchange', resolve, { once: true });
            });
        }

        // Tell the SW which client is the main thread
        if (navigator.serviceWorker.controller) {
            navigator.serviceWorker.controller.postMessage({
                type: 'CLIENT_IDENT',
                uuid: browserUUID
            });
        }
    }
});

window.addEventListener('beforeunload', async () => {
    if (navigator.serviceWorker.controller) {
        navigator.serviceWorker.controller
            .postMessage({ type: 'CLIENT_GOODBYE' });
    }
});
