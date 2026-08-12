const { app, nativeImage } = require('electron');
const fs = require('node:fs');
const path = require('node:path');
const SRC = path.join(__dirname, 'rotomdex_window_1.png');
const DST = path.join(__dirname, 'rotom.png');
const NX = 80;
const NY = 64;
const TERMBG = [30, 30, 46];
const SCREEN = { x: 24, y: 31, w: 32, h: 20 };
const MOUTH = { x0: 34, y0: 23, x1: 45, y1: 32 };
const KEEP = [
  [205, 234, 237],
  [142, 198, 221],
  [62, 123, 185],
  [18, 84, 148],
];
const dist = (a, b) => Math.abs(a[0] - b[0]) + Math.abs(a[1] - b[1]) + Math.abs(a[2] - b[2]);
app.whenReady().then(() => {
  const img = nativeImage.createFromPath(SRC);
  const { width: W, height: H } = img.getSize();
  const src = img.getBitmap();
  const at = (x, y) => {
    const i = (y * W + x) * 4;
    return { b: src[i], g: src[i + 1], r: src[i + 2], a: src[i + 3] };
  };
  const cw = W / NX;
  const ch = H / NY;
  const cells = [];
  for (let j = 0; j < NY; j++)
    for (let i = 0; i < NX; i++) {
      const x0 = Math.floor(i * cw + cw * 0.28);
      const x1 = Math.ceil(i * cw + cw * 0.72);
      const y0 = Math.floor(j * ch + ch * 0.28);
      const y1 = Math.ceil(j * ch + ch * 0.72);
      const bucket = new Map();
      let opaque = 0;
      let total = 0;
      for (let y = y0; y < y1 && y < H; y++)
        for (let x = x0; x < x1 && x < W; x++) {
          const p = at(x, y);
          total++;
          if (p.a > 128) {
            opaque++;
            const k = `${p.r >> 3}|${p.g >> 3}|${p.b >> 3}`;
            if (!bucket.has(k)) bucket.set(k, { n: 0, r: 0, g: 0, b: 0 });
            const e = bucket.get(k);
            e.n++;
            e.r += p.r;
            e.g += p.g;
            e.b += p.b;
          }
        }
      if (opaque * 2 < total || bucket.size === 0) {
        cells.push(null);
        continue;
      }
      let best = null;
      for (const e of bucket.values()) if (!best || e.n > best.n) best = e;
      cells.push([
        Math.round(best.r / best.n),
        Math.round(best.g / best.n),
        Math.round(best.b / best.n),
      ]);
    }
  const freq = new Map();
  for (const c of cells) {
    if (!c) continue;
    const k = c.join(',');
    if (!freq.has(k)) freq.set(k, { c, n: 0 });
    freq.get(k).n++;
  }
  const palette = [];
  for (const { c, n } of [...freq.values()].sort((a, b) => b.n - a.n)) {
    const hit = palette.find((p) => dist(p.c, c) < 70);
    if (hit) hit.n += n;
    else palette.push({ c: c.slice(), n });
  }
  const snap = (c) => {
    let best = null;
    for (const p of palette) {
      const d = dist(p.c, c);
      if (!best || d < best.d) best = { d, c: p.c };
    }
    return best.c;
  };
  const grid = cells.map((c) => (c ? snap(c) : null));
  const isEye = (c) => c && KEEP.some((k) => dist(k, c) < 60);
  const isDark = (c) => c && c[0] < 60 && c[1] < 60 && c[2] < 60;
  for (let j = SCREEN.y; j < SCREEN.y + SCREEN.h; j++)
    for (let i = SCREEN.x; i < SCREEN.x + SCREEN.w; i++) {
      const k = j * NX + i;
      const c = grid[k];
      if (isEye(c)) continue;
      if (isDark(c) && i >= MOUTH.x0 && i <= MOUTH.x1 && j >= MOUTH.y0 && j <= MOUTH.y1) continue;
      grid[k] = TERMBG;
    }
  for (let k = 0; k < grid.length; k++) {
    const c = grid[k];
    if (c && c[1] > 170 && c[0] < 110 && c[2] < 110) grid[k] = TERMBG;
  }
  const out = Buffer.alloc(NX * NY * 4);
  for (let k = 0; k < NX * NY; k++) {
    const c = grid[k];
    if (!c) continue;
    out[k * 4] = c[2];
    out[k * 4 + 1] = c[1];
    out[k * 4 + 2] = c[0];
    out[k * 4 + 3] = 255;
  }
  fs.writeFileSync(DST, nativeImage.createFromBitmap(out, { width: NX, height: NY }).toPNG());
  const isTerm = (i, j) => {
    const c = grid[j * NX + i];
    return c && c[0] === TERMBG[0] && c[1] === TERMBG[1] && c[2] === TERMBG[2];
  };
  const heights = new Array(NX).fill(0);
  let bestRect = { area: 0 };
  for (let j = 0; j < NY; j++) {
    for (let i = 0; i < NX; i++) heights[i] = isTerm(i, j) ? heights[i] + 1 : 0;
    const stack = [];
    for (let i = 0; i <= NX; i++) {
      const h = i === NX ? 0 : heights[i];
      let start = i;
      while (stack.length && stack[stack.length - 1].h >= h) {
        const top = stack.pop();
        const area = top.h * (i - top.i);
        if (area > bestRect.area)
          bestRect = { area, x: top.i, y: j - top.h + 1, w: i - top.i, h: top.h };
        start = top.i;
      }
      stack.push({ i: start, h });
    }
  }
  console.log(`wrote rotom.png ${NX}x${NY}, palette ${palette.length}`);
  console.log(`painted screen  x=${SCREEN.x} y=${SCREEN.y} w=${SCREEN.w} h=${SCREEN.h}`);
  console.log(`largest text rect x=${bestRect.x} y=${bestRect.y} w=${bestRect.w} h=${bestRect.h}`);
  app.quit();
});
