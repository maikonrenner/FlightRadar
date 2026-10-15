// Pagina servida pelo ESP32 (textos da tela em frances).
// Tela redonda (Waveshare AMOLED 1,75", 466x466): estilo "Controleur" (scope de controle de trafego) ou "Classique".
// Tela retangular (CYD 2,8", 320x240) = copia de radar/device.js.
#pragma once
const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr-CA"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radar de bureau</title>
<style>
:root{color-scheme:dark}
html,body{margin:0;background:#070b10;color:#a7b3c0;font:13px system-ui,sans-serif}
main{min-height:100vh;display:grid;place-items:center;padding:16px;box-sizing:border-box}
canvas{display:block;touch-action:manipulation;cursor:pointer}
canvas[hidden]{display:none}
#dlg{position:fixed;inset:0;background:rgba(0,0,0,.6);display:grid;place-items:center;padding:16px;z-index:9}#dlg[hidden]{display:none}
#dlg .box{background:#0e141c;border:1px solid rgba(255,255,255,.1);border-radius:16px;padding:16px;width:min(100%,380px);max-height:90vh;overflow:auto;color:#e6edf3}
#dlg h3{margin:0 0 8px;color:#ffc400}#dlg p{margin:8px 0;font-size:13px}#dlg .dim{color:#7a8794}
#dlg ul{list-style:none;margin:8px 0;padding:0;max-height:240px;overflow:auto}#dlg li{display:flex;justify-content:space-between;gap:8px;padding:10px 6px;border-bottom:1px solid rgba(255,255,255,.07);cursor:pointer}
#dlg li:hover{background:rgba(255,255,255,.05)}#dlg input{width:100%;box-sizing:border-box;background:#070b10;color:#e6edf3;border:1px solid rgba(255,255,255,.15);border-radius:8px;padding:10px;font:14px system-ui;margin:4px 0}
#dlg button{font:600 13px system-ui;color:#e6edf3;background:#18212c;border:1px solid rgba(255,255,255,.1);border-radius:8px;padding:9px 12px;cursor:pointer;margin-top:6px}#dlg button.go{background:#ffc400;color:#111;border-color:#ffc400}
#dlg .row{display:flex;gap:8px;align-items:center}#dlg .row input{flex:1}
#round{width:min(calc(100vw - 60px),calc(100vh - 60px),520px);aspect-ratio:1;border-radius:50%;box-shadow:0 0 0 12px #1b1f24,0 0 0 14px #2c333b}
#rect{width:min(calc(100vw - 52px),640px);aspect-ratio:4/3;image-rendering:pixelated;border-radius:6px;box-shadow:0 0 0 10px #1b1f24}
</style></head><body><main>
<canvas id="round" width="466" height="466" aria-label="Radar"></canvas>
<canvas id="rect" width="320" height="240" aria-label="Radar" hidden></canvas>
</main>
<div id="dlg" hidden></div>
<script>
const $=id=>document.getElementById(id);
const RANGES={metric:[10,25,50,100,200],imperial:[5,10,25,50,100]},KM_PER={metric:1,imperial:1.60934};
const ATC_NM=[5,10,25,50,100];   // alcances do estilo controlador, em milhas nauticas
const STOPS=[[0,'#f9e04b'],[5000,'#f6a33b'],[10000,'#e8673c'],[20000,'#c93d78'],[30000,'#8b3fb5'],[40000,'#4e5ad6'],[45000,'#3fa3f0']];
const altColor=ft=>{let c=STOPS[0][1];for(const[a,col]of STOPS)if(ft>=a)c=col;return c};
const fr=n=>Math.round(n).toLocaleString('fr-CA');
// aeroportos (OurAirports) a ate 200 km de casa: [icao, lat, lon, [pistas: lat1,lon1,lat2,lon2]]
// aeroportos e pistas perto da casa: [icao, lat, lon, [pistas: lat1,lon1,lat2,lon2]], do OurAirports
// (arquivo estatico no GitHub, ~4 MB, baixado so quando a posicao muda; o resultado fica no localStorage)
let APTS=[],aptsKey='',aptsRetry=0;
async function loadAirports(lat,lon){
  const key=lat.toFixed(1)+','+lon.toFixed(1);if(key===aptsKey||Date.now()<aptsRetry)return;aptsKey=key;
  try{const c=JSON.parse(localStorage.getItem('apts_'+key)||'null');if(c&&c.length){APTS=c;return}}catch(e){}
  try{
    const r=await fetch('https://davidmegginson.github.io/ourairports-data/runways.csv');if(!r.ok)throw 0;
    const by={},k=Math.cos(lat*Math.PI/180);
    for(const line of (await r.text()).split(String.fromCharCode(10))){
      const c=line.split(',');   // campos: 2 airport_ident, 3 length_ft, 7 closed, 9-10 e 15-16 = pontas da pista
      const id=(c[2]||'').replace(/"/g,''),len=+c[3],la1=+c[9],lo1=+c[10],la2=+c[15],lo2=+c[16];
      if(!/^[A-Z]{4}$/.test(id)||len<4000||c[7]==='1'||!la1||!la2)continue;   // so pistas de 1.200 m ou mais
      if(Math.hypot((la1-lat)*111,(lo1-lon)*111*k)>200)continue;
      (by[id]=by[id]||[]).push([+la1.toFixed(4),+lo1.toFixed(4),+la2.toFixed(4),+lo2.toFixed(4)]);
    }
    APTS=Object.entries(by).map(([id,rw])=>{const n=rw.length*2;
      return[id,+(rw.reduce((a,r)=>a+r[0]+r[2],0)/n).toFixed(4),+(rw.reduce((a,r)=>a+r[1]+r[3],0)/n).toFixed(4),rw]});
    try{localStorage.setItem('apts_'+key,JSON.stringify(APTS))}catch(e){}
  }catch(e){aptsKey='';aptsRetry=Date.now()+120000}   // falhou (sem internet): tenta de novo em 2 min, sem martelar
}
let pref={};try{pref=JSON.parse(localStorage.getItem('radar_esp')||'{}')}catch(e){}
let units=pref.units||'metric',rangeIdx=pref.rangeIdx??2,mode=pref.mode||'round',mapOn=pref.mapOn??true,style=pref.style||'atc',groundOn=pref.groundOn??false,actx=null,menuOpen=false;
// PC com o server.py (radar 3D): ele abre o audio da torre de controle nas caixas de som do PC
let PC='',apShown=false;let atc={on:false,available:false,reach:false};   // PC vem do ESP (PC_URL no secrets.h)
const savePref=()=>{try{localStorage.setItem('radar_esp',JSON.stringify({units,rangeIdx,mode,mapOn,style,groundOn}))}catch(e){}};
let D={planes:[],alerting:0,alertOn:true,soundOn:true},recvAt=0,selHex=null,lastAlert=0,conn=false;
const info={};   // hex -> {model, orig, dest} vindo do ESP (/info)
const hist=new Map();let histAt=0;   // rastro: ultimas posicoes (km x/y a partir de casa)

// ---------- primitivas (as mesmas que existem na Arduino_GFX / TFT_eSPI)
function gfx(cv){const g=cv.getContext('2d');return{g,
  fillRect(x,y,w,h,c){g.fillStyle=c;g.fillRect(x,y,w,h)},
  drawLine(x0,y0,x1,y1,c,w=1){g.strokeStyle=c;g.lineWidth=w;g.beginPath();g.moveTo(x0,y0);g.lineTo(x1,y1);g.stroke()},
  drawCircle(x,y,r,c,w=1){g.strokeStyle=c;g.lineWidth=w;g.beginPath();g.arc(x,y,r,0,Math.PI*2);g.stroke()},
  fillCircle(x,y,r,c){g.fillStyle=c;g.beginPath();g.arc(x,y,r,0,Math.PI*2);g.fill()},
  fillTriangle(x0,y0,x1,y1,x2,y2,c){g.fillStyle=c;g.beginPath();g.moveTo(x0,y0);g.lineTo(x1,y1);g.lineTo(x2,y2);g.fill()},
  fillArc(x,y,r0,r1,a0,a1,c){g.fillStyle=c;g.beginPath();g.arc(x,y,r1,a0,a1);g.arc(x,y,r0,a1,a0,true);g.fill()},
  fillRoundRect(x,y,w,h,r,c){g.fillStyle=c;g.beginPath();g.roundRect(x,y,w,h,r);g.fill()},
  text(s,x,y,px,c,align='left',bold=false){g.fillStyle=c;g.font=`${bold?700:500} ${px}px system-ui,sans-serif`;g.textAlign=align;g.textBaseline='middle';g.fillText(s,x,y)},
  mono(s,x,y,px,c,align='left'){g.fillStyle=c;g.font=`700 ${px}px ui-monospace,Consolas,monospace`;g.textAlign=align;g.textBaseline='middle';g.fillText(s,x,y)},
  fitText(s,x,y,px,maxW,c,bold=false){for(;px>9;px--){g.font=`${bold?700:500} ${px}px system-ui,sans-serif`;if(g.measureText(s).width<=maxW)break}this.text(s,x,y,px,c,'center',bold)},
}}

// ---------- tela redonda 466x466
const R={W:466,CX:233,CY:233,RMAX:188,minus:[32,233,24],plus:[434,233,24],blips:[],
  C:{bg:'#000',ring:'#0b3d1c',ringHi:'#16653a',text:'#c8d2dc',dim:'#5d6b78',sweep:'0,255,120',sel:'#ff3c3c',green:'#00ff78',home:'#3c8cff',accent:'#ffc400',card:'rgba(14,20,28,0.9)',btn:'#141c24'},
  d:gfx($('round'))};
function planeIcon(d,x,y,trk,size,c){const t=trk*Math.PI/180,s=Math.sin(t),co=Math.cos(t),P=(f,r)=>[x+s*f+co*r,y-co*f+s*r];
  const[nx,ny]=P(size,0),[lx,ly]=P(-size*.7,-size*.75),[rx,ry]=P(-size*.7,size*.75),[mx,my]=P(-size*.3,0);
  d.fillTriangle(nx,ny,lx,ly,mx,my,c);d.fillTriangle(nx,ny,mx,my,rx,ry,c)}
// ---------- mapa de fundo (tiles Esri Dark Gray, Web Mercator), centrado na casa e na escala do zoom
const tiles=new Map();
function tile(z,x,y){const k=z+'/'+x+'/'+y;let t=tiles.get(k);
  if(!t){t=new Image();t.src=`https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/${z}/${y}/${x}`;tiles.set(k,t)}
  return t.complete&&t.naturalWidth?t:null}
function drawMap(g,CX,CY,RMAX,rKm,shade=.4){
  if(!D.lat&&!D.lon)return;
  const lat=D.lat*Math.PI/180,screenMpp=rKm*1000/RMAX;
  const z=Math.max(3,Math.min(13,Math.round(Math.log2(156543.03*Math.cos(lat)/screenMpp))));
  const s=156543.03*Math.cos(lat)/2**z/screenMpp,n=2**z;   // tamanho do pixel do tile na tela
  const hx=(D.lon+180)/360*256*n,hy=(1-Math.log(Math.tan(lat)+1/Math.cos(lat))/Math.PI)/2*256*n;
  const span=RMAX/s,x0=Math.floor((hx-span)/256),x1=Math.floor((hx+span)/256),y0=Math.floor((hy-span)/256),y1=Math.floor((hy+span)/256);
  g.save();g.beginPath();g.arc(CX,CY,RMAX,0,Math.PI*2);g.clip();
  for(let ty=y0;ty<=y1;ty++)for(let tx=x0;tx<=x1;tx++){const im=tile(z,((tx%n)+n)%n,ty);if(im)g.drawImage(im,CX+(tx*256-hx)*s,CY+(ty*256-hy)*s,256*s+.5,256*s+.5)}
  g.fillStyle=`rgba(0,0,0,${shade})`;g.fillRect(CX-RMAX,CY-RMAX,2*RMAX,2*RMAX);   // escurece por cima (sem emendas claras entre tiles)
  g.restore();
}

// cartao do aviao selecionado (os dois estilos da tela redonda)
function drawCard(d,C,sel){
  const CX=R.CX,imp=units!=='metric',uD=imp?'mi':'km',inf=info[sel.hex]||{loading:true},wait=inf.loading?'…':'';
  // COMPANHIA AEREA / modelo / rota / altitude-velocidade-distancia (o indicativo ja esta na etiqueta do aviao)
  d.fillRoundRect(113,274,240,162,20,C.card);
  d.fitText(inf.airline||(inf.loading?'…':'compagnie inconnue'),CX,293,16,220,inf.airline?'#ffffff':C.dim,true);
  d.fitText(inf.model||sel.type||wait||'modèle inconnu',CX,314,12,220,C.text);
  const o=inf.orig,de=inf.dest;
  if(o||de){d.fitText(`${o?o.code:'?'}  →  ${de?de.code:'?'}`,CX,338,18,220,'#fff',true);d.fitText(`${o?o.city:''}  →  ${de?de.city:''}`,CX,357,11,220,C.dim)}
  else d.text(wait||'route inconnue',CX,346,13,C.dim,'center');
  const spd=imp?`${Math.round(sel.gs*1.15078)} mi/h`:`${Math.round(sel.gs*1.852)} km/h`;
  [['ALTITUDE',sel.ground?'au sol':`${fr(sel.alt)} pi`],['VITESSE',spd],['DISTANCE',`${(imp?sel.dist/1.60934:sel.dist).toFixed(1)} ${uD}`]]
    .forEach(([k,v],i)=>{const x=CX+(i-1)*76;d.text(k,x,383,9,C.dim,'center',true);d.fitText(v,x,401,15,72,i===1?C.green:'#fff',true)});
}
function zoomButtons(d,C){for(const[b,s]of[[R.minus,'−'],[R.plus,'+']]){d.fillCircle(b[0],b[1],b[2],C.btn);d.drawCircle(b[0],b[1],b[2],C.ringHi);d.text(s,b[0],b[1]+1,24,C.text,'center',true)}}
const avions=n=>`${n} avion${n>1?'s':''}`;

// ---------- estilo CLASSIQUE (varredura verde)
// etiqueta aparece quando a varredura passa pelo aviao e some em ~1,6 s (a volta inteira leva 4 s)
const SWEEP_MS=4000,LABEL_MS=1600;
function sweepFade(sw,a){const ms=((sw-a)%(2*Math.PI)+2*Math.PI)%(2*Math.PI)/(2*Math.PI)*SWEEP_MS;return ms<LABEL_MS?1-ms/LABEL_MS:0}
function drawRound(planes){
  const d=R.d,C=R.C,{CX,CY,RMAX}=R,now=performance.now(),imp=units!=='metric',uD=imp?'mi':'km';
  const rLbl=RANGES[units][rangeIdx],rKm=rLbl*KM_PER[units];
  d.fillRect(0,0,R.W,R.W,C.bg);
  if(mapOn)drawMap(d.g,CX,CY,RMAX,rKm);
  for(let i=1;i<=3;i++)d.drawCircle(CX,CY,RMAX*i/3,i===3?C.ringHi:C.ring,i===3?2:1);
  d.drawLine(CX,CY-RMAX,CX,CY+RMAX,C.ring);d.drawLine(CX-RMAX,CY,CX+RMAX,CY,C.ring);
  for(let g=0;g<360;g+=10){const a=g*Math.PI/180,r1=RMAX+(g%30?5:10);d.drawLine(CX+Math.sin(a)*RMAX,CY-Math.cos(a)*RMAX,CX+Math.sin(a)*r1,CY-Math.cos(a)*r1,g%30?C.ring:C.ringHi,g%30?1:2)}
  [['N',0],['E',90],['S',180],['O',270]].forEach(([l,g])=>{const a=g*Math.PI/180;d.text(l,CX+Math.sin(a)*(RMAX-16),CY-Math.cos(a)*(RMAX-16),15,l==='N'?C.accent:C.dim,'center',true)});
  [1,2].forEach(i=>d.text(''+ +(rLbl*i/3).toFixed(1),CX+4+RMAX*i/3*.707,CY-RMAX*i/3*.707-2,11,C.dim));
  const sw=(now/SWEEP_MS*Math.PI*2)%(Math.PI*2);
  for(let k=0;k<18;k++){const a1=sw-Math.PI/2-k*.035;d.fillArc(CX,CY,0,RMAX,a1-.035,a1,`rgba(${C.sweep},${(.28*(1-k/18)).toFixed(3)})`)}
  d.drawLine(CX,CY,CX+Math.sin(sw)*RMAX,CY-Math.cos(sw)*RMAX,`rgba(${C.sweep},.8)`,2);
  d.fillTriangle(CX,CY-8,CX-6,CY+6,CX+6,CY+6,C.home);
  const inRange=planes.filter(p=>p.dist<=rKm&&(!p.ground||groundOn)).sort((a,b)=>a.dist-b.dist);
  R.blips=[];let sel=null;
  inRange.forEach(p=>{const r=p.dist/rKm*RMAX,a=p.brg*Math.PI/180,x=CX+r*Math.sin(a),y=CY-r*Math.cos(a),isSel=p.hex===selHex;if(isSel)sel=p;
    planeIcon(d,x,y,p.trk,isSel?11:8,isSel?'#fff':p.ground?'#8a939c':altColor(p.alt));
    if(isSel){d.drawCircle(x,y,16,C.accent,2);d.text(p.flight||p.hex,x+11,y-10,12,C.accent,'left',true)}
    else{const k=sweepFade(sw,a);if(k>0)d.text(p.flight||p.hex,x+11,y-10,12,`rgba(200,210,220,${k.toFixed(2)})`)}
    R.blips.push({x,y,hex:p.hex})});
  // topo: hora + contagem e alcance (embaixo fica a engrenagem)
  const t=new Date();d.text(`${String(t.getHours()).padStart(2,'0')} h ${String(t.getMinutes()).padStart(2,'0')}  ·  ${avions(inRange.length)} · ${rLbl} ${uD}`,CX,18,12,C.dim,'center',true);
  zoomButtons(d,C);
  if(sel)drawCard(d,C,sel);
  if(D.alerting){const on=Math.floor(now/350)%2;d.drawCircle(CX,CY,229,on?C.sel:'rgba(255,60,60,.35)',8);
    if(on){d.fillRoundRect(CX-48,30,96,24,12,C.sel);d.text('ALERTE',CX,43,13,'#fff','center',true)}}
}

// ---------- estilo CONTROLEUR (como o scope de um controlador de trafego aereo)
// alvo quadrado, rastro de posicoes anteriores, vetor de 1 min e etiqueta de dados:
//   linha 1: indicativo   linha 2: nivel (centenas de pes) + seta de subida/descida + velocidade em dezenas de nos
//   linha 3: tipo + destino (quando conhecido)
const A={bg:'#05090e',ring:'#1b3140',ringTxt:'#41687d',rose:'#2a4a5e',roseTxt:'#5b8296',rwy:'#9fb4c3',apt:'#57788b',
  sweep:'190,200,210',gnd:'#8a939c',tgt:'#e6f7ff',hist:'59,140,200',lead:'#7fb7d6',tag:'#62f59a',sel:'#ffe94a',alert:'#ff4d4d'};
const RA=198;   // raio util do scope
function kmXY(lat,lon){return[(lon-D.lon)*111.32*Math.cos(D.lat*Math.PI/180),(lat-D.lat)*110.57]}
function drawATC(planes){
  const d=R.d,C=R.C,{CX,CY}=R,now=performance.now(),rNm=ATC_NM[rangeIdx],rKm=rNm*1.852,k=RA/rKm;
  const P=(x,y)=>[CX+x*k,CY-y*k];
  d.fillRect(0,0,R.W,R.W,A.bg);
  if(mapOn){drawMap(d.g,CX,CY,RA,rKm,.72)}
  d.g.save();d.g.beginPath();d.g.arc(CX,CY,RA,0,Math.PI*2);d.g.clip();
  // pistas e aeroportos (mapa video)
  if(D.lat||D.lon)for(const[icao,la,lo,rws]of APTS){
    const[ax,ay]=P(...kmXY(la,lo));if(Math.hypot(ax-CX,ay-CY)>RA+20)continue;
    for(const r of rws){const[x1,y1]=P(...kmXY(r[0],r[1])),[x2,y2]=P(...kmXY(r[2],r[3]));d.drawLine(x1,y1,x2,y2,A.rwy,rNm<=25?3:2)}
    d.mono(icao,ax+8,ay+10,10,A.apt);
  }
  // aneis a cada 1/5 do alcance, com distancia em NM
  for(let i=1;i<=5;i++){d.drawCircle(CX,CY,RA*i/5,A.ring);if(i<5)d.mono(String(+(rNm*i/5).toFixed(1)),CX+RA*i/5*.707+3,CY+RA*i/5*.707+8,9,A.ringTxt)}
  d.g.restore();
  // rosa dos ventos em graus
  for(let g=0;g<360;g+=5){const a=g*Math.PI/180,r1=RA+(g%10?3:g%30?6:9);d.drawLine(CX+Math.sin(a)*RA,CY-Math.cos(a)*RA,CX+Math.sin(a)*r1,CY-Math.cos(a)*r1,A.rose,g%30?1:2)}
  // 000/090/180/270 ficam sem numero: dariam em cima da hora e dos botoes de zoom
  for(let g=0;g<360;g+=30){if(g%90===0)continue;const a=g*Math.PI/180;d.mono(String(g||360).padStart(3,'0'),CX+Math.sin(a)*(RA+19),CY-Math.cos(a)*(RA+19),10,A.roseTxt,'center')}
  // linha de varredura cinza (discreta, para nao competir com as etiquetas)
  const sw=(now/SWEEP_MS*Math.PI*2)%(Math.PI*2);
  for(let q=0;q<18;q++){const a1=sw-Math.PI/2-q*.035;d.fillArc(CX,CY,0,RA,a1-.035,a1,`rgba(${A.sweep},${(.16*(1-q/18)).toFixed(3)})`)}
  d.drawLine(CX,CY,CX+Math.sin(sw)*RA,CY-Math.cos(sw)*RA,`rgba(${A.sweep},.6)`,2);
  // casa: cruz pequena
  d.drawLine(CX-5,CY,CX+5,CY,'#3c8cff',2);d.drawLine(CX,CY-5,CX,CY+5,'#3c8cff',2);
  // rastro: guarda a posicao a cada 3 s
  if(now-histAt>3000){histAt=now;for(const p of planes){const a=p.brg*Math.PI/180,h=hist.get(p.hex)||[];h.push([p.dist*Math.sin(a),p.dist*Math.cos(a)]);if(h.length>6)h.shift();hist.set(p.hex,h)}}
  const inRange=planes.filter(p=>p.dist<=rKm&&(!p.ground||groundOn)).sort((a,b)=>a.dist-b.dist);
  R.blips=[];let sel=null;const aKm=D.aKm||10,aFt=D.aFt||10000,blink=Math.floor(now/400)%2,tags=[];
  for(const p of inRange){
    const a=p.brg*Math.PI/180,[x,y]=P(p.dist*Math.sin(a),p.dist*Math.cos(a)),isSel=p.hex===selHex;if(isSel)sel=p;
    const alert=D.alertOn&&p.al;   // decidido pelo ESP (posicao estimada a cada 1 s, com memoria)
    (hist.get(p.hex)||[]).slice(0,-1).forEach(([hx,hy],i,arr)=>{const[qx,qy]=P(hx,hy);d.fillRect(qx-1.5,qy-1.5,3,3,`rgba(${A.hist},${(.25+.6*i/arr.length).toFixed(2)})`)});
    const t=p.trk*Math.PI/180,L=p.gs*1.852/60*k;   // vetor: posicao daqui a 1 minuto
    if(!p.ground)d.drawLine(x,y,x+Math.sin(t)*L,y-Math.cos(t)*L,A.lead);
    d.fillRect(x-3,y-3,6,6,isSel?A.sel:alert?A.alert:p.ground?A.gnd:A.tgt);
    const fl=String(Math.round(p.alt/100)).padStart(3,'0'),arw=p.vr>300?'↑':p.vr<-300?'↓':' ',inf=info[p.hex];
    const lines=p.ground?[p.flight||p.hex,'SOL']   // no solo: so indicativo
      :[p.flight||p.hex,`${fl}${arw}${String(Math.round(p.gs/10)).padStart(2,'0')}`,(p.type||'')+(inf&&inf.dest?' '+inf.dest.code:'')].filter(Boolean);
    tags.push({x,y,lines,isSel,col:alert?(blink?A.alert:'#ff9a9a'):isSel?A.sel:p.ground?A.gnd:A.tag});
    R.blips.push({x,y,hex:p.hex});
  }
  // etiquetas por ultimo: escolhe entre 4 posicoes (NE, NO, SE, SO) a que menos cobre as ja colocadas
  d.g.font='700 11px ui-monospace,Consolas,monospace';
  const placed=[],ov=(a,b)=>Math.max(0,Math.min(a[2],b[2])-Math.max(a[0],b[0]))*Math.max(0,Math.min(a[3],b[3])-Math.max(a[1],b[1]));
  for(const tg of tags.sort((a,b)=>b.isSel-a.isSel)){
    const w=Math.max(...tg.lines.map(s=>d.g.measureText(s).width)),h=tg.lines.length*12;
    let best=null;
    for(const[sx,sy]of[[1,-1],[-1,-1],[1,1],[-1,1]]){
      const bx=sx>0?tg.x+14:tg.x-14-w,by=sy<0?tg.y-14-h:tg.y+14,box=[bx,by,bx+w,by+h];
      const out=Math.hypot((box[0]+box[2])/2-CX,(box[1]+box[3])/2-CY)>RA?5000:0;   // evita sair do scope
      const cost=placed.reduce((s,b)=>s+ov(box,b),0)+out;
      if(!best||cost<best.cost)best={cost,box,sx,sy};
      if(cost===0)break;
    }
    placed.push(best.box);
    const[bx,by,bx2,by2]=best.box;
    d.drawLine(tg.x+best.sx*4,tg.y+best.sy*4,best.sx>0?bx-2:bx2+2,best.sy<0?by2:by,tg.col);
    tg.lines.forEach((s,i)=>d.mono(s,bx,by+6+i*12,11,tg.col));
  }
  const t=new Date();d.mono(`${String(t.getUTCHours()).padStart(2,'0')}${String(t.getUTCMinutes()).padStart(2,'0')}Z`,CX,13,12,A.roseTxt,'center');
  zoomButtons(d,C);
  if(sel)drawCard(d,C,sel);
  else if(!talking())d.mono(`${inRange.length} TRAFIC · ${rNm} NM`,CX,CY+RA-14,12,A.roseTxt,'center');   // logo acima do anel externo
  if(D.alerting){const on=Math.floor(now/350)%2;d.drawCircle(CX,CY,229,on?A.alert:'rgba(255,77,77,.3)',6);
    if(on){d.fillRoundRect(CX-48,26,96,22,11,A.alert);d.text('ALERTE',CX,37,12,'#fff','center',true)}}
}

// ---------- tela retangular 320x240 (CYD)
const Q={CX:120,CY:120,RMAX:110,minus:[244,206,34,28],plus:[282,206,34,28],blips:[],d:gfx($('rect'))};
function drawRect(planes){
  const d=Q.d,{CX,CY,RMAX}=Q,uD=units==='metric'?'km':'mi',rKm=RANGES[units][rangeIdx]*KM_PER[units],str=(s,x,y,sz,c)=>{d.g.fillStyle=c;d.g.font=(sz===2?16:9)+'px monospace';d.g.textAlign='left';d.g.textBaseline='top';d.g.fillText(s,x,y)};
  d.fillRect(0,0,320,240,'#000');
  if(mapOn)drawMap(d.g,CX,CY,RMAX,rKm);
  for(let i=1;i<=3;i++)d.drawCircle(CX+.5,CY+.5,Math.round(RMAX*i/3),'#005a00');
  d.drawLine(CX,CY-RMAX,CX,CY+RMAX,'#005a00');d.drawLine(CX-RMAX,CY,CX+RMAX,CY,'#005a00');
  const sw=(performance.now()/4000*Math.PI*2)%(Math.PI*2);
  for(let k=0;k<10;k++){const a=sw-k*.03;d.drawLine(CX,CY,CX+Math.sin(a)*RMAX,CY-Math.cos(a)*RMAX,`rgba(0,255,100,${(.35-k*.033).toFixed(3)})`)}
  str('N',CX-3,CY-RMAX-10,1,'#c8c8c8');str(RANGES[units][rangeIdx]+uD,CX+4,CY-RMAX+4,1,'#c8c8c8');
  d.fillTriangle(CX,CY-6,CX-5,CY+5,CX+5,CY+5,'#3c8cff');
  const inRange=planes.filter(p=>p.dist<=rKm&&(!p.ground||groundOn)).sort((a,b)=>a.dist-b.dist);Q.blips=[];let sel=null;
  for(const p of inRange){const r=p.dist/rKm*RMAX,a=p.brg*Math.PI/180,x=CX+Math.round(r*Math.sin(a)),y=CY-Math.round(r*Math.cos(a)),isSel=p.hex===selHex;if(isSel)sel=p;
    const col=isSel?'#ff3c3c':p.ground?'#8a939c':'#00ff64',t=p.trk*Math.PI/180;d.drawLine(x,y,x+Math.sin(t)*7,y-Math.cos(t)*7,col);d.fillCircle(x,y,isSel?3:2,col);if(isSel)d.drawCircle(x,y,7,col);Q.blips.push({x,y,hex:p.hex})}
  d.fillRect(236,0,84,240,'#101418');str(avions(inRange.length),242,6,1,'#c8c8c8');
  if(!sel){if(!inRange.length){str('Aucun trafic',242,30,1,'#c8c8c8');str('a portee',242,42,1,'#c8c8c8')}}
  else{const inf=info[sel.hex]||{},rt=inf.orig||inf.dest?`${inf.orig?inf.orig.code:'?'}>${inf.dest?inf.dest.code:'?'}`:'?';
    str((sel.flight||sel.hex).slice(0,8),242,24,2,'#ffc400');
    [['CIE',inf.airline||'?'],['ALT',Math.round(sel.alt)+'pi'],['VIT',Math.round(sel.gs)+'kt'],['ROUTE',rt],['DIST',(units==='metric'?sel.dist:sel.dist/1.60934).toFixed(1)+uD],['TYPE',sel.type||'?'],['IMMAT',sel.reg||'?']]
      .forEach(([k,v],i)=>{str(k,242,44+i*22,1,'#7a8794');str(String(v).slice(0,13),242,53+i*22,1,'#c8c8c8')})}
  d.fillRect(...Q.minus,'#1e2830');d.fillRect(...Q.plus,'#1e2830');str('-',Q.minus[0]+13,Q.minus[1]+6,2,'#c8c8c8');str('+',Q.plus[0]+12,Q.plus[1]+6,2,'#c8c8c8');
  if(D.alerting&&Math.floor(performance.now()/300)%2){d.fillRect(4,222,54,14,'#ff3c3c');str('ALERTE',8,225,1,'#fff')}
}

// posicao estimada entre leituras (o ESP atualiza a cada ~15 s)
function live(p,dt){let x=p.dist*Math.sin(p.brg*Math.PI/180),y=p.dist*Math.cos(p.brg*Math.PI/180);
  if(!p.ground&&dt<60){const k=p.gs*1.852/3600*dt,t=p.trk*Math.PI/180;x+=k*Math.sin(t);y+=k*Math.cos(t)}
  return{...p,dist:Math.hypot(x,y),brg:(Math.atan2(x,y)*180/Math.PI+360)%360}}
// ---------- engrenagem + menu de reglages (desenhados na propria tela, como no aparelho)
const GEAR={round:[233,453,10],rect:[309,12,6]};   // redonda: embaixo, as 6 horas, entre a rosa dos ventos e a borda
const MENU={round:{x:98,y:58,w:270,h:352,top:76,tabs:100,sub:121,row0:144,step:24.6,fs:13},
  rect:{x:28,y:2,w:264,h:236,top:11,tabs:25,sub:38,row0:52,step:16.8,fs:10}};
let menuTab='radar';
function drawGear(d,[x,y,r]){
  for(let k=0;k<8;k++){const a=k*Math.PI/4;d.drawLine(x+Math.cos(a)*r*.55,y+Math.sin(a)*r*.55,x+Math.cos(a)*r,y+Math.sin(a)*r,'#7a8794',r*.32)}
  d.fillCircle(x,y,r*.68,'#7a8794');d.fillCircle(x,y,r*.3,'#000');
}
const SRC={wifi:'Wi-Fi',ip:'IP',manuel:'manuel',secrets:'config'};
const menuRows=()=>menuTab==='radar'?[
  ['Son',D.soundOn,'sw'],['Alerte de proximité',D.alertOn,'sw'],['Zone d’alerte',`${D.aKm||5} km · ${fr(D.aFt||5000)} pi`,'val'],
  ['Carte de fond',mapOn,'sw'],['Avions au sol',groundOn,'sw'],
  ['Tour de contrôle',atc.on?feedName(atc.feed):atcFeeds.length?'Arrêt':feedsPending?'recherche…':'aucune',atc.reach?'val':'off'],
  ['Style',style==='atc'?'Contrôleur':'Classique','val'],['Unités',units==='metric'?'km':'mi','val'],
  ['Écran',mode==='round'?'Rond':'Rectangulaire','val'],['Tester le bip','▶','val'],['Fermer','','close']]:[
  ['Réseau Wi-Fi',D.ap?'à configurer':(D.ssid||'—'),'val'],
  ['Position',`${D.city||'?'} (${SRC[D.src]||D.src||'?'})`,'val'],
  ['Précision',D.acc?(D.acc>=1000?`± ${Math.round(D.acc/1000)} km`:`± ${D.acc} m`):'—','info'],
  ['Aéroport (radio)',atcAirport?`${atcAirport.icao}${atcAirport.iata?' · '+atcAirport.iata:''}`:'—','info'],
  ['Canaux de la tour',atcFeeds.length?String(atcFeeds.length):feedsPending?'recherche…':'0','info'],
  ['Fermer','','close']];
function drawMenu(d){
  const m=MENU[mode],C={card:'rgba(10,15,21,0.96)',dim:'#7a8794',text:'#e6edf3',on:'#00c853',off:'#3a4550',accent:'#ffc400'};
  d.fillRoundRect(m.x,m.y,m.w,m.h,18,C.card);
  d.text('Réglages',m.x+m.w/2,m.top,m.fs+3,C.accent,'center',true);
  // abas: Radar | Appareil (Wi-Fi e posicao)
  const tw=(m.w-36)/2,th=m.fs+8;
  [['radar','Radar'],['app','Appareil']].forEach(([k,l],i)=>{const x=m.x+18+i*tw;
    d.fillRoundRect(x,m.tabs-th/2,tw-4,th,th/2,menuTab===k?'#ffc400':'#18212c');d.text(l,x+(tw-4)/2,m.tabs,m.fs-2,menuTab===k?'#111':C.text,'center',true)});
  d.fitText(conn?(D.ok?`${D.city} · ${avions(D.planes.length)} · données il y a ${Math.round((performance.now()-recvAt)/1000)} s`:D.ap?`mode configuration · réseau ${D.apName}`:'ESP sans données adsb.lol'):'pas de connexion avec l’ESP32',m.x+m.w/2,m.sub,m.fs-4,m.w-30,conn&&D.ok?C.dim:'#ff6b6b');
  const rows=menuRows();
  rows.forEach(([label,v,kind],i)=>{
    const y=m.row0+i*m.step,xl=m.x+18,xr=m.x+m.w-18;
    if(kind==='close'){d.fillRoundRect(m.x+m.w/2-50,y-m.step*.38,100,m.step*.76,m.step*.38,'#18212c');d.text(label,m.x+m.w/2,y,m.fs-1,C.text,'center',true);return}
    d.text(label,xl,y,m.fs-1,C.text);
    if(kind==='off')d.text('PC hors ligne',xr,y,m.fs-2,'#ff6b6b','right');   // server.py nao respondeu
    else if(kind==='sw'){const h=m.fs+6,w=h*1.8;d.fillRoundRect(xr-w,y-h/2,w,h,h/2,v?C.on:C.off);d.fillCircle(v?xr-h/2:xr-w+h/2,y,h/2-3,'#fff')}
    else{d.g.font=`700 ${m.fs-1}px system-ui,sans-serif`;let t=String(v);while(t.length>3&&d.g.measureText(t).width>m.w*.5)t=t.slice(0,-2)+'…';
      d.text(t,xr,y,m.fs-1,kind==='info'?C.dim:C.accent,'right',true)}
    if(i<rows.length-2)d.drawLine(xl,y+m.step/2,xr,y+m.step/2,'rgba(255,255,255,.06)');
  });
}
async function menuTap(x,y){
  const m=MENU[mode];
  if(x<m.x||x>m.x+m.w||y<m.y||y>m.y+m.h){menuOpen=false;return}   // toque fora do menu fecha
  if(Math.abs(y-m.tabs)<(m.fs+8)/2+3){menuTab=x<m.x+m.w/2?'radar':'app';return}
  const i=Math.round((y-m.row0)/m.step);
  if(menuTab==='app'){
    if(i===0)openWifi();else if(i===1)openPosition();else if(i===5)menuOpen=false;
    return;
  }
  if(i===0){await fetch('/sound',{method:'POST'});poll()}
  else if(i===1){await fetch('/alert',{method:'POST'});poll()}
  else if(i===2){await fetch('/zone',{method:'POST'});poll()}
  else if(i===3){mapOn=!mapOn;savePref()}
  else if(i===4){groundOn=!groundOn;savePref()}
  else if(i===5){   // torre: Arret -> canal 1 -> canal 2 -> ... -> Arret (canais do aeroporto mais proximo)
    const order=atcFeeds.slice(0,6).map(f=>f.mount),k=atc.on?order.indexOf(atc.feed)+1:0;
    k<order.length?atcCall('start',order[k]):atcCall('stop');
  }
  else if(i===6){style=style==='atc'?'classic':'atc';savePref()}
  else if(i===7){units=units==='metric'?'imperial':'metric';savePref()}
  else if(i===8)setMode(mode==='round'?'rect':'round');
  else if(i===9){fetch('/beep',{method:'POST'});beep(true)}
  else if(i===10)menuOpen=false;
}
// ---------- torre de controle: o server.py do PC descobre os canais da LiveATC dos aeroportos perto da casa
// e toca nas caixas do PC (a frequencia de radio so tem som quando alguem fala: silencio e normal)
let atcFeeds=[],atcAirport=null,feedsPending=false,feedsKey='';
const feedName=m=>{const f=atcFeeds.find(f=>f.mount===m);return f?f.title:(m||'—')};
async function loadFeeds(){
  if(!PC||(!D.lat&&!D.lon))return;
  const key=D.lat.toFixed(2)+','+D.lon.toFixed(2);
  if(key===feedsKey&&!feedsPending)return;
  try{
    const j=await(await fetch(`${PC}/api/atc/feeds?lat=${D.lat}&lon=${D.lon}`,{cache:'no-store'})).json();
    atcFeeds=j.feeds||[];atcAirport=(j.airports||[]).find(a=>atcFeeds.some(f=>f.icao===a.icao))||(j.airports||[])[0]||null;
    feedsPending=!!j.pending;feedsKey=key;
    if(feedsPending)setTimeout(loadFeeds,5000);   // a descoberta roda no PC; pergunta de novo
  }catch(e){}
}
async function atcCall(action,feed=''){
  if(!PC){atc={on:false,available:false,reach:false};return}
  try{const r=await fetch(`${PC}/api/atc/${action}?feed=${feed}`,{cache:'no-store'});atc={...await r.json(),reach:true}}
  catch(e){atc={on:false,available:false,reach:false}}
}

// ---------- janelas de texto (Wi-Fi e posicao): precisam de teclado, entao sao HTML por cima da tela
function dlg(html){const el=$('dlg');el.innerHTML=`<div class="box">${html}</div>`;el.hidden=false;el.onclick=e=>{if(e.target===el||e.target.dataset.close!==undefined)el.hidden=true};return el}
const escH=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function openWifi(){
  menuOpen=false;
  const el=dlg(`<h3>Réseau Wi-Fi</h3><p class="dim">${D.ap?`Le radar est en mode configuration (réseau <b>${escH(D.apName)}</b>). Choisissez le Wi-Fi de la maison :`:`Connecté à <b>${escH(D.ssid)}</b>. Choisissez un autre réseau :`}</p><ul id="wl"><li class="dim">Recherche des réseaux…</li></ul><div id="wf"></div><button data-close>Fermer</button>`);
  try{
    const list=await(await fetch('/wifi/scan',{cache:'no-store'})).json();
    const bars=r=>r>-55?'▂▄▆█':r>-67?'▂▄▆':r>-78?'▂▄':'▂';
    el.querySelector('#wl').innerHTML=list.length?list.map((n,i)=>`<li data-i="${i}"><span>${escH(n.ssid)}${n.open?'':' 🔒'}</span><span class="dim">${bars(n.rssi)}</span></li>`).join(''):'<li class="dim">Aucun réseau trouvé.</li>';
    el.querySelector('#wl').onclick=e=>{const li=e.target.closest('li[data-i]');if(!li)return;const n=list[+li.dataset.i];
      el.querySelector('#wf').innerHTML=`<p>Mot de passe de <b>${escH(n.ssid)}</b></p>${n.open?'':'<input id="wp" type="password" autocomplete="off" placeholder="mot de passe">'}<button id="wc" class="go">Connecter</button>`;
      const wp=el.querySelector('#wp');if(wp)wp.focus();
      el.querySelector('#wc').onclick=async()=>{
        const b=new URLSearchParams({ssid:n.ssid,pass:wp?wp.value:''});
        try{await fetch('/wifi/save',{method:'POST',body:b})}catch(e){}
        el.querySelector('.box').innerHTML=`<h3>Connexion…</h3><p>Le radar redémarre et se connecte à <b>${escH(n.ssid)}</b>.</p><p class="dim">Remettez votre téléphone sur ce réseau et ouvrez <b>http://radar.local</b>. Si le mot de passe est faux, le radar revient au mode configuration (réseau ${escH(D.apName||'Radar-XXXX')}).</p>`;
      }};
  }catch(e){el.querySelector('#wl').innerHTML='<li class="dim">Le radar ne répond pas.</li>'}
}
function openPosition(){
  menuOpen=false;
  const el=dlg(`<h3>Position du radar</h3><p class="dim">Actuelle : <b>${escH(D.city||'?')}</b> (${SRC[D.src]||escH(D.src)||'?'}). Sans GPS, le radar se situe par les réseaux Wi-Fi voisins, par l’adresse IP ou par la ville choisie ici.</p>
    <div class="row"><button id="lw" class="go">Détecter par Wi-Fi</button><button id="li">Par l’IP</button></div>
    <p>Ou choisissez la ville :</p><div class="row"><input id="lq" placeholder="ville ou code postal"><button id="ls">Chercher</button></div><ul id="lr"></ul><p id="lm" class="dim"></p><button data-close>Fermer</button>`);
  const msg=t=>el.querySelector('#lm').textContent=t;
  const locate=async m=>{msg('Recherche…');try{const j=await(await fetch('/locate',{method:'POST',body:new URLSearchParams({method:m})})).json();
    if(j.ok===false){msg(m==='wifi'?'Les réseaux d’ici ne sont pas encore cartographiés. Essayez par l’IP ou la ville.':'Échec par l’IP.');return}
    D={...D,...j};msg(`Position : ${j.city} (${SRC[j.src]})`);aptsKey='';feedsKey='';loadAirports(j.lat,j.lon);loadFeeds()}catch(e){msg('Le radar ne répond pas.')}};
  el.querySelector('#lw').onclick=()=>locate('wifi');el.querySelector('#li').onclick=()=>locate('ip');
  const search=async()=>{const q=el.querySelector('#lq').value.trim();if(!q)return;msg('Recherche…');
    try{const j=await(await fetch(`https://geocoding-api.open-meteo.com/v1/search?count=6&language=fr&name=${encodeURIComponent(q)}`)).json();
      const r=j.results||[];msg(r.length?'':'Aucune ville trouvée.');
      el.querySelector('#lr').innerHTML=r.map((c,i)=>`<li data-i="${i}"><span>${escH(c.name)}</span><span class="dim">${escH([c.admin1,c.country].filter(Boolean).join(', '))}</span></li>`).join('');
      el.querySelector('#lr').onclick=async e=>{const li=e.target.closest('li[data-i]');if(!li)return;const c=r[+li.dataset.i];
        const j2=await(await fetch('/location',{method:'POST',body:new URLSearchParams({lat:c.latitude,lon:c.longitude,city:c.name})})).json();
        D={...D,...j2};msg(`Position : ${c.name}`);el.querySelector('#lr').innerHTML='';aptsKey='';feedsKey='';loadAirports(c.latitude,c.longitude);loadFeeds()}}
    catch(e){msg('Recherche impossible (pas d’internet ?).')}};
  el.querySelector('#ls').onclick=search;el.querySelector('#lq').onkeydown=e=>{if(e.key==='Enter')search()};
}

// ---------- onda de fala: quando a torre estiver tocando no PC, mostra as barras de volume
// e "TRANSMISSION" enquanto alguem fala (o server.py detecta a voz acima do chiado do radio)
let voice={talk:false,lv:[]};
async function pollVoice(){
  if(atc.on){try{voice=await(await fetch(`${PC}/api/atc/level`,{cache:'no-store'})).json()}catch(e){voice={talk:false,lv:[]}}}
  setTimeout(pollVoice,atc.on?250:2000);
}
const talking=()=>atc.on&&voice.talk;
function drawVoice(d,hasSel){
  if(!talking())return;   // so aparece quando alguem esta falando; no resto do tempo fica a contagem de avioes
  const r=mode==='round',H=r?36:26,n=r?14:10,bw=r?3:2.5,gap=r?2:1.5,pad=H/2,fs1=r?11:8,fs2=r?9:7;
  const t1='TRANSMISSION',t2=feedName(atc.feed);
  // largura da capsula calculada pelo texto: nada fica para fora
  d.g.font=`700 ${fs1}px system-ui,sans-serif`;const w1=d.g.measureText(t1).width;
  d.g.font=`500 ${fs2}px system-ui,sans-serif`;const w2=d.g.measureText(t2).width;
  const barsW=n*(bw+gap)-gap,W=pad+barsW+(r?10:6)+Math.max(w1,w2)+pad,x0=r?R.CX-W/2:4;
  const y0=r?(hasSel?226:style==='atc'?R.CY+RA-34:390):206;   // embaixo do radar (acima do cartao, se houver aviao tocado)
  d.fillRoundRect(x0,y0,W,H,H/2,'rgba(10,40,24,.92)');
  const bx=x0+pad,mid=y0+H/2,lv=voice.lv||[];
  for(let i=0;i<n;i++){const v=lv[lv.length-n+i]??0,h=Math.max(2,v*(H-10));d.fillRoundRect(bx+i*(bw+gap),mid-h/2,bw,h,1,'#62f59a')}
  const tx=bx+barsW+(r?10:6);
  d.text(t1,tx,mid-(r?6:4),fs1,'#62f59a','left',true);
  d.text(t2,tx,mid+(r?8:6),fs2,'#cfeedd');
}

function draw(){
  const dt=(performance.now()-recvAt)/1000,planes=D.planes.map(p=>live(p,dt)),d=mode==='round'?R.d:Q.d;
  if(mode!=='round')drawRect(planes);else if(style==='atc')drawATC(planes);else drawRound(planes);
  drawVoice(d,!!selHex&&D.planes.some(p=>p.hex===selHex));
  drawGear(d,GEAR[mode]);
  if(menuOpen)drawMenu(d);
}

async function loadInfo(p){
  if(info[p.hex])return;info[p.hex]={loading:true};
  try{info[p.hex]=await(await fetch(`/info?hex=${p.hex}&cs=${encodeURIComponent(p.flight||'')}`)).json()}catch(e){info[p.hex]={}}
}
function touch(cv,e,S,isRound){
  const b=cv.getBoundingClientRect(),W=cv.width,H=cv.height,x=(e.clientX-b.left)*W/b.width,y=(e.clientY-b.top)*H/b.height;
  if(menuOpen){menuTap(x,y);return}
  const g=GEAR[mode];if(Math.hypot(x-g[0],y-g[1])<=g[2]+10){menuOpen=true;atcCall('status');return}
  const hit=isRound?(([bx,by,r])=>Math.hypot(x-bx,y-by)<=r+8):(r=>x>=r[0]&&x<=r[0]+r[2]&&y>=r[1]&&y<=r[1]+r[3]);
  if(hit(S.minus)){rangeIdx=Math.max(0,rangeIdx-1);savePref();return}
  if(hit(S.plus)){rangeIdx=Math.min(4,rangeIdx+1);savePref();return}
  let best=null,bd=isRound?22:14;for(const q of S.blips){const d=Math.hypot(q.x-x,q.y-y);if(d<bd){bd=d;best=q}}
  selHex=best?best.hex:null;   // toque fora de um aviao = desmarcar
  const p=best&&D.planes.find(p=>p.hex===best.hex);if(p)loadInfo(p);
}
$('round').addEventListener('click',e=>touch($('round'),e,R,true));
$('rect').addEventListener('click',e=>touch($('rect'),e,Q,false));
function setMode(m){mode=m;savePref();$('round').hidden=m!=='round';$('rect').hidden=m==='round'}

// Som ligado por padrao. O navegador so libera audio depois do primeiro toque na pagina:
// esse primeiro toque (em qualquer lugar) ja destrava o bipe.
addEventListener('pointerdown',()=>{try{actx=actx||new AudioContext();actx.resume()}catch(e){}},{capture:true});
function beep(force){if(!actx||!(force||D.soundOn))return;const o=actx.createOscillator(),v=actx.createGain();o.frequency.value=880;v.gain.setValueAtTime(.15,actx.currentTime);v.gain.exponentialRampToValueAtTime(.001,actx.currentTime+.4);o.connect(v).connect(actx.destination);o.start();o.stop(actx.currentTime+.4)}
async function poll(){
  try{
    D=await(await fetch('/planes',{cache:'no-store'})).json();recvAt=performance.now()-D.age*1000;conn=true;
    if(D.pc&&!PC){PC=D.pc;atcCall('status')}
    if(D.lat||D.lon){loadAirports(D.lat,D.lon);loadFeeds()}
    if(D.ap&&!apShown){apShown=true;openWifi()}   // sem Wi-Fi configurado: ja abre a lista de redes
    if(D.alerting>lastAlert&&D.alertOn)beep();lastAlert=D.alerting;
    for(const h of hist.keys())if(!D.planes.some(p=>p.hex===h))hist.delete(h);
  }catch(e){conn=false}
}
setMode(mode);poll();setInterval(poll,2000);atcCall('status');setInterval(()=>atcCall('status'),15000);pollVoice();
(function loop(){draw();setTimeout(()=>requestAnimationFrame(loop),50)})();
</script></body></html>)HTML";
