const encoder = new TextEncoder("utf-8");
const pending = new Map();

let auth = false;
let boot = null;

const channel = new BroadcastChannel('em-browser');

channel.addEventListener('message', (event) => {
    if (event.data.type === 'CLIENT_RESPONSE') {
        console.log(
            '[worker] Responding', 
            event.data);
        const resolve = pending.get(event.data.id);
        if (resolve) {
            pending.delete(event.data.id);
            resolve(event.data);
        } else {
            console.warn('[worker] Nothing Pending',
                event.data.id);
        }
    }
});

async function resolveResponse(promise, event) {
    const result = await promise;
    return new Response(result.response.body, { 
        status:     result.response.status.code,
        statusText: result.response.status.text,
        headers: new Headers(result.response.headers)
    });
}

function resolveResource(url) {
    // Always return only URI and query string, stripping host/scheme/etc
    let urlObj;
    try {
        urlObj = new URL(url, self.location.origin);
    } catch (e) {
        // If url is not absolute, treat as path
        urlObj = { pathname: url, search: '' };
    }
    return urlObj.pathname + (urlObj.search || '');
}

self.addEventListener('fetch', event => {
    const url = new URL(event.request.url, self.location.url);

    if (url.pathname.endsWith(boot)) {
        /* let boot code pass */
        return;
    }

    if (url.origin != self.location.origin) {
        /* only respond to fetches with the same origin */
        return;
    }

    if (!auth) {
        /* not yet authorized to make requests */
        return;
    }

    event.respondWith((async () => {
        const id = crypto.randomUUID();
        const promise = new Promise(resolve => pending.set(id, resolve));

        let resourceUrl = resolveResource(event.request.url);

        console.log(
            `[worker] Intercepting ${event.request.url} -> ${resourceUrl}`);

        let head = `${event.request.method} ${resourceUrl}\r\n`;
        if (event.request.headers) {
            for (const [key, value] of event.request.headers.entries()) {
                head += `${key}: ${value}\r\n`;
            }
        }

        head += '\r\n';

        const body = await event.request.arrayBuffer();

        channel.postMessage({
            type: 'CLIENT_REQUEST',
            id: id,
            method: event.request.method,
            url: event.request.url,
            head: encoder.encode(head),
            body: body,
        });

        return resolveResponse(promise, event);
    })());
});

self.addEventListener('message', event => {    
    switch (event.data.type) {
        case 'SKIP_WAITING':
            self.skipWaiting();
            return;

        case 'CLIENTS_CLAIM':
            self.clients.claim();
            return;

        case 'CLIENT_INIT':
            /* allow boot code to run */
            console.log(
                "[worker] Initializing");
            boot = event.data.boot;
            auth = false;
            event.source.postMessage(
                { type: 'CLIENT_INIT_ACK' });
            return;

        case 'CLIENT_IDENT':
            /* authorize requests */
            console.log(
                "[worker] Dispatching");
            auth = true;
            return;
    }
});