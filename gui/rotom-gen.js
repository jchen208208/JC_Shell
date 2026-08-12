const { app, nativeImage } = require('electron');
const fs = require('node:fs');
const path = require('node:path');
const SRC = path.join(__dirname, 'rotomdex_window_1.png');
const HOLO = path.join(__dirname, 'hologram.png');
const DST = path.join(__dirname, 'rotom.png');
const DST_BG = path.join(__dirname, 'screen-bg.png');
const NX = 160;
const NY = 128;
const NAVY = [0, 17, 33];
const SCREEN = { x: 46, y: 62, w: 68, h: 40 };
const RADIUS = 6;
const MOUTH = { x0: 68, y0: 46, x1: 91, y1: 65 };
const HOLO_CROP = { x: 161, y: 67, w: 720, h: 464 };
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
      const x0 = Math.floor(i * cw + cw * 0.2);
      const x1 = Math.max(x0 + 1, Math.ceil(i * cw + cw * 0.8));
      const y0 = Math.floor(j * ch + ch * 0.2);
      const y1 = Math.max(y0 + 1, Math.ceil(j * ch + ch * 0.8));
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
    const hit = palette.find((p) => dist(p.c, c) < 58);
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
  const bezel = grid[(SCREEN.y + Math.floor(SCREEN.h / 2)) * NX + SCREEN.x - 1] || [1, 1, 1];
  const sx1 = SCREEN.x + SCREEN.w - 1;
  const sy1 = SCREEN.y + SCREEN.h - 1;
  const inRounded = (i, j) => {
    const qx = Math.min(Math.max(i, SCREEN.x + RADIUS), sx1 - RADIUS);
    const qy = Math.min(Math.max(j, SCREEN.y + RADIUS), sy1 - RADIUS);
    const dx = i - qx;
    const dy = j - qy;
    return dx * dx + dy * dy <= RADIUS * RADIUS;
  };
  const preserved = new Uint8Array(NX * NY);
  for (let j = SCREEN.y; j <= sy1; j++)
    for (let i = SCREEN.x; i <= sx1; i++) {
      const k = j * NX + i;
      const c = grid[k];
      if (isEye(c)) {
        preserved[k] = 1;
        continue;
      }
      if (isDark(c) && i >= MOUTH.x0 && i <= MOUTH.x1 && j >= MOUTH.y0 && j <= MOUTH.y1) {
        preserved[k] = 1;
        continue;
      }
      grid[k] = inRounded(i, j) ? NAVY : bezel;
    }
  for (let k = 0; k < grid.length; k++) {
    const c = grid[k];
    if (c && c[1] > 170 && c[0] < 110 && c[2] < 110) grid[k] = NAVY;
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
  const isNavy = (i, j) => {
    if (i < 0 || j < 0 || i >= NX || j >= NY) return 0;
    const c = grid[j * NX + i];
    return c && c[0] === NAVY[0] && c[1] === NAVY[1] && c[2] === NAVY[2] ? 1 : 0;
  };
  let mask = new Float32Array(NX * NY);
  for (let j = 0; j < NY; j++) for (let i = 0; i < NX; i++) mask[j * NX + i] = isNavy(i, j);
  for (let pass = 0; pass < 2; pass++) {
    const next = new Float32Array(NX * NY);
    for (let j = 0; j < NY; j++)
      for (let i = 0; i < NX; i++) {
        let s = 0;
        let n = 0;
        for (let dj = -1; dj <= 1; dj++)
          for (let di = -1; di <= 1; di++) {
            const y = j + dj;
            const x = i + di;
            if (x < 0 || y < 0 || x >= NX || y >= NY) continue;
            s += mask[y * NX + x];
            n++;
          }
        next[j * NX + i] = s / n;
      }
    mask = next;
  }
  for (let j = 0; j < NY; j++)
    for (let i = 0; i < NX; i++) {
      if (!preserved[j * NX + i]) continue;
      for (let dj = -1; dj <= 1; dj++)
        for (let di = -1; di <= 1; di++) {
          const y = j + dj;
          const x = i + di;
          if (x < 0 || y < 0 || x >= NX || y >= NY) continue;
          mask[y * NX + x] = 0;
        }
    }
  const holo = nativeImage.createFromPath(HOLO);
  const hs = holo.getSize();
  const hb = holo.getBitmap();
  const CW = HOLO_CROP.w;
  const CH = HOLO_CROP.h;
  const bg = Buffer.alloc(CW * CH * 4);
  for (let y = 0; y < CH; y++)
    for (let x = 0; x < CW; x++) {
      const sxp = Math.min(hs.width - 1, HOLO_CROP.x + x);
      const syp = Math.min(hs.height - 1, HOLO_CROP.y + y);
      const si = (syp * hs.width + sxp) * 4;
      const cellX = SCREEN.x + (x / CW) * SCREEN.w;
      const cellY = SCREEN.y + (y / CH) * SCREEN.h;
      const fx = Math.min(NX - 2, Math.max(0, Math.floor(cellX)));
      const fy = Math.min(NY - 2, Math.max(0, Math.floor(cellY)));
      const tx = cellX - fx;
      const ty = cellY - fy;
      const m =
        mask[fy * NX + fx] * (1 - tx) * (1 - ty) +
        mask[fy * NX + fx + 1] * tx * (1 - ty) +
        mask[(fy + 1) * NX + fx] * (1 - tx) * ty +
        mask[(fy + 1) * NX + fx + 1] * tx * ty;
      const a = Math.round(Math.min(1, Math.max(0, (m - 0.35) / 0.4)) * 255);
      const di = (y * CW + x) * 4;
      bg[di] = hb[si];
      bg[di + 1] = hb[si + 1];
      bg[di + 2] = hb[si + 2];
      bg[di + 3] = a;
    }
  fs.writeFileSync(DST_BG, nativeImage.createFromBitmap(bg, { width: CW, height: CH }).toPNG());
  console.log(`rotom.png ${NX}x${NY}, palette ${palette.length}`);
  console.log(`screen-bg.png ${CW}x${CH}`);
  console.log(`screen x=${SCREEN.x} y=${SCREEN.y} w=${SCREEN.w} h=${SCREEN.h}`);
  app.quit();
});
