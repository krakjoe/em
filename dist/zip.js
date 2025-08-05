/**
 * ZIP Import/Export functionality for em PHP editor
 * Requires JSZip library: https://cdnjs.cloudflare.com/ajax/libs/jszip/3.10.1/jszip.min.js
 */

// ZIP Import/Export Module
const ZipManager = {
    // Import ZIP file to VFS
    async importZip(file) {
        if (!window.JSZip) {
            throw new Error('JSZip library not loaded');
        }

        if (!isReady || !Module || !Module.vfs) {
            throw new Error('PHP runtime not ready');
        }

        try {
            const zip = await JSZip.loadAsync(file);
            let importedCount = 0;
            const errors = [];

            // Process all files in the ZIP
            for (const [relativePath, zipEntry] of Object.entries(zip.files)) {
                if (!zipEntry.dir) {
                    try {
                        // Always get file as Uint8Array for binary safety
                        const content = await zipEntry.async('uint8array');

                        // Create directory if needed
                        const pathParts = relativePath.split('/');
                        if (pathParts.length > 1) {
                            let currentPath = '';
                            for (let i = 0; i < pathParts.length - 1; i++) {
                                currentPath += (currentPath ? '/' : '') + pathParts[i];
                                try {
                                    Module.vfs.mkdir(currentPath);
                                } catch (e) {
                                    // Directory might already exist, ignore error
                                }
                            }
                        }

                        // Write file to VFS (must be Uint8Array)
                        const success = Module.vfs.put(relativePath, content);
                        if (success) {
                            importedCount++;
                        } else {
                            errors.push(`Failed to write: ${relativePath}`);
                        }
                    } catch (error) {
                        errors.push(`Error processing ${relativePath}: ${error.message}`);
                    }
                }
            }

            return {
                success: true,
                importedCount,
                errors,
                message: `Imported ${importedCount} files` + (errors.length ? ` (${errors.length} errors)` : '')
            };
        } catch (error) {
            return {
                success: false,
                error: error.message,
                message: `Failed to import ZIP: ${error.message}`
            };
        }
    },

    // Export VFS to ZIP file
    async exportZip(filename = 'vfs-export.zip') {
        if (!window.JSZip) {
            throw new Error('JSZip library not loaded');
        }

        if (!isReady || !Module || !Module.vfs) {
            throw new Error('PHP runtime not ready');
        }

        try {
            const zip = new JSZip();
            let exportedCount = 0;

            // Get all files from VFS
            const iterator = Module.vfs.iterate('/');
            const files = iterator.all(true);
            iterator.free();

            // Add files to ZIP recursively
            const addFilesToZip = (fileList, basePath = '') => {
                fileList.forEach(file => {
                    const fullPath = basePath + file.name;
                    
                    if (file.kind === Module.vfs.EM_VFS_FILE) {
                        // It's a file - get content and add to ZIP
                        const content = Module.vfs.get(fullPath);
                        if (content !== false) {
                            // Add as Uint8Array for binary safety
                            zip.file(fullPath, content);
                            exportedCount++;
                        }
// NOTE: For GitHub import, ensure you fetch and pass Uint8Array/binary data to vfs.put, never JS strings.
                    } else if (file.kind === Module.vfs.EM_VFS_DIR && file.children) {
                        // It's a directory with children - recurse
                        addFilesToZip(file.children, fullPath + '/');
                    }
                });
            };

            addFilesToZip(files);

            if (exportedCount === 0) {
                return {
                    success: false,
                    message: 'No files to export'
                };
            }

            // Generate ZIP file
            const zipBlob = await zip.generateAsync({ type: 'blob' });
            
            // Download the file
            const url = URL.createObjectURL(zipBlob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);

            return {
                success: true,
                exportedCount,
                message: `Exported ${exportedCount} files to ${filename}`
            };
        } catch (error) {
            return {
                success: false,
                error: error.message,
                message: `Failed to export ZIP: ${error.message}`
            };
        }
    }
};

// UI Event Handlers
function handleImportZip() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.zip';
    input.onchange = async (e) => {
        const file = e.target.files[0];
        if (!file) return;

        updateStatus('Importing ZIP file...', 'loading');
        
        try {
            const result = await ZipManager.importZip(file);
            
            if (result.success) {
                updateStatus(result.message, 'success');
                refreshFileTree();
                
                if (result.errors.length > 0) {
                    console.warn('Import errors:', result.errors);
                }
            } else {
                updateStatus(result.message, 'error');
                console.error('Import failed:', result.error);
            }
        } catch (error) {
            updateStatus(`Import failed: ${error.message}`, 'error');
            console.error('Import error:', error);
        }
    };
    input.click();
}

function handleExportZip() {
    if (!isReady || !Module || !Module.vfs) {
        updateStatus('PHP runtime not ready', 'error');
        return;
    }

    // Check if there are any files to export
    let iterator = Module.vfs.iterate('vfs://');
    const files = iterator.all(true);
    iterator.free();

    if (!files || files.length === 0) {
        updateStatus('No files to export', 'error');
        return;
    }

    const timestamp = new Date().toISOString().slice(0, 19).replace(/[T:]/g, '-');
    const filename = `vfs-export-${timestamp}.zip`;
    
    updateStatus('Exporting to ZIP...', 'loading');
    
    ZipManager.exportZip(filename)
        .then(result => {
            if (result.success) {
                updateStatus(result.message, 'success');
            } else {
                updateStatus(result.message, 'error');
                if (result.error) {
                    console.error('Export failed:', result.error);
                }
            }
        })
        .catch(error => {
            updateStatus(`Export failed: ${error.message}`, 'error');
            console.error('Export error:', error);
        });
}

// Initialize ZIP functionality when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    // Add event listeners to import/export buttons
    const importButton = document.getElementById('importZip');
    const exportButton = document.getElementById('exportZip');
    
    if (importButton) {
        importButton.addEventListener('click', handleImportZip);
    }
    
    if (exportButton) {
        exportButton.addEventListener('click', handleExportZip);
    }
});
