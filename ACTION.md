# em GitHub Action

Use em as a composable WebAssembly build pipeline for PHP in your GitHub Actions workflows. This action compiles PHP to WebAssembly with customizable extensions and configurations.

## 🚀 Quick Start

```yaml
name: Build PHP WebAssembly
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Checkout PHP source
        uses: actions/checkout@v4
        with:
          repository: php/php-src
          ref: PHP-8.3
          path: php-src
          
      - name: Build PHP WebAssembly
        uses: krakjoe/em@develop
        with:
          EM_PHP_DIR: ./php-src
          bake: all
          
      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: php-wasm
          path: |
            php-em.js
            php-em.wasm
```

## 📋 Inputs

| Input | Description | Required | Default |
|-------|-------------|----------|---------|
| `EM_PHP_DIR` | Path to PHP source directory | ✅ | |
| `EM_PHP_CONFIGURE` | Extra configure arguments for PHP | ❌ | `''` |
| `EM_EMSDK_CFLAGS` | Extra CFLAGS for emcc compilation | ❌ | `''` |
| `EM_EMSDK_LDFLAGS` | Extra LDFLAGS for emcc linking | ❌ | `''` |
| `bake` | Space-separated list of recipe collections | ❌ | `''` |
| `without` | Space-separated list of exclusions | ❌ | `''` |
| `with` | Space-separated list of individual recipes | ❌ | `''` |
| `emsdk` | Emscripten SDK version to use | ❌ | `'4.0.11'` |

## 📦 Outputs

| Output | Description |
|--------|-------------|
| `wasm-file` | Path to generated WASM file |
| `js-file` | Path to generated JavaScript file |

## 🏗️ Build Configurations

### Recipe Collections (bake)

Pre-defined recipe collections for common use cases:

#### `all` - Complete PHP Environment
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: all
```
Includes: bcmath, calendar, ctype, mbstring, opcache, tokenizer, compression, database, image, and XML extensions.

#### `compression` - Archive & Compression
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: compression
```
Includes: zlib, bz2, zip, phar

#### `database` - Database Extensions
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: database
```
Includes: sqlite3, pdo, pdo-sqlite

#### `image` - Image Processing
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: image
```
Includes: jpeg, png, gd

#### `xml` - XML Processing
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: xml
```
Includes: libxml, xml, dom, simplexml, xmlreader, xmlwriter

### Individual Extensions (with)

Build specific extensions only:

```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    with: "sqlite3 pdo gd"
```

Available extensions:
- **Core**: `bcmath`, `calendar`, `ctype`, `mbstring`, `opcache`, `tokenizer`
- **Compression**: `zlib`, `bz2`, `zip`, `phar`
- **Database**: `sqlite3`, `pdo`, `pdo-sqlite`
- **Image**: `jpeg`, `png`, `gd`
- **XML**: `libxml`, `xml`, `dom`, `simplexml`, `xmlreader`, `xmlwriter`

### Exclusions (without)

Exclude specific extensions from baked collections:

```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: all
    without: "opcache zip"
```

### Multiple PHP Versions Matrix
```yaml
strategy:
  matrix:
    php: ["PHP-8.0", "PHP-8.1", "PHP-8.2", "PHP-8.3", "PHP-8.4", "master"]
    
steps:
  - name: Checkout PHP ${{ matrix.php }}
    uses: actions/checkout@v4
    with:
      repository: php/php-src
      ref: ${{ matrix.php }}
      path: php-src
      
  - name: Build em (${{ matrix.php }})
    uses: krakjoe/em@develop
    with:
      EM_PHP_DIR: ./php-src
      bake: all
      
  - name: Upload ${{ matrix.php }} artifacts
    uses: actions/upload-artifact@v4
    with:
      name: ${{ matrix.php }}-em
      path: |
        php-em.js
        php-em.wasm
```

### Minimal Build
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    with: "ctype mbstring"
```

### Web-Optimized Build
```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: "compression database xml"
    without: "opcache"
```

## 🏷️ PHP Version Support

The action supports all maintained PHP versions:

| PHP Version | Branch/Tag | Status |
|-------------|------------|--------|
| PHP 8.4 | `PHP-8.4` | ✅ Latest |
| PHP 8.3 | `PHP-8.3` | ✅ Stable |
| PHP 8.2 | `PHP-8.2` | ✅ Stable |
| PHP 8.1 | `PHP-8.1` | ✅ Stable |
| PHP 8.0 | `PHP-8.0` | ✅ Security Only |
| Development | `master` | ⚠️ Experimental |

## 🔍 Testing Your Build

After building, you can test your WebAssembly build:

```yaml    
- name: Run test suite
  run: |
    ./php-em-test.js --verbose
```

## 🐛 Troubleshooting

### Build Fails with Missing Dependencies
```yaml
- name: Install build dependencies
  run: |
    sudo apt-get update
    sudo apt-get install build-essential re2c bison autoconf
```

### PHP Source Not Found
Ensure your PHP source checkout step is correct:
```yaml
- name: Checkout PHP source
  uses: actions/checkout@v4
  with:
    repository: php/php-src
    ref: PHP-8.3  # Make sure this branch/tag exists
    path: php-src
```

### Extension Conflicts
Some extensions may conflict, or be undesirable when using bakers. Use exclusions:

```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: all
    without: "opcache"  # Exclude problematic extensions
```

## 🤝 Contributing

Found a bug or want to add support for a new extension? 

1. Check existing [issues](https://github.com/krakjoe/em/issues)
2. Create a new issue or pull request
3. Follow the existing recipe patterns in `/recipe/`

## 📄 License

This action is licensed under the PHP License 3.01 - see the [LICENSE](LICENSE) file for details.