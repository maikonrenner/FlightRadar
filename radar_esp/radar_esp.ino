// SENTINEL 643 - Tactical Aviation Radar (radar de mesa com ESP32)
// O proprio ESP32 busca os avioes perto de casa (adsb.lol, sem chave) e serve o radar
// em http://radar.local (tela redonda 466x466 ou retangular 320x240, desenhada no navegador).
// LED GPIO2 pisca quando um aviao entra na zona de alerta; buzzer passivo no GPIO25. BOOT = liga/desliga os alertas.
// Particao: "Minimal SPIFFS" (1,9 MB de programa com OTA) -> atualizacao pelo Wi-Fi em Reglages > Appareil.
// Torre de controle: o ESP descobre os canais da LiveATC e repassa o audio pela porta 81; quem toca e o navegador.
// Configuracao pessoal (Wi-Fi, casa): copie secrets.example.h para secrets.h e preencha.
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <Update.h>
#include <esp_heap_caps.h>
#include "page.h"

#define FW_VERSION "S643 Mk I 1.0"

// secrets.h e opcional: sem ele o aparelho sobe no modo de configuracao (rede "S643-XXXX")
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

// Wi-Fi e casa ficam salvos na flash (menu Reglages > Appareil); o secrets.h so serve de padrao.
// Ate 3 redes: o aparelho entra na mais forte que estiver visivel (casa, trabalho, celular...).
const int MAX_NETS = 3;
String netS[MAX_NETS], netP[MAX_NETS];
String wifiSsid;   // rede em uso agora
bool apMode = false;
String apName;
DNSServer dns;
float homeLat = HOME_LAT, homeLon = HOME_LON;
String homeCity = HOME_CITY, locSrc = "secrets";
int locAcc = 0;   // precisao da localizacao em metros (0 = desconhecida)

const unsigned long POLL_MS = 15000;       // educado com a adsb.lol (a pagina estima a posicao entre leituras)
// Raio consultado acompanha o zoom da tela: min(260, max(12, 1,4 x alcance visivel)) km. A margem de 40% mostra
// quem esta para entrar (setas na borda). Sem ninguem olhando ha 60 s, usa 50 km (cobre a zona de alerta com folga).
float wantKm = 0, queryKm = 70;
unsigned long wantAt = 0;
float queryRadiusKm() { float vis = wantAt && millis() - wantAt < 60000 ? wantKm : 50; return constrain(1.4f * vis, 12.0f, 260.0f); }
// Alertas: modo (0 = desligado, 1 = so importantes: proximidade + emergencia, 2 = todos: + aviao novo na pagina),
// raio e teto da zona de proximidade, emergencia (squawk 7500/7600/7700) e bipe de aviao novo.
// A casa fica a ~13 km do CYUL: 10 km/10.000 ft pegava todo pouso.
const float RADII[] = {2, 5, 10};
const int CEILS[] = {3000, 5000, 10000};
int radIdx = 1, ceilIdx = 1, alertMode = 1, lastMode = 1;   // padrao: 5 km abaixo de 5.000 ft, so importantes
bool alertEm = true, alertNew = true;
const unsigned long REALERT_MS = 20UL * 60 * 1000;   // o mesmo aviao so alerta de novo depois de 20 min
Preferences prefs;                            // guarda alertas/som/redes na flash
const int MAX_PLANES = 80;                    // com mais avioes na area, ficam os mais perto
const int LED = 2, BOOT_BTN = 0;
// A placa ESP32 nao tem alto-falante: ligue um buzzer PASSIVO entre o GPIO25 e o GND.
const int BUZZER = 25;
int beepsLeft = 0;
unsigned long nextBeep = 0;
void beep(int n) { beepsLeft = n; nextBeep = 0; }

// gs/trk = NAN quando a adsb.lol nao manda (a pagina nao inventa vetor); sp = segundos desde a ultima posicao
// cat = categoria ADS-B (A1 leve ... A7 helicoptero, B* planador/ULM/drone); mil = militar segundo a base da adsb.lol
struct Plane { char hex[7], flight[9], reg[11], type[5], sq[5], cat[3]; int alt, vr; bool ground, al, mil; float gs, trk, distKm, brg, x, y; uint8_t sp; };
Plane planes[MAX_PLANES];
void logAlert(const Plane& p, char kind, float km);   // prototipo depois da struct (o Arduino poe os automaticos antes)
int nPlanes = 0;
unsigned long lastPoll = 0, dataAt = 0, backoffUntil = 0, lastCheck = 0;
// a busca dos avioes roda numa tarefa propria (a rede nunca trava a pagina nem os toques); o mutex protege planes[]
SemaphoreHandle_t planesMux;
volatile bool pollNow = true;
int lastHttp = 0, lastFail = 0;   // ultimo erro da adsb.lol (0 = ok), mostrado no estado ADS-B da tela
// memoria de alertas: quem ja alertou e quando (evita repetir quando o aviao oscila na borda da zona)
struct AlertMem { char hex[7]; unsigned long at; bool inside; };
AlertMem mem[24];
bool fetchOk = false, soundOn = true;   // som ligado por padrao
int alerting = 0;
// emergencias ja avisadas (squawk) e historico dos ultimos 10 alertas (menu Alertes)
struct EmMem { char hex[7]; unsigned long at; };
EmMem emMem[8];
struct AlertLog { char cs[11]; char kind; float km; int alt; unsigned long at; char sq[5]; };
AlertLog alog[10];
int alogN = 0;
// diagnostico
unsigned long pollMs = 0, polls = 0, pollFails = 0;

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
  xSemaphoreTake(planesMux, portMAX_DELAY); nPlanes = 0; xSemaphoreGive(planesMux);
  pollNow = true;   // busca os avioes do novo lugar ja
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

