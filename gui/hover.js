const HOVER = {
  hop: [6000, 11000],
  pause: [2200, 5000],
  step: [0.05, 0.18],
  bobAmp: 5,
  bobMs: 5200,
  el: null,
  box: { x: 0, y: 0, w: 0, h: 0 },
  size: { w: 0, h: 0 },
  from: { x: 0, y: 0 },
  to: { x: 0, y: 0 },
  t0: 0,
  dur: 0,
  raf: 0,
  rand(a, b) {
    return a + Math.random() * (b - a);
  },
  span() {
    return { w: Math.max(0, this.box.w - this.size.w), h: Math.max(0, this.box.h - this.size.h) };
  },
  pick() {
    const s = this.span();
    const diag = Math.hypot(s.w, s.h);
    const near = diag * this.step[0];
    const far = diag * this.step[1];
    for (let i = 0; i < 8; i++) {
      const a = Math.random() * Math.PI * 2;
      const r = this.rand(near, far);
      const x = Math.min(s.w, Math.max(0, this.from.x + Math.cos(a) * r));
      const y = Math.min(s.h, Math.max(0, this.from.y + Math.sin(a) * r));
      if (Math.hypot(x - this.from.x, y - this.from.y) >= near) return { x, y };
    }
    return { x: this.rand(0, s.w), y: this.rand(0, s.h) };
  },
  clamp(v, lo, hi) {
    return Math.min(hi, Math.max(lo, v));
  },
  ease(p) {
    return -(Math.cos(Math.PI * p) - 1) / 2;
  },
  frame(now) {
    const p = this.dur > 0 ? Math.min(1, Math.max(0, (now - this.t0) / this.dur)) : 1;
    const e = this.ease(p);
    const x = this.box.x + this.from.x + (this.to.x - this.from.x) * e;
    const y = this.box.y + this.from.y + (this.to.y - this.from.y) * e;
    const bob = Math.sin((now / this.bobMs) * Math.PI * 2) * this.bobAmp;
    const sp = this.span();
    const px = this.clamp(x, this.box.x, this.box.x + sp.w);
    const py = this.clamp(y + bob, this.box.y, this.box.y + sp.h);
    this.el.style.transform = `translate3d(${Math.round(px)}px, ${Math.round(py)}px, 0)`;
    if (now >= this.t0 + this.dur + this.wait) {
      this.from = this.to;
      this.to = this.pick();
      this.t0 = now;
      this.dur = this.rand(this.hop[0], this.hop[1]);
      this.wait = this.rand(this.pause[0], this.pause[1]);
    }
    this.raf = requestAnimationFrame((t) => this.frame(t));
  },
  init(el) {
    this.el = el;
  },
  setBounds(box, size) {
    this.box = box;
    this.size = size;
    if (this.el) {
      this.el.style.width = `${size.w}px`;
      this.el.style.height = `${size.h}px`;
    }
    const s = this.span();
    this.from = { x: Math.min(this.from.x, s.w), y: Math.min(this.from.y, s.h) };
    this.to = { x: Math.min(this.to.x, s.w), y: Math.min(this.to.y, s.h) };
  },
  start() {
    if (this.raf || !this.el) return;
    const s = this.span();
    this.from = { x: this.rand(0, s.w), y: this.rand(0, s.h) };
    this.to = this.pick();
    this.t0 = performance.now();
    this.dur = this.rand(this.hop[0], this.hop[1]);
    this.wait = this.rand(this.pause[0], this.pause[1]);
    this.raf = requestAnimationFrame((t) => this.frame(t));
  },
  stop() {
    if (!this.raf) return;
    cancelAnimationFrame(this.raf);
    this.raf = 0;
  },
};
