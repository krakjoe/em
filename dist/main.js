// All JavaScript logic for the IDE

// 1. Demo scripts
const demos = {
    hello: `<?php\n// Hello World demo\necho "Hello, World!";\n?>`,
    vfs: `<?php\n// Virtual File System demo\nfile_put_contents('/tmp/test.txt', 'Hello from VFS!');\necho file_get_contents('/tmp/test.txt');\n?>`,
    http: `<?php\n// HTTP Requests demo\n$url = 'https://api.github.com/zen';\n$opts = [\n    'http' => [\n        'method' => 'GET',\n        'header' => [\n            'User-Agent' => 'em-php-wasm'\n        ]\n    ]\n];\n$context = stream_context_create($opts);\necho file_get_contents($url, false, $context);\n?>`,
    sqlite: `<?php\n// SQLite Database demo\n$db = new SQLite3(':memory:');\n$db->exec('CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT);');\n$db->exec("INSERT INTO test (name) VALUES ('Alice'), ('Bob'), ('Charlie')");\n$res = $db->query('SELECT * FROM test');\nwhile ($row = $res->fetchArray(SQLITE3_ASSOC)) {\n    echo $row['id'] . ': ' . $row['name'] . "\\n";\n}\n?>`,
    oop: `<?php\n// Object-Oriented PHP demo\nclass Person {\n    public $name;\n    function __construct($name) { $this->name = $name; }\n    function greet() { return "Hello, $this->name!"; }\n}\n$p = new Person('World');\necho $p->greet();\n?>`,
    generators: `<?php\n// Generators & Iterators demo\nfunction numbers() {\n    for ($i = 1; $i <= 5; $i++) yield $i;\n}\nforeach (numbers() as $n) echo $n . " ";\n?>`,
    match: `<?php\n// Match Expression (PHP 8.0+) demo\n$input = 'foo';\necho match($input) {\n    'foo' => 'Matched foo',\n    'bar' => 'Matched bar',\n    default => 'No match',\n};\n?>`,
    enums: `<?php\n// Enums (PHP 8.1+) demo\nenum Status {\n    case Pending;\n    case Active;\n    case Done;\n}\n$status = Status::Active;\necho $status->name;\n?>`,
    readonly: `<?php\n// Readonly Properties (PHP 8.1+) demo\nclass Test {\n    public readonly int $x;\n    public function __construct(int $x) { $this->x = $x; }\n}\n$t = new Test(42);\necho $t->x;\n?>`
};

const modal = new Modal();

// 2. Global state variables
let currentOpenTab = null;
let currentOpenFile = null;
let openTabs = [];
let editor = null;
let currentContextPath = null;
let isReady = false;
let currentPHPVersion;
let vfsDecoder = new TextDecoder('utf-8');
let vfsEncoder = new TextEncoder('utf-8');

const tabBar = document.getElementById('tabBar');
const tabContainer = document.querySelector('.tab-container');
const tabScrollLeftButton = document.querySelector('.tab-nav-button.left');
const tabScrollRightButton = document.querySelector('.tab-nav-button.right');

const phpVersionSelect = document.getElementById('phpVersion');
const runButton = document.getElementById('runButton');
const clearOutputButton = document.getElementById('clearOutput');
const resetVFSButton = document.getElementById('resetVFS');
const explorerHeader = document.querySelector('.explorer-header');
const newFileButton = document.getElementById('newFile');
const newFolderButton = document.getElementById('newFolder');
const refreshFilesButton = document.getElementById('refreshFiles');
const demoSelect = document.getElementById('demoSelect');
const fileTree = document.getElementById('fileTree');
const currentFileElement = document.getElementById('currentFile');
const contextMenu = document.getElementById('contextMenu');
const statusBar = document.getElementById('statusBar');
const output = document.getElementById('output');
const outputStatus = document.getElementById('outputStatus');
const previewButton = document.getElementById('previewButton');
const configurationButton = document.getElementById('configurationButton');

const editorPanel = document.getElementById('editorPanel') || document.querySelector('.editor-panel') || document.getElementById('editor')?.parentElement;

function resetVFS() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }
    try {
        Module.vfs.reset();
        refreshFileTree();
        updateStatus('VFS reset', 'success');
    } catch (error) {
        console.error('Error resetting VFS:', error);
        updateStatus('Error resetting VFS', 'error');
    }
}

