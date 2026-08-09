// Electron main process: owns the window and the PTY.
//
// The shell (../build/shell) knows nothing about any of this. It reads stdin
// and writes stdout, exactly as it does in Terminal.app. The PTY is what makes
// that possible -- it hands the shell a real terminal device, so tcgetattr()
// and raw mode behave normally.

const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('node:path');
const pty = require('node-pty');

const SHELL_PATH = path.join(__dirname, '..', 'build', 'shell');

function createWindow() {
  const win = new BrowserWindow({
    width: 900,
    height: 600,
    backgroundColor: '#1e1e2e',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });

  win.loadFile('index.html');

  const shell = pty.spawn(SHELL_PATH, [], {
    name: 'xterm-color',
    cols: 80,
    rows: 24,
    cwd: process.env.HOME,
    env: process.env,
  });

  // shell -> screen
  shell.onData((data) => {
    if (!win.isDestroyed()) win.webContents.send('pty:data', data);
  });

  // shell exited (e.g. the user typed `exit`) -> close the window
  shell.onExit(() => {
    if (!win.isDestroyed()) win.close();
  });

  // screen -> shell
  ipcMain.on('pty:input', (_event, data) => shell.write(data));
  ipcMain.on('pty:resize', (_event, { cols, rows }) => shell.resize(cols, rows));

  win.on('closed', () => {
    ipcMain.removeAllListeners('pty:input');
    ipcMain.removeAllListeners('pty:resize');
    shell.kill();
  });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => app.quit());
