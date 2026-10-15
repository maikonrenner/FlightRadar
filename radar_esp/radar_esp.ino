// RADAR DE MESA - ESP32
// O proprio ESP32 busca os avioes perto de casa (adsb.lol, sem chave) e serve o radar
// em http://radar.local (tela redonda 466x466 ou retangular 320x240, desenhada no navegador).
// LED GPIO2 pisca quando um aviao entra na zona de alerta; buzzer passivo no GPIO25. BOOT = liga/desliga o alerta.
// Configuracao pessoal (Wi-Fi, casa, PC): copie secrets.example.h para secrets.h e preencha.
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DNSServer.h>
#include "page.h"

// secrets.h e opcional: sem ele o aparelho sobe no modo de configuracao (rede "Radar-XXXX")
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#define WIFI_PASS ""
#endif
#ifndef HOME_LAT
#define HOME_LAT 0.0
#define HOME_LON 0.0
#define HOME_CITY ""
#endif
#ifndef PC_URL
#define PC_URL ""
#endif

// Wi-Fi e casa ficam salvos na flash (menu Reglages > Appareil); o secrets.h so serve de padrao.
String wifiSsid, wifiPass;
bool apMode = false;
String apName;
DNSServer dns;
float homeLat = HOME_LAT, homeLon = HOME_LON;
String homeCity = HOME_CITY, locSrc = "secrets";
int locAcc = 0;   // precisao da localizacao em metros (0 = desconhecida)

const int RADIUS_NM = 110;                 // cobre o maior zoom (200 km)
const unsigned long POLL_MS = 15000;       // educado com a adsb.lol (a pagina estima a posicao entre leituras)
// Zonas de alerta (raio horizontal + teto). A casa fica a ~13 km do CYUL: 10 km/10.000 ft pegava todo pouso.
struct Zone { float km, ft; };
const Zone ZONES[] = {{2, 3000}, {5, 5000}, {10, 10000}};
int zoneIdx = 1;                              // padrao: 5 km abaixo de 5.000 ft
const unsigned long REALERT_MS = 20UL * 60 * 1000;   // o mesmo aviao so alerta de novo depois de 20 min
Preferences prefs;                            // guarda alerta/som/zona na flash
const int MAX_PLANES = 80;
const int LED = 2, BOOT_BTN = 0;
// A placa ESP32 nao tem alto-falante: ligue um buzzer PASSIVO entre o GPIO25 e o GND.
const int BUZZER = 25;
int beepsLeft = 0;
unsigned long nextBeep = 0;
void beep(int n) { beepsLeft = n; nextBeep = 0; }

struct Plane { char hex[7], flight[9], reg[11], type[5]; int alt, vr; bool ground, al; float gs, trk, distKm, brg, x, y; };
Plane planes[MAX_PLANES];
int nPlanes = 0;
unsigned long lastPoll = 0, dataAt = 0, backoffUntil = 0, lastCheck = 0;
// memoria de alertas: quem ja alertou e quando (evita repetir quando o aviao oscila na borda da zona)
struct AlertMem { char hex[7]; unsigned long at; bool inside; };
AlertMem mem[24];
bool fetchOk = false, alertOn = true, soundOn = true;   // som e alerta ligados por padrao
int alerting = 0;

WebServer server(80);

void copyStr(char* dst, size_t n, const char* src) {
  if (!src) src = "";
  size_t i = 0;
  for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
  while (i > 0 && dst[i - 1] == ' ') i--;   // callsign vem com espacos no fim
  dst[i] = 0;
}

// ---------- localizacao (sem GPS): redes Wi-Fi vizinhas, IP ou digitada pelo usuario
void saveLocation(const char* src) {
  locSrc = src;
  prefs.putFloat("lat", homeLat); prefs.putFloat("lon", homeLon); prefs.putString("city", homeCity);
  prefs.putString("src", locSrc); prefs.putInt("acc", locAcc);
  nPlanes = 0; lastPoll = 0;   // busca os avioes do novo lugar ja
  Serial.printf("Casa (%s): %s (%.4f, %.4f) +-%d m\n", src, homeCity.c_str(), homeLat, homeLon, locAcc);
}

