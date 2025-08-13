
const express = require('express')
const { 
    app, BrowserWindow 
} = require('electron')
const path = require('path')

let server;

function createWindow(url) {
    const win = new BrowserWindow({
        width: 1200,
        height: 800,
        show: false,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: true,
            webSecurity: false,
            allowRunningInsecureContent: true,
            sandbox: false,
            additionalArguments: ['--disable-web-security'],
            experimentalFeatures: true,
            preload: path.join(__dirname,
                'electron.preload.js')
        }
    })
    win.webContents.session.webRequest.onHeadersReceived((details, callback) => {
        const responseHeaders =
            { ...details.responseHeaders }
        delete responseHeaders[
            'content-security-policy']
        delete responseHeaders[
            'Content-Security-Policy']
        
        callback({ responseHeaders })
    })
    win.webContents.once('did-finish-load', () => {
        setTimeout(() => {
            win.webContents.reload()
        }, 200) // Give iframe time to attempt worker boot
    })
    win.maximize()
    win.show()
    win.loadURL(url)
}

app.whenReady().then(() => {
    const instance = express()

    instance.use(
        express.static(__dirname))

    server = instance.listen(
        0, 'localhost', () => {
        const port = server.address().port
        createWindow(
            `http://localhost:${port}/`)
    })
});

app.on('before-quit', () => {
    if (server) {
        server.close()
    }
})