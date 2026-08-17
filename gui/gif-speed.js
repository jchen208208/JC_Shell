const fs = require('node:fs');
const path = require('node:path');
const SRC = path.join(__dirname, 'sprites', 'src');
const DST = path.join(__dirname, 'sprites');
const FACTOR = 2.5;
function retime(buf, factor) {
  const out = Buffer.from(buf);
  const gctSize = out[10] & 0x80 ? 2 << (out[10] & 7) : 0;
  let i = 13 + gctSize * 3;
  let frames = 0;
  while (i < out.length) {
    const b = out[i];
    if (b === 0x21) {
      const label = out[i + 1];
      if (label === 0xf9) {
        const delay = out[i + 4] | (out[i + 5] << 8);
        const scaled = Math.min(65535, Math.max(2, Math.round(delay * factor)));
        out[i + 4] = scaled & 0xff;
        out[i + 5] = scaled >> 8;
        frames++;
      }
      i += 2;
      while (out[i]) i += out[i] + 1;
      i += 1;
    } else if (b === 0x2c) {
      const lflags = out[i + 9];
      i += 10 + (lflags & 0x80 ? (2 << (lflags & 7)) * 3 : 0) + 1;
      while (out[i]) i += out[i] + 1;
      i += 1;
    } else break;
  }
  return { out, frames };
}
for (const name of fs.readdirSync(SRC).filter((f) => f.endsWith('.gif'))) {
  const { out, frames } = retime(fs.readFileSync(path.join(SRC, name)), FACTOR);
  fs.writeFileSync(path.join(DST, name), out);
  console.log(`${name}: ${frames} frames retimed x${FACTOR}`);
}
