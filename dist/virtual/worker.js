const encoder = new TextEncoder("utf-8");
const pending = new Map();

let uuid    = null;
let boot    = null;
let channel = null;

function initializeChannel() {
    channel = new BroadcastChannel(
        `em-browser:${uuid}`);
    channel.addEventListener('message', async (event) => {
        if (event.data.type === 'CLIENT_RESPONSE') {
            const resolve = pending.get(event.data.id);
            if (resolve) {
                pending.delete(
                    event.data.id);
                resolve(event.data);
            } else {
                console.warn(
                    `[worker:${uuid}] Nothing Pending for ${event.data.id}`);
            }
        }
    });
    return channel;
}

class State {
    static IDB_DB_NAME    = "em-worker-state";
    static IDB_DB_VERSION = 1;
    static IDB_ST_NAME    = "state";
    static IDB_ST_KEY     = "saved";

    static async open() {
        return new Promise((resolve, reject) => {
            const request =
                indexedDB.open(
                    State.IDB_DB_NAME, State.IDB_DB_VERSION);

            request.onerror = () => reject(request.error);
            request.onsuccess = () => resolve(request.result);
            
            request.onupgradeneeded = (event) => {
                const db = event.target.result;
                if (!db.objectStoreNames.contains(State.IDB_ST_NAME)) {
                    db.createObjectStore(State.IDB_ST_NAME);
                }
            };
        });
    }

    static async save(db, stateParams) {
        try {
            const transaction =
                db.transaction(
                    [State.IDB_ST_NAME], 'readwrite');
            const store =
                transaction.objectStore(State.IDB_ST_NAME);
            
            const stateData = {
                ...stateParams,
                timestamp: Date.now()
            };

            return new Promise((resolve, reject) => {
                const request = store.put(stateData, State.IDB_ST_KEY);
                request.onerror = () => reject(request.error);
                request.onsuccess = () => {
                    db.close();
                    resolve(true);
                };
            });
        } catch (error) {
            console.error('[worker] Failed to persist state:', error);
            return false;
        }
    }

    static async load(db) {
        try {
            const transaction =
                db.transaction(
                    [State.IDB_ST_NAME], 'readonly');
            const store =
                transaction.objectStore(State.IDB_ST_NAME);
            
            return new Promise((resolve, reject) => {
                const request = store.get(State.IDB_ST_KEY);
                request.onerror = () => reject(request.error);
                request.onsuccess = () => {
                    db.close();
                    resolve(request.result || null);
                };
            });
        } catch (error) {
            console.error('[worker] Failed to load state:', error);
            return null;
        }
    }
}

async function handleResponse(promise, request, cache) {
    const result = await promise;
    const response = new Response(result.response.body, { 
        status:     result.response.status.code,
        statusText: result.response.status.text,
        headers: new Headers(result.response.headers)
    });

    if (request.method === "GET") {
        await cache.put(
            request, response.clone());
    }

    return response;
}

async function handleRequest(channel, request) {
    const cache  = await caches.open(`em-cache:${uuid}`);
    const cached = await cache.match(request);

    if (cached) {
        const expires = cached.headers.get('Expires');
        if (expires) {
            const expiresDate = new Date(expires);
            if (Date.now() < expiresDate.getTime()) {
                return cached;
            }
        }

        const condition = cached.headers.get('Last-Modified');
        if (condition) {
            return handleRequestConditional(
                condition, channel, request, cache, cached);
        }
    }

    return handleRequestUnconditional(channel, request, cache);
}

async function handleRequestConditional(condition, channel, request, cache, cached) {
    const id = crypto.randomUUID();
    const promise = new Promise(
        resolve => pending.set(id, resolve));

    let head = 
        `${request.method} ${request.url}\r\n` +
        `If-Modified-Since: ${condition}\r\n`;

    if (request.headers) {
        for (const [key, value] of request.headers.entries()) {
            if (key.toLowerCase() !== 'if-modified-since') {
                head += `${key}: ${value}\r\n`;
            }
        }
    }
    head += '\r\n';

    channel.postMessage({
        type: 'CLIENT_REQUEST',
        id: id,
        method: request.method,
        url: request.url,
        head: encoder.encode(head),
        body: new Uint8Array(
            await request.arrayBuffer()),
    });

    const result = await promise;

    if (result.response.status.code === 304) {
        return cached;
    }

    return handleResponse(promise, request, cache);
}

async function handleRequestUnconditional(channel, request, cache) {
    const id = crypto.randomUUID();
    const promise = new Promise(
        resolve => pending.set(id, resolve));

    let head = `${request.method} ${request.url}\r\n`;
    if (request.headers) {
        for (const [key, value] of request.headers.entries()) {
            head += `${key}: ${value}\r\n`;
        }
    }
    head += '\r\n';

    channel.postMessage({
        type: 'CLIENT_REQUEST',
        id: id,
        method: request.method,
        url: request.url,
        head: encoder.encode(head),
        body: new Uint8Array(
            await request.arrayBuffer()),
    });

    return handleResponse(promise, request, cache);
}

async function handleFetch(event) {
    const url = new URL(
        event.request.url,
        self.location.url);

    if (url.origin != self.location.origin) {
        return;
    }

    event.respondWith((async () => {
        if (!channel) {
            const db = await State.open();
            const state =
                await State.load(db);
            if (state) {
                uuid = state.uuid;
                boot = state.boot;
            }

            if (!uuid && !boot) {
                console.warn(
                    `[worker] Nobody Listening`);
                return fetch(event.request);
            } else {
                channel = initializeChannel();
                console.warn(
                    `[worker:${uuid}] Channel Restored`,
                    channel);
            }
        }

        if (url.pathname.endsWith(boot)) {
            console.warn(
                `[worker:${uuid}] Boot Pass`);
            return fetch(event.request);
        }

        return handleRequest(channel, event.request);
    })());
}

async function handleMessage(event) {
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

            const db = await State.open();
            State.save(db, {
                boot: event.data.boot,
                uuid: event.data.uuid
            });

            boot    = event.data.boot;
            uuid    = event.data.uuid;
            console.log(
                `[worker:${uuid}] Initializing`);
            channel = initializeChannel();
            console.log(
                `[worker:${uuid}] Acknowledging`);
            channel.postMessage({ 
                type: 'CLIENT_INIT_ACK',
                uuid: uuid
            });
            return;
    }
}

self.addEventListener('fetch',   handleFetch);
self.addEventListener('message', handleMessage);