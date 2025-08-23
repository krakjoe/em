document.addEventListener('dragover', (e) => e.preventDefault());
document.addEventListener('drop', (e) => e.preventDefault());
document.body.addEventListener('drop', async (e) => {
    e.preventDefault();

    if (!e.dataTransfer.files ||
        e.dataTransfer.files.length === 0) return;

    const files = Array.from(e.dataTransfer.files);

    let target = '';
    try {
        target = await window.Modal.show(
            'Upload Path',
            'Please enter the path for uploaded files:',
            '/',
            { text: 'Upload' },
            { text: 'Cancel' }
        );
    } catch { return; }

    target = (target || '').trim();
    if (!target) {
        updateStatus('Please enter a path', 'error');
        return;
    }

    updateStatus(`[drop] Uploading `
        +`${files.length} `
        +`${files.length > 1 ? "files" : "file"}`);

    showProgressBar("Uploading Files", files.length);

    for (let idx = 0;
             idx < files.length;
             idx++) {
        const file = files[idx];

        updateStatus(`[drop] ${file.name} uploading ...`);
        updateProgressBar(
            `Uploading ${file.name}`, idx+1,
            files.length);
        const contents = await file.arrayBuffer();
        Module.vfs.put(
            (target + "/" + file.name)
                .replace(/\/+/g, '/'),
            new Uint8Array(contents)    
        );
        updateStatus(`[drop] ${file.name} uploaded ...`);
    }

    hideProgressBar();
    updateStatus(`[drop] Uploaded `
        +`${files.length} `
        +`${files.length > 1 ? "files" : "file"}`
        +` to ${target}`);
});