function normalizePath(path) {
    if (!path || !path.length)
        return null;

    let normal = path.replace(/^vfs:\/\//, '');

    if (!normal.startsWith('/')) {
        normal = '/' + normal;
    }

    normal = normal.replace(/\/+/g, '/');

    return normal;
}

function renderTabs() {
    const tabContainer = document.querySelector('.tab-container');
    requestAnimationFrame(() => {
        const currentScroll = tabContainer.scrollLeft;
        tabContainer.innerHTML = '';
        openTabs.forEach((tab, index) => {
            tab.index = index;
            tab.handle = document.createElement('div');
            tab.handle.className = 'tab' + (currentOpenTab == tab ? ' active' : '');
            // Show a browser icon for browser tabs
            if (tab.type === 'browser') {
                tab.handle.innerHTML = '🖥️ ' + tab.name;
            } else {
                tab.handle.textContent = tab.name;
            }
            tab.handle.onclick = () => switchTabView(tab);
            // Close button
            tab.close = document.createElement('span');
            tab.close.textContent = ' ×';
            tab.close.className = 'tab-close';
            tab.close.onclick = (event) => {
                closeTabView(tab);
                event.stopPropagation();
            };
            tab.handle.appendChild(tab.close);
            tabContainer.appendChild(tab.handle);
        });
        const activeTab = tabContainer.querySelector('.tab.active');
        if (activeTab) {
            const containerWidth = tabContainer.clientWidth;
            const tabLeft = activeTab.offsetLeft;
            const tabWidth = activeTab.offsetWidth;
            const rightEdge = currentScroll + containerWidth;
            if (tabLeft < currentScroll || (tabLeft + tabWidth) > rightEdge) {
                const targetScroll = Math.max(0, tabLeft - (containerWidth - tabWidth) / 2);
                tabContainer.scrollTo({
                    left: targetScroll,
                    behavior: 'smooth'
                });
            } else {
                tabContainer.scrollLeft = currentScroll;
            }
        }
        tabContainer.offsetWidth;
        updateNavButtons();
        updateCurrentFileDisplay();
    });
}

function selectTab(path, name) {
    var path = normalizePath(path);
    let tab =  openTabs.find(selected => {
        return (selected.path === path) &&
               (selected.name === name);
    });

    if (!tab) {
        throw new Error(
            `Invalid Call, tab not found ` +
                `name=${name||'unknown'}, ` +
                `path=${path||'unknown'}`);
    }

    return tab;
}

function switchTabView(tab) {
    if ((currentOpenTab && tab) && (currentOpenTab !== tab)) {
        if (currentOpenTab.type !== 'browser') {
            const editorValue = editor.getValue();
            currentOpenTab.content = editorValue;
            if (currentOpenTab.path && currentOpenTab.vfsContent !== undefined) {
                currentOpenTab.unsaved =
                    (editorValue !== currentOpenTab.vfsContent);
            } else if (!currentOpenTab.path) {
                currentOpenTab.unsaved =
                    (editorValue !== demos[currentOpenTab.source]);
            }
        }
    }

    let browserContainer =
        editorPanel
            .querySelector('#browser-container');
    window.updateBrowserContainer(
        browserContainer, tab);

    let configurationContainer =
        editorPanel
            .querySelector('#configuration-container');
    window.updateConfigurationContainer(
        configurationContainer, tab);

    currentOpenTab = tab;
    // If browser tab, render browser UI, else show editor
    if (tab.type === 'browser') {
        if (editor && editor.getWrapperElement()) {
            editor.getWrapperElement().style.display = 'none';
        }
    } else {
        if (editor && editor.getWrapperElement()) {
            // Ensure the editor is attached and visible
            if (!editorPanel.contains(editor.getWrapperElement())) {
                editorPanel.appendChild(editor.getWrapperElement());
            }
            editor.getWrapperElement().style.display = '';
        }
        // Set editor mode and value
        const currentOpenExtension = currentOpenTab.path ?
            currentOpenTab.path.split('.').pop().toLowerCase() :
            currentOpenTab.name.split('.').pop().toLowerCase();
        let currentOpenMode = 'application/x-httpd-php';
        switch(currentOpenExtension) {
            case 'md':
            case 'markdown':
                currentOpenMode = 'markdown';
                break;
            case 'js':
                currentOpenMode = 'javascript';
                break;
            case 'css':
                currentOpenMode = 'css';
                break;
            case 'html':
            case 'htm':
                currentOpenMode = 'htmlmixed';
                break;
            case 'json':
                currentOpenMode = { name: 'javascript', json: true };
                break;
        }
        editor.setOption('mode', currentOpenMode);
        editor.setValue(currentOpenTab.content || '');
    }
    renderTabs();
}

// Add Preview button logic to open browser tab
if (previewButton) {
    previewButton.addEventListener('click', function() {
        // Only open one browser tab at a time
        let openBrowserTab = openTabs.find(
            tab => tab.type === 'browser');
        if (!openBrowserTab) {
            openTabs.push(window.browserTab);
        }
        switchTabView(
            openBrowserTab || window.browserTab);
    });
}

// Add Configuration button logic to open configuration tab
if (configurationButton) {
    configurationButton.addEventListener('click', function() {
        // Only open one browser tab at a time
        let openConfigurationTab = openTabs.find(
            tab => tab.type === 'configuration');
        if (!openConfigurationTab) {
            openTabs.push(window.configurationTab);
        }
        switchTabView(
            openConfigurationTab || window.configurationTab);
    });
}

function switchTab(path, name) {
    return switchTabView(
        selectTab(path, name));
}

function closeTabView(tab) {
    openTabs.splice(tab.index, 1);
    if (currentOpenTab == tab) {
        if (openTabs.length > 0) {
            switchTabView(openTabs[
                Math.max(0, tab.index - 1)]);
        } else {
            currentOpenTab = null;
            editor.setValue('');
            renderTabs();
        }
    } else {
        renderTabs();
    }
    updateCurrentFileDisplay();
}

function closeTab(path, name) {
    return closeTabView(
        selectTab(path, name));
}

function refreshFileTree() {
    if (!isReady || !Module || !Module.vfs) {
        return;
    }
    let iterator = null;
    try {
        iterator = Module.vfs.iterate('/');
        const files = iterator.all(true);
        renderFileTree(files);
    } catch (error) {
        console.error('Error refreshing file tree:', error);
        updateStatus('Error refreshing file tree', 'error');
    } finally {
        if (iterator) {
            iterator.free();
        }
    }
}

function renderFileTree(files, container = fileTree, basePath = '/') {
    if (container === fileTree) {
        const rootItem = container.querySelector('.file-item');
        const existingChildren = container.querySelector('.file-children');
        if (existingChildren) {
            existingChildren.remove();
        }
        if (files.length > 0) {
            const childrenContainer = document.createElement('div');
            childrenContainer.className = 'file-children expanded';
            renderFileItems(files, childrenContainer, '/');
            container.appendChild(childrenContainer);
            const expandIcon = rootItem.querySelector('.expand-icon');
            expandIcon.textContent = '▼';
            expandIcon.classList.add('expanded');
        }
    } else {
        renderFileItems(files, container, basePath);
    }
}

function renderFileItems(files, container, basePath) {
    container.innerHTML = '';
    files.forEach(file => {
        const item = document.createElement('div');
        item.className = 'file-item';
        let fullPath = basePath.endsWith('/') ? basePath + file.name : basePath + '/' + file.name;
        item.dataset.path = normalizePath(fullPath);
        if (file.kind === Module.vfs.EM_VFS_DIR) {
            item.classList.add('directory');
            const expandIcon = document.createElement('span');
            expandIcon.className = 'expand-icon';
            expandIcon.textContent = file.children ? '▼' : '▶';
            if (file.children) expandIcon.classList.add('expanded');
            item.appendChild(expandIcon);
            const icon = document.createElement('span');
            icon.className = 'file-icon';
            icon.textContent = '📁';
            item.appendChild(icon);
            const name = document.createElement('span');
            name.className = 'file-name';
            name.textContent = file.name;
            item.appendChild(name);
            container.appendChild(item);
            if (file.children) {
                const childrenContainer = document.createElement('div');
                childrenContainer.className = 'file-children expanded';
                renderFileItems(file.children, childrenContainer, basePath + file.name + '/');
                container.appendChild(childrenContainer);
            }
            expandIcon.addEventListener('click', (e) => {
                e.stopPropagation();
                toggleDirectory(item);
            });
        } else {
            const icon = document.createElement('span');
            icon.className = 'file-icon';
            icon.textContent = getFileIcon(file.name);
            item.appendChild(icon);
            const name = document.createElement('span');
            name.className = 'file-name';
            name.textContent = file.name;
            item.appendChild(name);
            container.appendChild(item);
        }
        item.addEventListener('click', () => selectFile(item));
        item.addEventListener('dblclick', () => openFile(item.dataset.path));
        item.addEventListener('contextmenu', (e) => showContextMenu(e, item.dataset.path));
    });
}

function getFileIcon(filename) {
    const ext = filename.split('.').pop().toLowerCase();
    switch (ext) {
        case 'php': return '🐘';
        case 'js': return '📜';
        case 'json': return '📋';
        case 'txt': return '📄';
        case 'md': return '📝';
        case 'html': case 'htm': return '🌐';
        case 'css': return '🎨';
        case 'sql': return '🗃️';
        default: return '📄';
    }
}

function toggleDirectory(item) {
    const expandIcon = item.querySelector('.expand-icon');
    const nextSibling = item.nextElementSibling;
    if (nextSibling && nextSibling.classList.contains('file-children')) {
        if (nextSibling.classList.contains('expanded')) {
            nextSibling.classList.remove('expanded');
            expandIcon.textContent = '▶';
            expandIcon.classList.remove('expanded');
        } else {
            nextSibling.classList.add('expanded');
            expandIcon.textContent = '▼';
            expandIcon.classList.add('expanded');
        }
    }
}

function selectFile(item) {
    document.querySelectorAll('.file-item.selected').forEach(el => {
        el.classList.remove('selected');
    });
    item.classList.add('selected');
}

function openFile(path) {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }

    var path = normalizePath(path);

    const content = Module.vfs.get(path);
    if (content === false) {
        updateStatus(`Failed to open file: ${path}`, 'error');
        return;
    }

    let fileTab = openTabs.find(tab => tab.path === path);

    try {
        const vfsContent = vfsDecoder.decode(content);

        if (!fileTab) {
            // Create a new tab for new content
            const newTab = {
                path: path,
                name: path.split('/').pop(),
                content:    vfsContent,
                vfsContent: vfsContent,
                unsaved: false
            };

            openTabs.push(fileTab = newTab);
        } else {
            // Update existing tab
            if (fileTab.content !== vfsContent) {
                // Content has changed
                fileTab.unsaved = true;
            }
        }

        switchTabView(fileTab);
        updateStatus(
            `Opened: ${fileTab.path}`, 'success');
    } catch (error) {
        console.error('Error opening file:', error);
        updateStatus(
            `Error opening: ${path}`, 'error');
    }
}

