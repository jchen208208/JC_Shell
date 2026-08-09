// Bridges the sandboxed page to the PTY in the main process.
// The page never gets Node access; it only gets these three functions.

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('pty', {
  onData: (callback) => ipcRenderer.on('pty:data', (_event, data) => callback(data)),
  send: (data) => ipcRenderer.send('pty:input', data),
  resize: (cols, rows) => ipcRenderer.send('pty:resize', { cols, rows }),
});
