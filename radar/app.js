import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { feature } from 'https://cdn.jsdelivr.net/npm/topojson-client@3.1.0/+esm';
import { Device } from './device.js';
import { RoundDevice } from './device_round.js';

const $ = id => document.getElementById(id);
const DEG = Math.PI / 180, EARTH_KM = 6371, ALT_EXAG = 6, MI = 1.60934;
const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

// ---------------- configuracao persistente
const DEF = { units: 'metric', fMin: 0, fMax: 50000, ground: false, aKm: 5, aFt: 5000, home: null,
  layers: { global: true, rain: false, routes: true, trails: true, labels: true } };
let cfg;
try { cfg = { ...DEF, ...JSON.parse(localStorage.getItem('radar_cfg') || '{}') }; cfg.layers = { ...DEF.layers, ...cfg.layers }; }
catch (e) { cfg = structuredClone(DEF); }
const save = () => { try { localStorage.setItem('radar_cfg', JSON.stringify(cfg)); } catch (e) {} };

// casa padrao ate o IP (ou o botao "Usar minha localizacao exata") responder
const S = { home: cfg.home || { lat: 45.5017, lon: -73.5673, city: 'Montréal' }, planes: new Map(), selected: null,
  view: 'global', q: '', sound: false, alerted: new Map(), alerting: false, follow: false, meta: {}, routes: {},
  routeAps: [], lastLocal: 0, globalTs: 0, anim: null };

// ---------------- geo
function vec(lat, lon, h = 0, o = new THREE.Vector3()) {
  const r = 1 + h, p = lat * DEG, l = lon * DEG;
  return o.set(r * Math.cos(p) * Math.sin(l), r * Math.sin(p), r * Math.cos(p) * Math.cos(l));
}
const altH = ft => Math.max(0, ft) * 0.3048 / 1000 / EARTH_KM * ALT_EXAG;
function distKm(la1, lo1, la2, lo2) {
  const a = Math.sin((la2 - la1) * DEG / 2) ** 2 + Math.cos(la1 * DEG) * Math.cos(la2 * DEG) * Math.sin((lo2 - lo1) * DEG / 2) ** 2;
  return 2 * EARTH_KM * Math.asin(Math.min(1, Math.sqrt(a)));
}
function bearing(la1, lo1, la2, lo2) {
  const y = Math.sin((lo2 - lo1) * DEG) * Math.cos(la2 * DEG);
  const x = Math.cos(la1 * DEG) * Math.sin(la2 * DEG) - Math.sin(la1 * DEG) * Math.cos(la2 * DEG) * Math.cos((lo2 - lo1) * DEG);
  return (Math.atan2(y, x) / DEG + 360) % 360;
}
function advance(lat, lon, brg, km) {
  const d = km / EARTH_KM, b = brg * DEG, p1 = lat * DEG, l1 = lon * DEG;
  const p2 = Math.asin(Math.sin(p1) * Math.cos(d) + Math.cos(p1) * Math.sin(d) * Math.cos(b));
  const l2 = l1 + Math.atan2(Math.sin(b) * Math.sin(d) * Math.cos(p1), Math.cos(d) - Math.sin(p1) * Math.sin(p2));
  return [p2 / DEG, ((l2 / DEG + 540) % 360) - 180];
}
const fmtDist = km => cfg.units === 'metric' ? `${km.toFixed(1)} km` : `${(km / MI).toFixed(1)} mi`;
const ago = ms => { const s = Math.max(0, (Date.now() - ms) / 1000); return s < 60 ? `há ${Math.round(s)} s` : `há ${Math.round(s / 60)} min`; };

// ---------------- cena
const canvas = $('scene');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, logarithmicDepthBuffer: true });
renderer.setPixelRatio(Math.min(2, devicePixelRatio));
const scene = new THREE.Scene();
scene.background = new THREE.Color('#070b10');
const camera = new THREE.PerspectiveCamera(45, 1, 1e-5, 100);
let controls = null;
function resize() { renderer.setSize(innerWidth, innerHeight, false); camera.aspect = innerWidth / innerHeight; camera.updateProjectionMatrix(); }
addEventListener('resize', resize); resize();

const earthCanvas = document.createElement('canvas'); earthCanvas.width = 4096; earthCanvas.height = 2048;
const ec = earthCanvas.getContext('2d'); ec.fillStyle = '#232227'; ec.fillRect(0, 0, 4096, 2048);
const earthTex = new THREE.CanvasTexture(earthCanvas);
earthTex.colorSpace = THREE.SRGBColorSpace; earthTex.wrapS = THREE.RepeatWrapping; earthTex.offset.x = 0.25;
earthTex.anisotropy = renderer.capabilities.getMaxAnisotropy();
scene.add(new THREE.Mesh(new THREE.SphereGeometry(1, 192, 96), new THREE.MeshBasicMaterial({ map: earthTex })));
scene.add(new THREE.Mesh(new THREE.SphereGeometry(1.025, 64, 32), new THREE.MeshBasicMaterial({ color: '#2f6fb5', transparent: true, opacity: .08, side: THREE.BackSide })));

async function loadWorld() {
  const topo = await (await fetch('https://cdn.jsdelivr.net/npm/world-atlas@2/countries-50m.json')).json();
  const W = 4096, H = 2048, X = l => (l + 180) / 360 * W, Y = l => (90 - l) / 180 * H;
  const trace = geom => {
    const polys = geom.type === 'Polygon' ? [geom.coordinates] : geom.coordinates;
    for (const poly of polys) for (const ring of poly) { ring.forEach(([lo, la], i) => i ? ec.lineTo(X(lo), Y(la)) : ec.moveTo(X(lo), Y(la))); ec.closePath(); }
  };
  const feats = o => { const f = feature(topo, o); return f.features || [f]; };
  ec.fillStyle = '#474749'; ec.beginPath(); for (const f of feats(topo.objects.land)) trace(f.geometry); ec.fill('evenodd');
  ec.strokeStyle = 'rgba(255,255,255,.04)'; ec.lineWidth = 1;
  for (let lo = -180; lo <= 180; lo += 15) { ec.beginPath(); ec.moveTo(X(lo), 0); ec.lineTo(X(lo), H); ec.stroke(); }
  for (let la = -75; la <= 75; la += 15) { ec.beginPath(); ec.moveTo(0, Y(la)); ec.lineTo(W, Y(la)); ec.stroke(); }
  ec.strokeStyle = 'rgba(150,170,190,.4)'; ec.lineWidth = 1.2;
  for (const f of feats(topo.objects.countries)) { if (!f.geometry) continue; ec.beginPath(); trace(f.geometry); ec.stroke(); }
  earthTex.needsUpdate = true;
}

