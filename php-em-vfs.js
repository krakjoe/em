#!/usr/bin/env node
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
 * Supports:
 *  Creation
 *  Extraction
 *  Information
 */

const fs   = require('fs');
const path = require('path');
const zlib = require('zlib');

class EmVfsToolLogger {
    /**
     * Shall Construct Logging
     * @param {EmVfsTool} tool 
     */
    constructor (namespace, options) {
        this.log = (...varargs) => {
            console.log(
                `\x1b[32m[%s] %s\x1b[0m`,
                namespace,
                varargs[0], ...varargs.slice(1));
        };
        this.error = (...varargs) => {
            console.error(
                 `\x1b[31m[%s] %s\x1b[0m`,
                namespace,
                varargs[0], ...varargs.slice(1));
            process.exit(1);
        };
        if (options.verbose) {
            this.debug = (...varargs) => {
                console.debug(
                    `\x1b[34m[%s] %s\x1b[0m`,
                    namespace, varargs[0], ...varargs.slice(1));
            };
        } else {
            this.debug =
                (...varargs) => {};
        }
    }
}

class EmVfsToolMode {
    static UNKNOWN = 0;
    static CREATE  = 1;
    static EXTRACT = 2;
    static INFO    = 4;

    /**
     * Shall parse the mode from command line
     * @param {string} string 
     * @returns {EmVfsToolMode.*}
     */
    static fromString (logger, string) {
        if (!string) {
            logger.debug(
                `no mode given on command line`);
            return EmVfsToolMode.UNKNOWN;
        }
        const selected = string
            .trim()
            .toLowerCase();

        logger.debug(`mode ${selected} given`);
        switch (selected) {
            case 'c':
            case 'create':
            case 'n':
                logger.debug(
                    `mode ${selected} resolved as CREATE`);
                return EmVfsToolMode.CREATE;
            case 'e':
            case 'extract':
            case 'x':
                logger.debug(
                    `mode ${selected} resolved as EXTRACT`);
                return EmVfsToolMode.EXTRACT;
            case 'i':
            case 'info':
                logger.debug(
                    `mode ${selected} resolved as INFO`);
                return EmVfsToolMode.INFO;
        }
        logger.debug(`mode ${selected} unresolved`);
        return EmVfsToolMode.UNKNOWN;
    }

    /**
     * Shall construct the mode for tool
     * @param {EmVfsTool} tool 
     * @param {EmVfsToolMode.*} mode 
     */
    constructor(tool) {
        this.tool = tool;
        this.mode = EmVfsToolMode.fromString(
            this.tool.logger,
            this.tool.options.mode);
        if (this.mode ===
            EmVfsToolMode.UNKNOWN) {
            this.tool.logger.error(
                `no cromulent mode given`);
        }
    }

