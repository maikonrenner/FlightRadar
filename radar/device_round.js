// Simulador da Waveshare ESP32-S3-Touch-AMOLED-1.75 (redonda, 466x466, CO5300).
// Mesma interface do Device da CYD. So primitivas que existem na Arduino_GFX
// (fillRect, drawLine, drawCircle, fillCircle, fillTriangle, fillArc, texto), para portar direto.
const RANGES = { metric: [10, 25, 50, 100, 200], imperial: [5, 10, 25, 50, 100] };
const KM_PER = { metric: 1, imperial: 1.60934 };
const W = 466, CX = 233, CY = 233, RMAX = 188;
const BTN_MINUS = [32, 233, 24], BTN_PLUS = [434, 233, 24];   // x, y, raio (nas laterais do circulo)
const C = { bg: '#000000', ring: '#0b3d1c', ringHi: '#16653a', text: '#c8d2dc', dim: '#5d6b78', sweep: [0, 255, 120],
  sel: '#ff3c3c', plane2: '#00ff78', home: '#3c8cff', accent: '#ffc400', card: 'rgba(14,20,28,0.88)', btn: '#141c24' };
// mesma escala de altitude do globo 3D
const STOPS = [[0, '#f9e04b'], [5000, '#f6a33b'], [10000, '#e8673c'], [20000, '#c93d78'], [30000, '#8b3fb5'], [40000, '#4e5ad6'], [45000, '#3fa3f0']];
const altColor = ft => { let c = STOPS[0][1]; for (const [a, col] of STOPS) if (ft >= a) c = col; return c; };

export class RoundDevice {
  constructor(canvas, { onSelect }) {
    this.cv = canvas; this.g = canvas.getContext('2d'); this.onSelect = onSelect;
    this.rangeIdx = 2; this.blips = []; this.sweep = 0;
    this.onClick = e => this.touch(e);
    canvas.addEventListener('click', this.onClick);
  }
  destroy() { this.cv.removeEventListener('click', this.onClick); }
  rangeKm(units) { return RANGES[units][this.rangeIdx] * KM_PER[units]; }

  touch(e) {
    const b = this.cv.getBoundingClientRect();
    const x = (e.clientX - b.left) * W / b.width, y = (e.clientY - b.top) * W / b.height;
    const hit = ([bx, by, r]) => Math.hypot(x - bx, y - by) <= r + 8;
    if (hit(BTN_MINUS)) { this.rangeIdx = Math.max(0, this.rangeIdx - 1); return; }
    if (hit(BTN_PLUS)) { this.rangeIdx = Math.min(4, this.rangeIdx + 1); return; }
    let best = null, bd = 22;
    for (const q of this.blips) { const d = Math.hypot(q.x - x, q.y - y); if (d < bd) { bd = d; best = q; } }
    this.onSelect(best ? best.hex : null);   // toque fora de um aviao = desmarcar
  }

  // --- primitivas estilo Arduino_GFX
  fillRect(x, y, w, h, c) { this.g.fillStyle = c; this.g.fillRect(x, y, w, h); }
  drawLine(x0, y0, x1, y1, c, w = 1) { const g = this.g; g.strokeStyle = c; g.lineWidth = w; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke(); }
  drawCircle(x, y, r, c, w = 1) { const g = this.g; g.strokeStyle = c; g.lineWidth = w; g.beginPath(); g.arc(x, y, r, 0, Math.PI * 2); g.stroke(); }
  fillCircle(x, y, r, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.arc(x, y, r, 0, Math.PI * 2); g.fill(); }
  fillTriangle(x0, y0, x1, y1, x2, y2, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.lineTo(x2, y2); g.fill(); }
  fillArc(x, y, r0, r1, a0, a1, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.arc(x, y, r1, a0, a1); g.arc(x, y, r0, a1, a0, true); g.fill(); }
  fillRoundRect(x, y, w, h, r, c) { const g = this.g; g.fillStyle = c; g.beginPath(); g.roundRect(x, y, w, h, r); g.fill(); }
  text(s, x, y, px, c, align = 'left', bold = false) { const g = this.g; g.fillStyle = c; g.font = `${bold ? 700 : 500} ${px}px system-ui,sans-serif`; g.textAlign = align; g.textBaseline = 'middle'; g.fillText(s, x, y); }

