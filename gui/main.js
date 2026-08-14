const { app, BrowserWindow, ipcMain, screen } = require('electron');
const path = require('node:path');
const pty = require('node-pty');
const LAYOUT = require('./layout');
const SHELL_PATH = path.join(__dirname, '..', 'build', 'shell');
function createWindow() {
  const wa = screen
    .getAllDisplays()
    .map((d) => d.workArea)
    .reduce((a, b) => (b.width * b.height > a.width * a.height ? b : a));
  const scale = Math.max(
    1,
    Math.min(Math.floor(wa.width / LAYOUT.art.w), Math.floor(wa.height / LAYOUT.art.h))
  );
  const winW = LAYOUT.art.w * scale;
  const winH = LAYOUT.art.h * scale;
  const win = new BrowserWindow({
    width: winW,
    height: winH,
    x: Math.round(wa.x + (wa.width - winW) / 2),
    y: Math.round(wa.y + (wa.height - winH) / 2),
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
  win.webContents.on('did-finish-load', () => {
    win.webContents.setVisualZoomLevelLimits(1, 1);
    win.webContents.setZoomFactor(1);
  });
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
  ipcMain.on('ui:fullscreen', (_event, on) => win.setFullScreen(!!on));
  win.on('closed', () => {
    ipcMain.removeAllListeners('pty:ready');
    ipcMain.removeAllListeners('pty:input');
    ipcMain.removeAllListeners('pty:resize');
    ipcMain.removeAllListeners('ui:close');
    ipcMain.removeAllListeners('ui:toggle-fullscreen');
    ipcMain.removeAllListeners('ui:fullscreen');
    shell?.kill();
  });
}
app.whenReady().then(createWindow);
app.on('window-all-closed', () => app.quit());
