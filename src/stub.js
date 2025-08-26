/*
  +----------------------------------------------------------------------+
  | em                                                                   |
  +----------------------------------------------------------------------+
  | Copyright (c) Joe Watkins 2025                                       |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: krakjoe                                                      |
  +----------------------------------------------------------------------+
 */

/**
 * Shall be true when MINIT yields SUCCESS
 */
Module.ready = false;

/**
 * Shall provide encoding services
 */
Module.encoding = {
    // For UTF-8 text (interface, source code, etc.)
    utf8: {
        encoder: new TextEncoder("utf-8"),
        decoder: new TextDecoder("utf-8"),

        in:  (str)   => Module.encoding.utf8.encoder.encode(str),
        out: (bytes) => Module.encoding.utf8.decoder.decode(bytes),
    },
    // For raw bytes/Latin-1, will delegate on encountering unicode to utf-8
    latin1: {
        in: function(str) {
            const bytes = new Uint8Array(str.length);
            for (let i = 0; i < str.length; i++) {
                if (str.charCodeAt(i) > 0xFF) {
                    return Module.encoding.utf8.in(str);
                }

                bytes[i] = str.charCodeAt(i) & 0xFF;
            }
            return bytes;
        },
        out: function(bytes) {
            let result = '';
            for (let i = 0; i < bytes.length;) {
                const b = bytes[i];
                // Check for valid UTF-8 multi-byte sequence
                if (b >= 0xC2 && b <= 0xDF && i + 1 < bytes.length &&
                    bytes[i+1] >= 0x80 && bytes[i+1] <= 0xBF) {
                    return Module.encoding.utf8.out(bytes);
                } else if (b >= 0xE0 && b <= 0xEF && i + 2 < bytes.length &&
                    bytes[i+1] >= 0x80 && bytes[i+1] <= 0xBF &&
                    bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF) {
                    return Module.encoding.utf8.out(bytes);
                } else if (b >= 0xF0 && b <= 0xF4 && i + 3 < bytes.length &&
                    bytes[i+1] >= 0x80 && bytes[i+1] <= 0xBF &&
                    bytes[i+2] >= 0x80 && bytes[i+2] <= 0xBF &&
                    bytes[i+3] >= 0x80 && bytes[i+3] <= 0xBF) {
                    return Module.encoding.utf8.out(bytes);
                }
                result += String.fromCharCode(b);
                i++;
            }
            return result;
        }
    }
};

/**
 * Shall be true when executing under nodejs
 */
Module.node = typeof process !== 'undefined' &&
                     process.versions &&
                     process.versions.node;

/**
 * Shall be true when executing under electron
 */
Module.electron =
    (typeof window !== 'undefined') &&
    (typeof window.navigator !== 'undefined') &&
        window.navigator.
            userAgent.includes('Electron');

/**
 * Shall store in flight requests
 */
Module.http = new Map();

/**
 * Shall startup (MINIT) em
 * @returns bool
 */
Module.startup = function() {
    return Module.ready = 
        !Module.ccall(
            'em_startup', 
            'number');
};

/**
 * Shall return a response object
 * @param {number} address 
 * @param {number} length 
 * @returns {Object}
 */
Module.response = function(address, length) {
    // First, copy all the raw data from heap before it gets freed
    const heap = new Uint8Array(
        Module.HEAPU8.buffer,
        address, length
    ).slice();

    // Convert to text for header parsing
    const text = Module.encoding.latin1.out(heap);

    let statusCode = 200;
    let statusText = 'OK';
    let headers = {};
    let headersEndOffset = 0;

    const lines = text.split('\r\n');
    // Parse status line
    const statusMatch = lines[0].match(
        /^HTTP\/[\d.]+\s+(\d{3})(?:\s+(.*))?$/i);
    if (statusMatch) {
        statusCode = parseInt(
            statusMatch[1], 10);
        statusText = statusMatch[2].trim();
    } else {
        console.error("Response is malformed, cannot continue", text)
        throw new Error(
            "Response is malformed, cannot continue, see console");
    }

    let headerEndIndex = -1;
    // Parse headers and find where body starts
    for (let i = 1; i < lines.length; i++) {
        if (lines[i] === '') {
            headerEndIndex = i;
            // Calculate byte offset where body starts
            const headerText = lines.slice(0, i + 1).join('\r\n');
            headersEndOffset = Module.encoding.latin1
                .in(headerText).length;
            break;
        }

        const colonIndex = lines[i].indexOf(':');
        if (colonIndex > 0) {
            const key = lines[i]
                .slice(0, colonIndex).trim();
            const value = lines[i]
                .slice(colonIndex + 1).trim();
            headers[key] = value;
        }
    }

    return {
        status: {
            code: statusCode,
            text: statusText,
        },
        headers: headers,
        body:     heap.slice(
            headersEndOffset + // end of headers \r\n 
            2)                 // terminating \r\n
    };
}