// ---------------- mosaicos de mapa e chuva sobre a esfera
const tileXY = (lat, lon, z) => { const n = 2 ** z; return [Math.floor((lon + 180) / 360 * n), Math.floor((1 - Math.log(Math.tan(lat * DEG) + 1 / Math.cos(lat * DEG)) / Math.PI) / 2 * n)]; };
const tileLat = (y, z) => Math.atan(Math.sinh(Math.PI - 2 * Math.PI * y / 2 ** z)) / DEG;
const loader = new THREE.TextureLoader();
function tileMesh(z, x, y, url, h, transparent = false) {
  const N = 8, pos = [], uv = [], idx = [], n = 2 ** z, lon0 = x / n * 360 - 180, lon1 = (x + 1) / n * 360 - 180, v = new THREE.Vector3();
  for (let j = 0; j <= N; j++) { const lat = tileLat(y + j / N, z); for (let i = 0; i <= N; i++) { vec(lat, lon0 + (lon1 - lon0) * i / N, h, v); pos.push(v.x, v.y, v.z); uv.push(i / N, 1 - j / N); } }
  for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) { const a = j * (N + 1) + i, b = a + 1, c = a + N + 1, d = c + 1; idx.push(a, c, b, b, c, d); }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3)); geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2)); geo.setIndex(idx);
  const mat = new THREE.MeshBasicMaterial({ transparent, opacity: transparent ? .7 : 1, depthWrite: !transparent, visible: false });
  loader.load(url, t => { t.colorSpace = THREE.SRGBColorSpace; mat.map = t; mat.visible = true; mat.needsUpdate = true; });
  return new THREE.Mesh(geo, mat);
}
function clearGroup(g) { for (const m of [...g.children]) { m.geometry?.dispose(); m.material?.map?.dispose(); m.material?.dispose(); g.remove(m); } }
const mapGroup = new THREE.Group(), rainGroup = new THREE.Group(); scene.add(mapGroup, rainGroup);
function addTiles(group, z, k, h, urlFn, transparent) {
  const n = 2 ** z, [cx, cy] = tileXY(S.home.lat, S.home.lon, z);
  const xs = k < 0 ? [...Array(n).keys()] : Array.from({ length: 2 * k + 1 }, (_, i) => cx - k + i);
  const ys = k < 0 ? [...Array(n).keys()] : Array.from({ length: 2 * k + 1 }, (_, i) => cy - k + i);
  for (const yy of ys) for (const xx of xs) { if (yy < 0 || yy >= n) continue; const x = (xx % n + n) % n; group.add(tileMesh(z, x, yy, urlFn(z, x, yy), h, transparent)); }
}
function buildMap() {
  clearGroup(mapGroup);
  for (const [z, k, h] of [[6, 2, 2e-6], [8, 3, 4e-6], [10, 2, 6e-6]]) addTiles(mapGroup, z, k, h, (z, x, y) => `/api/tile/${z}/${x}/${y}`, false);
}
async function loadRain() {
  rainGroup.visible = cfg.layers.rain;
  if (!cfg.layers.rain) return;
  setSrc('Rain', 'wait', '…');
  try {
    const m = await (await fetch('/api/rain/meta')).json();
    const fr = m.radar.past.at(-1), path = fr.path.replace(/^\//, '');
    clearGroup(rainGroup);
    addTiles(rainGroup, 2, -1, 3e-5, (z, x, y) => `/api/rain/${path}/${z}/${x}/${y}`, true);
    addTiles(rainGroup, 6, 2, 3.2e-5, (z, x, y) => `/api/rain/${path}/${z}/${x}/${y}`, true);
    S.rainTs = fr.time * 1000; setSrc('Rain', 'ok', ago(S.rainTs));
  } catch (e) { setSrc('Rain', 'err', 'erro'); }
}

// ---------------- casa, aneis e selecao
function spriteTex(draw) { const c = document.createElement('canvas'); c.width = c.height = 64; draw(c.getContext('2d')); const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t; }
const homeGroup = new THREE.Group(); scene.add(homeGroup);
const homePin = new THREE.Sprite(new THREE.SpriteMaterial({ map: spriteTex(g => { g.fillStyle = '#3987e5'; g.beginPath(); g.arc(32, 32, 20, 0, 7); g.fill(); g.lineWidth = 6; g.strokeStyle = '#fff'; g.stroke(); }), sizeAttenuation: false, depthTest: false }));
homePin.scale.set(.022, .022, 1); homePin.renderOrder = 9;
function circleLine(km, color, dashed) {
  const pts = []; for (let i = 0; i <= 160; i++) { const [la, lo] = advance(S.home.lat, S.home.lon, i / 160 * 360, km); pts.push(vec(la, lo, 8e-6)); }
  const g = new THREE.BufferGeometry().setFromPoints(pts);
  const m = dashed ? new THREE.LineDashedMaterial({ color, dashSize: km / EARTH_KM * .12, gapSize: km / EARTH_KM * .08, transparent: true, opacity: .85 })
                   : new THREE.LineBasicMaterial({ color, transparent: true, opacity: .55 });
  const l = new THREE.Line(g, m); if (dashed) l.computeLineDistances(); return l;
}
let ringKey = '';
function buildHome() {
  const key = `${S.home.lat},${S.home.lon},${cfg.aKm},${cfg.units},${device.rangeIdx}`;
  if (key === ringKey) return; ringKey = key;
  clearGroup(homeGroup);
  homeGroup.add(circleLine(cfg.aKm * (cfg.units === 'metric' ? 1 : MI), '#d03b3b', true));
  homeGroup.add(circleLine(device.rangeKm(cfg.units), '#1baf7a', false));
  homePin.position.copy(vec(S.home.lat, S.home.lon, 1e-5)); homeGroup.add(homePin);
}

const selGroup = new THREE.Group(); scene.add(selGroup);
const selRing = new THREE.Sprite(new THREE.SpriteMaterial({ map: spriteTex(g => { g.strokeStyle = '#ffc400'; g.lineWidth = 5; g.beginPath(); g.arc(32, 32, 26, 0, 7); g.stroke(); }), sizeAttenuation: false, depthTest: false, transparent: true }));
selRing.scale.set(.045, .045, 1); selRing.renderOrder = 10;
const dropGeo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3()]);
const dropLine = new THREE.Line(dropGeo, new THREE.LineBasicMaterial({ color: '#ffc400', transparent: true, opacity: .7 }));
let trailLine = null, routeLine = null;
selGroup.add(selRing, dropLine);