// ---------- Wi-Fi: ate 3 redes salvas; sem nenhuma por perto abre o modo de configuracao (rede S643-XXXX),
// mas continua procurando as redes salvas e volta sozinho quando uma aparece (intervalos de 1 min a 5 min).
String netKey(int i, char c) { return "w" + String(i) + c; }
void saveNets() { for (int i = 0; i < MAX_NETS; i++) { prefs.putString(netKey(i, 's').c_str(), netS[i]); prefs.putString(netKey(i, 'p').c_str(), netP[i]); } }
void loadNets() {
  for (int i = 0; i < MAX_NETS; i++) { netS[i] = prefs.getString(netKey(i, 's').c_str(), ""); netP[i] = prefs.getString(netKey(i, 'p').c_str(), ""); }
  if (!netS[0].length() && prefs.isKey("ssid")) {   // versao antiga: uma rede so
    netS[0] = prefs.getString("ssid", ""); netP[0] = prefs.getString("pass", "");
    saveNets(); prefs.remove("ssid"); prefs.remove("pass");
  }
  if (!netS[0].length() && strlen(WIFI_SSID)) { netS[0] = WIFI_SSID; netP[0] = WIFI_PASS; }
}
void addNet(const String& ss, const String& pw) {   // a nova vai para o topo; repetida nao duplica; no maximo 3
  String S[MAX_NETS], P[MAX_NETS]; int n = 0;
  S[n] = ss; P[n] = pw; n++;
  for (int i = 0; i < MAX_NETS && n < MAX_NETS; i++) if (netS[i].length() && netS[i] != ss) { S[n] = netS[i]; P[n] = netP[i]; n++; }
  for (int i = 0; i < MAX_NETS; i++) { netS[i] = i < n ? S[i] : ""; netP[i] = i < n ? P[i] : ""; }
  saveNets();
}
void forgetNet(const String& ss) {
  String S[MAX_NETS], P[MAX_NETS]; int n = 0;
  for (int i = 0; i < MAX_NETS; i++) if (netS[i].length() && netS[i] != ss) { S[n] = netS[i]; P[n] = netP[i]; n++; }
  for (int i = 0; i < MAX_NETS; i++) { netS[i] = i < n ? S[i] : ""; netP[i] = i < n ? P[i] : ""; }
  saveNets();
}
int pickNet() {   // indice da rede salva com o sinal mais forte agora (-1 = nenhuma visivel)
  int n = WiFi.scanNetworks(), best = -1, bestRssi = -999;
  for (int i = 0; i < n; i++)
    for (int j = 0; j < MAX_NETS; j++)
      if (netS[j].length() && WiFi.SSID(i) == netS[j] && WiFi.RSSI(i) > bestRssi) { best = j; bestRssi = WiFi.RSSI(i); }
  WiFi.scanDelete();
  return best;
}
bool connectWifi(int secs) {
  if (!netS[0].length()) return false;
  int j = pickNet();
  if (j < 0) j = 0;   // nenhuma visivel (rede oculta?): tenta a primeira mesmo assim
  wifiSsid = netS[j];
  WiFi.begin(netS[j].c_str(), netP[j].c_str());
  Serial.printf("Conectando em %s", netS[j].c_str());
  for (int i = 0; i < secs * 2 && WiFi.status() != WL_CONNECTED; i++) { delay(500); Serial.print("."); }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

void startSetupAP() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char nm[20]; snprintf(nm, sizeof nm, "S643-%02X%02X", mac[4], mac[5]);
  apName = nm;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName.c_str());
  dns.start(53, "*", WiFi.softAPIP());   // qualquer endereco cai na pagina de configuracao (portal cativo)
  apMode = true;
  Serial.printf("Modo configuracao: conecte na rede %s e abra http://192.168.4.1\n", apName.c_str());
}
void leaveSetupAP() {   // uma rede salva apareceu: fecha a rede de configuracao e volta ao normal
  dns.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
  MDNS.end();
  if (MDNS.begin("radar")) MDNS.addService("http", "tcp", 80);
  Serial.printf("Wi-Fi %s de volta: %s\n", wifiSsid.c_str(), WiFi.localIP().toString().c_str());
  if (homeLat == 0 && homeLon == 0) locateIp();
  pollNow = true;
}
unsigned long netRetryAt = 0, netBackoff = 60000, netDownSince = 0;
void wifiWatch() {
  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (apMode) leaveSetupAP();
    netDownSince = 0; netBackoff = 60000;
    return;
  }
  if (!netS[0].length()) return;
  if (!netDownSince) { netDownSince = now; netRetryAt = now + (apMode ? 60000 : 20000); }   // primeiro deixa a reconexao do proprio ESP tentar
  if ((long)(now - netRetryAt) < 0) return;
  netRetryAt = now + netBackoff; netBackoff = min(netBackoff * 2, 300000UL);
  int j = pickNet();
  if (j >= 0) { Serial.printf("Rede salva visivel: %s, conectando\n", netS[j].c_str()); wifiSsid = netS[j]; WiFi.begin(netS[j].c_str(), netP[j].c_str()); }
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
void sendWifiList() {   // redes salvas (sem as senhas) e a rede em uso
  JsonDocument d;
  d["cur"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
  JsonArray arr = d["nets"].to<JsonArray>();
  for (int i = 0; i < MAX_NETS; i++) if (netS[i].length()) arr.add(netS[i]);
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

Plane fresh[MAX_PLANES];   // leitura nova; so vai para planes[] (sob o mutex) quando estiver completa

void poll() {
  unsigned long t0 = millis();
  queryKm = queryRadiusKm();
  char url[96];
  snprintf(url, sizeof url, "http://api.adsb.lol/v2/point/%.4f/%.4f/%d", homeLat, homeLon, (int)ceilf(queryKm / 1.852f));
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);  // corpo sem chunked, para ler direto do stream
  if (!http.begin(url)) { fetchOk = false; return; }
  http.setUserAgent("radar-de-mesa/1.0 (ESP32)");  // a adsb.lol recusa (403) o agente padrao do ESP32
  int code = http.GET();
  if (code != 200) {
    Serial.printf("adsb.lol HTTP %d\n", code); http.end(); fetchOk = false; lastHttp = lastFail = code; pollFails++;
    if (code == 429) backoffUntil = millis() + 60000;  // muitas consultas: espera 1 min
    return;
  }

  // Le so os campos usados, direto da rede: cabe na RAM mesmo com centenas de avioes.
  JsonDocument filter;
  JsonObject f = filter["ac"][0].to<JsonObject>();
  for (const char* k : {"hex", "flight", "r", "t", "alt_baro", "baro_rate", "geom_rate", "gs", "track", "dst", "dir", "squawk", "seen_pos", "category", "dbFlags"}) f[k] = true;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("JSON: %s\n", err.c_str()); fetchOk = false; lastHttp = lastFail = -2; pollFails++; return; }

  int n = 0;
  for (JsonObject a : doc["ac"].as<JsonArray>()) {
    if (a["dst"].isNull() || a["dir"].isNull() || (a["seen_pos"] | 0.0f) > 60) continue;   // posicao de mais de 1 min: fora
    float dKm = (a["dst"] | 0.0f) * 1.852f;
    int slot = n;
    if (n >= MAX_PLANES) {   // cheio: substitui o mais longe, se este estiver mais perto
      slot = 0;
      for (int i = 1; i < n; i++) if (fresh[i].distKm > fresh[slot].distKm) slot = i;
      if (fresh[slot].distKm <= dKm) continue;
    } else n++;
    Plane& p = fresh[slot];
    copyStr(p.hex, sizeof p.hex, a["hex"]);
    copyStr(p.flight, sizeof p.flight, a["flight"]);
    copyStr(p.reg, sizeof p.reg, a["r"]);
    copyStr(p.type, sizeof p.type, a["t"]);
    copyStr(p.sq, sizeof p.sq, a["squawk"]);
    p.sp = (uint8_t)constrain((int)(a["seen_pos"] | 0.0f), 0, 255);
    copyStr(p.cat, sizeof p.cat, a["category"]);
    p.mil = ((a["dbFlags"] | 0) & 1) != 0;
    p.ground = a["alt_baro"].is<const char*>();   // "ground"
    p.alt = p.ground ? 0 : (a["alt_baro"] | 0);
    p.vr = a["baro_rate"] | (a["geom_rate"] | 0);   // ft/min (subindo > 0)
    p.gs = a["gs"].isNull() ? NAN : a["gs"].as<float>();
    p.trk = a["track"].isNull() ? NAN : a["track"].as<float>();
    p.distKm = dKm;      // dst vem em milhas nauticas
    p.brg = a["dir"] | 0.0f;
    p.x = p.distKm * sinf(p.brg * DEG_TO_RAD); p.y = p.distKm * cosf(p.brg * DEG_TO_RAD);   // km leste/norte de casa
    p.al = false;
  }
  xSemaphoreTake(planesMux, portMAX_DELAY);
  memcpy(planes, fresh, sizeof(Plane) * n);
  nPlanes = n; fetchOk = true; dataAt = millis(); lastHttp = 0;
  xSemaphoreGive(planesMux);
  pollMs = millis() - t0; polls++;
  checkAlerts();
  Serial.printf("%d avioes num raio de %.0f km em %lu ms (heap livre %u)\n", n, queryKm, pollMs, ESP.getFreeHeap());
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

void logAlert(const Plane& p, char kind, float km) {
  memmove(&alog[1], &alog[0], sizeof(AlertLog) * 9);
  AlertLog& l = alog[0];
  copyStr(l.cs, sizeof l.cs, p.flight[0] ? p.flight : p.reg[0] ? p.reg : p.hex);
  l.kind = kind; l.km = km; l.alt = p.alt; l.at = millis(); strcpy(l.sq, p.sq);
  if (alogN < 10) alogN++;
}
bool isEmergency(const char* sq) { return !strcmp(sq, "7500") || !strcmp(sq, "7600") || !strcmp(sq, "7700"); }

void checkAlerts() {
  xSemaphoreTake(planesMux, portMAX_DELAY);
  const float zKm = RADII[radIdx]; const int zFt = CEILS[ceilIdx];
  float dt = min(60.0f, (millis() - dataAt) / 1000.0f);
  int inside = 0;
  for (int i = 0; i < nPlanes; i++) {
    Plane& p = planes[i];
    bool mv = !isnan(p.gs) && !isnan(p.trk);
    float t = mv ? p.trk * DEG_TO_RAD : 0, k = mv ? p.gs * 1.852f / 3600 * dt : 0;
    float d = p.ground ? 1e9 : hypotf(p.x + k * sinf(t), p.y + k * cosf(t));
    int mi = findMem(p.hex, false);
    AlertMem* m = mi < 0 ? nullptr : &mem[mi];
    bool was = m && m->inside;
    bool in = alertMode > 0 && !p.ground && p.alt <= zFt + (was ? 300 : 0) && d <= zKm * (was ? 1.25f : 1.0f);
    if (in && !was) {
      m = &mem[findMem(p.hex, true)];
      if (!m->at || millis() - m->at > REALERT_MS) {
        m->at = millis();
        Serial.printf("ALERTA: %s %s a %.1f km, %d ft\n", p.flight, p.type, d, p.alt);
        logAlert(p, 'P', d);
        if (soundOn) beep(3);
      }
    }
    // emergencia (squawk 7500/7600/7700): avisa 1 vez a cada 20 min por aviao, em qualquer distancia
    if (alertMode > 0 && alertEm && isEmergency(p.sq)) {
      int e = -1, old = 0;
      for (int k2 = 0; k2 < 8; k2++) { if (!strcmp(emMem[k2].hex, p.hex)) e = k2; if (emMem[k2].at < emMem[old].at) old = k2; }
      if (e < 0 || millis() - emMem[e].at > REALERT_MS) {
        if (e < 0) { e = old; strcpy(emMem[e].hex, p.hex); }
        emMem[e].at = millis();
        Serial.printf("EMERGENCIA: %s squawk %s\n", p.flight, p.sq);
        logAlert(p, 'E', p.distKm);
        if (soundOn) beep(5);
      }
    }
    if (m) m->inside = in;
    p.al = in;
    if (in) inside++;
  }
  alerting = inside;
  xSemaphoreGive(planesMux);
}

void savePrefs() {
  prefs.putBool("sound", soundOn); prefs.putInt("aMode", alertMode); prefs.putInt("aLast", lastMode);
  prefs.putInt("aRad", radIdx); prefs.putInt("aCeil", ceilIdx); prefs.putBool("aEm", alertEm); prefs.putBool("aNew", alertNew);
}

void sendPlanes() {
  // r = alcance visivel da pagina (km): o raio consultado acompanha o maior alcance pedido nos ultimos segundos
  float r = server.arg("r").toFloat();
  if (r > 0 && (r >= wantKm || millis() - wantAt > 6000)) {
    wantKm = r; wantAt = millis();
    if (constrain(1.4f * r, 12.0f, 260.0f) > queryKm * 1.05f) pollNow = true;   // zoom maior: busca ja
  } else if (r > 0 && r == wantKm) wantAt = millis();
  JsonDocument d;
  d["lat"] = homeLat; d["lon"] = homeLon; d["city"] = homeCity;
  if (lastHttp) d["err"] = lastHttp;
  d["src"] = locSrc; d["acc"] = locAcc; d["ssid"] = wifiSsid; d["ap"] = apMode; d["apName"] = apName;
  d["online"] = WiFi.status() == WL_CONNECTED;
  d["ok"] = dataAt && millis() - dataAt < 90000;  // um 429 isolado nao apaga o radar
  d["age"] = (millis() - dataAt) / 1000.0;
  d["alerting"] = alertMode ? alerting : 0; d["alertOn"] = alertMode > 0; d["soundOn"] = soundOn;
  d["aMode"] = alertMode; d["aKm"] = RADII[radIdx]; d["aFt"] = CEILS[ceilIdx]; d["aEm"] = alertEm; d["aNew"] = alertNew;
  d["qKm"] = queryKm; d["nets"] = netS[0].length() ? (netS[1].length() ? (netS[2].length() ? 3 : 2) : 1) : 0;
  JsonArray arr = d["planes"].to<JsonArray>();
  xSemaphoreTake(planesMux, portMAX_DELAY);
  for (int i = 0; i < nPlanes; i++) {
    const Plane& p = planes[i];
    JsonObject o = arr.add<JsonObject>();
    o["hex"] = p.hex; o["flight"] = p.flight; o["reg"] = p.reg; o["type"] = p.type;
    o["alt"] = p.alt; o["vr"] = p.vr; o["ground"] = p.ground;
    if (!isnan(p.gs)) o["gs"] = p.gs;
    if (!isnan(p.trk)) o["trk"] = p.trk;
    if (p.sq[0]) o["sq"] = p.sq;
    if (p.sp) o["sp"] = p.sp;
    if (p.al) o["al"] = true;
    if (p.cat[0]) o["cat"] = p.cat;
    if (p.mil) o["mil"] = true;
    o["dist"] = serialized(String(p.distKm, 2)); o["brg"] = serialized(String(p.brg, 1));
  }
  xSemaphoreGive(planesMux);
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

// ---------- torre de controle (LiveATC)
// Um canal existe se o .pls dele (o link oficial para players de midia) devolve uma playlist.
// A descoberta roda numa tarefa separada e guarda o resultado na flash (namespace "atc", chave = ICAO).
const char* ATC_UA = "esp32-desk-radar/1.0 (hobby)";   // com agente de navegador a LiveATC pede o desafio do Cloudflare
const char* SUFFIXES[] = {"", "1", "2", "3", "_twr", "_twr1", "_twr2", "_twr3", "_app", "_app2", "_dep", "_gnd", "_del", "_ctr",
                          "_atis", "_ramp", "_ctaf", "_co"};
volatile bool discBusy = false;
WiFiServer atcSrv(81);

void discoverTask(void* arg) {
  String* list = (String*)arg;
  Preferences p; p.begin("atc", false);
  WiFiClientSecure tls; tls.setInsecure();
  HTTPClient http; http.setReuse(true); http.setTimeout(8000);
  for (int start = 0; start < (int)list->length();) {
    int e = list->indexOf(',', start); if (e < 0) e = list->length();
    String icao = list->substring(start, e); start = e + 1;
    String lc = icao; lc.toLowerCase();
    JsonDocument d; JsonArray arr = d.to<JsonArray>();
    for (const char* s : SUFFIXES) {
      String mount = lc + s;
      if (!http.begin(tls, "https://www.liveatc.net/play/" + mount + ".pls")) continue;
      http.setUserAgent(ATC_UA);
      String t = http.GET() == 200 ? http.getString() : "";
      http.end();
      if (!t.startsWith("[playlist]")) continue;
      String title = mount;
      int i = t.indexOf("Title1=");
      if (i >= 0) { int j = t.indexOf('\n', i); title = t.substring(i + 7, j < 0 ? t.length() : j); title.trim(); }
      bool dup = false;   // mesmo canal com dois mounts (ex.: cyul_twr e cyul_twr2): fica o primeiro
      for (JsonObject o : arr) if (title.equalsIgnoreCase(o["t"].as<const char*>())) dup = true;
      if (dup) continue;
      JsonObject o = arr.add<JsonObject>(); o["m"] = mount; o["t"] = title;
    }
    String out; serializeJson(d, out);
    p.putString(icao.c_str(), out);
    Serial.printf("LiveATC %s: %d canal(is) (heap livre %u)\n", icao.c_str(), arr.size(), ESP.getFreeHeap());
  }
  p.end();
  delete list;
  discBusy = false;
  vTaskDelete(NULL);
}

// /atc/feeds?icao=CYUL,CYHU,...  (a pagina manda os aeroportos mais perto, do mais perto para o mais longe)
void sendAtcFeeds() {
  String icaos = server.arg("icao"), missing;
  Preferences p; p.begin("atc", true);
  JsonDocument d; JsonArray feeds = d["feeds"].to<JsonArray>();
  for (int start = 0; start < (int)icaos.length();) {
    int e = icaos.indexOf(',', start); if (e < 0) e = icaos.length();
    String icao = icaos.substring(start, e); start = e + 1;
    if (icao.length() != 4) continue;
    if (!p.isKey(icao.c_str())) { if (missing.length()) missing += ','; missing += icao; continue; }
    JsonDocument f;
    if (deserializeJson(f, p.getString(icao.c_str()))) continue;
    for (JsonObject o : f.as<JsonArray>()) {
      JsonObject x = feeds.add<JsonObject>();
      x["icao"] = icao; x["mount"] = String(o["m"] | ""); x["title"] = String(o["t"] | "");
    }
  }
  p.end();
  if (missing.length() && !discBusy && WiFi.status() == WL_CONNECTED) {
    discBusy = true;
    xTaskCreatePinnedToCore(discoverTask, "atcDisc", 12288, new String(missing), 1, NULL, 0);
  }
  d["pending"] = missing.length() > 0;
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}

// Repassa o stream MP3 (16 kbps) para o navegador: http://radar.local:81/<mount>. Um ouvinte por vez;
// uma conexao nova (troca de canal) derruba a anterior.
void relayAtc(WiFiClient& cli, const String& mount) {
  String url = "http://d.liveatc.net/" + mount;
  {   // d.liveatc.net redireciona para o servidor do stream (HTTPS)
    HTTPClient h; h.setTimeout(8000);
    const char* keys[] = {"Location"}; h.collectHeaders(keys, 1);
    if (h.begin(url)) {
      h.setUserAgent(ATC_UA);
      int c = h.GET();
      if ((c == 301 || c == 302) && h.header("Location").length()) url = h.header("Location");
    }
    h.end();
  }
  WiFiClientSecure tls; tls.setInsecure();
  WiFiClient plain;
  HTTPClient h; h.setTimeout(10000); h.useHTTP10(true);
  bool ok = url.startsWith("https") ? h.begin(tls, url) : h.begin(plain, url);
  if (ok) h.setUserAgent(ATC_UA);
  int code = ok ? h.GET() : -1;
  if (code != 200) {
    Serial.printf("Torre %s: HTTP %d\n", mount.c_str(), code);
    cli.print("HTTP/1.1 502 Bad Gateway\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");
    h.end(); return;
  }
  // CORS liberado: o navegador precisa ler o audio para desenhar a onda de fala
  cli.print("HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nAccess-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n");
  Serial.printf("Torre: tocando %s (heap livre %u)\n", mount.c_str(), ESP.getFreeHeap());
  auto* s = h.getStreamPtr();
  uint8_t buf[1024];
  unsigned long last = millis();
  while (cli.connected() && !atcSrv.hasClient()) {
    int n = s->available();
    if (n > 0) {
      n = s->read(buf, min(n, (int)sizeof buf));
      if (n > 0) { cli.write(buf, n); last = millis(); }
    } else if (!s->connected() || millis() - last > 15000) break;   // stream caiu: o navegador reconecta
    else delay(10);
  }
  h.end();
  Serial.println("Torre: parou");
}

void atcRelayTask(void*) {
  for (;;) {
    WiFiClient cli = atcSrv.accept();
    if (!cli) { delay(50); continue; }
    cli.setTimeout(2);
    String req = cli.readStringUntil('\n');   // "GET /cyul_twr HTTP/1.1"
    while (cli.connected()) { String hl = cli.readStringUntil('\n'); if (hl.length() <= 1) break; }
    int a = req.indexOf('/'), b = req.indexOf(' ', a);
    String mount = a < 0 || b < 0 ? "" : req.substring(a + 1, b);
    bool valid = mount.length() >= 3 && mount.length() <= 24;
    for (char c : mount) if (!isalnum((unsigned char)c) && c != '_') valid = false;
    if (valid) relayAtc(cli, mount);
    else cli.print("HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n");
    cli.stop();
  }
}

// ---------- alertas: configuracao e historico
void sendAlertLog() {
  JsonDocument d; JsonArray arr = d.to<JsonArray>();
  for (int i = 0; i < alogN; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["cs"] = alog[i].cs; o["k"] = String(alog[i].kind); o["km"] = serialized(String(alog[i].km, 1)); o["alt"] = alog[i].alt;
    o["ago"] = (millis() - alog[i].at) / 1000; if (alog[i].sq[0]) o["sq"] = alog[i].sq;
  }
  String out; serializeJson(d, out);
  server.send(200, "application/json", out);
}
void alertCfg() {   // POST m (0-2), r (0-2), c (0-2), e (0/1), n (0/1): cada um opcional
  if (server.hasArg("m")) { alertMode = constrain(server.arg("m").toInt(), 0, 2); if (alertMode) lastMode = alertMode; }
  if (server.hasArg("r")) radIdx = constrain(server.arg("r").toInt(), 0, 2);
  if (server.hasArg("c")) ceilIdx = constrain(server.arg("c").toInt(), 0, 2);
  if (server.hasArg("e")) alertEm = server.arg("e") == "1";
  if (server.hasArg("n")) alertNew = server.arg("n") == "1";
  savePrefs();
  for (auto& m : mem) m.inside = false;
  checkAlerts();
  sendPlanes();
}

// ---------- METAR do aeroporto (aviationweather.gov: o navegador nao pode ler direto, sem CORS). Cache de 10 min.
String metarJson, metarIcao;
unsigned long metarAt = 0, metarTry = 0;
void sendMetar() {
  String icao = server.arg("icao"); icao.toUpperCase();
  bool ok = icao.length() == 4;
  for (char c : icao) if (!isalnum((unsigned char)c)) ok = false;
  if (!ok) { server.send(400, "application/json", "{}"); return; }
  bool stale = icao != metarIcao || !metarAt || millis() - metarAt > 600000;
  if (stale && (icao != metarIcao || !metarTry || millis() - metarTry > 60000)) {
    metarTry = millis();
    JsonDocument filter; JsonObject f = filter[0].to<JsonObject>();
    for (const char* k : {"icaoId", "name", "rawOb", "obsTime", "temp", "dewp", "wdir", "wspd", "wgst", "visib", "altim", "fltCat", "wxString"}) f[k] = true;
    f["clouds"][0]["cover"] = true; f["clouds"][0]["base"] = true;
    WiFiClientSecure tls; tls.setInsecure();
    HTTPClient http; http.setTimeout(8000); http.useHTTP10(true);
    if (http.begin(tls, "https://aviationweather.gov/api/data/metar?format=json&ids=" + icao)) {
      http.setUserAgent("radar-de-mesa/1.0 (ESP32)");
      if (http.GET() == 200) {
        JsonDocument d;
        if (!deserializeJson(d, http.getStream(), DeserializationOption::Filter(filter)) && d.size()) {
          metarJson = ""; serializeJson(d[0], metarJson); metarIcao = icao; metarAt = millis();
        }
      }
      http.end();
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", icao == metarIcao && metarJson.length() ? metarJson : String("{}"));
}

// ---------- diagnostico (Reglages > Appareil > Diagnostic)
void sendDiag() {
  JsonDocument d;
  d["ver"] = FW_VERSION; d["up"] = millis() / 1000; d["reset"] = (int)esp_reset_reason();
  d["heap"] = ESP.getFreeHeap(); d["heapMin"] = ESP.getMinFreeHeap(); d["block"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  if (ESP.getPsramSize()) d["psram"] = ESP.getFreePsram(); else d["psram"] = -1;   // -1 = placa sem PSRAM
  d["sketch"] = ESP.getSketchSize(); d["slot"] = ESP.getFreeSketchSpace();
  d["ssid"] = WiFi.SSID(); d["rssi"] = WiFi.RSSI(); d["ip"] = (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString(); d["ap"] = apMode;
  d["pollMs"] = pollMs; d["polls"] = polls; d["fails"] = pollFails; d["http"] = lastHttp; d["lastFail"] = lastFail; d["qKm"] = queryKm;
  d["planes"] = nPlanes; d["age"] = dataAt ? (millis() - dataAt) / 1000 : -1;
  String out; serializeJson(d, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

// ---------- atualizacao pelo Wi-Fi (OTA): a pagina envia o .bin; o Update confere o arquivo antes de trocar
void otaDone() {
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", ok ? String("{\"ok\":true}") : "{\"ok\":false,\"err\":\"" + String(Update.errorString()) + "\"}");
  if (ok) { delay(800); ESP.restart(); }
}
void otaUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) { Serial.printf("OTA: %s\n", u.filename.c_str()); if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial); }
  else if (u.status == UPLOAD_FILE_WRITE) { if (Update.isRunning() && Update.write(u.buf, u.currentSize) != u.currentSize) Update.printError(Serial); }
  else if (u.status == UPLOAD_FILE_END) { if (Update.end(true)) Serial.printf("OTA ok: %u bytes\n", u.totalSize); else Update.printError(Serial); }
  else if (u.status == UPLOAD_FILE_ABORTED) { Update.abort(); Serial.println("OTA cancelada"); }
}

void pollTask(void*) {
  for (;;) {
    unsigned long now = millis();
    // pollNow (zoom maior, posicao nova) adianta a consulta, mas nunca menos de 5 s depois da anterior (limite da adsb.lol)
    if (WiFi.status() == WL_CONNECTED && ((pollNow && now - lastPoll > 5000) || now - lastPoll > POLL_MS || !lastPoll) && (long)(now - backoffUntil) >= 0) {
      pollNow = false; lastPoll = now; poll();
    }
    delay(200);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nSENTINEL 643 - " FW_VERSION);
  planesMux = xSemaphoreCreateMutex();
  prefs.begin("radar", false);
  soundOn = prefs.getBool("sound", true);
  if (prefs.isKey("zone") && !prefs.isKey("aRad")) {   // versao antiga: zona unica e alerta liga/desliga
    radIdx = ceilIdx = constrain(prefs.getInt("zone", 1), 0, 2); alertMode = prefs.getBool("alert", true) ? 1 : 0;
    savePrefs(); prefs.remove("zone"); prefs.remove("alert");
  }
  alertMode = constrain(prefs.getInt("aMode", alertMode), 0, 2); lastMode = constrain(prefs.getInt("aLast", 1), 1, 2);
  radIdx = constrain(prefs.getInt("aRad", radIdx), 0, 2); ceilIdx = constrain(prefs.getInt("aCeil", ceilIdx), 0, 2);
  alertEm = prefs.getBool("aEm", true); alertNew = prefs.getBool("aNew", true);
  pinMode(LED, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  loadNets();
  if (prefs.isKey("lat")) {
    homeLat = prefs.getFloat("lat"); homeLon = prefs.getFloat("lon"); homeCity = prefs.getString("city", "");
    locSrc = prefs.getString("src", "?"); locAcc = prefs.getInt("acc", 0);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("radar");
  WiFi.setAutoReconnect(true);
  if (connectWifi(15)) Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  else startSetupAP();
  if (MDNS.begin("radar")) { MDNS.addService("http", "tcp", 80); Serial.println("Abra: http://radar.local"); }
  // primeira vez num lugar novo: tenta pelas redes Wi-Fi vizinhas (mais preciso), senao pelo IP
  if (homeLat == 0 && homeLon == 0 && WiFi.status() == WL_CONNECTED && !locateWifi()) locateIp();
  Serial.printf("Casa: %s (%.4f, %.4f) via %s\n", homeCity.c_str(), homeLat, homeLon, locSrc.c_str());

  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/planes", HTTP_GET, sendPlanes);
  server.on("/alertcfg", HTTP_POST, alertCfg);
  server.on("/alerts", HTTP_GET, sendAlertLog);
  server.on("/metar", HTTP_GET, sendMetar);
  server.on("/diag", HTTP_GET, sendDiag);
  server.on("/update", HTTP_POST, otaDone, otaUpload);
  server.on("/sound", HTTP_POST, [] { soundOn = !soundOn; savePrefs(); sendPlanes(); });
  server.on("/info", HTTP_GET, sendInfo);
  server.on("/beep", HTTP_POST, [] { beep(3); server.send(204); });
  server.on("/wifi/scan", HTTP_GET, sendWifiScan);
  server.on("/wifi/list", HTTP_GET, sendWifiList);
  server.on("/wifi/forget", HTTP_POST, [] { forgetNet(server.arg("ssid")); sendWifiList(); });
  server.on("/wifi/save", HTTP_POST, [] {
    addNet(server.arg("ssid"), server.arg("pass"));
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
  server.on("/atc/feeds", HTTP_GET, sendAtcFeeds);
  if (soundOn) beep(1);  // bipe de "liguei"
  server.begin();
  atcSrv.begin();
  xTaskCreatePinnedToCore(atcRelayTask, "atcRelay", 12288, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(pollTask, "adsb", 12288, NULL, 1, NULL, 1);
}

void loop() {
  if (apMode) dns.processNextRequest();
  server.handleClient();
  unsigned long now = millis();

  static bool btnPrev = HIGH;
  static unsigned long btnT = 0;
  bool btn = digitalRead(BOOT_BTN);
  if (btn != btnPrev && now - btnT > 40) {
    btnT = now;
    if (btn == LOW) { alertMode = alertMode ? 0 : lastMode; savePrefs(); Serial.printf("Alertas %s\n", alertMode ? "ligados" : "desligados"); }
    btnPrev = btn;
  }
  if (now - lastCheck > 1000) { lastCheck = now; checkAlerts(); wifiWatch(); }
  digitalWrite(LED, alertMode && alerting > 0 && (now / 250) % 2);

  if (beepsLeft > 0 && (long)(now - nextBeep) >= 0) {   // bipes sem travar o loop
    tone(BUZZER, 2000, 150);
    beepsLeft--; nextBeep = now + 300;
  }
}