/**
 * Shall update the execution environment
 * @param {object} environment 
 * @returns 
 */
Module.environ = function(environment) {
    const json =
        JSON.stringify(environment);

    const buffer =
        Module.encoding.latin1.in(json);
    let heap = Module._malloc(
        buffer.byteLength + 1);
    Module.HEAPU8.set(buffer, heap);
    Module.HEAPU8[heap + buffer.byteLength] = 0;

    let result = Module.ccall(
        "em_env_import", "number", [
            "number", "number"
        ],[
            heap,
            buffer.byteLength
    ]);

    Module._free(heap);
    return result;
};

/**
 * Shall dispatch the given request
 * @param {Uint8Array} env
 * @param {Uint8Array} head
 * @param {Uint8Array} body
 * @returns {*}
 */
Module.dispatch = async function(env, head, body) {
    Module.dispatchEvent(new CustomEvent('dispatch.begin', { 
        detail: {
            "env":   env,
            "head":  head,
            "body":  body,
        }
    }));

    let request = {
        env:  Module._malloc(env.byteLength +  1),
        head: Module._malloc(head.byteLength + 1),
        body: Module._malloc(body.byteLength + 1)
    };

    Module.HEAPU8.set(env,  request.env);
    Module.HEAPU8.set(head, request.head);
    Module.HEAPU8.set(body, request.body);

    Module.HEAPU8[request.env  + env.byteLength]  = 0;
    Module.HEAPU8[request.head + head.byteLength] = 0;
    Module.HEAPU8[request.body + body.byteLength] = 0;

    let context = null;

    try {
        context = await Module.ccall(
            'em_run_request',
            'number',
            [   'number','number',    /* const char* env,  size_t elen */
                'number', 'number',   /* const char* head, size_t hlen */
                'number', 'number'    /* const char* body, size_t blen */
            ], [ 
                request.env,  env.byteLength,
                request.head, head.byteLength,
                request.body, body.byteLength,
            ], { async: true });
    } finally {
        Module._free(request.env);
        Module._free(request.head);
        Module._free(request.body);
    }

    // check for errors
    if (context < 0) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('dispatch.error', { 
            detail: {
                "env":   env,
                "head":  head,
                "body":  body,
            }
        }));

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, dispatch failed");
    }

    let result = {
        address: Module.ccall(
            'em_run_result', 'number',
                [ 'number' ], [ context ]),
        length: Module.ccall(
            'em_run_length', 'number',
            [ 'number' ], [ context ])
    };

    // ensure there's stuff to read on the heap
    if (!result.address || !result.length) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('dispatch.error', { 
            detail: {
                "env":   env,
                "head":  head,
                "body":  body,
            }
        }));

        // release the context that em allocated for this request
        Module.ccall('em_run_free',
            'void', [ 'number' ], [ context ]);

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, no output");
    }

    let response = null;

    try {
        // This ensures consistent encoding handling
        response = Module.response(result.address, result.length);
    } catch (exception) {
        // Fire exception event
        Module.dispatchEvent(new CustomEvent('dispatch.exception', { 
            detail: {
                "env":       env,
                "head":      head,
                "body":      body,
                "exception": exception,
            }
        }));

        throw exception;
    } finally {
        // release the context that em allocated for this request
        Module.ccall('em_run_free',
            'void', [ 'number' ], [ context ]);
    }

   // Fire end event
    Module.dispatchEvent(new CustomEvent('dispatch.end', { 
        detail: {
            "env":      env,
            "head":     head,
            "body":     body,
            "response": response,
        }
    }));

    return response;
}

/**
 * Shall include a file from the vfs
 * @param {string} script 
 * @param {HTMLElement|Function|undefined} output
 * @returns string
 */
