const pending = new Map();
let   ident   = null;
let   uuid    = null;
let   auth    = false;

async function resolveResponse(promise, event) {
    const result = await promise;
    return new Response(result.response.body, { 
        status:     result.response.status.code,
        statusText: result.response.status.text,
        headers: new Headers(result.response.headers)
    });
}

self.addEventListener('fetch', event => {    
    const url = new URL(event.request.url, self.location.url);

    if (url.origin != self.location.origin) {
        return;
    }

    event.respondWith((async () => {
        const id = Math.random().toString();
        const promise = new Promise(resolve => pending.set(id, resolve));
        const clients = await self.clients.matchAll({ includeUncontrolled: true });
        const browser = clients.find(client => {
            return client.id === ident;
        });

        const body = await event.request.arrayBuffer();

        if (!browser) {
            clients.forEach(client => {
                client.postMessage({
                    type: 'CLIENT_AUTH',
                    queued: {
                        id: id,
                        method:  event.request.method,
                        url:     event.request.url,
                        headers: event.request.headers ? 
                            Object.fromEntries(
                                event.request.headers.entries()) : {},
                        body: body,
                    }
                });
            });

            return resolveResponse(promise, event);
        }

        browser.postMessage({
            type: 'dispatch-request',
            id: id,
            method: event.request.method,
            url:    event.request.url,
            headers: event.request.headers ? 
                Object.fromEntries(
                    event.request.headers.entries()) : {},
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

        case 'CLIENT_IDENT':
            if (auth && uuid != event.data.uuid) {
                return;
            }

            ident = event.source.id;
            uuid  = event.data.uuid;
            auth  = true;
            return;

        case 'dispatch-response':
            const resolve = pending.get(event.data.id);
            if (resolve) {
                pending.delete(
                    event.data.id);
                resolve(event.data);
            }
        break;
    }
});