  fitText(s, x, y, px, maxW, c, bold = false) {   // diminui a fonte ate caber na largura do cartao
    const g = this.g;
    for (; px > 9; px--) { g.font = `${bold ? 700 : 500} ${px}px system-ui,sans-serif`; if (g.measureText(s).width <= maxW) break; }
    this.text(s, x, y, px, c, 'center', bold);
  }

  plane(x, y, trk, size, c) {   // aviaozinho: seta apontando para o rumo
    const t = trk * Math.PI / 180, s = Math.sin(t), co = Math.cos(t);
    const P = (f, r) => [x + s * f + co * r, y - co * f + s * r];
    const [nx, ny] = P(size, 0), [lx, ly] = P(-size * .7, -size * .75), [rx, ry] = P(-size * .7, size * .75), [mx, my] = P(-size * .3, 0);
    this.fillTriangle(nx, ny, lx, ly, mx, my, c); this.fillTriangle(nx, ny, mx, my, rx, ry, c);
  }

  draw(st) {
    const { units, planes, selectedHex, alerting } = st;
    const rKm = this.rangeKm(units), rLbl = RANGES[units][this.rangeIdx], uD = units === 'metric' ? 'km' : 'mi', imp = units !== 'metric';
    const now = performance.now();
    this.fillRect(0, 0, W, W, C.bg);

    // aneis, cruz e marcas de 10 em 10 graus
    for (let i = 1; i <= 3; i++) this.drawCircle(CX, CY, RMAX * i / 3, i === 3 ? C.ringHi : C.ring, i === 3 ? 2 : 1);
    this.drawLine(CX, CY - RMAX, CX, CY + RMAX, C.ring); this.drawLine(CX - RMAX, CY, CX + RMAX, CY, C.ring);
    for (let d = 0; d < 360; d += 10) {
      const a = d * Math.PI / 180, r1 = RMAX + (d % 30 ? 5 : 10);
      this.drawLine(CX + Math.sin(a) * RMAX, CY - Math.cos(a) * RMAX, CX + Math.sin(a) * r1, CY - Math.cos(a) * r1, d % 30 ? C.ring : C.ringHi, d % 30 ? 1 : 2);
    }
    [['N', 0], ['L', 90], ['S', 180], ['O', 270]].forEach(([l, d]) => {
      const a = d * Math.PI / 180;
      this.text(l, CX + Math.sin(a) * (RMAX - 16), CY - Math.cos(a) * (RMAX - 16), 15, l === 'N' ? C.accent : C.dim, 'center', true);
    });
    [1, 2].forEach(i => this.text(`${+(rLbl * i / 3).toFixed(1)}`, CX + 4 + RMAX * i / 3 * .707, CY - RMAX * i / 3 * .707 - 2, 11, C.dim));

    // varredura: rastro em leque (fillArc com alfa decrescente)
    this.sweep = (now / 4000 * Math.PI * 2) % (Math.PI * 2);
    for (let k = 0; k < 18; k++) {
      const a1 = this.sweep - Math.PI / 2 - k * .035, a0 = a1 - .035;
      this.fillArc(CX, CY, 0, RMAX, a0, a1, `rgba(${C.sweep},${(.28 * (1 - k / 18)).toFixed(3)})`);
    }
    this.drawLine(CX, CY, CX + Math.sin(this.sweep) * RMAX, CY - Math.cos(this.sweep) * RMAX, `rgba(${C.sweep},.8)`, 2);

    // casa
    this.fillTriangle(CX, CY - 8, CX - 6, CY + 6, CX + 6, CY + 6, C.home);

    // avioes (cor = altitude, igual ao globo)
    const inRange = planes.filter(p => p.dist <= rKm && !p.ground).sort((a, b) => a.dist - b.dist);
    this.blips = []; let sel = null;
    inRange.forEach(p => {
      const r = p.dist / rKm * RMAX, a = p.brg * Math.PI / 180, x = CX + r * Math.sin(a), y = CY - r * Math.cos(a);
      const isSel = p.hex === selectedHex; if (isSel) sel = p;
      const col = isSel ? '#ffffff' : altColor(p.alt);
      this.plane(x, y, p.trk, isSel ? 11 : 8, col);
      if (isSel) this.drawCircle(x, y, 16, C.accent, 2);
      // etiqueta: fixa no selecionado; nos outros aparece quando a varredura passa e some em ~1,6 s
      const ms = ((this.sweep - a) % (2 * Math.PI) + 2 * Math.PI) % (2 * Math.PI) / (2 * Math.PI) * 4000, k = ms < 1600 ? 1 - ms / 1600 : 0;
      if (isSel) this.text(p.flight || p.hex, x + 11, y - 10, 12, C.accent, 'left', true);
      else if (k > 0) this.text(p.flight || p.hex, x + 11, y - 10, 12, `rgba(200,210,220,${k.toFixed(2)})`);
      this.blips.push({ x, y, hex: p.hex });
    });

    // topo: hora + contagem
    const d = new Date();
    this.text(`${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`, CX, 16, 13, C.dim, 'center', true);

    // botoes de zoom nas laterais
    for (const [b, s] of [[BTN_MINUS, '−'], [BTN_PLUS, '+']]) { this.fillCircle(b[0], b[1], b[2], C.btn); this.drawCircle(b[0], b[1], b[2], C.ringHi); this.text(s, b[0], b[1] + 1, 24, C.text, 'center', true); }

    // rodape: cartao do aviao selecionado ou resumo
    if (sel) {
      // cartao: voo / modelo / destino / velocidade (+ altitude e distancia menores)
      const info = st.selInfo || {}, wait = info.loading ? '…' : '';
      // o cartao fica estreito embaixo para nao ser cortado pela borda redonda
      this.fillRoundRect(113, 274, 240, 162, 20, C.card);
      this.text((sel.flight || sel.hex).slice(0, 9), CX, 292, 19, C.accent, 'center', true);
      this.fitText(info.model || sel.type || wait || 'modelo desconhecido', CX, 313, 13, 220, C.text);
      // rota: ORIGEM -> DESTINO em destaque, cidades embaixo
      const o = info.orig, d = info.dest;
      if (o || d) {
        this.fitText(`${o ? o.code : '?'}  →  ${d ? d.code : '?'}`, CX, 337, 19, 220, '#ffffff', true);
        this.fitText(`${o ? o.city : ''}  →  ${d ? d.city : ''}`, CX, 356, 11, 220, C.dim);
      } else this.text(wait || 'rota desconhecida', CX, 345, 13, C.dim, 'center');
      const spd = imp ? `${Math.round(sel.gs * 1.15078)} mph` : `${Math.round(sel.gs * 1.852)} km/h`;
      const cols = [['ALTITUDE', `${Math.round(sel.alt).toLocaleString('pt-BR')} ft`], ['VELOCIDADE', spd], ['DISTÂNCIA', `${(imp ? sel.dist / 1.60934 : sel.dist).toFixed(1)} ${uD}`]];
      cols.forEach(([k, v], i) => {
        const x = CX + (i - 1) * 76;
        this.text(k, x, 381, 9, C.dim, 'center', true);
        this.fitText(v, x, 399, 15, 72, i === 1 ? C.plane2 : '#ffffff', true);
      });
    } else {
      // contagem e alcance embaixo, fora do anel do radar
      this.text(`${inRange.length} aviões · ${rLbl} ${uD}`, CX, 447, 13, C.dim, 'center', true);
    }

    // alerta: anel vermelho pulsando na borda + aviso no topo
    if (alerting) {
      const on = Math.floor(now / 350) % 2;
      this.drawCircle(CX, CY, 229, on ? C.sel : 'rgba(255,60,60,.35)', 8);
      if (on) { this.fillRoundRect(CX - 48, 30, 96, 24, 12, C.sel); this.text('ALERTA', CX, 43, 13, '#fff', 'center', true); }
    }
  }
}