Module.include = async function(script, output) {
    // Fire start event
    Module.dispatchEvent(new CustomEvent('include.begin', { 
        detail: { 
            "script": script,
            "output": output 
        }
    }));

    let context = await Module.ccall(
        'em_run_script',
        'number',
        [ 'string' ],
        [  script  ], { async: true });

    // check for errors
    if (!context) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('include.error', { 
            detail: { 
                "script": script,
                "output": output }
        }));

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, include failed");
    }

    // the code ran, find the address and length of the result
    let result = {
        address: Module.ccall(
            'em_run_result', 'number',
                [ 'number' ], [ context ]),
        length: Module.ccall(
            'em_run_length', 'number',
            [ 'number' ], [ context ])
    };

    // ensure there's stuff to read on the heap
    if (!result.address || !result.length) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('include.error', { 
            detail: { 
                "script": script,
                "output": output,
                "result": result }
        }));

        // release the context that em allocated for this request
        Module.ccall('em_run_free',
            'void', [ 'number' ], [ context ]);

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, no output");
    }

    let text = null;

    try {
        text = Module.encoding.latin1.out(
            new Uint8Array(Module.HEAPU8.buffer,
                result.address, result.length)
        );
    } catch (exception) {
        // Fire exception event
        Module.dispatchEvent(new CustomEvent('include.exception', { 
            detail: { 
                "script": script,
                "output": output,
                "result": result,
                "exception": exception }
        }));

        throw exception;
    } finally {
        // release the buffer that em alloc'd
        Module.ccall('em_run_free');
    }

   // Fire end event
    Module.dispatchEvent(new CustomEvent('include.end', { 
        detail: { 
            "script":  script,
            "output": output,
            "text":   text }
    }));

    if (typeof output === 'undefined') {
        return text;
    } else if (typeof HTMLElement !== 'undefined' &&
        output instanceof HTMLElement) {
        return output.textContent = text;
    } else if (typeof output === 'function') {
        return output(text);
    }

    throw new TypeError(
        "Unexpected output type, " +
        "expected HTMLElement|Function|undefined");   
}

/**
 * Shall invoke code
 * 
 * Where input is HTMLTextAreaElement:
 *  input will be taken from input.value
 * Where input is Function:
 *  input will be invoked thus: function()
 * 
 * Note: input Function should return an object with value and length fields
 * 
 * Where output is HTMLElement:
 *  text will be inserted into output.textContent, and returned
 * Where output is Function:
 *  output will be invoked thus: output(text), and the result returned
 * Where output is undefined:
 *  text will be returned
 * 
 * @param {(HTMLTextAreaElement|Function|string)} input 
 * @param {(HTMLElement|Function|undefined)} output 
 * @returns string
 */
Module.invoke = async function(input, output = undefined) {
    let code = null;

    if (typeof HTMLTextAreaElement !== 'undefined' &&
        input instanceof HTMLTextAreaElement) {
        code = {
            value:  input.value,
            length: Module.lengthBytesUTF8(input.value),
        };
    } else if (typeof input === 'function') {
        code = input();

        if (typeof code !== 'object') {
            throw new TypeError(
                "Unexpected result from input(), " +
                "expected an object");
        }

        if (typeof code.value  !== 'string' ||
            typeof code.length !== 'number') {
            throw new TypeError(
                "Unexpected result from input(), " +
                "expected {string value, number length}");
        }
    } else if (typeof input == 'string') {
        code = {
            value:  input,
            length: Module.lengthBytesUTF8(input),
        };
    } else {
        throw new TypeError(
            "Unexpected input " +
            "expected HTMLTextAreaElement|Function|string");
    }

    if (!code.value || !code.length) {
        throw new Error("Unexpected input, zero length");
    }

    // Fire start event
    Module.dispatchEvent(new CustomEvent('invoke.begin', { 
        detail: { 
            "input": input,
            "output": output 
        } 
    }));

    let context = await Module.ccall(
        'em_run_string',
        'number',
        ['string', 'number'],
        [ code.value, code.length ], { async: true });

    // check for errors
    if (context < 0) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('invoke.error', { 
            detail: { 
                "input": input,
                "output": output,
                "result": result }
        }));

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, execution failed");
    }

    // the code ran, find the address and length of the result
    let result = {
        address: Module.ccall(
            'em_run_result', 'number',
                [ 'number' ], [ context ]),
        length: Module.ccall(
            'em_run_length', 'number',
            [ 'number' ], [ context ])
    };

    // ensure there's stuff to read on the heap
    if (!result.address || !result.length) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('invoke.error', { 
            detail: { 
                "input": input,
                "output": output,
                "result": result }
        }));

        // release the context that em allocated for this request
        Module.ccall('em_run_free',
            'void', [ 'number' ], [ context ]);

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, no output");
    }

    let text = null;

    try {
        text = Module.encoding.latin1.out(
            new Uint8Array(Module.HEAPU8.buffer,
                result.address, result.length)
        );
    } catch (exception) {
        // Fire exception event
        Module.dispatchEvent(new CustomEvent('invoke.exception', { 
            detail: { 
                "input": input,
                "output": output,
                "result": result,
                "exception": exception }
        }));

        throw exception;
    } finally {
        // release the context that em allocated for this request
        Module.ccall('em_run_free',
            'void', [ 'number' ], [ context ]);
    }

   // Fire end event
    Module.dispatchEvent(new CustomEvent('invoke.end', { 
        detail: { 
            "input":  input,
            "output": output,
            "text":   text }
    }));

    if (typeof output === 'undefined') {
        return text;
    } else if (typeof HTMLElement !== 'undefined' &&
        output instanceof HTMLElement) {
        return output.textContent = text;
    } else if (typeof output === 'function') {
        return output(text);
    }

    throw new TypeError(
        "Unexpected output type, " +
        "expected HTMLElement|Function|undefined");
};

