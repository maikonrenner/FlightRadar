"""Radar de voo local: serve a pagina e faz proxy (com cache) das APIs publicas.
Rodar:  python server.py   ->  http://localhost:8080
"""
import array, json, math, re, threading, time, urllib.parse, urllib.request
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

PORT = 8080
ROOT = Path(__file__).parent
UA = {"User-Agent": "esp32-desk-radar/1.0 (hobby)"}
_cache, _lock = {}, threading.Lock()


# Conta OpenSky do usuario (API client OAuth2): 4.000 creditos/dia em vez de 400 anonimo.
# O arquivo fica so no PC; a pagina e o ESP32 nunca recebem o segredo.
CRED_FILE = ROOT / "opensky_credentials.json"
TOKEN_URL = "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token"
_token = {"value": None, "exp": 0}


def opensky_token():
    if not CRED_FILE.exists():
        return None
    if _token["value"] and time.time() < _token["exp"] - 60:
        return _token["value"]
    try:
        c = json.loads(CRED_FILE.read_text())
        data = urllib.parse.urlencode({"grant_type": "client_credentials", "client_id": c["clientId"],
                                       "client_secret": c["clientSecret"]}).encode()
        with urllib.request.urlopen(urllib.request.Request(TOKEN_URL, data=data, headers=UA), timeout=20) as r:
            j = json.load(r)
        _token.update(value=j["access_token"], exp=time.time() + j.get("expires_in", 1800))
        return _token["value"]
    except Exception as e:
        print("OpenSky: falha no token, usando acesso anonimo:", e)
        return None


def opensky_global(q, m):
    tok = opensky_token()
    # global custa 4 creditos: 120 s = ~2.900/dia (sobra margem dos 4.000); anonimo: 15 min
    return fetch("https://opensky-network.org/api/states/all", 120 if tok else 900, timeout=40,
                 headers={"Authorization": "Bearer " + tok} if tok else None)


def fetch(url, ttl, binary=False, timeout=20, headers=None):
    now = time.time()
    with _lock:
        hit = _cache.get(url)
        if hit and now - hit[0] < ttl:
            return hit[1], hit[2], hit[3]
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers={**UA, **(headers or {})}), timeout=timeout) as r:
            body, status, ctype = r.read(), r.status, r.headers.get("Content-Type", "application/json")
    except urllib.error.HTTPError as e:
        body, status, ctype = e.read() or b"{}", e.code, "application/json"
    except Exception as e:
        if hit:
            return hit[1], hit[2], hit[3]
        return json.dumps({"error": str(e)}).encode(), 502, "application/json"
    if status == 200:
        with _lock:
            _cache[url] = (now, body, status, ctype)
    elif hit:
        return hit[1], hit[2], hit[3]
    return body, status, ctype


def num(q, k, d):
    try:
        return float(q.get(k, [d])[0])
    except ValueError:
        return d


