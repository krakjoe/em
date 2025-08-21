/**
 * VFS Image Import/Export functionality for em PHP editor
 * VFS images are binary snapshots of all or part of the VFS, suitable for backup/restore.
 *
 * Usage:
 * - Export: creates a .vfsi file containing the VFS image.
 * - Import: loads a .vfsi file and restores the VFS state.
 */

const VFSManager = {

    async exportVFS(path = '/', filename = 'em.vfsi') {
        if (!isReady || !Module || !Module.vfs) {
            return {
                success: false,
                message: `Failed to export VFS image, Module not ready`
            };
        }

        try {
            // Get VFS image as Uint8Array
            const memory = new Module.vfs.Memory(path);

            await memory.load();

            const image = Module.HEAPU8.slice(
                memory.address,
                memory.address + memory.header.size.consumed
            );

            // Create Blob and download
            const blob = new Blob([image], { type: 'application/octet-stream' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            return {
                success: true,
                message: `Exported VFS image to ${filename}`
            };
        } catch (error) {
            return {
                success: false,
                error: error.message,
                message: `Failed to export VFS image: ${error.message}`
            };
        }
    },

    async importVFS(file) {
        if (!isReady || !Module || !Module.vfs) {
            return {
                success: false,
                message: `Failed to import VFS image, Module not ready`
            };
        }

        try {
            Module.persistence.disable();

            const arrayBuffer = await file.arrayBuffer();
            const image = new Uint8Array(arrayBuffer);
            const memory =
                new Module.vfs.Memory(image);

            await memory.load();

            const writer = new Module.vfs.Writer(memory);

            try {
                await writer.write();
            } catch (error) {
                return {
                    success: false,
                    error: error.message,
                    message: `Failed to import VFS image: ${error.message}`
                };
            }

            return {
                success: true,
                message: 'Imported VFS image successfully'
            };
        } catch (error) {
            return {
                success: false,
                error: error.message,
                message: `Failed to import VFS image: ${error.message}`
            };
        } finally {
            Module.persistence.enable();
            Module.persistence.onDirty();
        }
    }
};

// UI Event Handlers
function handleImportVFS() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.vfsi';
    input.onchange = async (e) => {
        const file = e.target.files[0];
        if (!file) return;
        updateStatus('Importing VFS image...', 'loading');
        try {
            const result = await VFSManager.importVFS(file);
            if (result.success) {
                updateStatus(result.message, 'success');
            } else {
                updateStatus(result.message, 'error');
                if (result.error) {
                    console.error('Import failed:', result.error);
                }
            }
        } catch (error) {
            updateStatus(`Import failed: ${error.message}`, 'error');
            console.error('Import error:', error);
        }
    };
    input.click();
}

async function handleExportVFS(path = '/') {
    let filename = window.createExportFilename(path, 'vfsi');
    try {
        filename = await modal.show(
            'Export VFS Image',
            'Enter a filename for the VFS image (.vfsi)',
            filename,
            { text: 'OK' },
            { text: 'Cancel' }
        );
    } catch {
        updateStatus('Export cancelled', 'error');
        return;
    }

    filename = (filename || '').trim();
    if (!filename || !filename.endsWith('.vfsi')) {
        updateStatus(
            'Export cancelled, invalid filename entered, must end in .vfsi', 'error');
        return;
    }

    updateStatus('Exporting VFS image...', 'loading');
    VFSManager.exportVFS(path, filename)
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

// Initialize VFS image functionality when DOM is ready
// Add event listeners to import/export buttons
// (Assume buttons with IDs 'importVFS' and 'exportVFS')
document.addEventListener('DOMContentLoaded', () => {
    const importButton = document.getElementById('importVFS');
    const exportButton = document.getElementById('exportVFS');
    if (importButton) {
        importButton.addEventListener('click', handleImportVFS);
    }
    if (exportButton) {
        exportButton.addEventListener('click', handleExportVFS);
    }
});