/**
 * Shall shutdown (MSHUTDOWN) em
 * @returns void
 */
Module.shutdown = function() {
    Module.ccall(
        'em_shutdown');
};

/**
 * Shall provide vfs management
 */
Module.vfs = {
    /**
     * Type constants
     */
    EM_VFS_INV:  0,
    EM_VFS_DIR:  1,
    EM_VFS_FILE: 2,

    /**
     * Shall write file contents to the filesystem
     * @param {string} path 
     * @param {Uint8Array} contents 
     * @returns
     * Shall fire vfs.modified
     */
    put: function(path, contents) {
        if (!(contents instanceof Uint8Array)) {
            throw new TypeError("contents must be a Uint8Array");
        }

        let length = contents.length;
        let address = Module._malloc(length);
        Module.HEAPU8.set(contents, address);
        let result = Module.ccall('em_vfs_put', 'bool', [
            'string',
            'number', 'number'], [
            path,
            address, length
        ]);
        Module._free(address);

        Module.dispatchEvent(new CustomEvent('vfs.modified', { 
            detail: {
                "method":   "put",
                "paths":    [path] }
        }));

        return result;
    },
    /**
     * Shall retrieve file contents from the filesystem
     * @param {string} path 
     * @returns UInt8Array|bool
     */
    get: function(path) {
        let length = Module.ccall(
            'em_vfs_get_length', 'number', [
            'string' ], [
            path
        ]);

        if (length < 0) {
            return false;
        }

        let address = Module.ccall(
            'em_vfs_get_address', 'number', [
            'string' ], [
            path
        ]);

        let result = new Uint8Array(length);
        result.set(
            Module.HEAPU8.subarray(
                address, address + length));
        return result;
    },

    /**
     * Shall unlink the given path
     * @param {string} path 
     * @param {bool} directories 
     * @returns bool
     * Shall return false if directories if false and path is a directory
     * Shall fire vfs.modified on success
     */
    unlink: function(path, directories = false) {
        const result = Module.ccall('em_vfs_unlink', 'bool',
            ['string', 'bool'],
            [ path, directories ]);
        if (result) {
            Module.dispatchEvent(new CustomEvent('vfs.modified', { 
                detail: { 
                    "method": "unlink",
                    "paths":  [path] }
            }));
        }
        return result;
    },

    /**
     * Shall move the entry
     * @param {string} from 
     * @param {string} to 
     * @returns bool
     *  Shall fire vfs.modified on success
     */
    move: function(from, to) {
        const result = Module.ccall('em_vfs_move', 'bool',
            [ 'string', 'string' ],
            [ from, to ]);
        if (result) {
            Module.dispatchEvent(new CustomEvent('vfs.modified', { 
                detail: { 
                    "method": "move",
                    "paths":  [from, to] }
            }));
        }
        return result;
    },

    /**
     * Shall create a directory at the given path
     * @param {string} path
     * @returns bool 
     */
    mkdir: function(path) {
        const result = Module.ccall('em_vfs_mkdir', 'bool',
            [ 'string' ],
            [ path ]);
        if (result) {
            Module.dispatchEvent(new CustomEvent('vfs.modified', { 
                detail: { 
                    "method": "mkdir",
                    "paths":  [path] }
            }));
        }
        return result;
    },

    /**
     * Shall destroy the vfs and recreate it
     */
    reset: function() {
        Module.ccall('em_vfs_reset');
        Module.dispatchEvent(new CustomEvent('vfs.modified', { 
            detail: { 
                "method": "reset",
                "paths":  [] }
        }));
    },

    /**
     * Shall return an iterator object for path
     * @param {string} path 
     * @returns Iterator
     */
    iterate: function(path) {
        return new this.Iterator(
            path.endsWith("/") ?
                path : path + "/")
    },

    /**
     * Shall provide iteration for the VFS
     */
    Iterator: class {
        /**
         * Construct an Iterator for path
         * @param {string} path 
         */
        constructor(path) {
            this.iterator = Module.ccall(
                'em_vfs_iterator', 'number', 
                [ 'string' ],
                [ path ]);
            if (!this.iterator) {
                throw new Error(
                    "could not start iterator for " + path);
            }
            this.path = path;
        }

        /**
         * Shall count the number of items in this iterator
         * @returns number
         */
        count() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            return Module.ccall(
                'em_vfs_iterator_count', 'number',
                [ 'number' ],
                [ this.iterator ]
            );
        }

        /**
         * Shall determine the kind of node current being visited
         * @returns EM_VFS_INV|EM_VFS_DIR|EM_VFS_FILE
         */
        kind() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            let kind = Module.ccall(
                'em_vfs_iterator_kind', 'number',
                [ 'number' ],
                [ this.iterator ]
            );
            if (kind == Module.vfs.EM_VFS_INV) {
                throw new Error("invalid call");
            }
            return kind;
        }

        /**
         * Shall return the name of the node currently being visited
         * @returns string
         */
        name() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            let address = Module.ccall(
                'em_vfs_iterator_name', 'number',
                [ 'number' ],
                [ this.iterator ]
            );
            if (address < 0) {
                throw new Error("invalid call")
            }
            return Module.UTF8ToString(address);
        }

        /**
         * Shall return the address of the content of the node currently being visited
         * @returns number
         */
        address() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            let address = Module.ccall(
                'em_vfs_iterator_address', 'number',
                [ 'number' ],
                [ this.iterator ]
            );
            if (address < 0) {
                throw new Error("invalid call")
            }
            return address;
        }

        /**
         * Shall return the length of the content of the node currently being visited
         * @returns number
         */
        length() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            let length = Module.ccall(
                'em_vfs_iterator_length', 'number',
                [ 'number' ],
                [ this.iterator ]
            );
            if (length < 0) {
                throw new Error("invalid call");
            }
            return length;
        }

        /**
         * Shall return the created timestamp for the node currently being visited
         * @returns Date
         */
        created() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            let time = Module.ccall(
                'em_vfs_iterator_created', 'bigint',
                [ 'number' ],
                [ this.iterator ]
            );
            if (time < 0) {
                throw new Error("invalid call")
            }
            return new Date(new Number(time) * 1000);
        }

        /**
         * Shall return the modified timestamp for the node currently being visited
         * @returns Date
         * Directories do not have modified timestamps
         */
        modified() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }

            if (this.kind() != Module.vfs.EM_VFS_FILE) {
                throw new Error("invalid call");
            }

            let time = Module.ccall(
                'em_vfs_iterator_modified', 'bigint',
                [ 'number' ],
                [ this.iterator ]
            );
            if (time < 0) {
                throw new Error("invalid call")
            }
            return new Date(new Number(time) * 1000);
        }

        /**
         * Shall reset the iterator to the root of the current directory
         * @returns bool
         */
        reset() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            return Module.ccall(
                'em_vfs_iterator_reset', 'bool',
                [ 'number' ],
                [ this.iterator ]
            );
        }

        /**
         * Shall move the iterator forward
         * @returns bool
         */
        next() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            return Module.ccall(
                'em_vfs_iterator_next', 'bool',
                [ 'number' ],
                [ this.iterator ]
            );
        }

        /**
         * Shall iterate through the current directory with optional recursion
         * and return a dictionary for each entry:
         * 
         * { 
         *  name: name,
         *  kind: kind,
         *  created: created,
         *  modified: modified, # files only
         *  address: address, #files only
         *  length: length, #files only
         *  children: children, #directories only depends on recursive parameter 
         * }
         * 
         * @param {bool} recursive 
         * @returns object
         */
        all(recursive) {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }

            let tree = [];

            if (!this.reset()) {
                return tree;
            }

            do {
                let children = [];

                if (recursive &&
                    this.kind() == Module.vfs.EM_VFS_DIR) {
                    let path = 
                        this.path.endsWith("/") ?
                            this.path + this.name() :
                            this.path + "/" + this.name();
                    children = Module.vfs
                        .iterate(path + "/")
                            .all(true);
                }

                try {
                    tree.push({
                        name:     this.name(),
                        kind:     this.kind(),
                        created:  this.created(),
                        ...(this.kind() == Module.vfs.EM_VFS_FILE ? {
                            modified: this.modified(),
                            address:  this.address(),
                            length:   this.length()
                        } : {}),
                        ...(children.length ? { 
                            children: children 
                        } : {}),
                    });
                } finally {
                    if (children instanceof Module.vfs.Iterator) {
                        children.free();
                    }
                }
            } while (this.next());

            this.reset();

            return tree;
        }

        /**
         * Shall free the iterator
         * !important!
         */
        free() {
            if (!this.iterator) {
                throw new Error("invalid iterator");
            }
            Module.ccall(
                'em_vfs_iterator_free',
                'number',
                [ 'number' ],
                [ this.iterator ]);
            this.iterator = null;
        }
    },

    /*
    * Shall abstract the vfs into/from a stream
    */
    Memory: class {
        address = null;

        constructor(path = '/') {
            this.path = path;
        }

        async load() {
            if (!(this.path instanceof Uint8Array)) {
                this.address = await Module.ccall(
                    'em_vfs_memory_alloc',
                    'number',
                    [ 'string' ],
                    [ this.path ], { async: true });
                if (!this.address) {
                    throw new Error(
                        `Failed to stream vfs at ${path}`)
                };
            } else {
                this.address =
                    Module._malloc(this.path.length);
                Module.HEAPU8.set(
                    this.path, this.address);
            }
            return this.parse();
        }

        async parse() {
            this.header  = {
                magic:   Module.encoding.latin1.out(  /* char[6] */
                    new Uint8Array(Module.HEAPU8.buffer, this.address, 6)
                ),
                version: Module.encoding.latin1.out(  /* char[6] */
                    new Uint8Array(Module.HEAPU8.buffer, this.address+6, 6)
                ),
                size: {
                    header:  this.int32(this.address +  12),       /* uint32_t */
                    length:  this.int32(this.address +  16),       /* uint32_t */
                    records: this.int32(this.address +  20),       /* uint32_t */
                    consumed: this.int32(this.address + 24)        /* uint32_t */ 
                },
                offsets: [],
            };

            let offsets = this.address + 28; /* end of header */

            for (let record = 0;
                     record < this.header.size.records;
                     record++){
                this.header.offsets.push(
                    this.int32(
                        (offsets) + record * 4));
            }

            if (this.header.magic !== "EMFS1\0") {
                console.log(this);
                throw new Error(
                    "Invalid Call, disk is not magic");
            }
        }

        int16(address) {
            const view = new DataView(
                Module.HEAPU8.buffer, address, 2);
            return view.getUint16(0, true);
        }

        int32(address) {
            const view = new DataView(
                Module.HEAPU8.buffer, address, 4);
            return view.getUint32(0, true);
        }

        int64(address) {
            const view = new DataView(
                Module.HEAPU8.buffer, address, 8);
            return Number(view.getBigUint64(0, true));
        }

        free() {
            if (!this.address) {
                throw new Error(
                    "Invalid call, memory already freed");
            }

            Module.ccall(
                'em_vfs_memory_free',
                'number',
                [ 'number' ],
                [ this.address ]
            );

            this.address = 0;
        }
    },
    
    /**
     * Shall abstract a stream entry
     */
    Entry: class {
        static EM_VFS_MEMORY_VERBATIM   = 0;
        static EM_VFS_MEMORY_COMPRESSED = 1;

        constructor(memory, index) {
            // memory: Module.vfs.Memory instance
            // index: entry index in header.offsets

            this.memory = memory;
            this.index = index;

            // Calculate base address of entries region
            const entriesBase =
                memory.address +
                28 + // header size
                memory.header.size.header;

            // Offset of this entry relative to entriesBase
            const offset = memory.header.offsets[index];

            // Absolute address of this entry
            this.address = entriesBase + offset;

            // Parse fields
            let parsing = this.address;

            this.kind = Module.HEAPU8[parsing++];       // uint8
            this.flags = Module.HEAPU8[parsing++];      // uint8
            this.reserved = this.memory.int16(parsing); // uint16
            
            parsing += 2;

            this.size = {
                entry:  this.memory.int32(parsing),
                name:   this.memory.int32(parsing+4),
                data:   {
                    verbatim:   this.memory.int32(parsing+8),
                    compressed: this.memory.int32(parsing+12)
                }
            };
            parsing += 16;

            this.stat = {
                ctime:  this.memory.int64(parsing),
                mtime:  this.memory.int64(parsing+8)
            };
            parsing += 20; // 16 + 4 byte padding

            // Name (null-terminated, size.name bytes)
            this.name = Module.encoding.latin1.out(
                new Uint8Array(
                    Module.HEAPU8.buffer, parsing, this.size.name-1)
            );

            parsing += this.size.name;

            // Data, may be compressed
            this.data = null;
            if (this.kind === Module.vfs.EM_VFS_FILE && this.size.data.verbatim > 0) {
                this.data = Module.HEAPU8.slice(
                    parsing,
                    parsing +
                        ((this.flags & Module.vfs.Entry.EM_VFS_MEMORY_COMPRESSED) ?
                            this.size.data.compressed :
                            this.size.data.verbatim)
                );
            }
        }
    },

    /**
     * Shall abstract the reading of memory
     */
    Reader: class {
        constructor(memory) {
            this.memory = memory;
        }

        /**
         * Shall read from offset for records
         * @param {number} offset 
         * @param {number} records 
         * @returns Entry[]
         */
        read(offset = 0, records = 0) {
            let results = [];
            const total = this.memory.header.size.records;
            if (!records) {
                records = total - offset;
            }
            let count = 0;
            while (offset < total && count < records) {
                results.push(
                    new Module.vfs.Entry(
                        this.memory, offset));
                offset++;
                count++;
            }
            return results;
        }

        memory = null;
    },

    /**
     * Shall abstract writing memory to vfs
     */
    Writer : class {
        constructor(memory) {
            this.memory = memory;
        }

        /**
         * Shall write from memory into vfs from offset for records
         * @param {number} offset 
         * @param {number} records 
         * @returns number of records written
         * Note the number of records written may not match the number given
         * it depends which order the directories are created, which is not ideal ...
         */
        async write(offset = 0, records = 0) {
            if (!this.memory.address) {
                throw new Error(
                    "Invalid Call, memory already free");
            }
            const result = await Module.ccall(
                'em_vfs_memory_write',
                'number',
                [ 'number', 'number', 'number' ],
                [ this.memory.address, offset, records ],
                { async: true }
            );
            if (result) {
                Module.dispatchEvent(new CustomEvent('vfs.modified', { 
                    detail: { 
                        "method": "write",
                        "memory": this.memory,
                        "paths":  [] }
                }));
            }
            return result;
        }

        memory = null;
    },

    /**
     * Shall provide persistence for the vfs
     */
    Persistence: class {
        constructor(path = "em.vfsi", tick = 1000) {
            this.path = path;
            this.tick = tick;

            if (!this.hasFilesystem()) {
                return;
            }

            if (Module.electron) {
                this.fs = window.nodeFS;
            } else if (Module.node) {
                this.fs = require("fs");
            } else {
                /* browser middleware only needs
                    to provide a couple of methods */
                const argument = this.path;

                this.fs = {
                    writeFile: async (path, data) => {
                        if (('storage' in navigator &&
                             'getDirectory' in navigator.storage)) {
                            const root = await navigator
                                .storage.getDirectory();
                            const fh = await root.getFileHandle(
                                argument, { create: true });
                            const writable = await fh.createWritable();
                            await writable.write(data);
                            await writable.close();
                        } else {
                            const db = await new Promise((resolve, reject) => {
                                const request = indexedDB.open(argument, 1);

                                request.onerror = () => reject(request.error);
                                request.onsuccess = () => resolve(request.result);

                                request.onupgradeneeded = (event) => {
                                    const db = event.target.result;
                                    
                                    if (!db.objectStoreNames.contains('files')) {
                                        db.createObjectStore('files');
                                    }
                                };
                            });
                            const transaction =
                                db.transaction(['files'], 'readwrite');
                            const store = transaction.objectStore('files');

                            return new Promise((resolve, reject) => {
                                const request =
                                    store.put(data, path);
                                request.onsuccess = () => resolve();
                                request.onerror = () => reject(request.error);
                            });
                        }
                    },
                    readFile: async(path) => {
                        if (('storage' in navigator &&
                            'getDirectory' in navigator.storage)) {
                            const root = await navigator.storage.getDirectory();
                            const fh = await root.getFileHandle(argument);
                            const file = await fh.getFile();
                            return new Uint8Array(
                                await file.arrayBuffer());
                        } else {
                            const db = await new Promise((resolve, reject) => {
                                const request = indexedDB.open(argument, 1);
                                
                                request.onerror = () => reject(request.error);
                                request.onsuccess = () => resolve(request.result);
                                
                                request.onupgradeneeded = (event) => {
                                    const db = event.target.result;
                                    if (!db.objectStoreNames.contains('files')) {
                                        db.createObjectStore('files');
                                    }
                                };
                            });
                            
                            const transaction = db.transaction(['files'], 'readonly');
                            const store = transaction.objectStore('files');
                            
                            return new Promise((resolve, reject) => {
                                const request = store.get(path);
                                request.onsuccess = () => {
                                    if (request.result) {
                                        resolve(request.result);
                                    } else {
                                        reject(new Error(`File not found: ${path}`));
                                    }
                                };
                                request.onerror = () => reject(request.error);
                            });
                        }
                    }
                };
            }
        }

        hasFilesystem() {
            return Module.node || Module.electron ||
                   ('storage' in navigator &&
                        'getDirectory' in navigator.storage) ||
                   ('indexedDB' in window);
        }

        restore = async() => {
            if (!this.hasFilesystem()) {
                return;
            }

            try {
                const data  =
                    await this.fs.readFile(this.path);
                this.memory = new Module.vfs.Memory(data);
                await this.memory.load();
                const writer = 
                        new Module.vfs.Writer(this.memory);
                Module.disableEvent("vfs.modified");
                for (let record = 0;
                         record < this.memory.header.size.records;
                         record++) {
                    requestIdleCallback(async() => {
                        await writer.write(record, 1);
                    });
                }
            } catch (error) {
                console.warn(
                    `Persistence cannot read ${this.path} for restoration`, error);
            } finally {
                try {
                    requestIdleCallback(async() => {
                        Module.enableEvent("vfs.modified");
                        Module.dispatchEvent(new CustomEvent('vfs.modified', { 
                            detail: {
                                "method": "write",
                                "memory": this.memory,
                                "paths":  []
                            }
                        }));
                        this.memory.free();
                        this.memory = null;
                    });
                } catch (e) {}
            }
        }

        enable() {
            if (!this.hasFilesystem()) {
                console.warn(
                    "Persistence is not available in this environment");
                return false;
            }

            Module.addEventListener(
                "vfs.modified",
                this.onModified);
            this.ticking = setTimeout(
                this.onTick, this.tick);
            return true;
        }

        disable() {
            clearTimeout(this.ticking);
            Module.removeEventListener(
                "vfs.modified",
                this.onModified);
            this.ticking = null;
        }

        onModified = (event) => {
            if (event.detail.method === "write") {
                if (this.memory == event.detail.memory) {
                    return;
                }
            }

            this.dirty = true;
        }

        onTick = async () => {
            if (this.dirty) {
                await
                    this.onDirty();
                this.dirty = false;
            }
            this.ticking = setTimeout(this.onTick, this.tick);
        }

        onDirty = async () => {
            try {
                /* stop ticking while we work */
                this.disable();

                this.memory =
                    new Module.vfs.Memory();
                await this.memory.load();

                const buffer = new Uint8Array(
                    Module.HEAPU8.buffer, 
                    this.memory.address, 
                    this.memory.header.size.consumed
                );

                await this.fs.writeFile(this.path, buffer);
            } catch (error) {
                console.error(
                    `Persistence failed to update ${this.path}`,
                    error);
            } finally {
                /** cleanup gracefully **/
                try {
                    if (this.memory) {
                        this.memory.free();
                    }
                } catch (e) {} finally {
                    this.memory = null;
                }

                /* prevent premature re-entry */
                this.dirty = false;

                /* start ticking */
                this.enable();
            }
        }

        path    = null;
        tick    = 0;
        ticking = null;
        memory  = null;
        running = false;
        dirty   = false;
    }
};

