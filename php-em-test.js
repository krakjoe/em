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
 * - .phpt files (PHP test format)
 * - .jstest files (JavaScript test format) 
 * - Recursive directory scanning
 * - Test filtering and reporting
 * - Both browser and Node.js execution
 */

const fs = require('fs');
const path = require('path');

class EmTestRunner {
    constructor(options = {}) {
        this.options = {
            testDir: options.testDir || './tests',
            pattern: options.pattern || null,
            verbose: options.verbose || false,
            browser: options.browser || false,
            showProgress: options.showProgress !== false,
            ...options
        };
        
        this.stats = {
            total: 0,
            passed: 0,
            failed: 0,
            skipped: 0,
            errors: 0
        };
        
        this.results = [];
        this.Module = null;
    }
    
    async run() {
        console.log('em Test Runner');
        console.log('='.repeat(50));
        
        // Load em module
        await this.loadEmModule();
        
        // Find all test files
        const testFiles = this.findTestFiles(this.options.testDir);
        console.log(`Found ${testFiles.length} test files\n`);
        
        // Run tests
        for (const testFile of testFiles) {
            if (this.options.pattern && !testFile.includes(this.options.pattern)) {
                continue;
            }
            
            await this.runTest(testFile);
        }
        
        // Show summary
        this.showSummary();
        
        // Exit with appropriate code
        process.exit(this.stats.failed > 0 || this.stats.errors > 0 ? 1 : 0);
    }
    
    async loadEmModule() {
        if (this.options.browser) {
            // Browser environment - assume Module is global
            if (typeof Module === 'undefined') {
                throw new Error('Module not found. Make sure em.js is loaded.');
            }
            this.Module = Module;
        } else {
            // Node.js environment - load the module
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
                        emPath = fullPath;
                        emRoot = path.dirname(fullPath);
                        break;
                    }
                }
                
                if (!emPath) {
                    throw new Error(`php-em.js not found in any of: ${searchPaths.join(', ')}`);
                }
                
                console.log(`Found php-em.js at: ${emPath}`);

                // Create a minimal global environment for php-em.js
                let Module = require(emPath);

                // Wait for module to be ready
                if (!Module.ready) {
                    console.log("In Node " + Module.node)
                    await new Promise(resolve => {
                        const checkReady = () => {
                            if (Module.ready) {
                                resolve();
                            } else {
                                setTimeout(checkReady, 100);
                            }
                        };
                        checkReady();
                    });
                }