// ---------------- avioes (instanciados)
const MAXN = 30000;
const shape = new THREE.Shape();
const P = [[0, 1], [.12, .62], [.12, .22], [.92, -.08], [.92, -.24], [.12, -.08], [.09, -.6], [.36, -.8], [.36, -.93], [0, -.84]];
shape.moveTo(0, 1); for (const [x, y] of P.slice(1)) shape.lineTo(x, y); for (const [x, y] of P.slice(1, -1).reverse()) shape.lineTo(-x, y); shape.closePath();
const planeGeo = new THREE.ShapeGeometry(shape); planeGeo.rotateX(Math.PI / 2);
const inst = new THREE.InstancedMesh(planeGeo, new THREE.MeshBasicMaterial({ side: THREE.DoubleSide }), MAXN);
inst.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
inst.instanceColor = new THREE.InstancedBufferAttribute(new Float32Array(MAXN * 3), 3); inst.instanceColor.setUsage(THREE.DynamicDrawUsage);
inst.frustumCulled = false; inst.count = 0; scene.add(inst);
const dropGroup = new THREE.LineSegments(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ color: '#8fa3b8', transparent: true, opacity: .25 }));
dropGroup.frustumCulled = false; scene.add(dropGroup);

const STOPS = [[0, '#f9e04b'], [5000, '#f6a33b'], [10000, '#e8673c'], [20000, '#c93d78'], [30000, '#8b3fb5'], [40000, '#4e5ad6'], [45000, '#3fa3f0']];
const STOPC = STOPS.map(([a, c]) => [a, new THREE.Color(c)]), GROUND_C = new THREE.Color('#8a939c'), SEL_C = new THREE.Color('#ffffff');
function altColor(ft, out) {
  if (ft <= STOPC[0][0]) return out.copy(STOPC[0][1]);
  for (let i = 1; i < STOPC.length; i++) if (ft <= STOPC[i][0]) return out.copy(STOPC[i - 1][1]).lerp(STOPC[i][1], (ft - STOPC[i - 1][0]) / (STOPC[i][0] - STOPC[i - 1][0]));
  return out.copy(STOPC.at(-1)[1]);
}
$('legend').innerHTML = STOPS.slice(0, 6).map(([, c]) => `<i style="background:${c}"></i>`).join('');

function passes(p) {
  if (p.ground) { if (!cfg.ground) return false; }
  else if (p.alt < cfg.fMin || p.alt > cfg.fMax) return false;
  if (S.q) {
    const q = S.q, t = (p.type || '').toUpperCase();
    if (!((p.flight || '').toUpperCase().startsWith(q) || (p.reg || '').toUpperCase().replace('-', '').includes(q.replace('-', '')) || t === q || t.startsWith(q) || (q.length === 6 && p.hex.toUpperCase() === q))) return false;
  }
  return true;
}
function curPos(p, now) {
  const dt = Math.min(900, Math.max(0, (now - p.t0) / 1000));
  if (p.ground || p.gs < 30 || dt < .5) return [p.lat, p.lon];
  return advance(p.lat, p.lon, p.trk, p.gs * 1.852 * dt / 3600);
}

const M = new THREE.Matrix4(), vN = new THREE.Vector3(), vE = new THREE.Vector3(), vNo = new THREE.Vector3(), vD = new THREE.Vector3(), vS = new THREE.Vector3(), vP = new THREE.Vector3(), UPY = new THREE.Vector3(0, 1, 0), col = new THREE.Color();
let visible = [];
function camGround() { return S.view === 'local' ? camera.position.distanceTo(controls.target) : camera.position.length() - 1; }
function updatePlanes(now) {
  const s = clamp(camGround() * .009, 1.2e-4, .008), arr = inst.instanceColor.array, drops = [];
  const showDrops = S.view === 'local';
  let n = 0; visible = [];
  for (const p of S.planes.values()) {
    if (!passes(p) || n >= MAXN) continue;
    const [la, lo] = curPos(p, now); p.cl = la; p.co = lo; p.h = (p.ground ? 0 : altH(p.alt)) + 1.2e-5;
    vec(la, lo, 0, vN).normalize();
    vE.crossVectors(UPY, vN); if (vE.lengthSq() < 1e-10) vE.set(1, 0, 0); vE.normalize(); vNo.crossVectors(vN, vE);
    const t = p.trk * DEG; vD.copy(vNo).multiplyScalar(Math.cos(t)).addScaledVector(vE, Math.sin(t)); vS.crossVectors(vN, vD);
    vP.copy(vN).multiplyScalar(1 + p.h);
    if (showDrops && !p.ground && p.src === 'local') drops.push(vP.x, vP.y, vP.z, vN.x, vN.y, vN.z);
    const sc = p === S.selected ? s * 1.5 : s;
    M.makeBasis(vS.multiplyScalar(sc), vN.multiplyScalar(sc), vD.multiplyScalar(sc)); M.setPosition(vP); inst.setMatrixAt(n, M);
    (p === S.selected ? col.copy(SEL_C) : p.ground ? col.copy(GROUND_C) : altColor(p.alt, col)).toArray(arr, n * 3);
    (p.pos ||= new THREE.Vector3()).copy(vP); visible.push(p); n++;
  }
  inst.count = n; inst.instanceMatrix.needsUpdate = true; inst.instanceColor.needsUpdate = true;
  dropGroup.geometry.setAttribute('position', new THREE.Float32BufferAttribute(drops, 3)); dropGroup.visible = showDrops;
}

