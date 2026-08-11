const LAYOUT = {
  art: { w: 400, h: 320 },
  scale: 2,
  screen: { x: 56, y: 72, w: 288, h: 176 },
};
LAYOUT.window = {
  w: LAYOUT.art.w * LAYOUT.scale,
  h: LAYOUT.art.h * LAYOUT.scale,
};
if (typeof module !== 'undefined') module.exports = LAYOUT;
