const encoder = new TextEncoder("utf-8");
const pending = new Map();
let   uuid    = null;
let   auth    = false;
let   boot    = null;

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
        console.log("cross origin");
        return;
    }

    if (!auth) {
        /* not yet authorizaed to make requests */
        console.log("no auth");
        return;
    }

    event.respondWith((async () => {
        const id = Math.random().toString();
        const promise = new Promise(
            resolve => pending.set(id, resolve));
        const clients = await self.clients
            .matchAll({ includeUncontrolled: true });
            console.log(clients);
        if (clients.length == 0) {
            return fetch(event.request);
        }

        let url = resolveResource(event.request.url);

        console.log(`rewrite ${event.request.url} -> ${url}`);

        let head =
            `${event.request.method} ${url}\r\n`;
        if (event.request.headers) {
            for (const [key, value] of event.request.headers.entries()) {
                head += `${key}: ${value}\r\n`;
            }
        }
        head += '\r\n';

        const body = await event.request.arrayBuffer();

        clients.forEach(client => {
            client.postMessage({
                type: 'CLIENT_REQUEST',
                id:      id,
                method:  event.request.method,
                url:     event.request.url,
                head:    encoder.encode(head).buffer,
                body:    body,
            });
        })

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
            console.log("Initializing ");
            boot = event.data.boot;
            auth = false;
            uuid = null;
            /* Send CLIENT_INIT_ACK to client */
            event.source.postMessage(
                { type: 'CLIENT_INIT_ACK' });
            return;

        case 'CLIENT_IDENT':
            /* start forwarding requests for this client */
            console.log("Hello", 
                event.data.uuid);
            uuid  = event.data.uuid;
            auth  = true;
            return;

        case 'CLIENT_RESPONSE':
            const resolve = pending.get(event.data.id);
            if (resolve) {
                pending.delete(
                    event.data.id);
                resolve(event.data);
            }
        break;
    }
});