async function saveCurrentFile() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }

    if (!currentOpenTab) {
        throw Error("Invalid Call, current tab unknown");
    }

    if (!currentOpenTab.path) {
        let response = '';
        try {
            response = await modal.show(
                'Save File',
                'Enter a filename to save your code',
                '',
                { text: 'Save' },
                { text: 'Cancel' }
            );
        } catch {
            // Modal cancelled
            updateStatus('Save cancelled', 'error');
            return;
        }
        response = (response || '').trim();
        if (!response) {
            updateStatus('Please enter a filename', 'error');
            return;
        }

        currentOpenTab.path = normalizePath(response);
        currentOpenTab.name =
            currentOpenTab.path.split('/').pop()
        currentOpenTab.source = null;
    }

    try {
        // Always encode as UTF-8 Uint8Array
        const value = editor.getValue();
        if (Module.vfs.put(currentOpenTab.path, vfsEncoder.encode(value))) {
            currentOpenTab.unsaved    = false;
            currentOpenTab.vfsContent = value;
            updateStatus(
                `Saved: ${currentOpenTab.path}`, 'success');
        } else {
            console.error(`Failed to save: ${currentOpenTab.path}`);
            updateStatus(`Failed to save: ${currentOpenTab.path}`, 'error');
        }
    } catch (error) {
        console.error(`Error saving file: ${currentOpenTab.path}`, error);
        updateStatus(`Error saving file: ${currentOpenTab.path}`, 'error');
    } finally {
        refreshFileTree();
        renderTabs();
    }
}

