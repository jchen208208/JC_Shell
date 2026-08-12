const LAYOUT = {
  art: { w: 80, h: 64 },
  scale: 12,
  screen: { x: 26, y: 34, w: 28, h: 14 },
  buttons: {
    fullscreen: { x: 6, y: 44, w: 4, h: 3 },
    close: { x: 69, y: 44, w: 4, h: 3 },
  },
};
LAYOUT.window = {
  w: LAYOUT.art.w * LAYOUT.scale,
  h: LAYOUT.art.h * LAYOUT.scale,
};
if (typeof module !== 'undefined') module.exports = LAYOUT;
