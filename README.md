# em - PHP in WebAssembly

**em** is a complete PHP development environment that runs entirely in your browser or as a desktop application. It compiles PHP to WebAssembly, providing a sandboxed environment where you can write, test, and run PHP code without any server setup.

## 🚀 Try it Now

**[Launch em in your browser →](https://krakjoe.github.io/em/)**

No installation required - just click and start coding PHP instantly!

## ✨ Features

### 🌐 **Browser-Based IDE**
- Full-featured code editor with syntax highlighting
- Multiple PHP versions (8.0, 8.1, 8.2, 8.3, 8.4, master)
- Real-time code execution
- Built-in virtual browser for testing web applications

### 📁 **Virtual File System**
- Complete file system simulation in memory
- Import/export projects as ZIP files
- Persistent storage across sessions
- Directory operations and file management

### 🔗 **GitHub Integration**
- Load projects directly from GitHub repositories
- Import from GitHub Gists
- Secret Token Support (avoids REST api limits)

### 🗄️ **Database & Extensions**
- SQLite database support
- ZIP archive handling
- Image processing (GD, JPEG, PNG)
- XML/DOM manipulation
- Compression (zlib, bz2)
- And many more PHP extensions

### 🖥️ **Desktop Application**

Download the desktop version for offline development (uses electron):

| OS      | Arch  |  Format    | Link                                                                                     |
|:--------|:-----:|:----------:|:-----------------------------------------------------------------------------------------|
| Linux   |  x64  | AppImage   | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-linux-x64-AppImage)   |
| Linux   | arm64 | AppImage   | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-linux-arm64-AppImage) |
| Linux   |  x64  | zip        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-linux-x64-zip)        |
| Linux   | arm64 | zip        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-linux-arm64-zip)      |
| MacOS   |  x64  | dmg        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-macos-x64-dmg )       |
| MacOS   | arm64 | dmg        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-macos-arm64-dmg)      |
| MacOS   |  x64  | zip        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-macos-x64-zip)        |
| MacOS   | arm64 | zip        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-macos-arm64-zip)      |
| Windows |  x64  | portable   | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-windows-x64-exe)      |
| Windows |  x64  | setup      | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-windows-x64-nsis)     |
| Windows |  x64  | zip        | [Download](https://nightly.link/krakjoe/em/workflows/em/develop/em-windows-x64-zip)      |

***Note: Electron builds need to be started with `--no-sandbox` to function properly; It's a good idea to create a launcher for your operating system that automatically starts the application with `--no-sanbbox`. This is an annoyance and limitation of electron-builder that I can't find a gracious way around ...***

## 🛠️ Use Cases

### Learning & Education
- Perfect for PHP tutorials and learning
- No need to set up XAMPP, WAMP, or local servers
- Safe sandbox environment for experimentation

### Prototyping & Testing
- Quickly test PHP concepts and algorithms
- Test across multiple PHP versions instantly

### Development & Debugging
- Portable development environment
- Test code behavior in isolation
- Debug without affecting your main system

## 🎯 Quick Start

1. **[Open em](https://krakjoe.github.io/em/)** in your browser
2. Choose your PHP version from the dropdown
3. Write your PHP code in the editor
4. Click "Run Code" or press `Ctrl+Enter`
5. See the output instantly

## 🔧 For Developers

### GitHub Action
Use em as a build pipeline for your PHP WebAssembly projects:

```yaml
- uses: krakjoe/em@develop
  with:
    EM_PHP_DIR: ./php-src
    bake: all
```

See [ACTION.md](ACTION.md) for detailed usage instructions.

### API Integration
Embed PHP execution in your JavaScript applications:

```javascript
// Execute PHP code
const result = Module.invoke('<?php echo "Hello from PHP!"; ?>');
console.log(result); // "Hello from PHP!"

// Work with the virtual file system
Module.vfs.put('/data.txt', new TextEncoder().encode('Hello World'));
const content = Module.vfs.get('/data.txt');
```

## 📚 Advanced Features

### Virtual Browser
- Run PHP web applications in a simulated browser environment
- Test forms, sessions, and HTTP requests
- Debug web applications without a real server

### Project Management
- **New File/Folder**: Create and organize your project structure
- **Import/Export VFS**: Upload/Download and persist with native, optionally compressed, and random access memory streams.
- **Import/Export ZIP**: Upload/Download from external sources
- **GitHub Import**: Load repositories and gists directly

### VFS Images

While ZIP is a perfectly cromulent and portable format, it's rather wasteful, and does not lend itself to very fast reading or writing, so we could not base persistence on Zip.

Instead `em` implements streaming the vfs as a contiguous buffer, the buffer will use compression where `zlib` is available, is randomly accessible, produces smaller outputs than `zip` and is much more suitable for persistence.

So that you may create images outside of the ide, we provide `php-em-vfs.js`, this may be used to create and extract images from and into local directories.

#### Examples

To create `/tmp/my.vfsi` with the content of `/path/to/my/files`:

`php-em-vfs.js --mode create --in /path/to/my/files --out /tmp/my.vfsi`

To create `/tmp/my.vfsi` with the contents of `/path/to/my/files` where the root directory in the image is desired to be `files`:

`php-em-vfs.js --mode create --in /path/to/my/files --out /tmp/my.vfsi --strip 2`

To extra `/tmp/my.vfsi` into `/tmp/vfsi.my` (output directory must exist):

`php-em-vfs.js --mode extract --in /tmp/my.vfsi --out /tmp/vfsi.my`

### Multiple PHP Versions
Test your code across different PHP versions to ensure compatibility:
- PHP 8.0 through 8.4
- Latest development version (master branch)

## 🏗️ Architecture

em is built on several key technologies:

- **WebAssembly**: Runs PHP natively in the browser
- **Emscripten**: Compiles PHP C code to WebAssembly
- **Virtual File System**: Complete POSIX-like file operations
- **Service Workers**: Enables the virtual browser functionality
- **IndexedDB/FileSystem API**: Provides persistent storage

## 🤝 Contributing

We welcome contributions! Whether you're:
- 🐛 Reporting bugs
- 💡 Suggesting features  
- 📝 Improving documentation
- 🔧 Contributing code

Check out our [issues](../../issues) to get started.

## 📄 License

This project is licensed under the PHP License 3.01 - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- PHP Core Team for the amazing PHP language
- Emscripten team for WebAssembly tooling
- All contributors who help make em better

---

**Start coding PHP in your browser today!** → **[Launch em](https://krakjoe.github.io/em/)**