function updateCurrentFileDisplay() {
    if (currentOpenTab) {
        if (currentOpenTab.path) {
            const currentOpenFileName =
                currentOpenTab.path.split('/').pop();
            currentFileElement.textContent =
                currentOpenFileName +
                    (currentOpenTab.unsaved ?
                        ' •' : '');
            return;
        }

        currentFileElement.textContent =
            currentOpenTab.name +
                (currentOpenTab.unsaved ?
                    ' •' : '');
    } else {
        currentFileElement.textContent = 'PHP Editor';
    }
}

function showContextMenu(e, path) {
    e.preventDefault();
    currentContextPath = path;
    let isFile = false;
    if (Module && Module.vfs && path) {
        let testPath = path.endsWith('/') ? path.slice(0, -1) : path;
        try {
            let parentPath = testPath.substring(0, testPath.lastIndexOf('/') + 1);
            let name = testPath.substring(testPath.lastIndexOf('/') + 1);
            let iter = Module.vfs.iterate(parentPath);
            let found = false;
            if (iter.reset()) {
                do {
                    if (iter.name() === name) {
                        isFile = (iter.kind() === Module.vfs.EM_VFS_FILE);
                        found = true;
                        break;
                    }
                } while (iter.next());
            }
            iter.free();
        } catch (err) {}
    }
    const runMenuItem = contextMenu.querySelector('[data-action="run"]');
    const downloadMenuItem = contextMenu.querySelector('[data-action="download"]');
    if (runMenuItem) {
        runMenuItem.style.display =
            isFile ? '' : 'none';
    }
    if (downloadMenuItem) {
        downloadMenuItem.style.display =
            isFile ? '' : 'none';
    }
    contextMenu.style.display = 'block';
    contextMenu.style.left = e.pageX + 'px';
    contextMenu.style.top = e.pageY + 'px';
}

function hideContextMenu() {
    contextMenu.style.display = 'none';
}

async function createFile() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }
    let filename = '';
    try {
        filename = await modal.show(
            'New File',
            '',
            '',
            { text: 'Create' },
            { text: 'Cancel' }
        );
    } catch {
        // Modal cancelled
        return;
    }
    filename = (filename || '').trim();
    if (!filename) {
        updateStatus('Please enter a filename', 'error');
        return;
    }
    try {
        const success = Module.vfs.put(
            filename,
            vfsEncoder.encode(
                '/* ' + filename + ' */')
        );
        if (success) {
            refreshFileTree();
            openFile(filename);
            updateStatus(`Created: ${filename}`, 'success');
        } else {
            updateStatus(`Failed to create: ${filename}`, 'error');
        }
    } catch (error) {
        console.error('Error creating file:', error);
        updateStatus(`Error creating file: ${filename}`, 'error');
    }
}

async function createFolder() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }
    let foldername = '';
    try {
        foldername = await modal.show(
            'New Folder',
            '',
            '',
            { text: 'Create' },
            { text: 'Cancel' }
        );
    } catch {
        // Modal cancelled
        return;
    }
    foldername = (foldername || '').trim();
    if (!foldername) {
        updateStatus('Please enter a folder name', 'error');
        return;
    }
    const path = foldername.endsWith("/") ? foldername : foldername + "/";
    try {
        const success = Module.vfs.mkdir(path);
        if (success) {
            refreshFileTree();
            updateStatus(`Created folder: ${foldername}`, 'success');
        } else {
            updateStatus(`Failed to create folder: ${foldername}`, 'error');
        }
    } catch (error) {
        console.error('Error creating folder:', error);
        updateStatus(`Error creating folder: ${foldername}`, 'error');
    }
}

function performRename(newName) {
    if (!currentContextPath) {
        return;
    }
    newName = (newName || '').trim();
    if (!newName) {
        updateStatus('Please enter a new name', 'error');
        return;
    }
    
    let newPath;
    if (newName.startsWith('/')) {
        // Absolute path - use as is
        newPath = normalizePath(newName);
    } else {
        // Relative path - resolve relative to current file's directory
        const parentDir = currentContextPath.substring(0, currentContextPath.lastIndexOf('/'));
        if (newName.includes('/')) {
            // If relative path contains directories, resolve against parent
            newPath = normalizePath(parentDir + '/' + newName);
        } else {
            // Simple rename in same directory
            newPath = parentDir + '/' + newName;
        }
    }
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }
    try {
        const success = Module.vfs.move(currentContextPath, newPath);
        if (success) {
            updateStatus(`Renamed to: ${newName}`, 'success');
            if (currentOpenTab.path === currentContextPath) {
                currentOpenTab.path = newPath;
            }
            refreshFileTree();
            renderTabs();
        } else {
            updateStatus(`Failed to rename: ${currentContextPath}`, 'error');
        }
    } catch (error) {
        log.error(
            `Error renaming: ${currentContextPath} -> ${newPath}`, error)
        updateStatus(`Error renaming: {currentContextPath} -> ${newPath}`, 'error');
    }
}