ROUTES = [
    (r"/api/local", lambda q, m: fetch(
        "https://api.adsb.lol/v2/point/%.4f/%.4f/%d" % (num(q, "lat", 0), num(q, "lon", 0), min(250, int(num(q, "r", 150)))), 4)),
    (r"/api/global", opensky_global),
    (r"/api/type/([A-Za-z0-9]{2,4})", lambda q, m: fetch("https://api.adsb.lol/v2/type/" + m.group(1).upper(), 20)),
    (r"/api/route/([A-Za-z0-9]{3,8})", lambda q, m: fetch("https://api.adsbdb.com/v0/callsign/" + m.group(1).upper(), 3600)),
    (r"/api/aircraft/([0-9a-fA-F]{6})", lambda q, m: fetch("https://api.adsbdb.com/v0/aircraft/" + m.group(1).lower(), 86400)),
    (r"/api/metar", lambda q, m: fetch(
        "https://aviationweather.gov/api/data/metar?format=json&bbox=%.2f,%.2f,%.2f,%.2f" % (
            num(q, "lat", 0) - 1, num(q, "lon", 0) - 1.5, num(q, "lat", 0) + 1, num(q, "lon", 0) + 1.5), 600)),
    (r"/api/wx", lambda q, m: fetch(
        "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,precipitation,weather_code,wind_speed_10m,wind_direction_10m,cloud_cover,visibility" % (
            num(q, "lat", 0), num(q, "lon", 0)), 300)),
    (r"/api/geo", lambda q, m: fetch("http://ip-api.com/json/?fields=status,city,lat,lon", 3600)),
    (r"/api/rain/meta", lambda q, m: fetch("https://api.rainviewer.com/public/weather-maps.json", 120)),
    (r"/api/rain/(v2/radar/[0-9a-f]+)/(\d+)/(\d+)/(\d+)", lambda q, m: fetch(
        "https://tilecache.rainviewer.com/%s/256/%s/%s/%s/4/1_1.png" % m.groups(), 600, True)),
    (r"/api/tile/(\d+)/(\d+)/(\d+)", lambda q, m: fetch(
        "https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/%s/%s/%s"
        % (m.group(1), m.group(3), m.group(2)), 86400 * 7, True)),
]

# ---- Torre de controle: o proprio server.py toca o radio nas caixas do PC (sem navegador).
# Le o stream da LiveATC como um player de midia (o mesmo caminho do link "listen with Windows Media Player"
# que o site oferece), decodifica o MP3 com a biblioteca miniaudio e toca no dispositivo de som padrao.
# Uso pessoal, so escutando: nada e regravado nem repassado para outro lugar.
try:
    import miniaudio
except ImportError:
    miniaudio = None
# Canais conhecidos: mount -> {"title", "url"}. Preenchido pela descoberta automatica (aeroportos perto da casa).
ATC_FEEDS = {}
CACHE = ROOT / "cache"
_disc_lock, _disc_running = threading.Lock(), set()
# sufixos que a LiveATC costuma usar nos mounts (ex.: cyul_twr, cyul_app, sbgr, sbgr2)
SUFFIXES = ["", "1", "2", "3", "_twr", "_twr1", "_twr2", "_twr3", "_app", "_app2", "_dep", "_gnd", "_del", "_ctr",
            "_atis", "_ramp", "_ctaf", "_co"]
_airports = None


def airports():
    """Aeroportos grandes/medios do mundo (OurAirports, baixado 1 vez e guardado em cache/)."""
    global _airports
    if _airports is None:
        import csv, io
        CACHE.mkdir(exist_ok=True)
        f = CACHE / "airports.csv"
        if not f.exists() or time.time() - f.stat().st_mtime > 30 * 86400:
            with urllib.request.urlopen(urllib.request.Request(
                    "https://davidmegginson.github.io/ourairports-data/airports.csv", headers=UA), timeout=60) as r:
                f.write_bytes(r.read())
        out = []
        for a in csv.DictReader(io.StringIO(f.read_text(encoding="utf-8"))):
            icao = a.get("icao_code") or a.get("gps_code") or ""
            if a["type"] in ("large_airport", "medium_airport") and re.fullmatch(r"[A-Z]{4}", icao):
                out.append((icao, a["name"], float(a["latitude_deg"]), float(a["longitude_deg"]), a["type"], a["iata_code"]))
        _airports = out
    return _airports


def _km(la1, lo1, la2, lo2):
    p = math.pi / 180
    return 12742 * math.asin(math.sqrt(math.sin((la2 - la1) * p / 2) ** 2 +
                                       math.cos(la1 * p) * math.cos(la2 * p) * math.sin((lo2 - lo1) * p / 2) ** 2))


