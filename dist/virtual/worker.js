const encoder = new TextEncoder("utf-8");
const pending = new Map();

let boot = null;
let channel = null;
let uuid = null;

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

    if (url.origin != self.location.origin) {
        return;
    }

    if (!channel) {
        console.warn(
            `[worker] Nobody Listening`);
        return;
    }

    if (url.pathname.endsWith(boot)) {
        console.warn(
            `[worker:${uuid}] Boot ` +
                `${url.searchParams.has('ping') ? 'Ping' : 'Pass'}`);
        return;
    }

    event.respondWith((async () => {
        const id = crypto.randomUUID();
        const promise = new Promise(
            resolve => pending.set(id, resolve));

        console.log(
            `[worker:${uuid}] Intercepting ${event.request.url}`);

        let resourceUrl =
            resolveResource(event.request.url);
        let head = `${event.request.method} ${resourceUrl}\r\n`;
        if (event.request.headers) {
            for (const [key, value] of event.request.headers.entries()) {
                head += `${key}: ${value}\r\n`;
            }
        }
        head += '\r\n';

        const body = await event.request.arrayBuffer();

        channel.postMessage({
            type:    'CLIENT_REQUEST',
            id:      id,
            method:  event.request.method,
            url:     event.request.url,
            head:    encoder.encode(head),
            body:    new Uint8Array(body),
        });

        return resolveResponse(promise, event);
    })());
});

self.addEventListener('message', async (event) => {    
    switch (event.data.type) {
        case 'SKIP_WAITING':
            self.skipWaiting();
            return;

        case 'CLIENTS_CLAIM':
            self.clients.claim();
            return;

        case 'CLIENT_INIT':
            if (channel !== null &&
                channel.onmessage !== null) {
                /* nothing to do */
                return;
            }

            boot = event.data.boot;
            uuid = event.data.uuid;
            console.log(
                `[worker:${uuid}] Initializing`);
            channel = new BroadcastChannel(
                `em-browser:${uuid}`);
            channel.addEventListener('message', async (event) => {
                if (event.data.type === 'CLIENT_RESPONSE') {
                    const resolve = pending.get(event.data.id);
                    if (resolve) {
                        console.log(
                            `[worker:${uuid}] Responding ${event.data.url}`,
                        event.data);
                        pending.delete(
                            event.data.id);
                        resolve(event.data);
                    } else {
                        console.warn(
                            `[worker:${uuid}] Nothing Pending for ${event.data.id}`);
                    }
                } else if (event.data.type === 'CLIENT_PING') {
                    channel.postMessage({
                        type: 'CLIENT_PONG',
                        uuid: uuid
                    });
                }
            });
            console.log(
                `[worker:${uuid}] Acknowledging`);
            channel.postMessage({ 
                type: 'CLIENT_INIT_ACK',
                uuid: uuid
            });
            return;
    }
});