function renameFile() {
    if (!currentContextPath) {
        return;
    }
    const currentName = currentContextPath.split('/').pop();
    (async () => {
        let response = null;
        try {
            response = await modal.show(
                'Rename',
                '',
                currentName,
                { text: 'Rename' },
                { text: 'Cancel' }
            );
        } catch {
            return;
        }
        response = (response || '').trim();
        if (!response) {
            updateStatus('Please enter a new name', 'error');
            return;
        }
        performRename(response);
    })();
}

function downloadFile() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }

    if (!currentContextPath) return;
    try {
        const content = Module.vfs.get(currentContextPath);
        if (content === false) {
            updateStatus(`Failed to read file: ${currentContextPath}`, 'error');
            return;
        }
        const blob = new Blob([content], { type: 'application/octet-stream' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.style.display = 'none';
        a.href = url;
        a.download = currentContextPath.split('/').pop();
        document.body.appendChild(a);
        a.click();
        window.URL.revokeObjectURL(url);
        document.body.removeChild(a);
        updateStatus(`Downloaded: ${currentContextPath}`, 'success');
    } catch (error) {
        console.error('Error downloading file:', error);
        updateStatus(`Error downloading: ${currentContextPath}`, 'error');
    }
}

function deleteFile() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }

    if (!currentContextPath) return;

    try {
        modal.show(
            'Confirmation',
            `Are you sure you want to delete ${currentContextPath}?`,
            null,
            { text: 'Yes' },
            { text: 'Cancel' }
        ).then((response) => {
            if (Module.vfs.unlink(currentContextPath, true)) {
                const deletedTab = openTabs.find(
                    tab => tab.path === currentContextPath);
                if (deletedTab) {
                    closeTabView(deletedTab);
                }
                refreshFileTree();
                updateStatus(`Deleted: ${currentContextPath}`, 'success');
            } else {
                updateStatus(`Failed to delete: ${currentContextPath}`, 'error');
            }
        }).catch(() => {});
    } catch (error) {
        console.error('Error deleting file:', error);
        updateStatus(`Error deleting: ${currentContextPath}`, 'error');
    }
}

function updateNavButtons() {
    if (!tabContainer ||
        !tabScrollLeftButton ||
        !tabScrollRightButton) {
        return;
    }

    // Get all scroll values
    const scrollLeft = Math.floor(tabContainer.scrollLeft);
    const scrollWidth = tabContainer.scrollWidth;
    const clientWidth = tabContainer.clientWidth;
    const maxScroll = scrollWidth - clientWidth;

    tabScrollLeftButton.disabled = scrollLeft <= 0;
    tabScrollRightButton.disabled = scrollLeft >= maxScroll;
}

function initializeEventHandlers() {
    // Initial check
    updateNavButtons();

    tabScrollLeftButton.onclick = (e) => {
        e.preventDefault();
        tabContainer.scrollBy({
            left: -100,
            behavior: 'smooth'
        });
        setTimeout(updateNavButtons, 100);
    };

    tabScrollRightButton.onclick = (e) => {
        e.preventDefault();
        tabContainer.scrollBy({
            left: 100,
            behavior: 'smooth'
        });
        setTimeout(updateNavButtons, 100);
    };
    
    // Update buttons when tabs change or container scrolls
    tabContainer.addEventListener('scroll',
        () => requestAnimationFrame(updateNavButtons));

    // Watch for resize and mutations
    new ResizeObserver(
        () => requestAnimationFrame(updateNavButtons)
    ).observe(tabContainer);
    new MutationObserver(
        () => requestAnimationFrame(updateNavButtons)
    ).observe(tabContainer, { childList: true, subtree: true });

    phpVersionSelect.value = currentPHPVersion;
    phpVersionSelect.addEventListener('change', switchPHPVersion);
    runButton.addEventListener('click', runCode);
    clearOutputButton.addEventListener('click', clearOutput);
    resetVFSButton.addEventListener('click', () => {
        resetVFS();
        refreshFileTree();
    });
    demoSelect.addEventListener('change', loadDemo);
    newFileButton.addEventListener('click', () => {
        createFile();
    });
    newFolderButton.addEventListener('click', () => {
        createFolder();
    });
    refreshFilesButton.addEventListener('click', refreshFileTree);
    // All modal OK/cancel logic is now handled by Modal class
    document.addEventListener('click', hideContextMenu);
    contextMenu.addEventListener('click', (e) => {
        e.stopPropagation();
        const item = e.target.closest('.context-menu-item');
        if (!item) return;
        const action = item.dataset.action;
        switch (action) {
            case 'run':
                runFile(currentContextPath);
                break;
            case 'open':
                openFile(currentContextPath);
                break;
            case 'download':
                downloadFile();
                break;
            case 'rename':
                renameFile();
                break;
            case 'delete':
                deleteFile();
                break;
        }
        hideContextMenu();
    });
    const rootItem = fileTree.querySelector('.file-item');
    rootItem.addEventListener('click', () => selectFile(rootItem));
    const rootExpandIcon = rootItem.querySelector('.expand-icon');
    rootExpandIcon.addEventListener('click', (e) => {
        e.stopPropagation();
        toggleDirectory(rootItem);
    });
    document.addEventListener('keydown', (e) => {
        if (e.ctrlKey || e.metaKey) {
            switch (e.key) {
                case 's':
                    e.preventDefault();
                    saveCurrentFile();
                    break;
                case 'n':
                    e.preventDefault();
                    createFile();
                    break;
                case 't':
                    e.preventDefault();
                    let baseName = 'untitled.php';
                    let name = baseName;
                    let counter = 1;
                    while (openTabs.some(t => !t.path && t.name === name)) {
                        name = baseName.replace('.php', `-${counter}.php`);
                        counter++;
                    }
                    openTabs.push({ path: null, name, content: '', unsaved: true });
                    switchTab(null, name);
                    break;
            }
        }
    });
}

