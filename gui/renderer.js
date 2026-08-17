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
const FULL_FONT = 17;
const FRAMED_FONT = 14;
const fitAddon = new FitAddon.FitAddon();
term.loadAddon(fitAddon);
term.open(document.getElementById('terminal'));
const hoverEl = document.getElementById('hover');
const auraEl = document.getElementById('hover-aura');
const flashEl = document.getElementById('flash');
HOVER.init(hoverEl);
function place(el, r, sx, sy) {
  el.style.left = `${r.x * sx}px`;
  el.style.top = `${r.y * sy}px`;
  el.style.width = `${r.w * sx}px`;
  el.style.height = `${r.h * sy}px`;
}
function setRect(x, y, w, h) {
  const root = document.documentElement.style;
  root.setProperty('--text-x', `${x}px`);
  root.setProperty('--text-y', `${y}px`);
  root.setProperty('--text-w', `${w}px`);
  root.setProperty('--text-h', `${h}px`);
}
function isFullscreen() {
  return document.body.dataset.mode === 'fullscreen';
}
function applyFramed() {
  const sx = window.innerWidth / LAYOUT.art.w;
  const sy = window.innerHeight / LAYOUT.art.h;
  const root = document.documentElement.style;
  root.setProperty('--screen-x', `${LAYOUT.screen.x * sx}px`);
  root.setProperty('--screen-y', `${LAYOUT.screen.y * sy}px`);
  root.setProperty('--screen-w', `${LAYOUT.screen.w * sx}px`);
  root.setProperty('--screen-h', `${LAYOUT.screen.h * sy}px`);
  setRect(LAYOUT.text.x * sx, LAYOUT.text.y * sy, LAYOUT.text.w * sx, LAYOUT.text.h * sy);
  place(document.getElementById('btn-fullscreen'), LAYOUT.buttons.fullscreen, sx, sy);
  place(document.getElementById('btn-close'), LAYOUT.buttons.close, sx, sy);
  HOVER.stop();
}
function applyFullscreen() {
  const h = LAYOUT.holo;
  const W = window.innerWidth;
  const H = window.innerHeight;
  const s = Math.max(W / h.w, H / h.h);
  const ox = (W - h.w * s) / 2;
  const oy = (H - h.h * s) / 2;
  const rx = ox + h.ring.x * s;
  const ry = oy + h.ring.y * s;
  const rw = h.ring.w * s;
  const rh = h.ring.h * s;
  const pad = h.inset * s;
  setRect(rx + pad, ry + pad, rw - pad * 2, rh - pad * 2);
  const bs = h.btn * s;
  const gap = h.gap * s;
  const by = Math.max(gap, (ry - bs) / 2);
  const close = document.getElementById('btn-close');
  const full = document.getElementById('btn-fullscreen');
  close.style.cssText = `left:${rx + rw - bs}px;top:${by}px;width:${bs}px;height:${bs}px`;
  full.style.cssText = `left:${rx + rw - bs * 2 - gap}px;top:${by}px;width:${bs}px;height:${bs}px`;
  HOVER.setBounds(
    { x: rx + pad, y: ry + pad, w: rw - pad * 2, h: rh - pad * 2 },
    { w: LAYOUT.sprite.w * LAYOUT.sprite.scale, h: LAYOUT.sprite.h * LAYOUT.sprite.scale }
  );
  HOVER.start();
}
function applyLayout() {
  if (isFullscreen()) applyFullscreen();
  else applyFramed();
}
function syncSize() {
  fitAddon.fit();
  window.pty.resize(term.cols, term.rows);
}
const FLASH_OK = '#1fdd4e';
const FLASH_ERR = '#ff1f2e';
let statusFlash = null;
function pulse(ok) {
  if (statusFlash) statusFlash.cancel();
  const colour = ok ? FLASH_OK : FLASH_ERR;
  const el = isFullscreen() ? auraEl : flashEl;
  if (isFullscreen()) el.style.setProperty('--aura', colour);
  else el.style.background = colour;
  statusFlash = el.animate([{ opacity: 0 }, { opacity: 1, offset: 0.18 }, { opacity: 0 }], {
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
  term.options.fontSize = mode === 'fullscreen' ? FULL_FONT : FRAMED_FONT;
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