                this.Module = Module;
            } catch (error) {
                console.error('Failed to load em module:', error.message);
                process.exit(1);
            }
        }
    }
    
    findTestFiles(dir) {
        const files = [];
        
        const scan = (currentDir) => {
            if (!fs.existsSync(currentDir)) {
                return;
            }
            
            const entries = fs.readdirSync(currentDir, { withFileTypes: true });
            
            for (const entry of entries) {
                const fullPath = path.join(currentDir, entry.name);
                
                if (entry.isDirectory()) {
                    scan(fullPath);
                } else if (entry.isFile()) {
                    const ext = path.extname(entry.name);
                    if (ext === '.phpt' || ext === '.jstest') {
                        files.push(fullPath);
                    }
                }
            }
        };
        
        scan(dir);
        return files.sort();
    }
    
    async runTest(testFile) {
        const ext = path.extname(testFile);
        const testName = path.relative(this.options.testDir, testFile);
        
        this.stats.total++;
        
        if (this.options.showProgress) {
            process.stdout.write(`${testName.padEnd(60)} ... `);
        }
        
        try {
            let result;
            
            if (ext === '.phpt') {
                result = await this.runPhptTest(testFile);
            } else if (ext === '.jstest') {
                result = await this.runJsTest(testFile);
            } else {
                throw new Error(`Unknown test type: ${ext}`);
            }
            
            this.results.push({
                file: testName,
                status: result.status,
                message: result.message,
                output: result.output,
                expected: result.expected,
                actual: result.actual
            });
            
            this.stats[result.status]++;
            
            if (this.options.showProgress) {
                const statusColors = {
                    passed: '\x1b[32mPASS\x1b[0m',    // Green
                    failed: '\x1b[31mFAIL\x1b[0m',    // Red
                    skipped: '\x1b[33mSKIP\x1b[0m',   // Yellow
                    errors: '\x1b[31mERROR\x1b[0m'    // Red
                };
                console.log(statusColors[result.status]);
                
                if (result.status !== 'passed' && this.options.verbose) {
                    console.log(`    ${result.message}`);
                    if (result.output) {
                        console.log(`    Output: ${result.output}`);
                    }
                }
            }
            
        } catch (error) {
            this.stats.errors++;
            this.results.push({
                file: testName,
                status: 'errors',
                message: error.message,
                output: null,
                expected: null,
                actual: null
            });
            
            if (this.options.showProgress) {
                console.log('\x1b[31mERROR\x1b[0m');
                if (this.options.verbose) {
                    console.log(`    ${error.message}`);
                }
            }
        } finally {
            this.Module.vfs.reset();
        }
    }
    
    async runPhptTest(testFile) {
        const content = fs.readFileSync(testFile, 'utf8');
        const sections = this.parsePhptFile(content);
        
        // Check for SKIP section
        if (sections.SKIP) {
            const skipCode = sections.SKIP.trim();
            if (skipCode) {
                try {
                    const skipResult = this.Module.invoke(skipCode);
                    if (skipResult.trim()) {
                        return {
                            status: 'skipped',
                            message: skipResult.trim(),
                            output: null,
                            expected: null,
                            actual: null
                        };
                    }
                } catch (e) {
                    // If skip code fails, continue with test
                }
            }
        }
        
        // Run the test
        if (!sections.FILE) {
            throw new Error('No --FILE-- section found');
        }
        
        const testCode = sections.FILE;
        let output;
        
        try {
            output = this.Module.invoke(testCode);
        } catch (error) {
            return {
                status: 'failed',
                message: `Runtime error: ${error.message}`,
                output: null,
                expected: sections.EXPECT || sections.EXPECTF || null,
                actual: null
            };
        }
        
        // Compare output
        if (sections.EXPECT) {
            const expected = sections.EXPECT.trim();
            const actual = output.trim();
            
            if (expected === actual) {
                return {
                    status: 'passed',
                    message: 'Output matches expected',
                    output: actual,
                    expected: expected,
                    actual: actual
                };
            } else {
                return {
                    status: 'failed',
                    message: 'Output mismatch',
                    output: actual,
                    expected: expected,
                    actual: actual
                };
            }
        } else if (sections.EXPECTF) {
            // Simple pattern matching for EXPECTF
            const pattern = sections.EXPECTF.trim();
            const actual = output.trim();
            
            // Convert simple %s, %d patterns to regex
            const regexPattern = pattern
                .replace(/%s/g, '.*?')
                .replace(/%d/g, '\\d+')
                .replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
            
            const regex = new RegExp(`^${regexPattern}$`, 'm');
            
            if (regex.test(actual)) {
                return {
                    status: 'passed',
                    message: 'Output matches pattern',
                    output: actual,
                    expected: pattern,
                    actual: actual
                };
            } else {
                return {
                    status: 'failed',
                    message: 'Output does not match pattern',
                    output: actual,
                    expected: pattern,
                    actual: actual
                };
            }
        } else {
            // No expectation - just check if it runs without error
            return {
                status: 'passed',
                message: 'Test executed successfully',
                output: output,
                expected: null,
                actual: output
            };
        }
    }
    
    async runJsTest(testFile) {
        const content = fs.readFileSync(testFile, 'utf8');
        
        // Capture console output
        let output = '';
        const captureConsole = {
            log: (...args) => {
                const msg = args.join(' ');
                output += msg + '\n';
            },
            error: (...args) => {
                const msg = args.join(' ');
                output += 'ERROR: ' + msg + '\n';
            },
            warn: (...args) => {
                const msg = args.join(' ');
                output += 'WARN: ' + msg + '\n';
            }
        };
        
        try {
            // Create a test context with access to Module
            const testContext = {
                Module: this.Module,
                assert: (condition, message) => {
                    if (!condition) {
                        throw new Error(message || 'Assertion failed');
                    }
                },
                assertEquals: (expected, actual, message) => {
                    if (expected !== actual) {
                        throw new Error(message || `Expected '${expected}', got '${actual}'`);
                    }
                },
                assertContains: (expected, actual, message) => {
                    if (!actual.includes(expected)) {
                        throw new Error(message || `Expected '${expected}', got '${actual}'`);
                    }
                },
                assertArray: (expected, actual, message) => {
                    if (!actual && !expected) {
                        return;
                    }

                    if (!actual || !expected) {
                        throw new Error(message || `Expected array, got ${actual}`);
                    }
                    const expectedArray =
                        expected instanceof Uint8Array ?
                            expected : new Uint8Array(expected);
                    
                    if (!(actual instanceof Uint8Array)) {
                        throw new Error(message || 
                            `Expected Uint8Array, got ${typeof actual}`);
                    }
                    
                    if (expectedArray.length !== actual.length) {
                        throw new Error(message ||
                            `Array lengths differ: expected [${expectedArray}] `+
                                `${expectedArray.length}, got ${actual.length}, [${actual}]`);
                    }

                    for (let i = 0; i < expectedArray.length; i++) {
                        if (expectedArray[i] !== actual[i]) {
                            throw new Error(message ||
                                `Arrays differ at index ${i}: expected [${expectedArray}] `+
                                    `${expectedArray[i]}, got [${actual[i]}]`);
                        }
                    }
                },
                console: captureConsole
            };
            
            // Execute the test
            const testFunction = new Function(
                'Module',
                'assert',
                'assertEquals',
                'assertContains',
                'assertArray',
                'console',
                content);
            const result = testFunction(testContext.Module,
                testContext.assert,
                testContext.assertEquals,
                testContext.assertContains,
                testContext.assertArray,
                testContext.console);
            
            // Return captured output if any, otherwise result
            const finalOutput = output.trim() || (result ? String(result) : 'OK');
            
            return {
                status: 'passed',
                message: 'JavaScript test passed',
                output: finalOutput,
                expected: null,
                actual: null
            };
            
        } catch (error) {
            return {
                status: 'failed',
                message: error.message,
                output: output.trim() || null,
                expected: null,
                actual: null
            };
        }
    }
    
    parsePhptFile(content) {
        const sections = {};
        const lines = content.split('\n');
        let currentSection = null;
        let currentContent = [];
        
        for (const line of lines) {
            const sectionMatch = line.match(/^--([A-Z_]+)--$/);
            
            if (sectionMatch) {
                // Save previous section
                if (currentSection) {
                    sections[currentSection] = currentContent.join('\n');
                }
                
                // Start new section
                currentSection = sectionMatch[1];
                currentContent = [];
            } else if (currentSection) {
                currentContent.push(line);
            }
        }
        
        // Save last section
        if (currentSection) {
            sections[currentSection] = currentContent.join('\n');
        }
        
        return sections;
    }
    
    showSummary() {
        console.log('\n' + '='.repeat(50));
        console.log('TEST SUMMARY');
        console.log('='.repeat(50));
        console.log(`Total:   ${this.stats.total}`);
        console.log(`Passed:  \x1b[32m${this.stats.passed}\x1b[0m`);
        console.log(`Failed:  \x1b[31m${this.stats.failed}\x1b[0m`);
        console.log(`Skipped: \x1b[33m${this.stats.skipped}\x1b[0m`);
        console.log(`Errors:  \x1b[31m${this.stats.errors}\x1b[0m`);
        
        const failureRate = this.stats.total > 0 ? ((this.stats.failed + this.stats.errors) / this.stats.total * 100).toFixed(1) : 0;
        console.log(`Success: ${(100 - failureRate).toFixed(1)}%`);
        
        // Show failed tests
        const failedTests = this.results.filter(r => r.status === 'failed' || r.status === 'errors');
        if (failedTests.length > 0) {
            console.log('\nFAILED TESTS:');
            console.log('-'.repeat(50));
            
            for (const test of failedTests) {
                console.log(`\x1b[31m${test.file}\x1b[0m`);
                console.log(`  ${test.message}`);
                
                if (test.expected && test.actual) {
                    console.log(`  Expected: ${JSON.stringify(test.expected)}`);
                    console.log(`  Actual:   ${JSON.stringify(test.actual)}`);
                }
                console.log();
            }
        }
    }
}

