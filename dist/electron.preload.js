const { contextBridge } = 
    require('electron');
const fs = 
    require('fs').promises;
contextBridge.exposeInMainWorld('nodeFS', fs);