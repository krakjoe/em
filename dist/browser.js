const encoder = new TextEncoder("utf-8");

window.browserTab = {
    type: 'browser',
    name: 'Browser',
    url: '/',
    history: ['/'],
    historyIndex: 0,
    path: null,
    unsaved: false
};

class Browser {

    constructor(frame, vroot, droot, boot, worker) {
        this.frame  = frame;
        this.vroot  = vroot;
        this.droot  = droot;
        this.boot   = boot;
        this.worker = worker;
        this.uuid   = crypto.randomUUID();

        this.frame.addEventListener("load", function(){
            this.frame.contentWindow.postMessage({
                type: 'CLIENT_INIT',
                vroot:  this.vroot,
                boot:   this.boot,
                worker: this.worker,
            }, '*');
        }.bind(this), { once: true });

        this.frame.src = this.boot;

        window.addEventListener('message', async function(event) {
            if (event.data && event.data.type === 'CLIENT_INIT_ACK') {
                await this.frame.contentWindow.postMessage({
                    type:  'CLIENT_IDENT',
                    uuid:  this.uuid,
                });
                return;
            }

            if (typeof Module == 'undefined' || !Module.ready) {
                return;
            }

            if (event.data.type === 'CLIENT_REQUEST') {                
                const url = new URL(event.data.url, window.location.url);
                if (url.origin != window.location.origin) {
                    console.log("wrong origin");
                    return;
                }

                const response = Module.dispatch(
                    encoder.encode(JSON.stringify({
                        DOCUMENT_ROOT: this.droot,
                        VIRTUAL_ROOT:  this.vroot
                    })),
                    new Uint8Array(event.data.head),
                    event.data.body ?
                        new Uint8Array(event.data.body) :
                            null,
                );

                console.log("response", response);
                this.frame.contentWindow.postMessage({
                    type: 'CLIENT_RESPONSE',
                    id: event.data.id,
                    ident: this.uuid,
                    response: response});
            }
        }.bind(this));
    }

    frame  = null;
    vroot  = null;
    boot   = null;
    worker = null;
    uuid   = null;
}

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

window.updateBrowserButtons = async function(container) {
    const back =
        container.querySelector("#browser-back");
    back.disabled = window.browserTab.historyIndex <= 0;

    const forward =
        container.querySelector("#browser-back");
    forward.disabled =
        window.browserTab.historyIndex >=
            window.browserTab.history.length - 1;
}

window.setUpBrowserContainer = async function(container) {
    await window.loadConfiguration();

    const toolbar = container.querySelector('#browser-toolbar');
    const urlInput = toolbar.querySelector('#browser-url');
    const goBtn = toolbar.querySelector('#browser-go');
    const backBtn = toolbar.querySelector('#browser-back');
    const fwdBtn = toolbar.querySelector('#browser-forward');
    const refreshBtn = toolbar.querySelector('#browser-refresh');

    // setup toolbar
    goBtn.addEventListener('click',
        () => window.navigateBrowser(container, urlInput.value));
    urlInput.addEventListener('keydown', e => {
        if (e.key === 'Enter') {
            window.navigateBrowser(
                container, urlInput.value);
        }
    });
    backBtn.addEventListener('click', () => {
        if (window.browserTab.historyIndex > 0) {
            window.browserTab.historyIndex--;
            window.navigateBrowser(container,
                window.browserTab.history[
                    window.browserTab.historyIndex], false);
        }
    });
    fwdBtn.addEventListener('click', () => {
        if (window.browserTab.historyIndex <
                window.browserTab.history.length - 1) {
            window.browserTab.historyIndex++;
            window.navigateBrowser(container,
                window.browserTab.history[
                    window.browserTab.historyIndex], false);
        }
    });
    refreshBtn.addEventListener('click',
        () => window.navigateBrowser(container,
            window.browserTab.history[
                window.browserTab.historyIndex], false));
    
    // Setup content
    const iframe = container.querySelector('#browser-frame');

    iframe.Browser = new Browser(
        iframe,
        window.configurationTab.configuration["vroot"],
        window.configurationTab.configuration["droot"],
        window.configurationTab.configuration["boot"],
        window.configurationTab.configuration["worker"]);

    // Listen for navigation events in the iframe and update nav bar/history
    iframe.addEventListener('load', async function() {
        try {
            const newUrl =
                iframe.contentWindow.location.pathname +
                iframe.contentWindow.location.search +
                iframe.contentWindow.location.hash;
            if (newUrl &&
                newUrl !== window.browserTab.history[
                    window.browserTab.historyIndex
                ]) {
                urlInput.value = newUrl;
                window.browserTab.history = window.browserTab.history
                    .slice(0, window.browserTab.historyIndex + 1);
                window.browserTab.history.push(newUrl);
                window.browserTab.historyIndex =
                    window.browserTab.history.length - 1;
                window.updateBrowserButtons(container);
            }

            // Inject navigation guard for virtual webroot
            const basePath = window.location.pathname.endsWith('/')
                ? window.location.pathname
                : window.location.pathname.substring(
                    0, window.location.pathname.lastIndexOf('/') + 1);
            if (iframe.contentWindow && iframe.contentDocument) {
                const doc = iframe.contentDocument;
                doc.addEventListener('click', function(e)
                {
                    let a = e.target;
                    
                    while (a && a.tagName !== 'A') {
                        a = a.parentElement;
                    }

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
                            window.navigateBrowser(container, newHref);
                        }
                    }
                }, true);
            }
        } catch (e) {} 
    });
}

window.navigateBrowser = async function(container, toUrl, addToHistory = true) {
    // Compute the base path (virtual webroot) for this deployment
    const basePath = window.location.pathname.endsWith('/')
        ? window.location.pathname
        : window.location.pathname.substring(
            0, window.location.pathname.lastIndexOf('/') + 1);

    let url = toUrl.startsWith("/") ? toUrl : "/" + toUrl;

    // Prepend basePath if not already present
    if (basePath !== '/' && !url.startsWith(basePath)) {
        url = basePath.replace(/\/$/, '') + url;
    }

    const input = container
        .querySelector("#browser-url");
    input.value = url;

    if (addToHistory) {
        window.browserTab.history =
            window.browserTab.history.slice(
                0, window.browserTab.historyIndex + 1);
        window.browserTab.history.push(url);
        window.browserTab.historyIndex =
            window.browserTab.history.length - 1;
    }

    const frame = container
        .querySelector(
            "#browser-frame");
    frame.src = url;

    window.updateBrowserButtons(container);
}

window.updateBrowserContainer = async function(container, tab) {
    if (tab.type != "browser") {
        container.style.display = "none";
        return;
    }

    window.navigateBrowser(
        container,
        tab.history[tab.historyIndex]);
    container.style.display = "block";
};