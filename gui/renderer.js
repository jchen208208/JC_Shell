const term = new Terminal({
  fontFamily: 'Menlo, Monaco, monospace',
  fontSize: 14,
  cursorBlink: true,
  allowTransparency: true,
  theme: {
    background: '#00000000',
    foreground: '#cdd6f4',
    cursor: '#f5e0dc',
  },
});
const fitAddon = new FitAddon.FitAddon();
term.loadAddon(fitAddon);
term.open(document.getElementById('terminal'));
function place(el, r, sx, sy) {
  el.style.left = `${r.x * sx}px`;
  el.style.top = `${r.y * sy}px`;
  el.style.width = `${r.w * sx}px`;
  el.style.height = `${r.h * sy}px`;
}
function applyLayout() {
  const sx = window.innerWidth / LAYOUT.art.w;
  const sy = window.innerHeight / LAYOUT.art.h;
  const root = document.documentElement.style;
  root.setProperty('--screen-x', `${LAYOUT.screen.x * sx}px`);
  root.setProperty('--screen-y', `${LAYOUT.screen.y * sy}px`);
  root.setProperty('--screen-w', `${LAYOUT.screen.w * sx}px`);
  root.setProperty('--screen-h', `${LAYOUT.screen.h * sy}px`);
  root.setProperty('--text-x', `${LAYOUT.text.x * sx}px`);
  root.setProperty('--text-y', `${LAYOUT.text.y * sy}px`);
  root.setProperty('--text-w', `${LAYOUT.text.w * sx}px`);
  root.setProperty('--text-h', `${LAYOUT.text.h * sy}px`);
  place(document.getElementById('btn-fullscreen'), LAYOUT.buttons.fullscreen, sx, sy);
  place(document.getElementById('btn-close'), LAYOUT.buttons.close, sx, sy);
}
function syncSize() {
  fitAddon.fit();
  window.pty.resize(term.cols, term.rows);
}
const flashEl = document.getElementById('flash');
const FLASH_OK = '#1fdd4e';
const FLASH_ERR = '#ff1f2e';
let statusFlash = null;
function pulse(ok) {
  if (statusFlash) statusFlash.cancel();
  flashEl.style.background = ok ? FLASH_OK : FLASH_ERR;
  statusFlash = flashEl.animate([{ opacity: 0 }, { opacity: 1, offset: 0.18 }, { opacity: 0 }], {
    duration: 500,
    easing: 'ease-out',
  });
}
window.pty.onData((data) => term.write(data));
term.onData((data) => window.pty.send(data));
term.parser.registerOscHandler(7777, (payload) => {
  if (payload === 'expand') window.ui.setFullscreen(true);
  else if (payload === 'shrink') window.ui.setFullscreen(false);
  else if (payload.startsWith('status;')) pulse(payload.slice(7) === '0');
  return true;
});
window.addEventListener('resize', () => {
  applyLayout();
  syncSize();
});
window.ui.onMode((mode) => {
  document.body.dataset.mode = mode;
  applyLayout();
  syncSize();
});
document.getElementById('btn-close').addEventListener('click', () => window.ui.close());
document.getElementById('btn-fullscreen').addEventListener('click', () => {
  window.ui.toggleFullscreen();
  term.focus();
});
document.body.dataset.mode = 'framed';
applyLayout();
fitAddon.fit();
window.pty.ready(term.cols, term.rows);
term.focus();
