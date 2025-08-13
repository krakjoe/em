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
Download the desktop version for offline development:

- [**Linux**](https://nightly.link/krakjoe/em/workflows/em/develop/electron-ubuntu-latest.zip)
- [**macOS**](https://nightly.link/krakjoe/em/workflows/em/develop/electron-macos-latest.zip)
- [**Windows**](https://nightly.link/krakjoe/em/workflows/em/develop/electron-windows-latest.zip)

## 🛠️ Use Cases

### Learning & Education
- Perfect for PHP tutorials and learning
- No need to set up XAMPP, WAMP, or local servers
- Safe sandbox environment for experimentation

### Prototyping & Testing
- Quickly test PHP concepts and algorithms
- Share code snippets with others via URL
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
- **Import ZIP**: Upload existing projects
- **Export ZIP**: Download your work
- **GitHub Import**: Load repositories and gists directly

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