def _probe(mount):
    """Um mount existe se o .pls dele (o link oficial para players de midia) devolve uma playlist."""
    try:
        with urllib.request.urlopen(urllib.request.Request(
                f"https://www.liveatc.net/play/{mount}.pls", headers=UA), timeout=10) as r:
            t = r.read(2000).decode("utf-8", "replace")
    except Exception:
        return None
    if not t.startswith("[playlist]"):
        return None
    url = re.search(r"File1=(\S+)", t); title = re.search(r"Title1=(.+)", t)
    return {"mount": mount, "url": url.group(1) if url else "http://d.liveatc.net/" + mount,
            "title": title.group(1).strip() if title else mount.upper()}


def _discover(icao):
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(6) as ex:
        found = [f for f in ex.map(_probe, [icao.lower() + s for s in SUFFIXES]) if f]
    # mesmo canal com dois mounts (ex.: cyul_twr e cyul_twr2, ambos "CYUL Tower"): fica o primeiro
    seen, feeds = set(), []
    for f in found:
        if f["title"].lower() not in seen:
            seen.add(f["title"].lower()); feeds.append(f)
    with _disc_lock:
        db = _feeds_db()
        db[icao] = {"t": time.time(), "feeds": feeds}
        (CACHE / "liveatc.json").write_text(json.dumps(db, indent=1), encoding="utf-8")
        _disc_running.discard(icao)
    print(f"LiveATC {icao}: {len(feeds)} canal(is)")


def _feeds_db():
    f = CACHE / "liveatc.json"
    try:
        return json.loads(f.read_text(encoding="utf-8"))
    except Exception:
        return {}


def feeds_api(q, m):
    """Aeroportos perto de (lat, lon) e os canais da LiveATC de cada um. A descoberta roda em segundo plano."""
    lat, lon = num(q, "lat", 0), num(q, "lon", 0)
    near = sorted(((_km(lat, lon, a[2], a[3]), a) for a in airports()), key=lambda x: x[0])
    near = [(d, a) for d, a in near if d <= (150 if a[4] == "large_airport" else 60)][:4]
    db, pending, feeds, aps = _feeds_db(), False, [], []
    for d, a in near:
        icao = a[0]
        aps.append({"icao": icao, "iata": a[5], "name": a[1], "km": round(d, 1)})
        e = db.get(icao)
        if not e or time.time() - e["t"] > 7 * 86400:
            pending = True
            with _disc_lock:
                if icao not in _disc_running:
                    _disc_running.add(icao)
                    threading.Thread(target=_discover, args=(icao,), daemon=True).start()
        for f in (e or {}).get("feeds", []):
            ATC_FEEDS[f["mount"]] = f
            feeds.append({**f, "icao": icao})
    body = {"airports": aps, "feeds": feeds, "pending": pending}
    return json.dumps(body).encode(), 200, "application/json"


# canais ja descobertos em execucoes anteriores
for _e in _feeds_db().values():
    for _f in _e.get("feeds", []):
        ATC_FEEDS[_f["mount"]] = _f
_atc = {"feed": None, "stop": None, "thread": None, "live": False, "lv": [], "noise": 200.0, "talkAt": 0.0}
# "lv": volume dos ultimos ~3 s (fatias de ~100 ms) para a onda de fala no radar; "noise": chiado de fundo medido


class _AtcSource(miniaudio.StreamableSource if miniaudio else object):
    def __init__(self, url):
        self.r = urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=15)

    def read(self, n):
        return self.r.read(min(n, 4096))

    def close(self):
        self.r.close()


