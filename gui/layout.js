const LAYOUT = {
  art: { w: 160, h: 128 },
  screen: { x: 46, y: 62, w: 68, h: 40 },
  text: { x: 55, y: 69, w: 49, h: 25 },
  buttons: {
    fullscreen: { x: 12, y: 88, w: 8, h: 6 },
    close: { x: 138, y: 88, w: 8, h: 6 },
  },
};
if (typeof module !== 'undefined') module.exports = LAYOUT;