// CLI interface
if (require.main === module) {
    const args = process.argv.slice(2);
    const options = {};
    
    for (let i = 0; i < args.length; i++) {
        const arg = args[i];
        
        if (arg === '--verbose' || arg === '-v') {
            options.verbose = true;
        } else if (arg === '--pattern' || arg === '-p') {
            options.pattern = args[++i];
        } else if (arg === '--test-dir' || arg === '-d') {
            options.testDir = args[++i];
        } else if (arg === '--browser' || arg === '-b') {
            options.browser = true;
        } else if (arg === '--help' || arg === '-h') {
            console.log(`
Usage: node run-tests.js [options]

Options:
  --test-dir, -d <dir>    Test directory (default: ./tests)
  --pattern, -p <pattern> Only run tests matching pattern
  --verbose, -v           Show detailed output
  --browser, -b           Run in browser mode
  --help, -h              Show this help

Test file formats:
  .phpt - PHP test files (similar to PHP's test format)
  .jstest - JavaScript test files
`);
            process.exit(0);
        } else if (!arg.startsWith('-')) {
            options.testDir = arg;
        }
    }
    
    const runner = new EmTestRunner(options);
    runner.run().catch(error => {
        console.error('Test runner error:', error);
        process.exit(1);
    });
}

module.exports = EmTestRunner;