def _atc_loop(feed, stop):
    """Toca ate pedirem para parar; se o stream cair, reconecta em 5 s."""
    while not stop.is_set():
        src = None
        try:
            src = _AtcSource(ATC_FEEDS[feed]["url"])
            pcm = miniaudio.stream_any(src, source_format=miniaudio.FileFormat.MP3,
                                       output_format=miniaudio.SampleFormat.SIGNED16, nchannels=1, sample_rate=22050)
            ended = threading.Event()

            def frames():
                n = yield b""
                acc, cnt = 0.0, 0
                try:
                    while not stop.is_set():
                        f = pcm.send(n)
                        a = array.array("h", f)
                        if a:   # volume (RMS) em fatias de ~100 ms
                            acc += sum(x * x for x in a); cnt += len(a)
                            if cnt >= 2205:
                                _atc_level(math.sqrt(acc / cnt)); acc, cnt = 0.0, 0
                        n = yield f
                except StopIteration:
                    pass
                ended.set()
                yield b""

            gen = frames()
            next(gen)
            with miniaudio.PlaybackDevice(output_format=miniaudio.SampleFormat.SIGNED16, nchannels=1,
                                          sample_rate=22050) as dev:
                dev.start(gen)
                _atc["live"] = True
                while not stop.is_set() and not ended.is_set():
                    stop.wait(.3)
        except Exception as e:
            print("Torre: falha no stream", feed, "-", e)
        finally:
            _atc["live"] = False
            if src:
                src.close()
        stop.wait(5)


def _atc_level(rms):
    """Registra o volume e decide se ha alguem falando: bem acima do chiado de fundo (que se adapta)."""
    nz = _atc["noise"]
    _atc["noise"] = nz + (rms - nz) * (.002 if rms > nz else .2)   # sobe bem devagar (fala longa nao vira "chiado"), desce rapido
    if rms > max(_atc["noise"] * 3, 350):
        _atc["talkAt"] = time.time()
    _atc["lv"] = (_atc["lv"] + [round(min(1.0, rms / 12000), 3)])[-30:]


def atc_on():
    t = _atc["thread"]
    return bool(t and t.is_alive() and _atc["feed"])


def atc_stop():
    if _atc["stop"]:
        _atc["stop"].set()
    t = _atc["thread"]
    if t:
        t.join(timeout=3)
    _atc.update(feed=None, stop=None, thread=None, live=False)


def atc_api(q, m):
    action, feed = m.group(1), q.get("feed", [""])[0]
    if action == "start" and miniaudio and feed in ATC_FEEDS:
        atc_stop()
        stop = threading.Event()
        t = threading.Thread(target=_atc_loop, args=(feed, stop), daemon=True)
        _atc.update(feed=feed, stop=stop, thread=t)
        t.start()
        for _ in range(40):           # espera conectar (ate ~8 s) para responder
            if _atc["live"]:
                break
            time.sleep(.2)
    elif action == "stop":
        atc_stop()
    on = atc_on()
    body = {"on": on, "live": _atc["live"], "feed": _atc["feed"] if on else None,
            "talk": on and time.time() - _atc["talkAt"] < .8, "lv": _atc["lv"][-24:] if on else [],
            "name": ATC_FEEDS.get(_atc["feed"], {}).get("title") if on else None, "available": bool(miniaudio)}
    return json.dumps(body).encode(), 200, "application/json"


ROUTES.append((r"/api/atc/(start|stop|status|level)", atc_api))
ROUTES.append((r"/api/atc/feeds", feeds_api))


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=str(ROOT), **k)

    def log_message(self, fmt, *args):
        pass

    def end_headers(self):
        # arquivos da pagina: o navegador sempre confere se mudaram (dispensa Ctrl+F5 depois de editar)
        if not self.path.startswith("/api/"):
            self.send_header("Cache-Control", "no-cache")
        super().end_headers()

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        for pat, fn in ROUTES:
            m = re.fullmatch(pat, u.path)
            if m:
                body, status, ctype = fn(urllib.parse.parse_qs(u.query), m)
                self.send_response(status)
                self.send_header("Content-Type", ctype)
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Cache-Control", "no-store")
                if u.path.startswith("/api/atc"):
                    self.send_header("Access-Control-Allow-Origin", "*")   # chamado pela pagina do ESP32
                self.end_headers()
                self.wfile.write(body)
                return
        if u.path.startswith("/api/"):
            self.send_error(404)
            return
        super().do_GET()


if __name__ == "__main__":
    print(f"Radar em http://localhost:{PORT}")
    print("OpenSky:", "conta do usuario (atualiza a cada 2 min)" if opensky_token() else "anonimo (atualiza a cada 15 min)")
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
