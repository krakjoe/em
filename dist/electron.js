
const { app, BrowserWindow } = require('electron')
const path = require('path')
const express = require('express')

let server;

function startServer() {
  const serverApp = express()
  serverApp.use(express.static(__dirname))
  
  server = serverApp.listen(0, 'localhost', () => {
    const port = server.address().port
    createWindow(`http://localhost:${port}/`)
  })
}

function createWindow(url) {
  const win = new BrowserWindow({
    width: 1200,
    height: 800,
    show: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      webSecurity: false,
      allowRunningInsecureContent: true,
      sandbox: false,
    }
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

app.whenReady().then(startServer)

app.on('before-quit', () => {
  if (server) server.close()
})