// ---------------- selecao
function select(p) {
  S.selected = p || null; S.follow = false;
  if (trailLine) { selGroup.remove(trailLine); trailLine.geometry.dispose(); trailLine = null; }
  if (routeLine) { selGroup.remove(routeLine); routeLine.geometry.dispose(); routeLine = null; }
  S.routeAps = [];
  renderDetail();
  if (p) { loadMeta(p); loadRoute(p); }
}
let trailT = 0;
function updateSel(t) {
  const p = S.selected, ok = !!(p && p.pos && visible.includes(p));
  selRing.visible = dropLine.visible = ok;
  if (!ok) return;
  selRing.position.copy(p.pos);
  const g = vec(p.cl, p.co, 0); const a = dropGeo.attributes.position;
  a.setXYZ(0, p.pos.x, p.pos.y, p.pos.z); a.setXYZ(1, g.x, g.y, g.z); a.needsUpdate = true;
  if (t - trailT > 1000) {
    trailT = t;
    if (trailLine) { selGroup.remove(trailLine); trailLine.geometry.dispose(); trailLine = null; }
    if (cfg.layers.trails && p.hist.length > 1) {
      const pts = p.hist.map(([la, lo, al]) => vec(la, lo, altH(al) + 1.2e-5)); pts.push(p.pos.clone());
      trailLine = new THREE.Line(new THREE.BufferGeometry().setFromPoints(pts), new THREE.LineBasicMaterial({ color: '#ffc400', transparent: true, opacity: .9 }));
      selGroup.add(trailLine);
    }
  }
  if (routeLine) routeLine.visible = cfg.layers.routes;
}
function buildRoute(r) {
  if (!r || !r.o || !r.d || !cfg.layers.routes) return;
  const a = vec(r.o.lat, r.o.lon).normalize(), b = vec(r.d.lat, r.d.lon).normalize(), pts = [], q = new THREE.Vector3();
  const ang = a.angleTo(b), top = altH(36000);
  for (let i = 0; i <= 128; i++) {
    const t = i / 128, s = Math.sin(ang);
    q.copy(a).multiplyScalar(Math.sin((1 - t) * ang) / s).addScaledVector(b, Math.sin(t * ang) / s).normalize();
    pts.push(q.clone().multiplyScalar(1 + 1e-5 + top * Math.min(1, Math.sin(Math.PI * t) * 4)));
  }
  routeLine = new THREE.Line(new THREE.BufferGeometry().setFromPoints(pts), new THREE.LineDashedMaterial({ color: '#ffc400', dashSize: .004, gapSize: .003, transparent: true, opacity: .75 }));
  routeLine.computeLineDistances(); selGroup.add(routeLine);
  S.routeAps = [{ code: r.o.iata || r.o.icao, v: vec(r.o.lat, r.o.lon, 1e-5) }, { code: r.d.iata || r.d.icao, v: vec(r.d.lat, r.d.lon, 1e-5) }];
}
async function loadMeta(p) {
  if (!S.meta[p.hex]) {
    S.meta[p.hex] = { loading: true };
    try {
      const j = await (await fetch('/api/aircraft/' + p.hex)).json(), a = j.response && j.response.aircraft;
      S.meta[p.hex] = a ? { model: [a.manufacturer, a.type].filter(Boolean).join(' '), icao: a.icao_type, reg: a.registration, owner: a.registered_owner, country: a.registered_owner_country_name, photo: a.url_photo_thumbnail } : { none: true };
    } catch (e) { S.meta[p.hex] = { none: true }; }
  }
  const m = S.meta[p.hex];
  if (!p.type && m.icao) p.type = m.icao;
  if (!p.reg && m.reg) p.reg = m.reg;
  if (S.selected === p) renderDetail();
}
async function loadRoute(p) {
  const cs = p.flight; if (!cs) return;
  if (!(cs in S.routes)) {
    S.routes[cs] = null;
    try {
      const j = await (await fetch('/api/route/' + encodeURIComponent(cs))).json(), fr = j.response && j.response.flightroute;
      const ap = x => x && { iata: x.iata_code, icao: x.icao_code, city: x.municipality, name: x.name, lat: x.latitude, lon: x.longitude };
      S.routes[cs] = fr ? { airline: fr.airline && fr.airline.name, o: ap(fr.origin), d: ap(fr.destination) } : { none: true };
    } catch (e) { S.routes[cs] = { none: true }; }
  }
  if (S.selected === p) { buildRoute(S.routes[cs]); renderDetail(); }
}

const EMERG = { '7500': 'SEQUESTRO (7500)', '7600': 'FALHA DE RÁDIO (7600)', '7700': 'EMERGÊNCIA (7700)' };
function renderDetail() {
  const el = $('detail'), p = S.selected;
  if (!p) { el.classList.remove('show'); return; }
  el.classList.add('show');
  const m = S.meta[p.hex] || {}, r = S.routes[p.flight], d = distKm(S.home.lat, S.home.lon, p.cl ?? p.lat, p.co ?? p.lon);
  const em = EMERG[p.squawk] || (p.emergency ? `Emergência: ${p.emergency}` : '');
  const ap = (x, fb) => x ? `<div class="ap"><b>${esc(x.iata || x.icao || '?')}</b><span>${esc(x.city || x.name || '')}</span></div>` : `<div class="ap"><b>—</b><span>${fb}</span></div>`;
  el.innerHTML = `
    <div class="cs"><span>${esc(p.flight || p.reg || p.hex.toUpperCase())}</span><span><button id="followBtn" class="${S.follow ? 'on' : ''}">Seguir</button> <button id="closeBtn" aria-label="Fechar">✕</button></span></div>
    <div class="air">${esc((r && r.airline) || m.owner || '')}</div>
    ${em ? `<div class="emerg">${esc(em)}</div>` : ''}
    ${r && r.o ? `<div class="route">${ap(r.o)}<span class="arrow">→</span>${ap(r.d)}</div>` : `<div class="route">${ap(null, r === null ? 'buscando rota…' : 'rota desconhecida')}<span class="arrow">→</span>${ap(null, '')}</div>`}
    ${m.photo ? `<img src="${esc(m.photo)}" alt="Foto da aeronave ${esc(p.reg || '')}" referrerpolicy="no-referrer">` : ''}
    <div class="kv">
      <div>Modelo<b>${esc(m.model || p.desc || p.type || (m.loading ? '…' : 'desconhecido'))}</b></div>
      <div>Código ICAO<b>${esc(p.type || m.icao || '—')}</b></div>
      <div>Matrícula<b>${esc(p.reg || '—')}</b></div>
      <div>Hex<b>${esc(p.hex.toUpperCase())}</b></div>
      <div>Altitude<b>${p.ground ? 'no solo' : Math.round(p.alt).toLocaleString('pt-BR') + ' ft'}</b></div>
      <div>Razão vertical<b>${Math.round(p.vs || 0).toLocaleString('pt-BR')} ft/min</b></div>
      <div>Velocidade<b>${Math.round(p.gs)} kt</b></div>
      <div>Rumo<b>${Math.round(p.trk)}°</b></div>
      <div>Squawk<b>${esc(p.squawk || '—')}</b></div>
      <div>Distância de casa<b>${fmtDist(d)}</b></div>
      <div>Fonte<b>${{ local: 'adsb.lol (5 s)', global: 'OpenSky (15 min)', type: 'busca por modelo' }[p.src] || p.src}</b></div>
      <div>País (registro)<b>${esc(m.country || p.country || '—')}</b></div>
    </div>`;
  $('closeBtn').onclick = () => select(null);
  $('followBtn').onclick = () => { S.follow = !S.follow; if (S.follow && S.view !== 'local') setView('local', p); renderDetail(); };
}