bool locateIp() {
  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin("http://ip-api.com/json/?fields=status,city,lat,lon")) return false;
  bool ok = false;
  if (http.GET() == 200) {
    JsonDocument d;
    if (!deserializeJson(d, http.getString()) && d["status"] == "success") {
      homeLat = d["lat"]; homeLon = d["lon"]; homeCity = d["city"] | "?"; locAcc = 10000; ok = true;
    }
  }
  http.end();
  if (ok) saveLocation("ip");
  return ok;
}

String httpsJson(const String& url, const String& body) {   // GET (body vazio) ou POST JSON por HTTPS
  WiFiClientSecure tls; tls.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(tls, url)) return "";
  http.setUserAgent("radar-de-mesa/1.0 (ESP32)");
  int code;
  if (body.length()) { http.addHeader("Content-Type", "application/json"); code = http.POST(body); } else code = http.GET();
  String r = code == 200 ? http.getString() : "";
  http.end();
  return r;
}

// Localizacao pelas redes Wi-Fi vizinhas (BeaconDB: base comunitaria, sem chave). Envia so os enderecos MAC
// e a forca do sinal das redes que o aparelho enxerga; nada de nomes ou senhas.
bool locateWifi() {
  int n = WiFi.scanNetworks();
  if (n < 2) { WiFi.scanDelete(); return false; }
  JsonDocument q;
  q["considerIp"] = false;
  JsonArray aps = q["wifiAccessPoints"].to<JsonArray>();
  for (int i = 0; i < n && i < 20; i++) {
    if (WiFi.SSID(i).endsWith("_nomap")) continue;   // quem pediu para nao ser mapeado fica de fora
    JsonObject a = aps.add<JsonObject>();
    a["macAddress"] = WiFi.BSSIDstr(i); a["signalStrength"] = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  String body; serializeJson(q, body);
  JsonDocument r;
  if (deserializeJson(r, httpsJson("https://api.beacondb.net/v1/geolocate", body)) || r["location"]["lat"].isNull()) {
    Serial.println("Localizacao por Wi-Fi: sem resposta (redes ainda nao mapeadas aqui)");
    return false;
  }
  homeLat = r["location"]["lat"]; homeLon = r["location"]["lng"]; locAcc = r["accuracy"] | 0;
  // nome da cidade (OpenStreetMap)
  JsonDocument g;
  char u[160];
  snprintf(u, sizeof u, "https://nominatim.openstreetmap.org/reverse?format=json&zoom=10&lat=%.5f&lon=%.5f", homeLat, homeLon);
  if (!deserializeJson(g, httpsJson(u, ""))) {
    JsonObject ad = g["address"];
    homeCity = ad["city"] | (ad["town"] | (ad["village"] | (ad["municipality"] | "?")));
  }
  saveLocation("wifi");
  return true;
}

// ---------- Wi-Fi: rede salva na flash; sem rede (ou senha errada) abre o modo de configuracao
bool connectWifi(int secs) {
  if (!wifiSsid.length()) return false;
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  Serial.printf("Conectando em %s", wifiSsid.c_str());
  for (int i = 0; i < secs * 2 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

void startSetupAP() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char nm[20]; snprintf(nm, sizeof nm, "Radar-%02X%02X", mac[4], mac[5]);
  apName = nm;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str());
  dns.start(53, "*", WiFi.softAPIP());   // qualquer endereco cai na pagina de configuracao (portal cativo)
  apMode = true;
  Serial.printf("Modo configuracao: conecte na rede %s e abra http://192.168.4.1\n", apName.c_str());
}

void sendWifiScan() {
  int n = WiFi.scanNetworks();
  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (JsonObject o : arr) if (o["ssid"] == s) { dup = true; break; }   // a lista ja vem do sinal mais forte
    if (dup) continue;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = s; o["rssi"] = WiFi.RSSI(i); o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

void poll() {
  char url[96];
  snprintf(url, sizeof url, "http://api.adsb.lol/v2/point/%.4f/%.4f/%d", homeLat, homeLon, RADIUS_NM);
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);  // corpo sem chunked, para ler direto do stream
  if (!http.begin(url)) { fetchOk = false; return; }
  http.setUserAgent("radar-de-mesa/1.0 (ESP32)");  // a adsb.lol recusa (403) o agente padrao do ESP32
  int code = http.GET();
  if (code != 200) {
    Serial.printf("adsb.lol HTTP %d\n", code); http.end(); fetchOk = false;
    if (code == 429) backoffUntil = millis() + 60000;  // muitas consultas: espera 1 min
    return;
  }

  // Le so os campos usados, direto da rede: cabe na RAM mesmo com centenas de avioes.
  JsonDocument filter;
  JsonObject f = filter["ac"][0].to<JsonObject>();
  for (const char* k : {"hex", "flight", "r", "t", "alt_baro", "baro_rate", "geom_rate", "gs", "track", "dst", "dir"}) f[k] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("JSON: %s\n", err.c_str()); fetchOk = false; return; }

  int n = 0;
  for (JsonObject a : doc["ac"].as<JsonArray>()) {
    if (n >= MAX_PLANES || a["dst"].isNull() || a["dir"].isNull()) continue;
    Plane& p = planes[n++];
    copyStr(p.hex, sizeof p.hex, a["hex"]);
    copyStr(p.flight, sizeof p.flight, a["flight"]);
    copyStr(p.reg, sizeof p.reg, a["r"]);
    copyStr(p.type, sizeof p.type, a["t"]);
    p.ground = a["alt_baro"].is<const char*>();   // "ground"
    p.alt = p.ground ? 0 : (a["alt_baro"] | 0);
    p.vr = a["baro_rate"] | (a["geom_rate"] | 0);   // ft/min (subindo > 0)
    p.gs = a["gs"] | 0.0f;
    p.trk = a["track"] | 0.0f;
    p.distKm = (a["dst"] | 0.0f) * 1.852f;      // dst vem em milhas nauticas
    p.brg = a["dir"] | 0.0f;
    p.x = p.distKm * sinf(p.brg * DEG_TO_RAD); p.y = p.distKm * cosf(p.brg * DEG_TO_RAD);   // km leste/norte de casa
    p.al = false;
  }
  nPlanes = n; fetchOk = true; dataAt = millis();
  checkAlerts();
  Serial.printf("%d avioes (heap livre %u)\n", n, ESP.getFreeHeap());
}

