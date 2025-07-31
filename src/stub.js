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
 * Shall be true when executing under nodejs
 */
Module.node = typeof process !== 'undefined' &&
                     process.versions &&
                     process.versions.node;

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
Module.invoke = function(input, output = undefined) {
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

    // run the code, getting it's address and length in return
    let result = {
        address: Module.ccall(
            'em_run_string',
            'number',
            ['string', 'number'],
            [code.value, code.length]),
        length: Module.ccall(
            'em_run_length', 'number')
    };

    // check for errors
    if (result.address < 0) {
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

    // ensure there's stuff to read on the heap
    if (!result.length) {
        // Fire error event
        Module.dispatchEvent(new CustomEvent('invoke.error', { 
            detail: { 
                "input": input,
                "output": output,
                "result": result }
        }));

        // we don't need to care about freeing, nothing was allocated
        throw new Error("Unexpected result, no output");
    }

    let text = null;

    try {
        // This ensures consistent encoding handling
        text = Module.iou.fromBytes(result.address, result.length);
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
        // release the buffer that em alloc'd
        Module.ccall('em_run_free');
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
     * @param {string} contents 
     * @returns
     */
    put: function(path, contents) {
        return Module.ccall('em_vfs_put', 'bool', [
            'string',
            'string', 'number'], [
            path,
            contents, Module.lengthBytesUTF8(contents)
        ]);
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
     * Shall return false if directories if false and path is a directory
     */
    unlink: function(path, directories = false) {
        return Module.ccall('em_vfs_unlink', 'bool',
            ['string', 'bool'],
            [ path, directories ]);
    },

    /**
     * Shall create a directory at the given path
     * @param {string} path
     * @returns bool 
     */
    mkdir: function(path) {
        return Module.ccall('em_vfs_mkdir', 'bool',
            ['string' ],
            [ path ]);
    },

    /**
     * Shall destroy the vfs and recreate it
     */
    reset: function() {
        Module.ccall('em_vfs_reset');
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
    }
};

/**
 * Shall provide buffer conversion from JS -> PHP and PHP -> JS
 */
Module.iou = {
    // JS → PHP: Convert JS string to raw bytes
    toBytes: function(jsString, buffer) {
        let written = 0;
        for (let i = 0; i < jsString.length; i++) {
            const code = jsString.charCodeAt(i);

            if (code < 0x80) {
                // ASCII - direct mapping
                Module.HEAPU8[buffer + written] = code;
                written++;
            } else if (code < 0x800) {
                // 2-byte UTF-8
                Module.HEAPU8[buffer + written] = 0xC0 | (code >> 6);
                Module.HEAPU8[buffer + written + 1] = 0x80 | (code & 0x3F);
                written += 2;
            } else {
                // 3-byte UTF-8
                Module.HEAPU8[buffer + written] = 0xE0 | (code >> 12);
                Module.HEAPU8[buffer + written + 1] = 0x80 | ((code >> 6) & 0x3F);
                Module.HEAPU8[buffer + written + 2] = 0x80 | (code & 0x3F);
                written += 3;
            }
        }
        return written;
    },
    
    // Calculate byte length for JS string
    getByteLength: function(jsString) {
        let bytes = 0;
        for (let i = 0; i < jsString.length; i++) {
            const code = jsString.charCodeAt(i);
            if (code < 0x80) {
                bytes += 1;
            } else if (code < 0x800) {
                bytes += 2;
            } else {
                bytes += 3;
            }
        }
        return bytes;
    },

    // PHP → JS: Convert raw bytes to JS string (Latin-1)
    fromBytes: function(buffer, length) {
        let result = '';
        for(let i = 0; i < length; i++) {
            result += String.fromCharCode(
                Module.HEAPU8[buffer + i]);
        }
        return result;
    }
};

Module['onRuntimeInitialized'] = function() {
    Module.startup();

    if (typeof window !== 'undefined') {
        window.addEventListener('unload', function() {
            if (typeof Module !== 'undefined' && Module.shutdown) {
                Module.shutdown();
            }
        });
    }

    Module.decoder = new TextDecoder();

    if (Module.node) {
        try {
            const EventEmitter = require('events');
            Module.events = new EventEmitter();
            Module.addEventListener = (type, fn) => Module.events.on(type, fn);
            Module.dispatchEvent = (event) => Module.events.emit(event.type, event);
        } catch (e) {
            Module.events = {};
            Module.addEventListener = (type, fn) => {
                if (!Module.events[type]) Module.events[type] = [];
                Module.events[type].push(fn);
            };
            Module.dispatchEvent = (event) => {
                if (Module.events[event.type]) {
                    Module.events[event.type].forEach(fn => fn(event));
                }
            };
        }
        return;
    }
    Module.events = new EventTarget();
    Module.addEventListener = (type, fn) =>
        Module.events.addEventListener(type, fn);
    Module.dispatchEvent = (event) =>
        Module.events.dispatchEvent(event);
};