// ---------------- camera
function setView(v, center) {
  S.view = v; $('vGlobal').classList.toggle('on', v === 'global'); $('vLocal').classList.toggle('on', v === 'local');
  if (controls) controls.dispose();
  if (v === 'global') {
    const dir = camera.position.clone().normalize();
    camera.up.set(0, 1, 0);
    if (camera.position.length() < 1.3) camera.position.copy(dir.multiplyScalar(1.6));
    controls = new OrbitControls(camera, canvas);
    controls.target.set(0, 0, 0); controls.enablePan = false; controls.minDistance = 1.003; controls.maxDistance = 8;
  } else {
    const c = center ? [center.cl ?? center.lat, center.co ?? center.lon] : [S.home.lat, S.home.lon];
    const n = vec(c[0], c[1]).normalize(), east = new THREE.Vector3().crossVectors(UPY, n).normalize(), north = new THREE.Vector3().crossVectors(n, east);
    camera.up.copy(n);
    camera.position.copy(n).addScaledVector(n, .03).addScaledVector(north, -.045);
    controls = new OrbitControls(camera, canvas);
    controls.target.copy(n); controls.screenSpacePanning = false; controls.minDistance = .001; controls.maxDistance = .7; controls.maxPolarAngle = Math.PI * .48;
  }
  controls.enableDamping = true; controls.update();
}
function flyTo(p) {
  if (!p) return;
  const la = p.cl ?? p.lat, lo = p.co ?? p.lon;
  if (S.view === 'local' && distKm(...targetLatLon(), la, lo) < 500) {
    const to = vec(la, lo).normalize(); S.anim = { kind: 'local', from: controls.target.clone(), to, t0: performance.now() };
  } else {
    if (S.view !== 'global') setView('global');
    S.anim = { kind: 'global', from: camera.position.clone(), to: vec(la, lo).normalize().multiplyScalar(1.12), t0: performance.now() };
  }
}
function targetLatLon() { const t = controls.target.clone().normalize(); return [Math.asin(t.y) / DEG, Math.atan2(t.x, t.z) / DEG]; }
function stepAnim(now) {
  const a = S.anim, k = clamp((now - a.t0) / 1200, 0, 1), e = k < .5 ? 2 * k * k : 1 - (-2 * k + 2) ** 2 / 2;
  if (a.kind === 'global') {
    const r = a.from.length() + (a.to.length() - a.from.length()) * e;
    camera.position.copy(a.from.clone().normalize().lerp(a.to.clone().normalize(), e).normalize().multiplyScalar(r));
  } else {
    const nt = a.from.clone().lerp(a.to, e).normalize(), d = nt.clone().sub(controls.target);
    controls.target.copy(nt); camera.position.add(d);
  }
  if (k >= 1) S.anim = null;
}
function followStep() {
  const p = S.selected;
  if (!S.follow || !p || S.view !== 'local' || p.cl == null || S.anim) return;
  const g = vec(p.cl, p.co).normalize(), d = g.clone().sub(controls.target);
  controls.target.copy(g); camera.position.add(d);
}

// ---------------- rotulos HTML
const lblPool = [];
function lbl(i) { while (lblPool.length <= i) { const d = document.createElement('div'); d.className = 'lbl'; $('labels').appendChild(d); lblPool.push(d); } return lblPool[i]; }
const tv = new THREE.Vector3(), tc = new THREE.Vector3();
function place(el, v3, html, cls, dy = -9) {
  tv.copy(v3).project(camera);
  if (tv.z > 1 || Math.abs(tv.x) > 1.05 || Math.abs(tv.y) > 1.05) return false;
  tc.copy(camera.position).sub(v3); if (tc.dot(v3) < 0 && S.view === 'global') return false;
  const x = (tv.x + 1) / 2 * innerWidth, y = (1 - tv.y) / 2 * innerHeight;
  if (el._h !== html) { el.innerHTML = html; el._h = html; }
  el.className = 'lbl' + (cls ? ' ' + cls : ''); el.style.transform = `translate(${(x + 9) | 0}px,${(y + dy) | 0}px)`; el.style.display = '';
  return true;
}
function updateLabels() {
  let k = 0;
  const cand = [];
  if (S.selected && S.selected.pos && visible.includes(S.selected)) cand.push(S.selected);
  if (cfg.layers.labels && camGround() < .35) {
    const cp = camera.position, near = [];
    for (const p of visible) { const d = p.pos.distanceToSquared(cp); if (d < .09) near.push([d, p]); }
    near.sort((a, b) => a[0] - b[0]);
    for (const [, p] of near.slice(0, 35)) if (p !== S.selected) cand.push(p);
  }
  for (const p of cand) {
    const html = `${esc(p.flight || p.reg || p.hex)}<small>${p.ground ? 'solo' : 'FL' + String(Math.round(p.alt / 100)).padStart(3, '0')}${p.type ? ' · ' + esc(p.type) : ''}</small>`;
    if (place(lbl(k), p.pos, html, p === S.selected ? 'sel' : '')) k++;
  }
  if (cfg.layers.routes) for (const a of S.routeAps) if (a.code && place(lbl(k), a.v, `✈ ${esc(a.code)}`, 'sel ap', 12)) k++;
  for (let i = k; i < lblPool.length; i++) lblPool[i].style.display = 'none';
}

// ---------------- picking
let down = null;
canvas.addEventListener('pointerdown', e => { down = [e.clientX, e.clientY]; S.anim = null; });
canvas.addEventListener('pointerup', e => {
  if (!down || Math.hypot(e.clientX - down[0], e.clientY - down[1]) > 5) return;
  let best = null, bd = 16;
  for (const p of visible) {
    tv.copy(p.pos).project(camera); if (tv.z > 1) continue;
    const d = Math.hypot((tv.x + 1) / 2 * innerWidth - e.clientX, (1 - tv.y) / 2 * innerHeight - e.clientY);
    if (d < bd) { tc.copy(camera.position).sub(p.pos); if (S.view === 'global' && tc.dot(p.pos) < 0) continue; bd = d; best = p; }
  }
  select(best);
});