/**
 * Shall contain disabled events
 */
Module.disabled = {};

/**
 * Shall disable an event, must be explicitly enabled before it will fire
 * @param {string} type
 */
Module.disableEvent = (type) =>
    Module.disabled[type] = true;
/**
 * Shall disable an event
 * @param {string} type
 */
Module.enableEvent  = (type) =>
    Module.disabled[type] = false;

/**
 * Shall initialize the runtime
 */
Module['onRuntimeInitialized'] = function() {
    Module.startup();

    if (typeof window !== 'undefined') {
        window.addEventListener('unload', function() {
            if (typeof Module !== 'undefined') {
                Module.shutdown();
            }
        });
    }

    if (Module.node) {
        try {
            const EventEmitter = require('events');
            Module.events = new EventEmitter();
            Module.addEventListener =    (type, fn) => {
                if (!type in Module.disabled)
                    Module.disabled[type] = false;
                Module.events.on(type, fn);
            };
            Module.removeEventListener = (type, fn) =>
                Module.events.off(type, fn);
            Module.dispatchEvent =       (event)    => {
                if (Module.disabled[event.type]) {
                    return;
                }
                Module.events.emit(event.type, event);
            };
        } catch (e) {
            Module.events = {};
            Module.addEventListener = (type, fn) => {
                if (!Module.events[type])
                    Module.events[type] = [];
                if (!type in Module.disabled)
                    Module.disabled[type] = false;
                Module.events[type].push(fn);
            };
            Module.removeEventListener = (type, fn) => {
                if (Module.events[type]) {
                    const index = Module.events[type].indexOf(fn);
                    if (index > -1) {
                        Module.events[type].splice(index, 1);
                    }
                }
            };
            Module.dispatchEvent = (event) => {
                if (Module.disabled[event.type]) {
                    return;
                }

                if (Module.events[event.type]) {
                    Module.events[event.type]
                        .forEach(fn => fn(event));
                }
            };
        }
        return;
    }

    Module.events = new EventTarget();
    Module.addEventListener = (type, fn) => {
        if (!type in Module.disabled) {
            Module.disabled[type] = false;
        }
        Module.events.addEventListener(type, fn);
    };
    Module.removeEventListener = (type, fn) =>
        Module.events.removeEventListener(type, fn);
    Module.dispatchEvent = (event) => {
        if (Module.disabled[event.type]) {
            return;
        }
        Module.events.dispatchEvent(event);
    };

    (async () => {
        Module.persistence =
            new Module.vfs.Persistence();
        await Module.persistence.restore();
        Module.persistence.enable();
    }) ();
};