    /**
     * Shall determine the correct desintation path
     * @param {string} destination 
     * @returns string
     */
    path(destination) {
        let result = destination
            .replace(/\/+/g, '/');

        if (!this.tool.options.strip) {
            return result;
        }

        result = result.split(/\//)
            .slice(
                1 + this.tool.options.strip);
        return result.join('/');
    }

    /**
     * Shall enter into the configured mode
     * @returns 
     */
    enter() {
        switch (this.mode) {
            case EmVfsToolMode.CREATE:
                return this.create();
            case EmVfsToolMode.EXTRACT:
                return this.extract();
            case EmVfsToolMode.INFO:
                return this.info();
        }
    }

    /**
     * Shall create tool.options.out from the contents of tool.options.in
     */
    async create() {
        if (!this.tool.options.in) {
            this.tool.logger.error(
                `Creation Mode requires --in`);
        }
        this.tool.logger.debug(
            `Creation Input [${this.tool.options.in}]`);
        if (!this.tool.options.out) {
            this.tool.logger.error(
                `Creation Mode requires --out`);
        }
        this.tool.logger.debug(
            `Creation Output [${this.tool.options.out}]`);
        
        try {
            fs.accessSync(this.tool.options.in, fs.constants.R_OK);
        } catch (error) {
            this.tool.logger.error(
                `${this.tool.options.in} is not readable`);
        }

        this.tool.logger.debug(
            `${this.tool.options.in} is readable`);

        try {
            const writable = path.dirname(this.tool.options.out);
            (fs.existsSync(this.tool.options.out) &&
                fs.accessSync(
                    this.tool.options.out, fs.constants.W_OK)) ||
            fs.accessSync(writable, fs.constants.W_OK);
        } catch(error) {
            this.tool.logger.error(
                `${this.tool.options.out} is not writable`);
        }

        this.scandir(this.tool.options.in, (type, path) => {
            if (type == this.tool.Module.vfs.EM_VFS_DIR) {
                this.tool.logger.debug(
                    `mkdir(${this.path(path)})`);
                this.tool.Module.vfs.mkdir(
                    this.path(path));
            } else {
                this.tool.logger.debug(
                    `put(${this.path(path)}, ...)`);
                this.tool.Module.vfs.put(
                    this.path(path),
                    new Uint8Array(
                        fs.readFileSync(path)));
            }
        });

        this.tool.logger.log(
            `${this.tool.options.in} added to VFS`);
        this.tool.logger.debug(
            `streaming ${this.tool.options.out}`);
        this.memory =
            new this.tool.Module.vfs.Memory();
        await this.memory.load();
        this.tool.logger.debug(
            `stream ${this.memory.header.size.consumed} bytes`);
        fs.writeFileSync(
            this.tool.options.out, 
            this.tool.Module.HEAPU8.slice(
                this.memory.address,
                this.memory.address +
                    this.memory.header.size.consumed));
        this.tool.logger.log(
            `${this.tool.options.out} created`);
        this.memory.free();
    }

    /**
     * Shall extract the contents of tool.options.in into tool.options.out
     */
    async extract() {
        if (!this.tool.options.in) {
            this.tool.logger.error(
                `Extraction Mode requires --in`);
        }
        this.tool.logger.debug(
            `Extraction Input [${this.tool.options.in}]`);
        if (!this.tool.options.out) {
            this.tool.logger.error(
                `Extraction Mode requires --out`);
        }
        this.tool.logger.debug(
            `Extraction Output [${this.tool.options.out}]`);
        
        try {
            fs.accessSync(this.tool.options.in, fs.constants.R_OK);
        } catch (error) {
            this.tool.logger.error(
                `${this.tool.options.in} is not readable`);
        }

        this.tool.logger.debug(
            `${this.tool.options.in} is readable`);

        try {
            const writable = path.dirname(this.tool.options.out);
            (fs.existsSync(this.tool.options.out) &&
                fs.accessSync(
                    this.tool.options.out, fs.constants.W_OK)) ||
            fs.accessSync(writable, fs.constants.W_OK);
        } catch(error) {
            this.tool.logger.error(
                `${this.tool.options.out} is not writable`);
        }

        this.tool.logger.debug(`streaming ${this.tool.options.in}`);
        this.memory =
            new this.tool.Module.vfs.Memory(
                new Uint8Array(
                    fs.readFileSync(this.tool.options.in)));
        await this.memory.load();
        
        this.tool.logger.debug(
            `stream  ${this.memory.header.size.consumed} bytes`);
        this.reader =
            new this.tool.Module.vfs.Reader(this.memory);
        this.entries = this.reader.read();
        this.tool.logger.debug(
            `stream  ${this.entries.length} entries`);
        for (const entry of this.entries) {
            if (entry.name === '/') {
                continue;
            }

            const destination = [
                this.tool.options.out,
                this.path(
                    entry.name)
            ].join('/')
             .replace(/\/+/g, '/');

            this.tool.logger.debug(
                `stream ${entry.name} -> ${destination}`, entry);
            
            if (entry.kind == this.tool.Module.vfs.EM_VFS_DIR) {
                this.tool.logger.debug(
                    `mkdir(${destination})`);
                try {
                    fs.existsSync(destination) ||
                        fs.mkdirSync(destination, true);
                } catch (error) {}
            } else {
                let data = [];
                if ((entry.flags &
                        this.tool.Module.vfs.Entry.EM_VFS_MEMORY_COMPRESSED)) {
                    this.tool.logger.debug(
                        `uncompress(${destination})`);
                    data = zlib.inflateSync(entry.data);
                    if (data.length != entry.size.data.verbatim) {
                        this.tool.logger.error(
                            `uncompresss ${entry.name}, ` +
                            `size mismatch ${data.length} != ${entry.size.data.verbatim}`);
                    }
                } else {
                    data = entry.data;
                }

                if (!data || !data.length) {
                    this.tool.logger
                        .debug(`zero(${destination})`);
                    continue;
                }

                this.tool.logger.debug(
                    `write(${destination})`);
                fs.writeFileSync(
                    destination, data);
                this.tool.logger
                    .log(`wrote ${destination}`);
            }
        }
        this.memory.free();
    }

    info() {}

    scandir(root, callback) {
        const entries = fs.readdirSync(root, { withFileTypes: true });
        for (const entry of entries) {
            const fullPath = path.join(root, entry.name);
            if (entry.isDirectory()) {
                callback(
                    this.tool.Module.vfs.EM_VFS_DIR, fullPath);
                this.scandir(
                    fullPath, callback);
            } else if (entry.isFile()) {
                callback(
                    this.tool.Module.vfs.EM_VFS_FILE, fullPath);
            }
        }
    }
}

class EmVfsTool {
    /**
     * Shall construct the tool
     * @param {EmVfsToolLogger} logger 
     * @param {*} options 
     */
    constructor(logger, options = {}) {
        this.logger = logger;
        this.options = {
            mode:    options.mode    || null,
            in:      options.in      || null,
            out:     options.out     || null,
            strip:   options.strip   || 0,
            verbose: options.verbose || false,
            ...options
        };
        this.Module = null;
    }
    