// ---------------- dados
function setSrc(k, state, txt) { $('d' + k).className = 'dot ' + state; $('t' + k).textContent = txt || ''; }
function upsert(a, src, nowMs) {
  if (a.lat == null || a.lon == null) return null;
  let p = S.planes.get(a.hex); if (!p) { p = { hex: a.hex, hist: [] }; S.planes.set(a.hex, p); }
  const ground = a.alt_baro === 'ground', alt = ground ? 0 : (typeof a.alt_baro === 'number' ? a.alt_baro : (a.alt_geom ?? 0));
  Object.assign(p, { flight: (a.flight || '').trim(), reg: a.r || p.reg || '', type: a.t || p.type || '', desc: a.desc || p.desc || '',
    lat: a.lat, lon: a.lon, t0: nowMs - (a.seen_pos || 0) * 1000, alt, ground, gs: a.gs || 0, trk: a.track ?? a.true_heading ?? p.trk ?? 0,
    vs: a.baro_rate ?? a.geom_rate ?? 0, squawk: a.squawk || '', src, seen: Date.now(), emergency: a.emergency && a.emergency !== 'none' ? a.emergency : '' });
  const last = p.hist.at(-1); if (!last || last[0] !== a.lat || last[1] !== a.lon) { p.hist.push([a.lat, a.lon, alt]); if (p.hist.length > 300) p.hist.shift(); }
  return p;
}
async function pollLocal() {
  try {
    const j = await (await fetch(`/api/local?lat=${S.home.lat}&lon=${S.home.lon}&r=150`)).json();
    if (!j.ac) throw new Error(j.error || 'sem dados');
    let now = j.now || Date.now(); if (now < 1e12) now *= 1000;
    for (const a of j.ac) upsert(a, 'local', now);
    for (const [hex, p] of S.planes) if (p.src === 'local' && Date.now() - p.seen > 60000) S.planes.delete(hex);
    S.lastLocal = Date.now(); setSrc('Local', 'ok', ago(S.lastLocal));
    checkAlerts(); if (S.q) renderResults();
  } catch (e) { setSrc('Local', 'err', 'erro'); }
}
async function pollGlobal() {
  if (!cfg.layers.global) { for (const [hex, p] of S.planes) if (p.src === 'global') S.planes.delete(hex); setSrc('Global', '', 'desligado'); return; }
  try {
    const r = await fetch('/api/global'), j = await r.json();
    if (!j.states) throw new Error(r.status === 429 ? 'limite atingido' : 'erro');
    const ts = (j.time || Date.now() / 1000) * 1000;
    if (ts !== S.globalTs) {
      S.globalTs = ts; const keep = new Set();
      for (const s of j.states) {
        if (s[5] == null || s[6] == null) continue;
        const hex = s[0]; keep.add(hex);
        let p = S.planes.get(hex); if (p && p.src !== 'global') continue;
        if (!p) { p = { hex, hist: [] }; S.planes.set(hex, p); }
        const ground = !!s[8];
        Object.assign(p, { flight: (s[1] || '').trim(), lat: s[6], lon: s[5], t0: (s[3] || j.time) * 1000, alt: ground ? 0 : (s[7] ?? s[13] ?? 0) * 3.28084,
          ground, gs: (s[9] || 0) * 1.94384, trk: s[10] || 0, vs: (s[11] || 0) * 196.85, squawk: s[14] || '', src: 'global', seen: Date.now(), country: s[2] });
      }
      for (const [hex, p] of S.planes) if (p.src === 'global' && !keep.has(hex)) S.planes.delete(hex);
    }
    setSrc('Global', 'ok', ago(S.globalTs));
  } catch (e) { setSrc('Global', 'err', e.message); }
}
async function typeSearch() {
  const t = $('q').value.trim().toUpperCase();
  if (!/^[A-Z0-9]{2,4}$/.test(t)) { toast('Digite o código ICAO do modelo, por exemplo A388, B77W, B38M ou E190.', false); return; }
  $('typeBtn').disabled = true;
  try {
    const j = await (await fetch('/api/type/' + t)).json();
    for (const [hex, p] of S.planes) if (p.src === 'type') S.planes.delete(hex);
    let n = 0; const now = Date.now();
    for (const a of j.ac || []) { const ex = S.planes.get(a.hex); if (ex && ex.src === 'local') { n++; continue; } if (upsert(a, 'type', now)) n++; }
    S.q = t; renderResults();
    toast(`${n} aeronave(s) ${t} no ar agora pelo mundo.`, false);
  } catch (e) { toast('Falha na busca por modelo.', false); }
  $('typeBtn').disabled = false;
}