function switchPHPVersion() {
    const newVersion = phpVersionSelect.value;
    if (newVersion !== currentPHPVersion) {
        // Save the selected PHP version in localStorage
        localStorage.setItem('em-php-version', newVersion);
        if (editor) {
            const currentCode = editor.getValue();
            if (currentCode) {
                sessionStorage.setItem('em-demo-code', currentCode);
            }
        }
        // Reload the page to apply the new version
        window.location.reload();
    }
}

function loadDemoName(selected) {
    let counter = 0;
    while (openTabs.some(tab => tab.source == selected && 
           tab.name == `untitled-${selected}${counter ? `-${counter}` : ''}.php`)) {
        counter++;
    }
    
    return `untitled-${selected}${counter ? `-${counter}` : ''}.php`;
}

function loadDemo() {
    const selectedDemo = demoSelect.value;
    if (selectedDemo && demos[selectedDemo]) {
        const newTab = {
            path: null,
            source: selectedDemo,
            name: loadDemoName(selectedDemo),
            content: demos[selectedDemo],
            unsaved: false };
        openTabs.push(newTab);
        try {
            switchTabView(newTab);
            updateStatus(
                `Demo loaded: ${newTab.name}`, 'success');
        } catch (error) {
            console.error(`Demo failed to load: ${newTab.name}`, error);
            updateStatus(
                `Demo failed to load: ${newTab.name}`, 'error');
        } finally {
            demoSelect.value = '';
        }
    }
}

function clearOutput() {
    output.textContent = '';
    outputStatus.textContent = '';
}

function updateStatus(message, type) {
    if (!statusBar) return;
    statusBar.textContent = message;
    statusBar.classList.remove(
        'loading', 'error', 'success');
    if (type) {
        statusBar.classList.add(type);
    }
}

// Run a file using Module.include and display output
function runFile(path) {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }
    try {
        updateStatus(`Running: ${path}`, 'loading');
        outputStatus.textContent = 'Running...';
        output.textContent = Module.include(path);
        updateStatus(`Complete: ${path}`, 'success');
        outputStatus.textContent = 'Complete';
    } catch (error) {
        output.textContent = `Error: ${error.message}`;
        console.error(
            `Execution failed: ${path}`, error);
        updateStatus(`Execution failed: ${path}`, 'error');
        outputStatus.textContent = 'Error';
    }
}

function initializeEditor() {
    editor = CodeMirror.fromTextArea(document.getElementById('codeEditor'), {
        mode: 'application/x-httpd-php',
        theme: 'monokai',
        lineNumbers: true,
        indentUnit: 4,
        indentWithTabs: false,
        lineWrapping: true,
        extraKeys: {
            'Ctrl-Enter': runCode,
            'Cmd-Enter': runCode,
            'Ctrl-S': saveCurrentFile,
            'Cmd-S': saveCurrentFile
        }
    });

    editor.on('change', () => {
        if (currentOpenTab) {
            const currentContent = editor.getValue();
            if (currentOpenTab.path) {
                // Compare against vfs content for files
                currentOpenTab.unsaved = 
                    (currentContent !== currentOpenTab.vfsContent);
            } else {
                // Compare against demo for demos
                currentOpenTab.unsaved =
                    (currentContent !== demos[currentOpenTab.source]);
            }

            updateCurrentFileDisplay();
        }
    });

    const initialContent =
        sessionStorage.getItem('em-demo-code');
    const initialTab = {
        name: 'untitled.php',
        path: null,
        content: initialContent ?
            initialContent : demos["hello"],
        unsaved: false,
    };
    openTabs.push(initialTab);
    switchTabView(initialTab);

    if (initialContent) {
        sessionStorage.removeItem('em-demo-code');
    }

    renderTabs();
}

function initializeGithubButton() {
    const githubBtn = document.getElementById('githubBtn');
    const githubContextMenu = document.getElementById('githubContextMenu');
    let contextMenuVisible = false;

    githubBtn.addEventListener('click', (e) => {
        e.preventDefault();
        e.stopPropagation();
        // Position context menu below the button
        const rect = githubBtn.getBoundingClientRect();
        githubContextMenu.style.left = rect.left + 'px';
        githubContextMenu.style.top = (rect.bottom + window.scrollY) + 'px';
        githubContextMenu.style.display = 'block';
        contextMenuVisible = true;
    });

    // Hide menu on click elsewhere
    document.addEventListener('click', (e) => {
        if (contextMenuVisible) {
            githubContextMenu.style.display = 'none';
            contextMenuVisible = false;
        }
    });

    githubContextMenu.addEventListener('click', async (e) => {
        e.stopPropagation();
        const item = e.target.closest('.context-menu-item');
        if (!item) return;
        githubContextMenu.style.display = 'none';
        contextMenuVisible = false;
        switch (item.dataset.action) {
            case 'set-token': {
                let token = '';
                try {
                    token = await modal.show(
                        'Set GitHub Token',
                        'Paste your GitHub personal access token',
                        githubToken || '',
                        { text: 'Save' },
                        { text: 'Cancel' }
                    );
                } catch {
                    return;
                }
                token = (token || '').trim();
                if (token) {
                    githubToken = token;
                    localStorage.setItem('em-github-token', token);
                    updateStatus('GitHub token saved', 'success');
                }
                break;
            }
            case 'load-repo': {
                let repoInput = '';
                try {
                    repoInput = await modal.show(
                        'Load from GitHub Repo',
                        'user/repo or full URL',
                        '',
                        { text: 'Load' },
                        { text: 'Cancel' }
                    );
                } catch {
                    return;
                }
                repoInput = (repoInput || '').trim();
                if (repoInput) {
                    loadGithubRepo(repoInput);
                }
                break;
            }
            case 'load-gist': {
                let gistInput = '';
                try {
                    gistInput = await modal.show(
                        'Load from GitHub Gist',
                        'gist id or gist URL',
                        '',
                        { text: 'Load' },
                        { text: 'Cancel' }
                    );
                } catch {
                    return;
                }
                gistInput = (gistInput || '').trim();
                if (gistInput) {
                    loadGithubGist(gistInput);
                }
                break;
            }
        }
    });
}

