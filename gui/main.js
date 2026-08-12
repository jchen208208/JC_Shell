const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('node:path');
const pty = require('node-pty');
const LAYOUT = require('./layout');
const SHELL_PATH = path.join(__dirname, '..', 'build', 'shell');
function createWindow() {
  const win = new BrowserWindow({
    width: LAYOUT.window.w,
    height: LAYOUT.window.h,
    transparent: true,
    frame: false,
    hasShadow: false,
    backgroundColor: '#00000000',
    resizable: false,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  win.loadFile('index.html');
  const sendMode = (mode) => {
    if (!win.isDestroyed()) win.webContents.send('ui:mode', mode);
  };
  win.on('enter-full-screen', () => sendMode('fullscreen'));
  win.on('leave-full-screen', () => sendMode('framed'));
  let shell = null;
  ipcMain.once('pty:ready', (_event, { cols, rows }) => {
    shell = pty.spawn(SHELL_PATH, [], {
      name: 'xterm-color',
      cols,
      rows,
      cwd: process.env.HOME,
      env: process.env,
    });
    shell.onData((data) => {
      if (!win.isDestroyed()) win.webContents.send('pty:data', data);
    });
    shell.onExit(() => {
      if (!win.isDestroyed()) win.close();
    });
  });
  ipcMain.on('pty:input', (_event, data) => shell?.write(data));
  ipcMain.on('pty:resize', (_event, { cols, rows }) => shell?.resize(cols, rows));
  ipcMain.on('ui:close', () => win.close());
  ipcMain.on('ui:toggle-fullscreen', () => win.setFullScreen(!win.isFullScreen()));
  win.on('closed', () => {
    ipcMain.removeAllListeners('pty:ready');
    ipcMain.removeAllListeners('pty:input');
    ipcMain.removeAllListeners('pty:resize');
    ipcMain.removeAllListeners('ui:close');
    ipcMain.removeAllListeners('ui:toggle-fullscreen');
    shell?.kill();
  });
}
app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
