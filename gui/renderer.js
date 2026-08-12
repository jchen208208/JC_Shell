const term = new Terminal({
  fontFamily: 'Menlo, Monaco, monospace',
  fontSize: 14,
  cursorBlink: true,
  theme: {
    background: '#1e1e2e',
    foreground: '#cdd6f4',
    cursor: '#f5e0dc',
  },
});
const fitAddon = new FitAddon.FitAddon();
term.loadAddon(fitAddon);
term.open(document.getElementById('terminal'));
function place(el, rect) {
  const { scale } = LAYOUT;
  el.style.left = `${rect.x * scale}px`;
  el.style.top = `${rect.y * scale}px`;
  el.style.width = `${rect.w * scale}px`;
  el.style.height = `${rect.h * scale}px`;
}
function applyLayout() {
  const { screen, scale, buttons } = LAYOUT;
  const root = document.documentElement.style;
  root.setProperty('--screen-x', `${screen.x * scale}px`);
  root.setProperty('--screen-y', `${screen.y * scale}px`);
  root.setProperty('--screen-w', `${screen.w * scale}px`);
  root.setProperty('--screen-h', `${screen.h * scale}px`);
  place(document.getElementById('btn-fullscreen'), buttons.fullscreen);
  place(document.getElementById('btn-close'), buttons.close);
}
function syncSize() {
  fitAddon.fit();
  window.pty.resize(term.cols, term.rows);
}
window.pty.onData((data) => term.write(data));
term.onData((data) => window.pty.send(data));
window.addEventListener('resize', syncSize);
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