    async run() {
        this.logger.log('Welcome');
        this.logger.log('='.repeat(50));
        
        // Load module
        await this.load();

        // Enter into mode
        this.mode =
            new EmVfsToolMode(this);
        this.mode.enter();

        this.logger.log('Bye!');
        this.logger.log('='.repeat(50));
    }
    
    async load() {
        try {
            // Try to find php-em.js in multiple locations
            const searchPaths = [
                './php-em.js',           // Current directory
                '../php-em.js',          // One level up
                '../dist/php-em.js'      // dist directory
            ];
            
            let emPath = null;
            let emRoot = null;
            
            for (const searchPath of searchPaths) {
                const fullPath = path.resolve(searchPath);
                if (fs.existsSync(fullPath)) {
                    this.logger.debug(
                        `found php-em.js at ${fullPath}`);
                    emPath = fullPath;
                    emRoot = path.dirname(fullPath);
                    break;
                } else {
                    this.logger.debug(
                        `could not find php-em.js at ${fullPath}`);
                }
            }

            if (!emPath) {
                this.logger.error(
                    `php-em.js not found in any of: ${searchPaths.join(', ')}`);
            }

            // Create a minimal global environment for php-em.js
            let Module = require(emPath);

            // Wait for module to be ready
            if (!Module.ready) {
                await new Promise(resolve => {
                    const checkReady = () => {
                        if (Module.ready) {
                            this.logger.debug(
                                `${emPath} ready`);
                            resolve();
                        } else {
                            this.logger.debug(
                                `${emPath} not ready yet ...`);
                            setTimeout(checkReady, 100);
                        }
                    };
                    checkReady();
                });
            }

            this.Module = Module;
        } catch (error) {
            this.logger.error('loading exception occured', error);
        }
    }
}

help = (status, message = null) => {
    console.log(`
Usage: node php-em-vfs.js [options]

Options:
  --mode                      [create|extract]
  --in,      -i               Input Path
  --out,     -o               Output Path   
  --strip    -s               Strip Output Path Components
  --verbose, -v               Be Verbose
  --help,    -h               Show this help
`);

    if (message) {
        console.log(
            `\n${message}\n`);
    }

    process.exit(status);
}

// CLI interface
if (require.main === module) {
    const args = process.argv.slice(2);
    const options = {
        verbose: false,
        in:      null,
        out:     null,
        strip:   0,
        mode:    null,
    };

    for (let i = 0; i < args.length; i++) {
        const arg = args[i];

        if (arg === '--verbose' || arg === '-v') {
            options.verbose = true;
        } else if (arg === '--mode' || arg === '-m') {
            options.mode = args[++i];
        } else if (arg === '--in' || arg === '-i') {
            options.in = args[++i];
        } else if (arg === '--out' || arg === '-o') {
            options.out = args[++i];
        } else if (arg === '--strip' || arg === '-s') {
            options.strip = parseInt(args[++i]);
        } else if (arg === '--help' || arg === '-h') {
            help(0);
        } else {
            help(1, `unknown argument at ${i+1}: ${arg}`);
        }
    }

    const logger = new EmVfsToolLogger(
        'EmVfsTool', options);
    const runner = new EmVfsTool(logger, options);

    runner.run().catch(error => {
        logger.error(
            'VFS Tool Exception:', error);
    });
}

module.exports = EmVfsTool;