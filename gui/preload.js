const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('pty', {
  onData: (callback) => ipcRenderer.on('pty:data', (_event, data) => callback(data)),
  ready: (cols, rows) => ipcRenderer.send('pty:ready', { cols, rows }),
  send: (data) => ipcRenderer.send('pty:input', data),
  resize: (cols, rows) => ipcRenderer.send('pty:resize', { cols, rows }),
});
contextBridge.exposeInMainWorld('ui', {
  onMode: (callback) => ipcRenderer.on('ui:mode', (_event, mode) => callback(mode)),
});
