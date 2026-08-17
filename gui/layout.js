const LAYOUT = {
  art: { w: 160, h: 128 },
  screen: { x: 46, y: 62, w: 68, h: 40 },
  text: { x: 55, y: 69, w: 49, h: 25 },
  buttons: {
    fullscreen: { x: 12, y: 88, w: 8, h: 6 },
    close: { x: 138, y: 88, w: 8, h: 6 },
  },
  holo: {
    w: 1024,
    h: 572,
    bleed: { x: 46, y: 40, w: 961, h: 501 },
    ring: { x: 111, y: 102, w: 834, h: 382 },
    inset: 16,
    btn: 22,
    gap: 12,
  },
  sprite: { w: 88, h: 60, scale: 3 },
};
if (typeof module !== 'undefined') module.exports = LAYOUT;
