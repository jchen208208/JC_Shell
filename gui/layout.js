const LAYOUT = {
  art: { w: 400, h: 320 },
  scale: 2,
  screen: { x: 56, y: 72, w: 288, h: 176 },
  buttons: {
    fullscreen: { x: 340, y: 12, w: 20, h: 20 },
    close: { x: 368, y: 12, w: 20, h: 20 },
  },
};
LAYOUT.window = {
  w: LAYOUT.art.w * LAYOUT.scale,
  h: LAYOUT.art.h * LAYOUT.scale,
};
if (typeof module !== 'undefined') module.exports = LAYOUT;