// Confere o alerta a cada 1 s com a posicao ESTIMADA agora (rumo + velocidade desde a leitura),
// nao com o dado de ate 15 s atras. Entra na zona: alerta 1 vez. Sai so com folga de 25% (histerese).
int findMem(const char* hex, bool create) {   // indice em mem[] (-1 = nao achou); devolve int por causa dos prototipos do Arduino
  int oldest = 0;
  for (int i = 0; i < 24; i++) { if (!strcmp(mem[i].hex, hex)) return i; if (mem[i].at < mem[oldest].at) oldest = i; }
  if (!create) return -1;
  strcpy(mem[oldest].hex, hex); mem[oldest].at = 0; mem[oldest].inside = false;
  return oldest;
}

void checkAlerts() {
  const Zone z = ZONES[zoneIdx];
  float dt = min(60.0f, (millis() - dataAt) / 1000.0f);
  int inside = 0;
  for (int i = 0; i < nPlanes; i++) {
    Plane& p = planes[i];
    float t = p.trk * DEG_TO_RAD, k = p.gs * 1.852f / 3600 * dt;
    float d = p.ground ? 1e9 : hypotf(p.x + k * sinf(t), p.y + k * cosf(t));
    int mi = findMem(p.hex, false);
    AlertMem* m = mi < 0 ? nullptr : &mem[mi];
    bool was = m && m->inside;
    bool in = !p.ground && p.alt <= z.ft + (was ? 300 : 0) && d <= z.km * (was ? 1.25f : 1.0f);
    if (in && !was) {
      m = &mem[findMem(p.hex, true)];
      if (!m->at || millis() - m->at > REALERT_MS) {
        m->at = millis();
        Serial.printf("ALERTA: %s %s a %.1f km, %d ft\n", p.flight, p.type, d, p.alt);
        if (alertOn && soundOn) beep(3);
      }
    }
    if (m) m->inside = in;
    p.al = in;
    if (in) inside++;
  }
  alerting = inside;
}

void savePrefs() { prefs.putBool("alert", alertOn); prefs.putBool("sound", soundOn); prefs.putInt("zone", zoneIdx); }

