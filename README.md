# ✈️ FlightRadar — Radar de Mesa com ESP32

Um radar de aviões de verdade para a mesa: um **ESP32** busca sozinho os aviões que estão voando perto de você, mostra tudo numa tela no estilo do **monitor de um controlador de tráfego aéreo**, apita quando um avião passa baixo e perto da sua casa e ainda toca o **rádio da torre de controle** do aeroporto mais próximo.

O projeto tem duas partes:

| Parte | Pasta | O que faz |
|---|---|---|
| **Radar de mesa** (o aparelho) | [`radar_esp/`](radar_esp) | Firmware do ESP32: busca os aviões, calcula o alerta de aproximação e serve a tela do radar em `http://radar.local`. |
| **Estação no PC** (opcional) | [`radar/`](radar) | `server.py` + um radar **3D estilo Flightradar24** (Three.js) com tráfego mundial, clima e rotas. |

> A interface do aparelho está em **francês** (feito em Québec 🇨🇦). Este README está em português.

---

## Sumário

- [O que ele faz](#o-que-ele-faz)
- [Hardware](#hardware)
- [Como funciona](#como-funciona)
- [Instalação do radar de mesa (ESP32)](#instalação-do-radar-de-mesa-esp32)
- [Primeira configuração: Wi-Fi e posição](#primeira-configuração-wi-fi-e-posição)
- [Usando o radar](#usando-o-radar)
- [Menu de configurações](#menu-de-configurações-réglages)
- [Alerta de aproximação](#alerta-de-aproximação)
- [Estação no PC: radar 3D e áudio da torre](#estação-no-pc-radar-3d-e-áudio-da-torre)
- [Fontes de dados e limites](#fontes-de-dados-e-limites)
- [API do aparelho](#api-do-aparelho)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Aviso legal sobre o rádio da torre](#aviso-legal-sobre-o-rádio-da-torre)
- [Próximos passos](#próximos-passos)
- [Contribuições e melhorias](#contribuições-e-melhorias)
- [Créditos](#créditos)
- [Licença](#licença)

---

## O que ele faz

### No aparelho (ESP32)
- 🛰️ **Tráfego ao vivo** num raio de ~200 km, atualizado a cada 15 s e com a posição de cada avião **estimada** entre as leituras (rumo + velocidade), para os alvos andarem suavemente.
- 🖥️ **Dois estilos de tela redonda** (466×466, feita para a Waveshare AMOLED 1,75"):
  - **Contrôleur** (padrão): como o scope de um controlador — alvo quadrado, **rastro** das posições anteriores, **vetor** de 1 minuto, **etiqueta de dados** de 3 linhas, **pistas reais** dos aeroportos, anéis em milhas náuticas, rosa dos ventos em graus, hora UTC e varredura cinza.
  - **Classique**: radar verde com varredura; o nome do voo aparece quando a varredura passa e some em seguida.
- 📋 **Toque num avião** → cartão com **companhia aérea**, **modelo**, **origem → destino**, altitude, velocidade e distância.
- 🔔 **Alerta de aproximação** com zonas configuráveis, posição conferida a cada segundo e memória para não repetir (detalhes [abaixo](#alerta-de-aproximação)). Borda da tela pisca em vermelho, LED da placa pisca e o buzzer apita.
- 📻 **Rádio da torre de controle** do aeroporto mais próximo (o próprio ESP32 repassa o áudio, sem PC), com uma **onda de fala** no radar que só aparece enquanto alguém está falando.
- 📶 **Configuração de Wi-Fi pela própria tela** e **modo de configuração** automático (para levar o aparelho a qualquer lugar).
- 📍 **Localização sem GPS**: pelas redes Wi-Fi vizinhas, pelo IP ou pela cidade digitada.
- 🗺️ Mapa escuro de fundo, aviões no solo (opcional), km ou milhas, tela redonda ou retangular (CYD 2,8").
- 💾 Som, alerta, zona, Wi-Fi e posição ficam **salvos na flash** do ESP32.

### No PC (opcional)
- 🌍 Globo 3D com **todo o tráfego mundial** (OpenSky) e visão local 3D com mapa detalhado.
- 🔎 Busca por voo, matrícula, hex ou **modelo de avião no mundo inteiro** (ex.: todos os A380 no ar).
- 🌦️ Clima atual, **METAR** do aeroporto mais próximo e radar de chuva.
- 🧭 Rota do voo desenhada no globo, foto do avião e simulador das telas do aparelho.
- 🎧 **Toca o áudio da torre** nas caixas de som, sem abrir navegador, e descobre sozinho os canais da LiveATC dos aeroportos perto de onde o aparelho está (YUL em Montreal, GRU em São Paulo…).

---

## Hardware

| Peça | Situação | Observação |
|---|---|---|
| **ESP32 clássico** (ESP32-WROOM-32, ex.: ELEGOO) | ✅ testado | É o aparelho atual: busca os dados e serve a tela pela rede. |
| **Waveshare ESP32-S3-Touch-AMOLED-1.75** | 🛒 próximo passo | Tela redonda 466×466 (CO5300 QSPI), toque CST9217, áudio ES8311 + alto-falante, bateria. A tela redonda do projeto já foi desenhada para ela. **Evite a versão "-B"** (vem com capa e não cabe nos cases). |
| Cheap Yellow Display 2,8" (ESP32-2432S028R) | alternativa | Tela retangular 320×240 já suportada no modo *Rectangulaire*. |
| Buzzer **passivo** | opcional | Entre o **GPIO25** e o **GND**. A placa ESP32 não tem alto-falante. |
| LED azul da placa (GPIO2) | já vem na placa | Pisca durante o alerta. |
| Botão **BOOT** (GPIO0) | já vem na placa | Liga/desliga o alerta. |

Para o rádio da torre com o **seu próprio receptor** (em vez da LiveATC), veja [Próximos passos](#próximos-passos).

---

## Como funciona

```
                 internet (APIs públicas, sem chave)
   adsb.lol ─ adsbdb ─ ip-api ─ BeaconDB ─ OSM Nominatim ─ OurAirports ─ Esri
        │         │                                              │         │
        ▼         ▼                                              │         │
  ┌──────────────────────────┐  JSON   ┌───────────────────────────────────────┐
  │  ESP32  (radar_esp.ino)  │ ──────▶ │  Tela do radar (page.h, Canvas)       │
  │  • busca aviões / 15 s   │         │  aberta em http://radar.local         │
  │  • alerta a cada 1 s     │ ◀────── │  no celular, PC ou (futuro) na própria│
  │  • Wi-Fi / posição       │  toques │  tela AMOLED do aparelho              │
  │  • LED + buzzer          │ ──────▶ │  • toca o áudio da torre              │
  │  • canais LiveATC +      │  MP3    │  • detecta fala → onda no radar       │
  │    repasse do áudio (:81)│         └───────────────────────────────────────┘
  └──────────────────────────┘
                                     ┌───────────────────────────────────────┐
                                     │  PC (opcional): radar/server.py       │
                                     │  • radar 3D, tráfego mundial, clima   │
                                     │  • áudio da torre nas caixas do PC    │
                                     │  • radar 3D em http://localhost:8080  │
                                     └───────────────────────────────────────┘
```

A tela é desenhada só com primitivas que existem nas bibliotecas gráficas do ESP32 (`fillRect`, `drawLine`, `fillArc`, `fillTriangle`, texto…), para ser portada para a tela física com o mínimo de mudança.

---

## Instalação do radar de mesa (ESP32)

### 1. Ferramentas
- [Arduino IDE 2](https://www.arduino.cc/en/software) **ou** [`arduino-cli`](https://arduino.github.io/arduino-cli/).
- Core **esp32** da Espressif (Gerenciador de Placas → "esp32").
- Biblioteca **ArduinoJson 7.x** (Benoit Blanchon).

Com `arduino-cli`:
```bash
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson
```

### 2. Configuração pessoal (opcional)
O aparelho funciona **sem** nenhum arquivo de configuração: na primeira vez ele abre o modo de configuração de Wi-Fi (veja abaixo). Se preferir já gravar com o seu Wi-Fi:

```bash
cp radar_esp/secrets.example.h radar_esp/secrets.h
```
e preencha:

```c
#define WIFI_SSID "nome-da-sua-rede"
#define WIFI_PASS "senha-da-sua-rede"
#define HOME_LAT  0.0        // 0 e 0 = descobre a posição sozinho
#define HOME_LON  0.0
#define HOME_CITY "casa"
```

> 🔒 `secrets.h` está no `.gitignore`: a senha do seu Wi-Fi nunca vai para o repositório.

### 3. Gravar
```bash
arduino-cli compile --upload -p COM3 --fqbn esp32:esp32:esp32 radar_esp
```
(troque `COM3` pela porta da sua placa; no Linux/Mac algo como `/dev/ttyUSB0`). Na Arduino IDE: placa **ESP32 Dev Module**, abra `radar_esp/radar_esp.ino` e clique em *Carregar*.

O firmware ocupa ~90% da partição padrão de 1,3 MB. Se você aumentar o código, escolha o esquema de partição **"Huge APP (3MB No OTA)"**.

### 4. Abrir o radar
No celular ou PC **na mesma rede**: **http://radar.local** (ou o IP que aparece no monitor serial, 115200 baud).

---

## Primeira configuração: Wi-Fi e posição

### Wi-Fi (para levar o radar para outro lugar)
1. Se o aparelho não conhece nenhuma rede (ou a senha mudou), depois de ~15 s ele cria a rede **`Radar-XXXX`** (sem senha).
2. Conecte o celular nela. A página do radar abre sozinha (portal cativo) ou acesse **http://192.168.4.1**.
3. A lista de redes aparece: escolha a sua, digite a senha e toque em **Connecter**.
4. O radar reinicia e entra na rede nova. Volte o celular para essa rede e abra **http://radar.local**.

Para trocar de rede depois: **⚙ → Appareil → Réseau Wi-Fi**.

### Posição (o aparelho não tem GPS)
O centro do radar, o mapa, as pistas e o aeroporto do rádio dependem da posição. Ela é descoberta assim, nesta ordem:

| Método | Precisão | Como |
|---|---|---|
| **Redes Wi-Fi vizinhas** | dezenas de metros | O ESP32 lista as redes que enxerga e pergunta ao [BeaconDB](https://beacondb.net) (base comunitária, sem chave). Só os endereços MAC e a força do sinal são enviados — nada de nomes ou senhas; redes terminadas em `_nomap` são ignoradas. **Depende de a sua região estar mapeada.** |
| **IP** | cidade (≈ 5–30 km) | [ip-api.com](https://ip-api.com). Reserva automática. |
| **Cidade digitada** | o centro da cidade | **⚙ → Appareil → Position**, busca pelo [Open-Meteo Geocoding](https://open-meteo.com/en/docs/geocoding-api). |

> **E pelo Bluetooth/GPS do celular?** Navegadores só liberam GPS e Bluetooth para sites **HTTPS**, e o aparelho serve a página em HTTP na rede local. Por isso a posição vem do Wi-Fi/IP/cidade. Um app nativo de celular poderia fazer isso no futuro.

---

## Usando o radar

| Gesto | Ação |
|---|---|
| Tocar num avião | Seleciona e abre o cartão (companhia, modelo, rota, altitude, velocidade, distância) |
| Tocar fora de um avião | Desmarca |
| **−** / **+** nas laterais | Alcance: **5 · 10 · 25 · 50 · 100 NM** (Contrôleur) ou **10 · 25 · 50 · 100 · 200 km** (Classique) |
| **⚙** embaixo (6 horas) | Abre as configurações |

### Etiqueta de dados (estilo Contrôleur)
```
ACA432        ← indicativo do voo
118↓37        ← nível 11.800 ft, descendo (↑ subindo), 370 nós
A320 YYZ      ← tipo do avião + destino (quando já carregado)
```
- Quadrado **branco** = alvo; **amarelo** = selecionado; **vermelho piscando** = em alerta; **cinza** = no solo.
- Pontos azuis atrás do alvo = rastro; linha à frente = onde o avião estará em 1 minuto.
- As etiquetas escolhem sozinhas o lado (NE/NO/SE/SO) que menos cobre as outras.

---

## Menu de configurações (Réglages)

### Aba **Radar**
| Item | Valores | O que faz |
|---|---|---|
| Son | ligado/desligado | Bipe do alerta (página e buzzer). **Ligado por padrão.** |
| Alerte de proximité | ligado/desligado | Liga/desliga o alerta. **Ligado por padrão.** |
| Zone d'alerte | 2 km · 3.000 pés / **5 km · 5.000 pés** / 10 km · 10.000 pés | Tamanho da zona de alerta. |
| Carte de fond | ligado/desligado | Mapa escuro no fundo. |
| Avions au sol | ligado/desligado | Mostra os aviões no chão (cinza, etiqueta "SOL"). |
| Tour de contrôle | canal tocando ou *Arrêt* | Abre a **janela de canais** da torre (veja [Áudio da torre](#áudio-da-torre)). |
| Style | Contrôleur / Classique | Estilo da tela redonda. |
| Unités | km / mi | Unidades do cartão e do alcance clássico. |
| Écran | Rond / Rectangulaire | Tela redonda (AMOLED) ou retangular (CYD). |
| Tester le bip | ▶ | Bipe de teste na página e no buzzer. |

### Aba **Appareil**
| Item | O que mostra/faz |
|---|---|
| Réseau Wi-Fi | Rede atual. Toque para ver as redes disponíveis e trocar (com senha). |
| Position | Cidade e origem da posição (Wi-Fi, IP, manual). Toque para detectar de novo ou escolher a cidade. |
| Précision | Precisão estimada da posição. |
| Aéroport (radio) | Aeroporto mais próximo com rádio na LiveATC. |
| Canaux de la tour | Quantos canais de rádio foram encontrados perto. |

---

## Alerta de aproximação

O alerta foi feito para tocar **só quando um avião passa perto e baixo de verdade** — morar a ~15 km de um aeroporto grande disparava o tempo todo com regras simples.

- **Zona**: raio horizontal + teto de altitude (padrão **5 km abaixo de 5.000 pés**).
- **Posição conferida a cada 1 s** com a posição **estimada agora** (rumo + velocidade desde a última leitura), não com o dado de até 15 s atrás.
- **Uma vez por avião**: depois de alertar, o mesmo avião só alerta de novo **20 minutos** depois.
- **Histerese**: para "sair" da zona o avião precisa passar de **125 % do raio** (ou 300 pés acima do teto) — evita liga-desliga na borda.
- Aviões no solo nunca alertam.
- Ao alertar: 3 bipes no buzzer, LED piscando, borda vermelha na tela e etiqueta do avião piscando em vermelho.

---

## Estação no PC: radar 3D e áudio da torre

### Rodar
```bash
cd radar
pip install -r requirements.txt     # só para o áudio da torre (miniaudio)
python server.py
```
Abra **http://localhost:8080**. Requer Python 3.10+.

### Conta OpenSky (opcional)
Sem conta, o tráfego **mundial** atualiza a cada 15 min (limite anônimo). Com uma conta gratuita da [OpenSky Network](https://opensky-network.org) (API client), atualiza a cada **2 min**:
```bash
cp radar/opensky_credentials.example.json radar/opensky_credentials.json
# preencha clientId e clientSecret
```
> 🔒 `opensky_credentials.json` está no `.gitignore`. O segredo fica só no PC — nunca vai para a página nem para o ESP32.

### Áudio da torre
Não precisa do PC: o próprio ESP32 cuida disso, e o som sai **no aparelho que abriu a página** (PC, celular ou, no futuro, a tela redonda com alto-falante).
1. No radar: **⚙ → Radar → Tour de contrôle**. Abre uma janela com os canais **agrupados por aeroporto**, do mais perto para o mais longe:
   - toque num canal para ouvir (ele fica em verde com 🔊);
   - toque nele de novo, ou em **■ Arrêter le son**, para parar.
2. O ESP32:
   - recebe da página os **6 aeroportos mais perto (até 100 km)** da posição do aparelho ([OurAirports](https://ourairports.com));
   - descobre quais canais existem na LiveATC pelos links `.pls` oficiais (leva ~1 min por aeroporto na primeira vez; o resultado fica salvo na flash);
   - repassa o stream MP3 (16 kbps) em `http://radar.local:81/<canal>`, um ouvinte por vez.
3. A página toca o áudio, mede o volume e mostra a **onda de fala** ("TRANSMISSION") embaixo do radar quando **alguém está falando**; se o stream cair, reconecta em 5 s.

O `server.py` continua tocando a torre nas caixas do PC para o radar 3D (`/api/atc/…`, com [miniaudio](https://github.com/irmen/pyminiaudio)).

> Silêncio no canal é normal: a frequência só tem som quando um piloto ou o controlador fala. De madrugada pode ficar minutos sem nada.
>
> O radar mostra **que** há uma transmissão, mas **não** distingue se é a torre ou o piloto (os dois chegam misturados no mesmo canal).

---

## Fontes de dados e limites

| Dado | Fonte | Chave? | Limites |
|---|---|---|---|
| Aviões perto de casa | [adsb.lol](https://adsb.lol) | não | O ESP32 consulta a cada 15 s; se receber "429" espera 1 min. Exige um `User-Agent` próprio. |
| Modelo, companhia, rota | [adsbdb](https://www.adsbdb.com) | não | A rota é a **cadastrada para o número do voo**, nem sempre a real do dia. |
| Tráfego mundial (PC) | [OpenSky Network](https://opensky-network.org) | opcional | 15 min anônimo / 2 min com conta. |
| Pistas dos aeroportos | [OurAirports](https://ourairports.com/data/) | não | Pistas ≥ 1.200 m num raio de 200 km. |
| Mapa de fundo | Esri World Dark Gray | não | — |
| Posição | BeaconDB · ip-api · Nominatim · Open-Meteo | não | Precisão do Wi-Fi depende da cobertura da região. |
| Clima, METAR, chuva (PC) | Open-Meteo · NOAA AWC · RainViewer | não | — |
| Rádio da torre (PC) | [LiveATC.net](https://www.liveatc.net) | não | Veja o aviso legal abaixo. |

---

## API do aparelho

Endpoints HTTP servidos pelo ESP32 (úteis para integrar com outras coisas):

| Método | Rota | Descrição |
|---|---|---|
| GET | `/` | Tela do radar |
| GET | `/planes` | Aviões, posição da casa, estado do alerta/Wi-Fi |
| GET | `/info?hex=…&cs=…` | Modelo, companhia, origem e destino de um avião |
| POST | `/sound` · `/alert` · `/zone` | Alterna som / alerta / zona de alerta |
| POST | `/beep` | Bipe de teste |
| GET | `/wifi/scan` | Redes Wi-Fi visíveis |
| POST | `/wifi/save` (`ssid`, `pass`) | Salva a rede e reinicia |
| POST | `/locate` (`method=wifi\|ip`) | Detecta a posição |
| POST | `/location` (`lat`, `lon`, `city`) | Define a posição manualmente |
| GET | `/atc/feeds?icao=CYUL,CYHU` | Canais da LiveATC desses aeroportos (`pending` = descoberta em andamento) |
| GET | `:81/<canal>` | Áudio da torre (MP3, CORS liberado) |

E pelo `server.py` (PC): `/api/atc/feeds?lat&lon`, `/api/atc/start?feed=`, `/api/atc/stop`, `/api/atc/level`, além das rotas do radar 3D (`/api/local`, `/api/global`, `/api/route/…`, `/api/aircraft/…`, `/api/metar`, `/api/wx`…).

---

## Estrutura do repositório

```
FlightRadar/
├── radar_esp/                     # firmware do radar de mesa (ESP32)
│   ├── radar_esp.ino              # dados, alerta, Wi-Fi, posição, API
│   ├── page.h                     # tela do radar (HTML + Canvas), em francês
│   └── secrets.example.h          # modelo de configuração pessoal (copie para secrets.h)
└── radar/                         # estação no PC
    ├── server.py                  # proxy/cache das APIs + áudio da torre
    ├── index.html · app.js        # radar 3D (Three.js)
    ├── device.js                  # simulador da tela retangular 320x240
    ├── device_round.js            # simulador da tela redonda 466x466
    ├── requirements.txt
    └── opensky_credentials.example.json
```

---

## Aviso legal sobre o rádio da torre

- **Ouvir** comunicações aeronáuticas é tolerado no Canadá e em muitos países, mas **divulgar ou usar o conteúdo** pode ser proibido (no Canadá, *Loi sur la radiocommunication*, art. 9). Regras variam por país — confira as do seu.
- O áudio vem da **[LiveATC.net](https://www.liveatc.net)**, cujos termos de uso (no rodapé do site) dizem que os streams **não podem ser usados em produtos de terceiros**. Este projeto acessa o stream pelo mesmo caminho que o site oferece para players de mídia, **apenas para escuta pessoal**, sem gravar nem retransmitir. **Antes de vender ou distribuir aparelhos com essa função, peça autorização à LiveATC** ou use o seu próprio receptor (veja abaixo).

---

## Próximos passos

- [ ] Firmware nativo para a **Waveshare AMOLED 1,75"**: desenhar direto na tela (Arduino_GFX/LVGL), toque, alto-falante ES8311 e bateria — sem depender de navegador.
- [ ] **Case impresso em 3D** (OpenSCAD) para a tela redonda.
- [ ] Teclado na própria tela para configurar o Wi-Fi sem celular.
- [ ] **Receptor próprio** (RTL-SDR + [RTLSDR-Airband](https://github.com/rtl-airband/RTLSDR-Airband) → Icecast) para o áudio da torre sem depender da LiveATC.
- [ ] Transcrição das falas (Whisper) para identificar **torre × piloto** e ligar cada fala ao avião certo no radar.

---

## Contribuições e melhorias

Este é um projeto aberto e **melhorias são muito bem-vindas**! Algumas formas de ajudar:

- 🐛 **Encontrou um problema?** Abra uma [issue](https://github.com/maikonrenner/FlightRadar/issues) descrevendo o que aconteceu, a placa usada e, se possível, o log do monitor serial (115200 baud).
- 💡 **Tem uma ideia?** Sugestões de recursos também entram como issue — principalmente as da lista de [Próximos passos](#próximos-passos).
- 🔧 **Quer mudar o código?** Faça um *fork*, crie um branch (`git checkout -b minha-melhoria`), teste na placa e abra um *pull request* explicando o que mudou e como testou.
- 🌍 **Mapeie a sua região no BeaconDB** (ex.: app [NeoStumbler](https://github.com/mjaakko/NeoStumbler) no Android): melhora a localização por Wi-Fi do radar — e de todo mundo.
- 🌐 **Traduções**: a tela está em francês; versões em português e inglês seriam ótimas.

Ao contribuir:
- **nunca** inclua senhas, chaves ou a posição da sua casa (`secrets.h` e `opensky_credentials.json` ficam fora do Git);
- mantenha a tela desenhada só com primitivas simples (linhas, círculos, triângulos, texto), para continuar portável para as telas do ESP32;
- teste a página com `node --check` antes de gravar (um erro de JavaScript deixa a tela em branco).

## Créditos

- Inspirado no **[Desk Plane Radar](https://makerworld.com/fr/models/3090203-desk-plane-radar-esp32-with-2-screens)** de Noah Thio (MakerWorld) e no **Capsule Radar**. Nenhum código desses projetos foi copiado.
- Dados: [adsb.lol](https://adsb.lol), [adsbdb](https://www.adsbdb.com), [OpenSky Network](https://opensky-network.org), [OurAirports](https://ourairports.com), [BeaconDB](https://beacondb.net), [OpenStreetMap/Nominatim](https://nominatim.org), [Open-Meteo](https://open-meteo.com), [NOAA Aviation Weather Center](https://aviationweather.gov), [RainViewer](https://www.rainviewer.com), [LiveATC.net](https://www.liveatc.net), mapa © Esri.
- Bibliotecas: [ArduinoJson](https://arduinojson.org), [Three.js](https://threejs.org), [topojson-client](https://github.com/topojson/topojson-client), [miniaudio](https://github.com/irmen/pyminiaudio).

## Licença

Distribuído sob a **licença MIT** — veja o arquivo [`LICENSE`](LICENSE). Você pode usar, modificar e até vender, desde que mantenha o aviso de copyright.

> A licença cobre **o código deste repositório**. Os dados e serviços usados (adsb.lol, adsbdb, OpenSky, LiveATC, mapas etc.) têm **termos próprios** — em especial, o áudio da LiveATC exige autorização para uso comercial (veja o [aviso legal](#aviso-legal-sobre-o-rádio-da-torre)).
