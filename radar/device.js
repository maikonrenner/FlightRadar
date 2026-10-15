// Simulador da Cheap Yellow Display (ESP32-2432S028R, 320x240).
// Usa so primitivas que existem na TFT_eSPI, para portar direto ao firmware.
const RANGES = { metric: [10, 25, 50, 100, 200], imperial: [5, 10, 25, 50, 100] };
const KM_PER = { metric: 1, imperial: 1.60934 };
const CX = 120, CY = 120, RMAX = 110;
const C = { bg: '#000000', ring: '#005a00', text: '#c8c8c8', plane: '#00ff64', sel: '#ff3c3c', home: '#3c8cff', panel: '#101418', accent: '#ffc400', btn: '#1e2830' };
const BTN_MINUS = [244, 206, 34, 28], BTN_PLUS = [282, 206, 34, 28];

export class Device {
  constructor(canvas, { onSelect }) {
    this.g = canvas.getContext('2d');
    this.cv = canvas;
    this.onSelect = onSelect;
    this.rangeIdx = 2;
    this.blips = [];
    this.sweep = 0;
    canvas.addEventListener('click', e => this.touch(e));
  }

  rangeKm(units) { return RANGES[units][this.rangeIdx] * KM_PER[units]; }

  touch(e) {
    const b = this.cv.getBoundingClientRect();
    const x = (e.clientX - b.left) * 320 / b.width, y = (e.clientY - b.top) * 240 / b.height;
    const inside = (r) => x >= r[0] && x <= r[0] + r[2] && y >= r[1] && y <= r[1] + r[3];
    if (inside(BTN_MINUS)) { this.rangeIdx = Math.max(0, this.rangeIdx - 1); return; }
    if (inside(BTN_PLUS)) { this.rangeIdx = Math.min(4, this.rangeIdx + 1); return; }
    let best = null, bd = 14;
    for (const b2 of this.blips) { const d = Math.hypot(b2.x - x, b2.y - y); if (d < bd) { bd = d; best = b2; } }
    this.onSelect(best ? best.hex : null);   // toque fora de um aviao = desmarcar
  }

  // --- primitivas estilo TFT_eSPI
  fillRect(x, y, w, h, c) { this.g.fillStyle = c; this.g.fillRect(x, y, w, h); }
  drawLine(x0, y0, x1, y1, c) { const g = this.g; g.strokeStyle = c; g.lineWidth = 1; g.beginPath(); g.moveTo(x0 + .5, y0 + .5); g.lineTo(x1 + .5, y1 + .5); g.stroke(); }
  drawCircle(x, y, r, c) { const g = this.g; g.strokeStyle = c; g.lineWidth = 1; g.beginPath(); g.arc(x + .5, y + .5, r, 0, Math.PI * 2); g.stroke(); }
  fillCircle(x, y, r, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.arc(x + .5, y + .5, r, 0, Math.PI * 2); g.fill(); }
  fillTriangle(x0, y0, x1, y1, x2, y2, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.lineTo(x2, y2); g.fill(); }
  drawString(s, x, y, size, c) { const g = this.g; g.fillStyle = c; g.font = `${size === 2 ? 16 : 9}px monospace`; g.textBaseline = 'top'; g.fillText(s, x, y); }

  draw(st) {
    const { home, units, planes, selectedHex, alerting } = st;
    const rKm = this.rangeKm(units), rLbl = RANGES[units][this.rangeIdx], uD = units === 'metric' ? 'km' : 'mi';
    this.fillRect(0, 0, 320, 240, C.bg);

    for (let i = 1; i <= 3; i++) this.drawCircle(CX, CY, Math.round(RMAX * i / 3), C.ring);
    this.drawLine(CX, CY - RMAX, CX, CY + RMAX, C.ring);
    this.drawLine(CX - RMAX, CY, CX + RMAX, CY, C.ring);
    this.sweep = (this.sweep + 0.04) % (Math.PI * 2);
    for (let k = 0; k < 10; k++) {
      const a = this.sweep - k * 0.03;
      this.drawLine(CX, CY, CX + Math.round(Math.sin(a) * RMAX), CY - Math.round(Math.cos(a) * RMAX), `rgba(0,255,100,${(0.35 - k * 0.033).toFixed(3)})`);
    }
    this.drawString('N', CX - 3, CY - RMAX - 10, 1, C.text);
    this.drawString('E', CX + RMAX + 2, CY - 4, 1, C.text);
    this.drawString('S', CX - 3, CY + RMAX + 1, 1, C.text);
    this.drawString('W', CX - RMAX - 9, CY - 4, 1, C.text);
    this.drawString(`${rLbl}${uD}`, CX + 4, CY - RMAX + 4, 1, C.text);
    this.fillTriangle(CX, CY - 6, CX - 5, CY + 5, CX + 5, CY + 5, C.home);

    const inRange = planes.filter(p => p.dist <= rKm && !p.ground).sort((a, b) => a.dist - b.dist);
    this.blips = [];
    let sel = null;
    for (const p of inRange) {
      const r = p.dist / rKm * RMAX, a = p.brg * Math.PI / 180;
      const x = CX + Math.round(r * Math.sin(a)), y = CY - Math.round(r * Math.cos(a));
      const isSel = p.hex === selectedHex;
      if (isSel) sel = p;
      const col = isSel ? C.sel : C.plane, t = p.trk * Math.PI / 180;
      this.drawLine(x, y, x + Math.round(Math.sin(t) * 7), y - Math.round(Math.cos(t) * 7), col);
      this.fillCircle(x, y, isSel ? 3 : 2, col);
      if (isSel) this.drawCircle(x, y, 7, col);
      this.blips.push({ x, y, hex: p.hex });
    }

    // painel direito
    this.fillRect(236, 0, 84, 240, C.panel);
    this.drawLine(236, 0, 236, 239, C.ring);
    this.drawString(`${inRange.length} avioes`, 242, 6, 1, C.text);
    const p = sel || null;
    if (!p) {
      if (!inRange.length) { this.drawString('Sem trafego', 242, 30, 1, C.text); this.drawString('no alcance', 242, 42, 1, C.text); }
    } else {
      const imp = units !== 'metric';
      this.drawString((p.flight || p.hex).slice(0, 8), 242, 24, 2, C.accent);
      const lines = [
        ['ALT', p.ground ? 'solo' : `${Math.round(p.alt)}ft`],
        ['VEL', `${Math.round(p.gs)}kt`],
        ['RUMO', `${Math.round(p.trk)}`],
        ['DIST', `${(imp ? p.dist / 1.60934 : p.dist).toFixed(1)}${uD}`],
        ['TIPO', p.type || '?'],
        ['REG', p.reg || '?'],
      ];
      lines.forEach(([k, v], i) => { this.drawString(k, 242, 50 + i * 24, 1, '#7a8794'); this.drawString(String(v).slice(0, 12), 242, 60 + i * 24, 1, C.text); });
    }
    this.fillRect(...BTN_MINUS, C.btn); this.fillRect(...BTN_PLUS, C.btn);
    this.drawString('-', BTN_MINUS[0] + 13, BTN_MINUS[1] + 6, 2, C.text);
    this.drawString('+', BTN_PLUS[0] + 12, BTN_PLUS[1] + 6, 2, C.text);
    if (alerting && Math.floor(performance.now() / 300) % 2) {
      this.fillRect(4, 222, 54, 14, C.sel); this.drawString('ALERTA', 8, 225, 1, '#ffffff');
    }
  }
}