void sendPlanes() {
  JsonDocument d;
  d["lat"] = homeLat; d["lon"] = homeLon; d["city"] = homeCity; d["pc"] = PC_URL;   // PC com o server.py (audio da torre)
  d["src"] = locSrc; d["acc"] = locAcc; d["ssid"] = wifiSsid; d["ap"] = apMode; d["apName"] = apName;
  d["online"] = WiFi.status() == WL_CONNECTED;
  d["ok"] = dataAt && millis() - dataAt < 90000;  // um 429 isolado nao apaga o radar
  d["age"] = (millis() - dataAt) / 1000.0;
  d["alerting"] = alertOn ? alerting : 0; d["alertOn"] = alertOn; d["soundOn"] = soundOn;
  d["aKm"] = ZONES[zoneIdx].km; d["aFt"] = ZONES[zoneIdx].ft; d["zone"] = zoneIdx;
  JsonArray arr = d["planes"].to<JsonArray>();
  for (int i = 0; i < nPlanes; i++) {
    const Plane& p = planes[i];
    JsonObject o = arr.add<JsonObject>();
    o["hex"] = p.hex; o["flight"] = p.flight; o["reg"] = p.reg; o["type"] = p.type;
    o["alt"] = p.alt; o["vr"] = p.vr; o["ground"] = p.ground; o["gs"] = p.gs; o["trk"] = p.trk;
    if (p.al) o["al"] = true;
    o["dist"] = serialized(String(p.distKm, 2)); o["brg"] = serialized(String(p.brg, 1));
  }
  String out;
  serializeJson(d, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

// Modelo + rota (origem/destino) do aviao tocado, pela adsbdb (HTTPS, sem chave). Guarda os ultimos 8.
struct Info { String hex, cs, json; };
Info infoCache[8];
int infoNext = 0;

String fetchAdsbdb(const String& path, JsonDocument& filter, JsonDocument& out) {
  WiFiClientSecure tls;
  tls.setInsecure();  // so leitura de dados publicos; nao ha segredo trafegando
  HTTPClient http;
  http.setTimeout(8000);
  http.useHTTP10(true);
  if (!http.begin(tls, "https://api.adsbdb.com/v0/" + path)) return "begin";
  http.setUserAgent("radar-de-mesa/1.0 (ESP32)");
  int code = http.GET();
  String err = "";
  if (code != 200) err = String(code);
  else if (deserializeJson(out, http.getStream(), DeserializationOption::Filter(filter))) err = "json";
  http.end();
  return err;
}

void sendInfo() {
  String hex = server.arg("hex"), cs = server.arg("cs");
  for (auto& c : infoCache) if (c.hex == hex && c.cs == cs && c.json.length()) { server.send(200, "application/json", c.json); return; }

  JsonDocument filter;
  JsonObject fa = filter["response"]["aircraft"].to<JsonObject>();
  fa["manufacturer"] = true; fa["type"] = true; fa["icao_type"] = true; fa["registration"] = true;
  JsonObject fr = filter["response"]["flightroute"].to<JsonObject>();
  fr["airline"]["name"] = true;
  for (const char* k : {"origin", "destination"}) { JsonObject a = fr[k].to<JsonObject>(); a["iata_code"] = true; a["icao_code"] = true; a["municipality"] = true; }

  JsonDocument in;
  String path = "aircraft/" + hex + (cs.length() ? "?callsign=" + cs : "");
  String err = fetchAdsbdb(path, filter, in);
  // aviao desconhecido na base: tenta so a rota pelo callsign
  if ((err.length() || !in["response"]["aircraft"].is<JsonObject>()) && cs.length()) { in.clear(); fetchAdsbdb("callsign/" + cs, filter, in); }

  JsonDocument d;
  JsonObject a = in["response"]["aircraft"];
  if (!a.isNull()) { String m = String(a["manufacturer"] | "") + " " + String(a["type"] | ""); m.trim(); d["model"] = m; d["reg"] = a["registration"] | ""; }
  JsonObject r = in["response"]["flightroute"];
  if (r["airline"]["name"].is<const char*>()) d["airline"] = r["airline"]["name"];
  for (const char* k : {"origin", "destination"}) {
    JsonObject ap = r[k];
    if (ap.isNull()) continue;
    JsonObject o = d[k[0] == 'o' ? "orig" : "dest"].to<JsonObject>();
    o["code"] = ap["iata_code"] | (ap["icao_code"] | "?");
    o["city"] = ap["municipality"] | "";
  }
  String out;
  serializeJson(d, out);
  infoCache[infoNext] = { hex, cs, out }; infoNext = (infoNext + 1) % 8;
  server.send(200, "application/json", out);
}

void setup() {
  Serial.begin(115200);
  prefs.begin("radar", false);
  alertOn = prefs.getBool("alert", true); soundOn = prefs.getBool("sound", true); zoneIdx = constrain(prefs.getInt("zone", 1), 0, 2);
  pinMode(LED, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  wifiSsid = prefs.getString("ssid", WIFI_SSID); wifiPass = prefs.getString("pass", WIFI_PASS);
  if (prefs.isKey("lat")) {
    homeLat = prefs.getFloat("lat"); homeLon = prefs.getFloat("lon"); homeCity = prefs.getString("city", "");
    locSrc = prefs.getString("src", "?"); locAcc = prefs.getInt("acc", 0);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("radar");
  if (connectWifi(15)) Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  else startSetupAP();
  if (MDNS.begin("radar")) { MDNS.addService("http", "tcp", 80); Serial.println("Abra: http://radar.local"); }
  // primeira vez num lugar novo: tenta pelas redes Wi-Fi vizinhas (mais preciso), senao pelo IP
  if (homeLat == 0 && homeLon == 0 && WiFi.status() == WL_CONNECTED && !locateWifi()) locateIp();
  Serial.printf("Casa: %s (%.4f, %.4f) via %s\n", homeCity.c_str(), homeLat, homeLon, locSrc.c_str());

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/planes", HTTP_GET, sendPlanes);
  server.on("/alert", HTTP_POST, [] { alertOn = !alertOn; savePrefs(); sendPlanes(); });
  server.on("/zone", HTTP_POST, [] { zoneIdx = (zoneIdx + 1) % 3; savePrefs(); for (auto& m : mem) m.inside = false; checkAlerts(); sendPlanes(); });
  server.on("/sound", HTTP_POST, [] { soundOn = !soundOn; savePrefs(); sendPlanes(); });
  server.on("/info", HTTP_GET, sendInfo);
  server.on("/beep", HTTP_POST, [] { beep(3); server.send(204); });
  server.on("/wifi/scan", HTTP_GET, sendWifiScan);
  server.on("/wifi/save", HTTP_POST, [] {
    prefs.putString("ssid", server.arg("ssid")); prefs.putString("pass", server.arg("pass"));
    server.send(200, "application/json", "{\"ok\":true}");
    delay(800); ESP.restart();   // reinicia ja conectando na rede nova
  });
  server.on("/locate", HTTP_POST, [] {
    bool ok = server.arg("method") == "ip" ? locateIp() : locateWifi();
    if (!ok) { server.send(200, "application/json", "{\"ok\":false}"); return; }
    sendPlanes();
  });
  server.on("/location", HTTP_POST, [] {   // cidade escolhida pelo usuario
    homeLat = server.arg("lat").toFloat(); homeLon = server.arg("lon").toFloat(); homeCity = server.arg("city"); locAcc = 0;
    saveLocation("manuel"); sendPlanes();
  });
  server.onNotFound([] {   // portal cativo: no modo configuracao qualquer endereco abre o radar
    if (apMode) { server.sendHeader("Location", "http://192.168.4.1/"); server.send(302); }
    else server.send(404, "text/plain", "404");
  });
  if (soundOn) beep(1);  // bipe de "liguei"
  server.begin();
}

void loop() {
  if (apMode) dns.processNextRequest();
  server.handleClient();
  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED && (now - lastPoll > POLL_MS || lastPoll == 0) && (long)(now - backoffUntil) >= 0) { lastPoll = now; poll(); }

  static bool btnPrev = HIGH;
  static unsigned long btnT = 0;
  bool btn = digitalRead(BOOT_BTN);
  if (btn != btnPrev && now - btnT > 40) {
    btnT = now;
    if (btn == LOW) { alertOn = !alertOn; savePrefs(); Serial.printf("Alerta %s\n", alertOn ? "ligado" : "desligado"); }
    btnPrev = btn;
  }
  if (now - lastCheck > 1000) { lastCheck = now; checkAlerts(); }
  digitalWrite(LED, alertOn && alerting > 0 && (now / 250) % 2);

  if (beepsLeft > 0 && (long)(now - nextBeep) >= 0) {   // bipes sem travar o loop
    tone(BUZZER, 2000, 150);
    beepsLeft--; nextBeep = now + 300;
  }
}