// Store GitHub token in localStorage
let githubToken = localStorage.getItem('em-github-token') || '';

// Progress bar logic
let progressBar = null;
function showProgressBar(label, max) {
    if (!progressBar) {
        progressBar = document.createElement('div');
        progressBar.className = 'progress-bar-overlay';
        progressBar.innerHTML = `
            <div class="progress-bar-container">
                <span class="progress-bar-label"></span>
                <div class="progress-bar-track"><div class="progress-bar-fill"></div></div>
            </div>
        `;
        document.body.appendChild(progressBar);
    }
    progressBar.style.display = 'flex';
    progressBar.querySelector('.progress-bar-label').textContent = label;
    progressBar.querySelector('.progress-bar-fill').style.width = '0%';
    progressBar.max = max;
    progressBar.value = 0;
}
function updateProgressBar(label, value, max) {
    if (!progressBar) return;
    progressBar.querySelector('.progress-bar-label').textContent = label;
    const percent = max ? Math.round((value / max) * 100) : 0;
    progressBar.querySelector('.progress-bar-fill').style.width = percent + '%';
}
function hideProgressBar() {
    if (progressBar) progressBar.style.display = 'none';
}

// Load a GitHub repo (user/repo or URL) using API for file content (no CORS issues)
async function loadGithubRepo(repoInput) {
    let repo = repoInput;
    if (repo.startsWith('https://github.com/')) {
        repo = repo.replace('https://github.com/', '').replace(/\.git$/, '');
    }
    repo = repo.replace(/^\//, '').replace(/\/$/, '');
    if (!repo.match(/^[\w.-]+\/[\w.-]+$/)) {
        updateStatus('Invalid repo format', 'error');
        return;
    }
    showProgressBar('Fetching repo file list...', 1);
    try {
        let branch = 'main';
        let repoMeta = await githubApiRequest(`/repos/${repo}`);
        if (repoMeta && repoMeta.default_branch) branch = repoMeta.default_branch;
        let tree = await githubApiRequest(`/repos/${repo}/git/trees/${branch}?recursive=1`);
        if (!tree || !tree.tree) throw new Error('No tree found');
        Module.vfs.reset();
        let fileEntries = tree.tree.filter(e => e.type === 'blob');
        let dirEntries = tree.tree.filter(e => e.type === 'tree');
        // Create directories first
        for (const dir of dirEntries) {
            Module.vfs.mkdir('/' + dir.path + '/');
        }
        let fileCount = 0;
        let total = fileEntries.length;
        for (let i = 0; i < fileEntries.length; i++) {
            const entry = fileEntries[i];
            updateProgressBar(`Fetching: ${entry.path}`, i, total);
            // Use GitHub API to fetch file content (avoid CORS)
            let blob = await githubApiRequest(`/repos/${repo}/git/blobs/${entry.sha}`);
            let contentBytes;
            if (blob.encoding === 'base64') {
                // Decode base64 to Uint8Array
                const binaryStr = atob(blob.content.replace(/\n/g, ''));
                contentBytes = new Uint8Array(binaryStr.length);
                for (let j = 0; j < binaryStr.length; j++) {
                    contentBytes[j] = binaryStr.charCodeAt(j);
                }
            } else {
                // Fallback: treat as UTF-8 string
                contentBytes = vfsEncoder.encode(blob.content);
            }
            Module.vfs.put('/' + entry.path, contentBytes);
            fileCount++;
        }
        refreshFileTree();
        updateStatus(`Loaded ${fileCount} files from ${repo}`, 'success');
    } catch (err) {
        updateStatus('GitHub repo load failed: ' + err.message, 'error');
    } finally {
        hideProgressBar();
    }
}

// Load a GitHub gist (id or URL) with progress bar
async function loadGithubGist(gistInput) {
    let gistId = gistInput;
    if (gistId.startsWith('https://gist.github.com/')) {
        gistId = gistId.split('/').pop();
    }
    gistId = gistId.replace(/\/.*/, '');
    if (!gistId.match(/^[a-fA-F0-9]+$/)) {
        updateStatus('Invalid gist id', 'error');
        return;
    }
    showProgressBar('Fetching gist...', 1);
    try {
        let gist = await githubApiRequest(`/gists/${gistId}`);
        if (!gist || !gist.files) throw new Error('No files in gist');
        Module.vfs.reset();
        let files = Object.entries(gist.files);
        let fileCount = 0;
        let total = files.length;
        for (let i = 0; i < files.length; i++) {
            const [fname, file] = files[i];
            updateProgressBar(`Fetching: ${fname}`, i, total);
            if (file && file.content !== undefined) {
                // Always encode as UTF-8 bytes for VFS
                const contentBytes = vfsEncoder.encode(file.content);
                Module.vfs.put('/' + fname, contentBytes);
                fileCount++;
            }
        }
        refreshFileTree();
        updateStatus(`Loaded ${fileCount} files from gist ${gistId}`, 'success');
    } catch (err) {
        updateStatus('GitHub gist load failed: ' + err.message, 'error');
    } finally {
        hideProgressBar();
    }
}

// Helper: GitHub API request (uses token if set)
async function githubApiRequest(path) {
    const url = 'https://api.github.com' + path;
    const headers = { 
        'Accept': 'application/vnd.github.v3+json' 
    };

    if (githubToken)
        headers['Authorization'] = 'token ' + githubToken;
    
    const resp = await fetch(url, { headers });
    
    if (!resp.ok) 
        throw new Error(`GitHub API error: ${resp.status}`);
    
    return await resp.json();
}

function runCode() {
    if (!isReady || !Module) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }

    if (currentOpenTab && currentOpenTab.path) {
        if (currentOpenTab.unsaved) {
            const currentOpenTabContent = editor.getValue();
            if (Module.vfs.put(
                    currentOpenTab.path, vfsEncoder.encode(currentOpenTabContent))) {
                currentOpenTab.content = currentOpenTabContent;
                currentOpenTab.unsaved = false;
                updateStatus(`Saved: ${currentOpenTab.path}`, 'success');
                refreshFileTree();
                renderTabs();
            } else {
                updateStatus(`Failed to save: ${currentOpenTab.path}`, 'error');
                return;
            }
        }
        runButton.disabled = true;
        updateStatus(`Running: ${currentOpenTab.path}`, 'loading');
        outputStatus.textContent = 'Running...';
        try {
            const result = Module.include(currentOpenTab.path);
            output.textContent = result;
            updateStatus(`Ran: ${currentOpenTab.path}`, 'success');
            outputStatus.textContent = 'Complete';
        } catch (error) {
            output.textContent = `Error: ${error.message}`;
            updateStatus(`Execution failed: ${currentOpenTab.path}`, 'error');
            outputStatus.textContent = 'Error';
            console.error('Execution error:', error);
        } finally {
            runButton.disabled = false;
            refreshFileTree();
        }
    } else {
        const currentEditorCode = editor.getValue().trim();
        
        runButton.disabled = true;
        updateStatus('Running code...', 'loading');
        outputStatus.textContent = 'Running...';
        try {
            const result = Module.invoke(currentEditorCode);
            output.textContent = result;
            updateStatus('Code executed successfully', 'success');
            outputStatus.textContent = 'Complete';
        } catch (error) {
            output.textContent = `Error: ${error.message}`;
            updateStatus('Execution failed', 'error');
            outputStatus.textContent = 'Error';
            console.error('Execution error:', error);
        } finally {
            runButton.disabled = false;
            refreshFileTree();
        }
    }
}