// ---------------- alertas
let actx = null;
function beep() {
  if (!S.sound) return;
  actx ||= new AudioContext();
  const t = actx.currentTime;
  [[0, 880], [.2, 1320]].forEach(([o, f]) => {
    const osc = actx.createOscillator(), g = actx.createGain();
    osc.frequency.value = f; g.gain.setValueAtTime(.0001, t + o); g.gain.exponentialRampToValueAtTime(.25, t + o + .02); g.gain.exponentialRampToValueAtTime(.0001, t + o + .16);
    osc.connect(g).connect(actx.destination); osc.start(t + o); osc.stop(t + o + .17);
  });
}
function toast(msg, alert = true) {
  const d = document.createElement('div'); d.className = 'toast'; if (!alert) d.style.borderColor = 'var(--line)';
  d.textContent = msg; $('toasts').appendChild(d); setTimeout(() => d.remove(), 6000);
}
const alertLog = [];
function checkAlerts() {
  // Entra na zona: alerta 1 vez. Sai so com 25% de folga (histerese) e o mesmo aviao
  // so alerta de novo depois de 20 min: antes, oscilar na borda repetia o alerta.
  const kmR = cfg.aKm * (cfg.units === 'metric' ? 1 : MI), inside = new Set(), now = Date.now();
  for (const p of S.planes.values()) {
    if (p.src !== 'local' || p.ground) continue;
    const m = S.alerted.get(p.hex), was = m && m.inside;
    const d = distKm(S.home.lat, S.home.lon, p.cl ?? p.lat, p.co ?? p.lon);   // posicao estimada agora
    if (p.alt > cfg.aFt + (was ? 300 : 0) || d > kmR * (was ? 1.25 : 1)) { if (m) m.inside = false; continue; }
    inside.add(p.hex);
    if (m) m.inside = true;
    if (!m || (!was && now - m.at > 20 * 60000)) {
      S.alerted.set(p.hex, { at: now, inside: true }); beep();
      const name = p.flight || p.reg || p.hex;
      toast(`Aproximação: ${name}${p.type ? ' (' + p.type + ')' : ''} a ${fmtDist(d)}, ${Math.round(p.alt)} ft`);
      alertLog.unshift({ hex: p.hex, txt: name + (p.type ? ' · ' + p.type : ''), info: `${fmtDist(d)} · ${Math.round(p.alt)} ft`, t: new Date().toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' }) });
      alertLog.length = Math.min(alertLog.length, 20);
    }
  }
  for (const [hex, m] of S.alerted) if (now - m.at > 20 * 60000 && !m.inside) S.alerted.delete(hex);
  S.alerting = inside.size > 0;
  $('alerts').innerHTML = alertLog.length ? alertLog.map(a => `<li data-hex="${esc(a.hex)}"><span>${esc(a.t)} ${esc(a.txt)}</span><span>${esc(a.info)}</span></li>`).join('') : '<li class="empty">Nenhum alerta.</li>';
}

// ---------------- clima
const WMO = { 0: 'Céu limpo', 1: 'Quase limpo', 2: 'Parcialmente nublado', 3: 'Nublado', 45: 'Nevoeiro', 48: 'Nevoeiro com geada', 51: 'Garoa fraca', 53: 'Garoa', 55: 'Garoa forte',
  56: 'Garoa congelante', 57: 'Garoa congelante', 61: 'Chuva fraca', 63: 'Chuva', 65: 'Chuva forte', 66: 'Chuva congelante', 67: 'Chuva congelante forte', 71: 'Neve fraca', 73: 'Neve', 75: 'Neve forte',
  77: 'Grãos de neve', 80: 'Pancadas fracas', 81: 'Pancadas de chuva', 82: 'Pancadas fortes', 85: 'Pancadas de neve', 86: 'Pancadas de neve fortes', 95: 'Trovoada', 96: 'Trovoada com granizo', 99: 'Trovoada com granizo' };
const CAT = { VFR: '#0ca30c', MVFR: '#3987e5', IFR: '#d03b3b', LIFR: '#c13ab0' };
async function loadWx() {
  $('wxCity').textContent = '· ' + (S.home.city || '');
  try {
    const c = (await (await fetch(`/api/wx?lat=${S.home.lat}&lon=${S.home.lon}`)).json()).current;
    const imp = cfg.units !== 'metric', wind = imp ? `${Math.round(c.wind_speed_10m / MI)} mph` : `${Math.round(c.wind_speed_10m)} km/h`;
    const vis = imp ? `${(c.visibility / 1609).toFixed(0)} mi` : `${(c.visibility / 1000).toFixed(0)} km`;
    $('wx').innerHTML = `<b>${Math.round(c.temperature_2m)}°C</b> · ${esc(WMO[c.weather_code] || '—')}<br>Vento ${wind} de ${Math.round(c.wind_direction_10m)}° · Nuvens ${c.cloud_cover}% · Visib. ${vis}`;
  } catch (e) { $('wx').textContent = 'Clima indisponível.'; }
  try {
    const list = await (await fetch(`/api/metar?lat=${S.home.lat}&lon=${S.home.lon}`)).json();
    let st = null, bd = 1e9;
    for (const m of list) { const d = distKm(S.home.lat, S.home.lon, m.lat, m.lon) + (m.fltCat ? 0 : 1000); if (d < bd) { bd = d; st = m; } }
    if (st) setAtcAirport(st.icaoId, (st.name || '').split(',')[0]);
    $('metar').innerHTML = st ? `<b>${esc(st.icaoId)}</b> ${esc((st.name || '').split(',')[0])}${st.fltCat ? `<span class="cat" style="background:${CAT[st.fltCat] || '#888'}">${esc(st.fltCat)}</span>` : ''}<br>${esc(st.rawOb)}` : 'Sem METAR próximo.';
  } catch (e) { $('metar').textContent = 'METAR indisponível.'; }
}

// ---------------- simulador
const devOpts = { onSelect: hex => select(S.planes.get(hex)) };
const devices = { cyd: new Device($('cyd'), devOpts), round: new RoundDevice($('round'), devOpts) };
let device = devices.cyd;
function setDevice(k) {
  if (!devices[k]) k = 'cyd';
  devices[k].rangeIdx = device.rangeIdx; device = devices[k]; cfg.dev = k; save();
  $('cyd').hidden = k !== 'cyd'; $('round').hidden = k !== 'round';
  $('dCyd').classList.toggle('on', k === 'cyd'); $('dRound').classList.toggle('on', k === 'round');}
$('dCyd').onclick = () => setDevice('cyd'); $('dRound').onclick = () => setDevice('round');
setDevice(cfg.dev || 'cyd');
function drawDevice() {
  const list = [];
  for (const p of visible) {
    if (p.src !== 'local') continue;
    list.push({ hex: p.hex, flight: p.flight, reg: p.reg, type: p.type, alt: p.alt, gs: p.gs, trk: p.trk, ground: p.ground,
      dist: distKm(S.home.lat, S.home.lon, p.cl, p.co), brg: bearing(S.home.lat, S.home.lon, p.cl, p.co) });
  }
  // modelo e destino do selecionado (adsbdb, ja carregados pelo painel de detalhes)
  let info = null;
  if (S.selected) {
    const m = S.meta[S.selected.hex] || {}, r = S.routes[S.selected.flight];
    const ap = x => x ? { code: x.iata || x.icao || '?', city: x.city || x.name || '' } : null;
    info = { model: m.model || '', orig: ap(r && r.o), dest: ap(r && r.d),
      loading: m.loading || r === null || (S.selected.flight && !(S.selected.flight in S.routes)) };
  }
  device.draw({ home: S.home, units: cfg.units, planes: list, selectedHex: S.selected && S.selected.hex, selInfo: info, alerting: S.alerting });
  buildHome();
}

// ---------------- UI
function renderResults() {
  const ul = $('results');
  if (!S.q) { ul.innerHTML = '<li class="empty">Digite para filtrar o mapa.</li>'; return; }
  const list = [...S.planes.values()].filter(passes).map(p => [distKm(S.home.lat, S.home.lon, p.lat, p.lon), p]).sort((a, b) => a[0] - b[0]).slice(0, 80);
  ul.innerHTML = list.length ? list.map(([d, p]) => `<li data-hex="${esc(p.hex)}"><span>${esc(p.flight || p.reg || p.hex)} <small style="color:var(--muted)">${esc(p.type)}</small></span><span>${p.ground ? 'solo' : Math.round(p.alt) + ' ft'} · ${fmtDist(d)}</span></li>`).join('')
    : '<li class="empty">Nada no tráfego carregado. Para um modelo, use “Buscar este modelo no mundo”.</li>';
}
for (const id of ['results', 'alerts']) $(id).addEventListener('click', e => { const li = e.target.closest('li[data-hex]'); const p = li && S.planes.get(li.dataset.hex); if (p) { select(p); flyTo(p); } });
function bindNum(id, key, cb) { const el = $(id); el.value = cfg[key]; el.addEventListener('change', () => { const v = parseFloat(el.value); if (!isNaN(v)) { cfg[key] = v; save(); cb && cb(); } }); }
bindNum('fMin', 'fMin'); bindNum('fMax', 'fMax', () => $('fMaxR').value = cfg.fMax);
$('fMaxR').value = cfg.fMax; $('fMaxR').oninput = () => { cfg.fMax = +$('fMaxR').value; $('fMax').value = cfg.fMax; save(); };
$('fGround').checked = cfg.ground; $('fGround').onchange = () => { cfg.ground = $('fGround').checked; save(); };
bindNum('aKm', 'aKm', checkAlerts); bindNum('aFt', 'aFt', checkAlerts);
$('q').addEventListener('input', () => { S.q = $('q').value.trim().toUpperCase(); renderResults(); });
$('q').addEventListener('keydown', e => { if (e.key === 'Enter') typeSearch(); });
$('typeBtn').onclick = typeSearch;
$('soundBtn').onclick = () => { S.sound = !S.sound; if (S.sound) { actx ||= new AudioContext(); actx.resume(); } $('soundBtn').textContent = 'Som: ' + (S.sound ? 'ligado' : 'desligado'); $('soundBtn').classList.toggle('on', S.sound); };
$('testBtn').onclick = () => { const was = S.sound; S.sound = true; actx ||= new AudioContext(); actx.resume(); beep(); S.sound = was; };
for (const [id, key, fn] of [['lGlobal', 'global', pollGlobal], ['lRain', 'rain', loadRain], ['lRoutes', 'routes', () => S.selected && select(S.selected)], ['lTrails', 'trails'], ['lLabels', 'labels']]) {
  $(id).checked = cfg.layers[key]; $(id).onchange = () => { cfg.layers[key] = $(id).checked; save(); fn && fn(); };
}
// ---------------- audio da torre: LiveATC (player oficial deles) + receptor proprio
// Os termos da LiveATC proibem tocar os streams dentro de outro app, entao abrimos o player deles numa janelinha.
const LIVEATC = {
  CYUL: [
    ['cyul_twr2', 'Torre', '119.300 · 119.900 · 124.300'],
    ['cyul_app', 'Aproximação / Saída', '118.900 · 120.425 · 124.650 · 126.900 · 132.850'],
    ['cyul_gnd', 'Autorização / Solo / Pátio', '121.000 · 121.900 · 122.075 · 125.600'],
    ['cyul_ctr_mtl', 'Centro Montreal (CZUL)', '123.925 – 135.600'],
    ['czul_yow', 'Centro – Setor Vallie', '134.975'],
    ['cyul_ice', 'Degelo (inverno)', '122.525 · 123.125'],
  ],
};
let atcIcao = '';
function setAtcAirport(icao, name) {
  if (icao === atcIcao) return;
  atcIcao = icao;
  $('atcAp').innerHTML = `Aeroporto mais próximo: <b>${esc(icao)}</b> ${esc(name)}`;
  const feeds = LIVEATC[icao];
  $('atcFeed').innerHTML = feeds ? feeds.map(([m, n]) => `<option value="${m}">${esc(n)}</option>`).join('') : '<option value="">Ver canais na LiveATC</option>';
  if (feeds && cfg.atcMount && feeds.some(f => f[0] === cfg.atcMount)) $('atcFeed').value = cfg.atcMount;
  showAtcFreq();
}
function showAtcFreq() {
  const f = (LIVEATC[atcIcao] || []).find(f => f[0] === $('atcFeed').value);
  $('atcFreq').textContent = f ? `MHz: ${f[2]}` : '';
}
$('atcFeed').onchange = () => { cfg.atcMount = $('atcFeed').value; save(); showAtcFreq(); };
$('atcLive').onclick = () => {
  if (!atcIcao) { toast('Ainda procurando o aeroporto mais próximo…', false); return; }
  const m = $('atcFeed').value, ic = atcIcao.toLowerCase();
  const url = m ? `https://www.liveatc.net/hlisten.php?mount=${encodeURIComponent(m)}&icao=${ic}` : `https://www.liveatc.net/search/?icao=${ic}`;
  const w = window.open(url, 'liveatc', m ? 'popup,width=520,height=620' : '');
  if (!w) toast('O navegador bloqueou a janela. Permita pop-ups para localhost.', false);
};

const atc = $('atcAudio');
$('atcUrl').value = cfg.atcUrl || '';
$('atcUrl').addEventListener('change', () => { cfg.atcUrl = $('atcUrl').value.trim(); save(); });
const atcState = (txt, on) => { $('atcTxt').textContent = txt; $('atcBtn').textContent = on ? '■ Parar' : '▶ Ouvir meu receptor'; $('atcBtn').classList.toggle('on', on); };
$('atcBtn').onclick = () => {
  if (!atc.paused) { atc.pause(); atc.removeAttribute('src'); atc.load(); atcState('Parado.', false); return; }
  const url = $('atcUrl').value.trim();
  if (!/^https?:\/\//i.test(url)) { toast('Informe a URL do stream do seu receptor (http://…).', false); return; }
  cfg.atcUrl = url; save(); atc.src = url; atcState('Conectando…', true);
  atc.play().then(() => atcState('Ao vivo.', true)).catch(() => atcState('Não consegui tocar esse stream. Confira a URL e se o receptor está ligado.', false));
};
atc.onerror = () => { if (atc.getAttribute('src')) atcState('O stream caiu ou não respondeu.', false); };

function setUnits(u) {
  cfg.units = u; save();
  $('uMetric').classList.toggle('on', u === 'metric'); $('uImp').classList.toggle('on', u !== 'metric');
  document.querySelectorAll('.uDist').forEach(e => e.textContent = u === 'metric' ? 'km' : 'mi');
  loadWx();
}
$('uMetric').onclick = () => setUnits('metric'); $('uImp').onclick = () => setUnits('imperial');
$('vGlobal').onclick = () => setView('global'); $('vLocal').onclick = () => setView('local');
$('menuBtn').onclick = () => $('side').classList.toggle('open');
$('geoBtn').onclick = () => navigator.geolocation ? navigator.geolocation.getCurrentPosition(
  pos => { S.home = { lat: +pos.coords.latitude.toFixed(4), lon: +pos.coords.longitude.toFixed(4), city: 'minha localização' }; cfg.home = S.home; save(); homeChanged(); },
  () => toast('Não consegui acessar a localização do navegador.', false)) : toast('Navegador sem geolocalização.', false);
function homeChanged() {
  $('homeTxt').textContent = `Casa: ${S.home.city} (${S.home.lat.toFixed(3)}, ${S.home.lon.toFixed(3)})`;
  ringKey = ''; buildMap(); loadWx(); loadRain(); pollLocal(); S.planes.forEach((p, h) => p.src === 'local' && S.planes.delete(h));
  if (S.view === 'local') setView('local');
}
function updateUi() {
  $('sCount').textContent = visible.length.toLocaleString('pt-BR');
  if (S.lastLocal) setSrc('Local', Date.now() - S.lastLocal < 20000 ? 'ok' : 'err', ago(S.lastLocal));
  if (S.globalTs && cfg.layers.global) $('tGlobal').textContent = ago(S.globalTs);
  if (S.rainTs && cfg.layers.rain) $('tRain').textContent = ago(S.rainTs);
  if (S.selected) { if (!S.planes.has(S.selected.hex)) select(null); else renderDetail(); }
}

// ---------------- loop
let tLbl = 0, tDev = 0, tUi = 0;
function loop(t) {
  requestAnimationFrame(loop);
  if (S.anim) stepAnim(t);
  if (S.view === 'global') controls.rotateSpeed = clamp((camera.position.length() - 1) * .6, .015, 1);
  else controls.rotateSpeed = .5;
  followStep();
  controls.update();
  mapGroup.visible = camGround() < .3;
  updatePlanes(Date.now());
  updateSel(t);
  renderer.render(scene, camera);
  if (t - tLbl > 90) { tLbl = t; updateLabels(); }
  if (t - tDev > 66) { tDev = t; drawDevice(); }
  if (t - tUi > 1000) { tUi = t; updateUi(); }
}

async function init() {
  if (!cfg.home) {
    try { const g = await (await fetch('/api/geo')).json(); if (g.status === 'success') S.home = { lat: g.lat, lon: g.lon, city: g.city }; } catch (e) {}
  }
  camera.position.copy(vec(S.home.lat, S.home.lon, 1.4));
  setView('global'); setUnits(cfg.units); homeChanged();
  loadWorld().catch(() => toast('Não consegui carregar o mapa-múndi.', false));
  pollGlobal();
  setInterval(pollLocal, 5000); setInterval(pollGlobal, 60000); setInterval(loadWx, 600000); setInterval(() => cfg.layers.rain && loadRain(), 300000);
  requestAnimationFrame(loop);
}
window.radar = { S, select, setView, flyTo };
init();