async function initializeBrowser() {
    await window.setUpBrowserContainer(
        document.getElementById(
            "browser-container"));
}

async function initializeConfiguration() {
    await window.setUpConfigurationContainer(
        document.getElementById(
            "configuration-container"));
}

function loadPHPRuntime() {
    updateStatus(`Loading PHP ${currentPHPVersion}...`, 'loading');
    if (typeof editorStatus !== 'undefined' && editorStatus)
        editorStatus.textContent = 'Loading...';
    const script = document.createElement('script');
    script.src = `./latest/${currentPHPVersion}/php-em.js`;
    script.onload = () => {
        const checkReady = () => {
            if (typeof Module !== 'undefined' && Module && Module.ready) {
                isReady = true;
                updateStatus(`PHP ${currentPHPVersion} ready - Press Ctrl+Enter to run code`, 'success');
                if (typeof editorStatus !== 'undefined' && editorStatus)
                    editorStatus.textContent = `PHP ${currentPHPVersion}`;
                runButton.disabled = false;
                output.textContent = 'Ready! Click "Run Code" or press Ctrl+Enter to execute PHP code.';
                if (typeof newFileButton !== 'undefined' && newFileButton) newFileButton.disabled = false;
                if (typeof newFolderButton !== 'undefined' && newFolderButton) newFolderButton.disabled = false;
                if (typeof refreshFilesButton !== 'undefined' && refreshFilesButton) refreshFilesButton.disabled = false;
                Module.addEventListener(
                    "vfs.modified", refreshFileTree);
                refreshFileTree();
            } else {
                setTimeout(checkReady, 100);
            }
        };
        checkReady();
    };
    script.onerror = () => {
        updateStatus(`Failed to load PHP ${currentPHPVersion}`, 'error');
        if (typeof editorStatus !== 'undefined' && editorStatus) editorStatus.textContent = 'Error';
        output.textContent = `Failed to load PHP ${currentPHPVersion}. Make sure the files are available at ./latest/${currentPHPVersion}/php-em.js`;
    };
    document.head.appendChild(script);
}

document.addEventListener('DOMContentLoaded', async () => {
    // Get PHP version from localStorage or default to 8.3
    currentPHPVersion = localStorage.getItem('em-php-version') || '8.3';
    initializeEditor();
    await initializeBrowser();
    initializeConfiguration();
    initializeEventHandlers();
    initializeGithubButton();
    loadPHPRuntime();
});

window.openTabs = openTabs;
window.switchTabView = switchTabView;
