// Pagina servida pelo ESP32 - SENTINEL 643 (textos em frances, portugues ou ingles: Reglages > Appareil > Langue).
// Tela redonda (Waveshare AMOLED 1,75", 466x466): estilo "Controleur" (scope de controle de trafego) ou "Classique".
// Tela retangular (CYD 2,8", 320x240) = copia de radar/device.js.
// Fonte editavel: este arquivo mesmo (o dicionario L10N e gerado a partir da lista de traducoes).
#pragma once
const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr-CA"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SENTINEL 643</title>
<style>
:root{color-scheme:dark}
html,body{margin:0;background:#04080c;color:#a7b3c0;font:13px system-ui,sans-serif}
main{min-height:100vh;display:grid;place-items:center;padding:16px;box-sizing:border-box}
canvas{display:block;touch-action:manipulation;cursor:pointer;-webkit-touch-callout:none;-webkit-user-select:none;user-select:none;transition:transform .3s}
canvas[hidden]{display:none}
#dlg{position:fixed;inset:0;background:rgba(0,0,0,.45);z-index:9}#dlg[hidden]{display:none}
#dlg .box{position:fixed;box-sizing:border-box;background:#0b131b;border:1px solid rgba(201,162,39,.25);overflow:auto;color:#e6edf3;font-size:12px;scrollbar-width:thin;transform-origin:50% 50%}
#dlg h3{margin:0 0 6px;color:#e0b53c;text-align:center;font-size:15px;letter-spacing:.04em}#dlg h4{margin:12px 0 4px;color:#9fb4c3;font-size:11px;letter-spacing:.06em;text-transform:uppercase}
#dlg p{margin:6px 0;font-size:12px}#dlg .dim{color:#7a8794}
#dlg ul{list-style:none;margin:4px 0;padding:0}#dlg li{display:flex;justify-content:space-between;gap:8px;padding:8px 6px;border-bottom:1px solid rgba(255,255,255,.07);cursor:pointer}
#dlg li:hover{background:rgba(255,255,255,.05)}#dlg li.kv{cursor:default}#dlg li.kv:hover{background:none}#dlg li.on{background:rgba(98,245,154,.12);color:#62f59a;font-weight:600}#dlg ul+ul{margin-top:0}
#dlg input{width:100%;box-sizing:border-box;background:#04080c;color:#e6edf3;border:1px solid rgba(255,255,255,.15);border-radius:8px;padding:10px;font:14px system-ui;margin:4px 0}
#dlg button{font:600 13px system-ui;color:#e6edf3;background:#18212c;border:1px solid rgba(255,255,255,.1);border-radius:8px;padding:9px 12px;cursor:pointer;margin-top:6px}#dlg button.go{background:#e0b53c;color:#111;border-color:#e0b53c}
#dlg .row{display:flex;gap:8px;align-items:center}#dlg .row input{flex:1}#dlg .row button{margin-top:6px}
#dlg .chips{display:flex;flex-wrap:wrap;gap:5px;margin:4px 0}#dlg .chips button{margin:0;padding:7px 10px;font-size:12px;border-radius:16px}#dlg .chips button.on{background:#62f59a;color:#06120b;border-color:#62f59a}
#dlg .note{background:rgba(98,245,154,.07);border:1px solid rgba(98,245,154,.2);border-radius:8px;padding:8px}
#dlg .bar{height:10px;background:#18212c;border-radius:5px;overflow:hidden;margin:8px 0}#dlg .bar div{height:100%;width:0;background:#62f59a;transition:width .2s}
#dlg pre{white-space:pre-wrap;word-break:break-word;font:11px ui-monospace,Consolas,monospace;color:#9fb4c3;margin:6px 0}
#dlg .vk{margin:8px 0 4px;user-select:none;-webkit-user-select:none}#dlg .vk div{display:flex;gap:3px;margin-bottom:3px}
#dlg .vk button{flex:1 1 0;min-width:0;margin:0;padding:9px 0;font:600 14px system-ui;border-radius:6px;touch-action:manipulation}
#dlg .vk button.fn{flex:1.5 1 0;font-size:12px}#dlg .vk button.sp{flex:4 1 0;font-size:11px}#dlg .vk button.on{background:#e0b53c;color:#111}
#round{width:min(calc(100vw - 60px),calc(100vh - 60px),520px);aspect-ratio:1;border-radius:50%;box-shadow:0 0 0 12px #161b21,0 0 0 14px #2a3139,0 0 0 15px rgba(201,162,39,.35)}
#rect{width:min(calc(100vw - 52px),640px);aspect-ratio:4/3;image-rendering:pixelated;border-radius:6px;box-shadow:0 0 0 10px #161b21}
</style></head><body><main>
<canvas id="round" width="466" height="466" aria-label="SENTINEL 643"></canvas>
<canvas id="rect" width="320" height="240" aria-label="SENTINEL 643" hidden></canvas>
</main>
<div id="dlg" hidden></div>
<script>
const $=id=>document.getElementById(id);
// ---------- idioma: frances (padrao), portugues ou ingles. A chave e o proprio texto em frances; {x} = valor
const L10N={"pt":{"ADS-B : dernière requête":"ADS-B: última consulta","ADS-B : requêtes / échecs":"ADS-B: consultas / falhas","ADS-B PERDU":"ADS-B PERDIDO","ALERTE":"ALERTA","AU SOL":"NO SOLO","Actuelle : <b>{c}</b> ({s}). Sans GPS, le radar se situe par les réseaux Wi-Fi voisins, par l’adresse IP ou par la ville choisie ici.":"Atual: <b>{c}</b> ({s}). Sem GPS, o radar se localiza pelas redes Wi-Fi vizinhas, pelo endereço IP ou pela cidade escolhida aqui.","Adresse":"Endereço","Alertes":"Alertas","Altitude maximale":"Altitude máxima","Appareil":"Aparelho","Arrêt":"Desligado","Arrêter le son":"Parar o som","Aucun canal pour cet aéroport.":"Nenhum canal para este aeroporto.","Aucun réseau trouvé.":"Nenhuma rede encontrada.","Aucun trafic":"Sem tráfego","Aucune":"Nenhum","Aucune alerte : le radar reste silencieux.":"Nenhum alerta: o radar fica em silêncio.","Aucune alerte depuis le démarrage.":"Nenhum alerta desde que ligou.","Aucune ville trouvée.":"Nenhuma cidade encontrada.","Automatique":"Automático","Autre aéroport (code OACI) :":"Outro aeroporto (código ICAO):","Autres":"Outros","Avions au sol":"Aviões no solo","Avions reçus":"Aviões recebidos","Aéroport":"Aeroporto","Aéroport (radio)":"Aeroporto (rádio)","Aéroport de la radio":"Aeroporto do rádio","Balayage":"Varredura","Bip court pour chaque nouvel avion.":"Bipe curto a cada avião novo.","CAP":"RUMO","CIE":"CIA","CONFIGURATION":"CONFIGURAÇÃO","Cap (route)":"Rumo (trajetória)","Catégorie":"Categoria","Catégorie de vol":"Categoria de voo","Catégories (aucune choisie = toutes)":"Categorias (nenhuma escolhida = todas)","Cet avion a quitté le radar.":"Este avião saiu do radar.","Chercher":"Buscar","Choisissez le fichier .bin du nouveau micrologiciel :":"Escolha o arquivo .bin do novo firmware:","Choisissez un canal pour l’écouter sur cet appareil.":"Escolha um canal para ouvir neste aparelho.","Choisissez un fichier .bin.":"Escolha um arquivo .bin.","Classique":"Clássico","Code OACI 24 bits":"Código ICAO 24 bits","Compagnie":"Companhia","Connecter":"Conectar","Connecté à <b>{n}</b>. Choisissez un autre réseau :":"Conectado a <b>{n}</b>. Escolha outra rede:","Connexion…":"Conectando…","Contrôleur":"Controlador","Courte":"Curto","DESTINATION":"DESTINO","DISTANCE":"DISTÂNCIA","DONNÉES ANCIENNES · {s} s":"DADOS ANTIGOS · {s} s","Dernier redémarrage":"Último reinício","Dernières alertes":"Últimos alertas","Diagnostic":"Diagnóstico","Distance":"Distância","DÉTOURNEMENT":"SEQUESTRO","Désactivées":"Desligados","Détecter par Wi-Fi":"Detectar pelo Wi-Fi","E":"L","ESP HORS LIGNE":"ESP FORA DO AR","ESP sans données adsb.lol":"ESP sem dados da adsb.lol","En marche depuis":"Ligado há","En écoute":"Ouvindo","Envoi… {p} %":"Enviando… {p} %","Envoyer":"Enviar","Faible":"Baixo","Fermer":"Fechar","Filtres":"Filtros","Fixes":"Fixas","Fort":"Alto","Grand":"Grande","Gros porteurs":"Widebody","Haute performance":"Alto desempenho","Hélicoptères":"Helicópteros","IMMAT":"MATR","INITIALISATION DU SYSTÈME":"INICIALIZAÇÃO DO SISTEMA","Immatriculation":"Matrícula","Inconnue":"Desconhecida","LIAISON AVEC L’ESP PERDUE":"LIGAÇÃO COM O ESP PERDIDA","LIAISON ESP32":"LIGAÇÃO ESP32","Langue":"Idioma","Le radar est en mode configuration (réseau <b>{n}</b>). Choisissez le Wi-Fi de la maison :":"O radar está no modo de configuração (rede <b>{n}</b>). Escolha o Wi-Fi da casa:","Le radar ne répond pas.":"O radar não responde.","Le radar redémarre et se connecte à <b>{n}</b>.":"O radar está reiniciando e vai se conectar a <b>{n}</b>.","Le radar se connecte au plus fort de ces réseaux et y revient tout seul quand il réapparaît.":"O radar se conecta à rede mais forte destas e volta sozinho quando ela reaparece.","Le silence est normal : on n’entend quelque chose que quand un pilote ou le contrôleur parle.":"O silêncio é normal: só se ouve algo quando um piloto ou o controlador fala.","Les réseaux d’ici ne sont pas encore cartographiés. Essayez par l’IP ou la ville.":"As redes daqui ainda não estão mapeadas. Tente pelo IP ou pela cidade.","Ligne":"Comerciais","Longue":"Longo","Légers":"Leves","METAR indisponible pour {a}.":"METAR indisponível para {a}.","Militaire":"Militar","Militaires seulement":"Só militares","Minimum atteint":"Mínimo atingido","Mise à jour":"Atualização","Mise à jour réussie. Le radar redémarre…":"Atualização concluída. O radar está reiniciando…","Mode":"Modo","Moyen":"Médio","Moyenne":"Médio","Moyens":"Médios","Mémoire libre":"Memória livre","Météo":"Meteorologia","Ne débranchez pas le radar pendant l’envoi.":"Não desligue o radar durante o envio.","Ne plus suivre":"Parar de seguir","Nouvel avion (mode Toutes)":"Avião novo (modo Todos)","Nuages":"Nuvens","ORIGINE":"ORIGEM","Observation {t}":"Observação {t}","Ou choisissez la ville :":"Ou escolha a cidade:","Oublier le réseau {n} ?":"Esquecer a rede {n}?","PANNE RADIO":"FALHA DE RÁDIO","Page : images/s":"Página: quadros/s","Par défaut, le radar écoute l’aéroport le plus proche qui a des canaux.":"Por padrão, o radar ouve o aeroporto mais próximo que tem canais.","Par l’IP":"Pelo IP","Phénomènes":"Fenômenos","Planeurs / ULM":"Planadores / ultraleves","Plus grand bloc libre":"Maior bloco livre","Position":"Posição","Position : {c}":"Posição: {c}","Position du radar":"Posição do radar","Pression (QNH)":"Pressão (QNH)","Programme":"Programa","Propriétaire":"Proprietário","Proximité : avions à moins de {r} km et sous {c} ft.":"Proximidade: aviões a menos de {r} km e abaixo de {c} ft.","Précision : ± {a}.":"Precisão: ± {a}.","Quel avion ?":"Qual avião?","RADAR · ADS-B · SURVEILLANCE AÉRIENNE":"RADAR · ADS-B · VIGILÂNCIA AÉREA","Rayon de proximité":"Raio de proximidade","Recherche (indicatif, immatriculation, type)":"Busca (indicativo, matrícula, tipo)","Recherche des canaux de cet aéroport…":"Procurando os canais deste aeroporto…","Recherche des réseaux…":"Procurando redes…","Recherche impossible (pas d’internet ?).":"Busca impossível (sem internet?).","Recherche…":"Procurando…","Relèvement":"Marcação","Remettez votre téléphone sur ce réseau et ouvrez <b>http://radar.local</b>. Si le mot de passe est faux, le radar revient au mode configuration (réseau {a}).":"Volte o celular para essa rede e abra <b>http://radar.local</b>. Se a senha estiver errada, o radar volta ao modo de configuração (rede {a}).","Rotation":"Rotação","Route":"Rota","Réglages":"Ajustes","Réseau":"Rede","Réseau Wi-Fi":"Rede Wi-Fi","Réseau ouvert : pas de mot de passe.":"Rede aberta: sem senha.","Réseaux":"Redes","Réseaux enregistrés (3 au maximum)":"Redes salvas (até 3)","SOL":"SOLO","SUIVI":"SEGUINDO","Son":"Som","Style":"Estilo","Suivre":"Seguir","Taux vertical":"Razão vertical","Température / point de rosée":"Temperatura / ponto de orvalho","Tester le bip":"Testar o bipe","Texte":"Texto","Tour de contrôle":"Torre de controle","Toutes":"Todos","Transpondeur":"Transponder","Traînée":"Rastro","URGENCE":"EMERGÊNCIA","Unités":"Unidades","Urgences (7500 / 7600 / 7700)":"Emergências (7500 / 7600 / 7700)","Urgences signalées.":"Emergências avisadas.","VIT":"VEL","VITESSE":"VELOCIDADE","VOL SÉLECTIONNÉ":"VOO SELECIONADO","Vecteur":"Vetor","Vent":"Vento","Version":"Versão","Version actuelle : {v}":"Versão atual: {v}","Visibilité":"Visibilidade","Vitesse sol":"Velocidade solo","Volume des bips (cet appareil)":"Volume dos bipes (este aparelho)","absente":"ausente","actifs":"ativos","ancienne":"antiga","au sol":"no solo","au voisinage":"nas proximidades","aucun":"nenhum","aucun avion à {r}":"nenhum avião a {r}","aucun canal":"nenhum canal","aucune":"nenhum","averses":"pancadas","baisse de tension":"queda de tensão","brouillard":"nevoeiro","bruine":"garoa","brume":"névoa úmida","brume sèche":"névoa seca","calme":"calmo","changer":"trocar","chasse":"soprada","chien de garde":"watchdog","ciel dégagé":"céu limpo","compagnie inconnue":"companhia desconhecida","connecté":"conectado","dernier code {c}":"último código {c}","données il y a {s} s":"dados de {s} s atrás","espace":"espaço","ex. ACA, C-G, A320":"ex.: TAM, PR-, A320","ex. CYYZ":"ex.: SBGR","externe":"externo","faible":"fraca","fort":"forte","fumée":"fumaça","grains":"rajadas","granules de glace":"grãos de gelo","grésil":"granizo miúdo","grêle":"granizo","il y a {n} h":"há {n} h","il y a {n} min":"há {n} min","il y a {n} s":"há {n} s","le plus proche":"o mais próximo","logiciel":"software","manuel":"manual","mise sous tension":"ligado na energia","mode configuration · réseau {n}":"modo de configuração · rede {n}","modèle inconnu":"modelo desconhecido","mot de passe":"senha","neige":"neve","neige en grains":"neve granulada","orage":"trovoada","oui":"sim","panique":"pânico","pas de connexion avec l’ESP32":"sem conexão com o ESP32","pluie":"chuva","poussière":"poeira","proximité":"proximidade","précipitations":"precipitação","rafales":"rajadas","rayon {r} km":"raio {r} km","recherche…":"procurando…","route inconnue":"rota desconhecida","sable":"areia","sortie de veille":"saída do modo de espera","stable":"estável","tornade":"tornado","variable":"variável","vent":"vento","vent calme":"vento calmo","verglaçant":"congelante","ville ou code postal":"cidade ou CEP","{a} avions affichés sur {b} reçus.":"{a} aviões exibidos de {b} recebidos.","{n} AVIONS":"{n} AVIÕES","{n} avion":"{n} avião","{n} avions":"{n} aviões","{n} canaux":"{n} canais","« Militaire » seulement quand la base de données de l’adsb.lol l’indique.":"“Militar” só quando a base de dados da adsb.lol indica.","Échec : {e}":"Falha: {e}","Échec de l’envoi.":"Falha no envio.","Échec par l’IP.":"Falha pelo IP.","Étiquettes":"Etiquetas","Événements":"Eventos","à configurer":"a configurar","à l’instant":"agora mesmo","à portée":"ao alcance"},"en":{"0 – 10 000 ft":"0 – 10,000 ft","0 – 20 000 ft":"0 – 20,000 ft","> 10 000 ft":"> 10,000 ft","ADS-B : dernière requête":"ADS-B: last request","ADS-B : requêtes / échecs":"ADS-B: requests / failures","ADS-B PERDU":"ADS-B LOST","ALERTE":"ALERT","AU SOL":"ON GROUND","Actuelle : <b>{c}</b> ({s}). Sans GPS, le radar se situe par les réseaux Wi-Fi voisins, par l’adresse IP ou par la ville choisie ici.":"Current: <b>{c}</b> ({s}). Without GPS, the radar locates itself from nearby Wi-Fi networks, the IP address or the city chosen here.","Adresse":"Address","Alertes":"Alerts","Altitude maximale":"Maximum altitude","Appareil":"Device","Arrêt":"Off","Arrêter le son":"Stop audio","Aucun canal pour cet aéroport.":"No channel for this airport.","Aucun réseau trouvé.":"No network found.","Aucun trafic":"No traffic","Aucune":"None","Aucune alerte : le radar reste silencieux.":"No alerts: the radar stays silent.","Aucune alerte depuis le démarrage.":"No alerts since start-up.","Aucune ville trouvée.":"No city found.","Automatique":"Automatic","Autre aéroport (code OACI) :":"Other airport (ICAO code):","Autres":"Other","Avions au sol":"Aircraft on ground","Avions reçus":"Aircraft received","Aéroport":"Airport","Aéroport (radio)":"Airport (radio)","Aéroport de la radio":"Radio airport","Balayage":"Sweep","Bip court pour chaque nouvel avion.":"Short beep for each new aircraft.","CAP":"TRACK","CIE":"AIRL","CONFIGURATION":"SETUP","Cap (route)":"Track","Catégorie":"Category","Catégorie de vol":"Flight category","Catégories (aucune choisie = toutes)":"Categories (none selected = all)","Cet avion a quitté le radar.":"This aircraft has left the radar.","Chercher":"Search","Choisissez le fichier .bin du nouveau micrologiciel :":"Choose the .bin file of the new firmware:","Choisissez un canal pour l’écouter sur cet appareil.":"Pick a channel to listen on this device.","Choisissez un fichier .bin.":"Choose a .bin file.","Classique":"Classic","Code OACI 24 bits":"ICAO 24-bit code","Compagnie":"Airline","Connecter":"Connect","Connecté à <b>{n}</b>. Choisissez un autre réseau :":"Connected to <b>{n}</b>. Choose another network:","Connexion…":"Connecting…","Contrôleur":"Controller","Courte":"Short","DONNÉES ANCIENNES · {s} s":"STALE DATA · {s} s","Dernier redémarrage":"Last restart","Dernières alertes":"Recent alerts","Diagnostic":"Diagnostics","DÉTOURNEMENT":"HIJACK","Désactivées":"Off","Détecter par Wi-Fi":"Detect by Wi-Fi","ESP HORS LIGNE":"ESP OFFLINE","ESP sans données adsb.lol":"ESP without adsb.lol data","En marche depuis":"Up for","En écoute":"Listening","Envoi… {p} %":"Uploading… {p} %","Envoyer":"Upload","Faible":"Low","Fermer":"Close","Filtres":"Filters","Fixes":"Fixed","Fort":"High","Grand":"Large","Gros porteurs":"Heavy","Haute performance":"High performance","Hélicoptères":"Helicopters","IMMAT":"REG","INITIALISATION DU SYSTÈME":"SYSTEM INITIALIZATION","Immatriculation":"Registration","Importantes":"Important","Inconnue":"Unknown","LIAISON AVEC L’ESP PERDUE":"LINK TO ESP LOST","LIAISON ESP32":"ESP32 LINK","Langue":"Language","Le radar est en mode configuration (réseau <b>{n}</b>). Choisissez le Wi-Fi de la maison :":"The radar is in setup mode (network <b>{n}</b>). Choose your home Wi-Fi:","Le radar ne répond pas.":"The radar is not responding.","Le radar redémarre et se connecte à <b>{n}</b>.":"The radar is restarting and will connect to <b>{n}</b>.","Le radar se connecte au plus fort de ces réseaux et y revient tout seul quand il réapparaît.":"The radar joins the strongest of these networks and reconnects on its own when it comes back.","Le silence est normal : on n’entend quelque chose que quand un pilote ou le contrôleur parle.":"Silence is normal: you only hear something when a pilot or the controller speaks.","Les réseaux d’ici ne sont pas encore cartographiés. Essayez par l’IP ou la ville.":"The networks here are not mapped yet. Try by IP or city.","Ligne":"Airliners","Longue":"Long","Légers":"Light","METAR indisponible pour {a}.":"METAR unavailable for {a}.","Militaire":"Military","Militaires seulement":"Military only","Minimum atteint":"Lowest reached","Mise à jour":"Update","Mise à jour réussie. Le radar redémarre…":"Update successful. The radar is restarting…","Moyen":"Medium","Moyenne":"Medium","Moyens":"Medium","Mémoire libre":"Free memory","Météo":"Weather","Ne débranchez pas le radar pendant l’envoi.":"Do not unplug the radar during the upload.","Ne plus suivre":"Stop following","Nouvel avion (mode Toutes)":"New aircraft (All mode)","Nuages":"Clouds","O":"W","ORIGINE":"ORIGIN","Observation {t}":"Observed {t}","Ou choisissez la ville :":"Or choose the city:","Oublier le réseau {n} ?":"Forget network {n}?","PANNE RADIO":"RADIO FAILURE","Page : images/s":"Page: frames/s","Par défaut, le radar écoute l’aéroport le plus proche qui a des canaux.":"By default, the radar listens to the nearest airport that has channels.","Par l’IP":"By IP","Phénomènes":"Weather","Planeurs / ULM":"Gliders / ultralights","Plus grand bloc libre":"Largest free block","Position : {c}":"Position: {c}","Position du radar":"Radar position","Pression (QNH)":"Pressure (QNH)","Programme":"Firmware","Propriétaire":"Owner","Proximité : avions à moins de {r} km et sous {c} ft.":"Proximity: aircraft within {r} km and below {c} ft.","Précision : ± {a}.":"Accuracy: ± {a}.","Quel avion ?":"Which aircraft?","RADAR · ADS-B · SURVEILLANCE AÉRIENNE":"RADAR · ADS-B · AIR SURVEILLANCE","Rayon de proximité":"Proximity radius","Recherche (indicatif, immatriculation, type)":"Search (callsign, registration, type)","Recherche des canaux de cet aéroport…":"Looking for this airport’s channels…","Recherche des réseaux…":"Scanning networks…","Recherche impossible (pas d’internet ?).":"Search failed (no internet?).","Recherche…":"Searching…","Relèvement":"Bearing","Remettez votre téléphone sur ce réseau et ouvrez <b>http://radar.local</b>. Si le mot de passe est faux, le radar revient au mode configuration (réseau {a}).":"Put your phone back on that network and open <b>http://radar.local</b>. If the password is wrong, the radar returns to setup mode (network {a}).","Réglages":"Settings","Réseau":"Network","Réseau Wi-Fi":"Wi-Fi network","Réseau ouvert : pas de mot de passe.":"Open network: no password.","Réseaux":"Networks","Réseaux enregistrés (3 au maximum)":"Saved networks (up to 3)","SOL":"GND","SUIVI":"FOLLOWING","Son":"Sound","Suivre":"Follow","Taux vertical":"Vertical rate","Température / point de rosée":"Temperature / dew point","Tester le bip":"Test beep","Texte":"Text","Tour de contrôle":"Control tower","Toutes":"All","Transpondeur":"Transponder","Traînée":"Trail","URGENCE":"EMERGENCY","Unités":"Units","Urgences (7500 / 7600 / 7700)":"Emergencies (7500 / 7600 / 7700)","Urgences signalées.":"Emergencies flagged.","VIT":"SPD","VITESSE":"SPEED","VOL SÉLECTIONNÉ":"SELECTED FLIGHT","Vecteur":"Vector","Vent":"Wind","Version actuelle : {v}":"Current version: {v}","Visibilité":"Visibility","Vitesse sol":"Ground speed","Volume des bips (cet appareil)":"Beep volume (this device)","absente":"none","actifs":"active","ancienne":"old","au sol":"on ground","au voisinage":"in the vicinity","aucun":"none","aucun avion à {r}":"no aircraft within {r}","aucun canal":"no channel","aucune":"none","averses":"showers","baisse de tension":"brownout","brouillard":"fog","bruine":"drizzle","brume":"mist","brume sèche":"haze","calme":"calm","changer":"change","chasse":"blowing","chien de garde":"watchdog","ciel dégagé":"clear sky","compagnie inconnue":"unknown airline","connecté":"connected","dernier code {c}":"last code {c}","données il y a {s} s":"data {s} s old","espace":"space","ex. ACA, C-G, A320":"e.g. ACA, C-G, A320","ex. CYYZ":"e.g. CYYZ","externe":"external","faible":"light","fort":"heavy","fumée":"smoke","grains":"squalls","granules de glace":"ice pellets","grésil":"small hail","grêle":"hail","il y a {n} h":"{n} h ago","il y a {n} min":"{n} min ago","il y a {n} s":"{n} s ago","le plus proche":"nearest","logiciel":"software","manuel":"manual","mise sous tension":"power-on","mode configuration · réseau {n}":"setup mode · network {n}","modèle inconnu":"unknown model","mot de passe":"password","neige":"snow","neige en grains":"snow grains","orage":"thunderstorm","oui":"yes","panique":"panic","pas de connexion avec l’ESP32":"no connection to the ESP32","pluie":"rain","poussière":"dust","proximité":"proximity","précipitations":"precipitation","rafales":"gusts","rayon {r} km":"radius {r} km","recherche…":"searching…","route inconnue":"unknown route","sable":"sand","sortie de veille":"wake from sleep","stable":"level","tornade":"funnel cloud","vent":"wind","vent calme":"calm wind","verglaçant":"freezing","ville ou code postal":"city or postal code","{a} avions affichés sur {b} reçus.":"{a} of {b} aircraft shown.","{n} AVIONS":"{n} AIRCRAFT","{n} avion":"{n} aircraft","{n} avions":"{n} aircraft","{n} canal":"{n} channel","{n} canaux":"{n} channels","« Militaire » seulement quand la base de données de l’adsb.lol l’indique.":"“Military” only when the adsb.lol database says so.","Échec : {e}":"Failed: {e}","Échec de l’envoi.":"Upload failed.","Échec par l’IP.":"IP lookup failed.","Étiquettes":"Labels","Événements":"Events","à configurer":"to set up","à l’instant":"just now","à portée":"in range"}};
let pref={};try{pref=JSON.parse(localStorage.getItem('radar_esp')||'{}')}catch(e){}
let lang=pref.lang||'fr';
const LOC={fr:'fr-CA',pt:'pt-BR',en:'en-CA'};
function T(s,v){let t=(lang!=='fr'&&L10N[lang]&&L10N[lang][s])||s;if(v)for(const k in v)t=t.split('{'+k+'}').join(v[k]);return t}
const fr=n=>Math.round(n).toLocaleString(LOC[lang]);
const dec=(n,d=1)=>n.toLocaleString(LOC[lang],{minimumFractionDigits:d,maximumFractionDigits:d});
const RANGES={metric:[10,25,50,100,200],imperial:[5,10,25,50,100]},KM_PER={metric:1,imperial:1.60934};
const ATC_NM=[5,10,25,50,100];   // alcances do estilo controlador, em milhas nauticas
const STOPS=[[0,'#f9e04b'],[5000,'#f6a33b'],[10000,'#e8673c'],[20000,'#c93d78'],[30000,'#8b3fb5'],[40000,'#4e5ad6'],[45000,'#3fa3f0']];
const altColor=ft=>{let c=STOPS[0][1];for(const[a,col]of STOPS)if(ft>=a)c=col;return c};
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
// ---------- preferencias deste navegador
let units=pref.units||'metric',rangeIdx=pref.rangeIdx??2,mode=pref.mode||'round',mapOn=true,style=pref.style||'atc',
  sweepTags=pref.sweepTags??true,trailIdx=pref.trailIdx??1,vecIdx=pref.vecIdx??1,atcIcao=pref.atcIcao||'',
  rot=pref.rot||0,bigText=!!pref.bigText,volIdx=pref.volIdx??1,actx=null,menuOpen=false;
// filtros: faixa de altitude, categorias ADS-B (vazio = todas), so militares, avioes no solo e busca por texto
const F=Object.assign({alt:0,cats:[],mil:false,ground:pref.groundOn??false,q:''},pref.F||{});
const savePref=()=>{try{localStorage.setItem('radar_esp',JSON.stringify({units,rangeIdx,mode,style,sweepTags,trailIdx,vecIdx,atcIcao,lang,rot,bigText,volIdx,F}))}catch(e){}};
// torre de controle: o ESP repassa o audio da LiveATC (porta 81) e o navegador toca no aparelho que abriu a pagina
let apShown=false;let atc={on:false,feed:null,live:false};
let D={planes:[],alerting:0,alertOn:true,soundOn:true,aMode:1},recvAt=0,selHex=null,lastAlert=0,conn=false,follow=null;
const info={};   // hex -> {model, airline, orig, dest} (adsbdb)
const hist=new Map();let histAt=0;   // rastro: ultimas posicoes (km x/y a partir de casa)
const TS=()=>bigText?1.25:1;   // texto maior (menu Appareil > Texte)

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
const R={W:466,CX:233,CY:233,RMAX:188,minus:[32,233,24],plus:[434,233,24],blips:[],edge:[],
  C:{bg:'#000',ring:'#0b3d1c',ringHi:'#16653a',text:'#c8d2dc',dim:'#5d6b78',sweep:'0,255,120',sel:'#ff3c3c',green:'#00ff78',home:'#3c8cff',accent:'#e0b53c',card:'rgba(14,20,28,0.9)',btn:'#141c24'},
  d:gfx($('round'))};
function planeIcon(d,x,y,trk,size,c){const t=trk*Math.PI/180,s=Math.sin(t),co=Math.cos(t),P=(f,r)=>[x+s*f+co*r,y-co*f+s*r];
  const[nx,ny]=P(size,0),[lx,ly]=P(-size*.7,-size*.75),[rx,ry]=P(-size*.7,size*.75),[mx,my]=P(-size*.3,0);
  d.fillTriangle(nx,ny,lx,ly,mx,my,c);d.fillTriangle(nx,ny,mx,my,rx,ry,c)}
// ---------- mapa de fundo (tiles Esri Dark Gray, Web Mercator), centrado na casa (ou no aviao seguido) e na escala do zoom
const tiles=new Map();
function tile(z,x,y){const k=z+'/'+x+'/'+y;let t=tiles.get(k);
  if(!t){t=new Image();t.src=`https://server.arcgisonline.com/ArcGIS/rest/services/Canvas/World_Dark_Gray_Base/MapServer/tile/${z}/${y}/${x}`;tiles.set(k,t)}
  return t.complete&&t.naturalWidth?t:null}
function drawMap(g,CX,CY,RMAX,rKm,shade=.4,clat=D.lat,clon=D.lon){
  if(!D.lat&&!D.lon)return;
  const lat=clat*Math.PI/180,screenMpp=rKm*1000/RMAX;
  const z=Math.max(3,Math.min(13,Math.round(Math.log2(156543.03*Math.cos(lat)/screenMpp))));
  const s=156543.03*Math.cos(lat)/2**z/screenMpp,n=2**z;   // tamanho do pixel do tile na tela
  const hx=(clon+180)/360*256*n,hy=(1-Math.log(Math.tan(lat)+1/Math.cos(lat))/Math.PI)/2*256*n;
  const span=RMAX/s,x0=Math.floor((hx-span)/256),x1=Math.floor((hx+span)/256),y0=Math.floor((hy-span)/256),y1=Math.floor((hy+span)/256);
  g.save();g.beginPath();g.arc(CX,CY,RMAX,0,Math.PI*2);g.clip();
  for(let ty=y0;ty<=y1;ty++)for(let tx=x0;tx<=x1;tx++){const im=tile(z,((tx%n)+n)%n,ty);if(im)g.drawImage(im,CX+(tx*256-hx)*s,CY+(ty*256-hy)*s,256*s+.5,256*s+.5)}
  g.fillStyle=`rgba(0,0,0,${shade})`;g.fillRect(CX-RMAX,CY-RMAX,2*RMAX,2*RMAX);   // escurece por cima (sem emendas claras entre tiles)
  g.restore();
}

// ---------- filtros (Reglages > Radar > Filtres): valem para a tela, as setas da borda e o bipe de aviao novo
const ALTS=[[0,1e9,'Toutes'],[0,10000,'0 – 10 000 ft'],[0,20000,'0 – 20 000 ft'],[10000,1e9,'> 10 000 ft']];
const CATS=[['A1','Légers',['A1']],['A2','Moyens',['A2']],['LN','Ligne',['A3','A4']],['A5','Gros porteurs',['A5']],['A6','Haute performance',['A6']],
  ['A7','Hélicoptères',['A7']],['B','Planeurs / ULM',['B1','B2','B3','B4']],['B6','Drones',['B6']],['?','Inconnue',['']]];
function passFilter(p){
  if(p.ground)return F.ground;
  const[lo,hi]=ALTS[F.alt]||ALTS[0];if(p.alt<lo||p.alt>hi)return false;
  if(F.cats.length&&!F.cats.some(k=>(CATS.find(c=>c[0]===k)||[,,[]])[2].includes(p.cat||'')))return false;
  if(F.mil&&!p.mil)return false;
  if(F.q){const q=F.q.toUpperCase();if(![p.flight,p.reg,p.type,p.hex].some(s=>s&&s.toUpperCase().includes(q)))return false}
  return true;
}
const filtersOn=()=>F.alt||F.cats.length||F.mil||F.q;

// cartao do aviao selecionado (estilo Classique)
function drawCard(d,C,sel){
  const CX=R.CX,imp=units!=='metric',uD=imp?'mi':'km',inf=info[sel.hex]||{loading:true},wait=inf.loading?'…':'';
  d.fillRoundRect(113,274,240,162,20,C.card);
  d.fitText(inf.airline||(inf.loading?'…':T('compagnie inconnue')),CX,293,16,220,inf.airline?'#ffffff':C.dim,true);
  d.fitText(inf.model||sel.type||wait||T('modèle inconnu'),CX,314,12,220,C.text);
  const o=inf.orig,de=inf.dest;
  if(o||de){d.fitText(`${o?o.code:'?'}  →  ${de?de.code:'?'}`,CX,338,18,220,'#fff',true);d.fitText(`${o?o.city:''}  →  ${de?de.city:''}`,CX,357,11,220,C.dim)}
  else d.text(wait||T('route inconnue'),CX,346,13,C.dim,'center');
  const spd=sel.gs==null?'—':imp?`${Math.round(sel.gs*1.15078)} mi/h`:`${Math.round(sel.gs*1.852)} km/h`;
  [[T('ALTITUDE'),sel.ground?T('au sol'):`${fr(sel.alt)} ft`],[T('VITESSE'),spd],[T('DISTANCE'),`${dec(imp?sel.dist/1.60934:sel.dist)} ${uD}`]]
    .forEach(([k,v],i)=>{const x=CX+(i-1)*76;d.text(k,x,383,9,C.dim,'center',true);d.fitText(v,x,401,15,72,i===1?C.green:'#fff',true)});
}
function zoomButtons(d,C){for(const[b,s]of[[R.minus,'−'],[R.plus,'+']]){d.fillCircle(b[0],b[1],b[2],C.btn);d.drawCircle(b[0],b[1],b[2],C.ringHi);d.text(s,b[0],b[1]+1,24,C.text,'center',true)}}
const avions=n=>T(n>1?'{n} avions':'{n} avion',{n});

// ---------- relogio quando nao ha avioes na tela: aparece depois de 15 s vazio, some quando um aviao fica 2 s
let clockOn=false,emptyAt=0,busyAt=0;
function clockState(n,now){
  if(n===0){busyAt=0;if(!emptyAt)emptyAt=now;if(now-emptyAt>15000)clockOn=true}
  else{emptyAt=0;if(!busyAt)busyAt=now;if(now-busyAt>2000)clockOn=false}
  return clockOn&&!selHex&&!follow&&conn;
}
function drawClock(d,rangeTxt){
  const{CX,CY}=R,t=new Date(),p2=n=>String(n).padStart(2,'0');
  d.fillCircle(CX,CY,200,'rgba(4,9,14,.9)');d.drawCircle(CX,CY,200,'rgba(201,162,39,.25)');
  d.mono('SENTINEL 643',CX,CY-112,13,'#e0b53c','center');
  d.mono(`${p2(t.getHours())}:${p2(t.getMinutes())}`,CX,CY-50,74,'#e6f2f7','center');
  d.mono(p2(t.getSeconds()),CX+112,CY-30,18,'#5f8599','left');
  const ds=t.toLocaleDateString(LOC[lang],{weekday:'long',day:'numeric',month:'long'});
  d.text(ds.charAt(0).toUpperCase()+ds.slice(1),CX,CY+6,15,'#9fb4c3','center',true);
  d.mono(`UTC ${p2(t.getUTCHours())}${p2(t.getUTCMinutes())}Z`,CX,CY+32,13,'#5f8599','center');
  if(METAR.icaoId){const m=METAR,c=FLTCOL[m.fltCat]||'#9fb4c3';
    d.mono(`${m.icaoId}  ${m.fltCat||''}`,CX,CY+66,13,c,'center');
    d.fitText([windTxt(m),m.temp!=null?`${m.temp}°C`:'',m.altim?`${Math.round(m.altim)} hPa`:''].filter(Boolean).join('  ·  '),CX,CY+86,13,300,'#c9d6de')}
  const[st,sc]=adsbState();d.fillCircle(CX-80,CY+122,4,sc);
  d.fitText(`${st}  ·  ${T('aucun avion à {r}',{r:rangeTxt})}`,CX+6,CY+122,11,150,'#7a8794');
}

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
  [[T('N'),0],[T('E'),90],[T('S'),180],[T('O'),270]].forEach(([l,g],i)=>{const a=g*Math.PI/180;d.text(l,CX+Math.sin(a)*(RMAX-16),CY-Math.cos(a)*(RMAX-16),15,i===0?C.accent:C.dim,'center',true)});
  [1,2].forEach(i=>d.text(''+ +(rLbl*i/3).toFixed(1),CX+4+RMAX*i/3*.707,CY-RMAX*i/3*.707-2,11,C.dim));
  const sw=(now/SWEEP_MS*Math.PI*2)%(Math.PI*2);
  for(let k=0;k<18;k++){const a1=sw-Math.PI/2-k*.035;d.fillArc(CX,CY,0,RMAX,a1-.035,a1,`rgba(${C.sweep},${(.28*(1-k/18)).toFixed(3)})`)}
  d.drawLine(CX,CY,CX+Math.sin(sw)*RMAX,CY-Math.cos(sw)*RMAX,`rgba(${C.sweep},.8)`,2);
  d.fillTriangle(CX,CY-8,CX-6,CY+6,CX+6,CY+6,C.home);
  const inRange=planes.filter(p=>p.dist<=rKm&&passFilter(p)).sort((a,b)=>a.dist-b.dist);
  R.blips=[];R.edge=[];let sel=null;
  inRange.forEach(p=>{const r=p.dist/rKm*RMAX,a=p.brg*Math.PI/180,x=CX+r*Math.sin(a),y=CY-r*Math.cos(a),isSel=p.hex===selHex;if(isSel)sel=p;
    planeIcon(d,x,y,p.trk||0,isSel?11:8,isSel?'#fff':p.ground?'#8a939c':altColor(p.alt));
    if(isSel){d.drawCircle(x,y,16,C.accent,2);d.text(p.flight||p.hex,x+11,y-10,12*TS(),C.accent,'left',true)}
    else{const k=sweepTags?sweepFade(sw,a):1;if(k>0)d.text(p.flight||p.hex,x+11,y-10,12*TS(),`rgba(200,210,220,${k.toFixed(2)})`)}
    R.blips.push({x,y,hex:p.hex})});
  const t=new Date();d.text(`${String(t.getHours()).padStart(2,'0')} h ${String(t.getMinutes()).padStart(2,'0')}  ·  ${avions(inRange.length)} · ${rLbl} ${uD}`,CX,18,12,C.dim,'center',true);
  zoomButtons(d,C);
  if(clockState(inRange.length,now))drawClock(d,`${rLbl} ${uD}`);
  else if(sel)drawCard(d,C,sel);
  if(D.alerting){const on=Math.floor(now/350)%2;d.drawCircle(CX,CY,229,on?C.sel:'rgba(255,60,60,.35)',8);
    if(on){d.fillRoundRect(CX-48,30,96,24,12,C.sel);d.text(T('ALERTE'),CX,43,13,'#fff','center',true)}}
}

// ---------- estilo CONTROLEUR (como o scope de um controlador de trafego aereo)
// alvo quadrado, rastro de posicoes anteriores, vetor de previsao e etiqueta de dados:
//   linha 1: indicativo (+ squawk de emergencia)
//   linha 2: altitude em centenas de pes + seta de subida/descida + velocidade em nos (ex.: 118↓370)
//   linha 3: tipo + destino (rota da adsbdb, buscada em segundo plano pelo navegador)
const A={bg:'#050D13',ring:'#243A48',ringTxt:'#4d6a7c',rose:'#2f4b5d',roseTxt:'#5f8599',rwy:'#9fb4c3',apt:'#57788b',cl:'120,150,170',
  sweep:'110,210,160',gnd:'#78858C',tgt:'#DCEAF0',hist:'130,180,215',lead:'#8fbcd6',tag:'#c9dde7',sel:'#FFD166',alert:'#FF4D4D',
  ok:'#3ddc84',warn:'#ffb340',bad:'#ff4d4d',lbl:'#6f8797',gold:'#e0b53c'};
const RA=198;   // raio util do scope
const TRAIL=[0,5,10,20],VEC=[30,60,120];   // rastro (pontos a cada 4 s) e vetor de previsao (s): opcoes do menu
const EMERG={'7500':'DÉTOURNEMENT','7600':'PANNE RADIO','7700':'URGENCE'};
const COAST=30;   // posicao com mais de 30 s: alvo vazado ("coast"), sem vetor
const CARD=[113,256,353,438];   // cartao do aviao selecionado (x0,y0,x1,y1)
function kmXY(lat,lon){return[(lon-D.lon)*111.32*Math.cos(D.lat*Math.PI/180),(lat-D.lat)*110.57]}
const csOf=p=>p.flight||p.reg||p.hex.toUpperCase();
const routeOf=p=>{const i=info[p.hex];if(i&&(i.orig||i.dest))return{o:i.orig&&i.orig.code,d:i.dest&&i.dest.code};return p.flight&&routes.get(p.flight)||null};
// estado da fonte de dados: verde = em dia, laranja = atrasada, vermelho = perdida
function adsbState(){
  if(!conn)return[T('ESP HORS LIGNE'),A.bad];
  if(D.ap)return[T('CONFIGURATION'),A.warn];
  const a=dataAge();
  if(!D.ok||a>90)return[D.err?`ADS-B ERR ${D.err}`:T('ADS-B PERDU'),A.bad];
  if(a>30)return[`ADS-B ${Math.round(a)} s`,A.warn];
  return['ADS-B',A.ok];
}
function pill(d,t,y,bg,fg){d.g.font='700 11px ui-monospace,Consolas,monospace';const w=d.g.measureText(t).width+20;d.fillRoundRect(R.CX-w/2,y-11,w,22,11,bg);d.mono(t,R.CX,y,11,fg,'center');return[R.CX-w/2,y-11,R.CX+w/2,y+11]}
// rastro: guarda a posicao a cada 4 s; salto grande (dado falho) recomeca o rastro em vez de riscar o radar
function sampleTrails(planes,now){
  if(now-histAt<4000)return;histAt=now;
  for(const p of planes){const h=hist.get(p.hex)||[],l=h[h.length-1];
    if(l&&Math.hypot(p.x-l[0],p.y-l[1])>Math.max(3,(p.gs||0)*1.852/3600*4*4))h.length=0;
    h.push([p.x,p.y]);if(h.length>21)h.shift();hist.set(p.hex,h)}
}
const tagSlot=new Map();   // hex -> posicao da etiqueta escolhida antes (so muda de lado se precisar)
const OFFS=[[1,-1],[-1,-1],[1,1],[-1,1],[1,0],[-1,0]];   // NE, NO, SE, SO, L, O
function drawATC(planes){
  const d=R.d,C=R.C,{CX,CY}=R,now=performance.now(),rNm=ATC_NM[rangeIdx],rKm=rNm*1.852,k=RA/rKm,blink=Math.floor(now/400)%2,ts=TS();
  // seguir um aviao: o centro do scope vai para ele (norte continua para cima); a casa vira uma cruz que se move
  const fo=follow&&planes.find(p=>p.hex===follow),fx=fo?fo.x:0,fy=fo?fo.y:0;
  const P=(x,y)=>[CX+(x-fx)*k,CY-(y-fy)*k],dash=a=>d.g.setLineDash(a);
  d.fillRect(0,0,R.W,R.W,A.bg);
  if(mapOn)drawMap(d.g,CX,CY,RA,rKm,.72,D.lat+fy/110.57,D.lon+fx/(111.32*Math.cos(D.lat*Math.PI/180)));
  d.g.save();d.g.beginPath();d.g.arc(CX,CY,RA,0,Math.PI*2);d.g.clip();
  if(D.lat||D.lon){
    // eixo das pistas prolongado 10 NM nos aeroportos com torre na LiveATC (os principais da regiao)
    if(rNm<=50){const big=new Set(atcFeeds.map(f=>f.icao));dash([3,5]);
      for(const[icao,,,rws]of APTS){if(!big.has(icao))continue;
        for(const r of rws){const[x1,y1]=kmXY(r[0],r[1]),[x2,y2]=kmXY(r[2],r[3]),L=Math.hypot(x2-x1,y2-y1)||1,ux=(x2-x1)/L,uy=(y2-y1)/L,E=18.52;
          d.drawLine(...P(x1,y1),...P(x1-ux*E,y1-uy*E),`rgba(${A.cl},.32)`);d.drawLine(...P(x2,y2),...P(x2+ux*E,y2+uy*E),`rgba(${A.cl},.32)`)}}
      dash([])}
    // pistas e aeroportos (mapa video)
    for(const[icao,la,lo,rws]of APTS){
      const[ax,ay]=P(...kmXY(la,lo));if(Math.hypot(ax-CX,ay-CY)>RA+20)continue;
      for(const r of rws){const[x1,y1]=P(...kmXY(r[0],r[1])),[x2,y2]=P(...kmXY(r[2],r[3]));d.drawLine(x1,y1,x2,y2,A.rwy,rNm<=25?3:2)}
      d.mono(icao,ax+8,ay+10,10,A.apt);
    }
  }
  // 4 aneis, cada um 1/4 do alcance (ex.: 50 NM -> 12.5 / 25 / 37.5 / 50)
  for(let i=1;i<=4;i++){d.drawCircle(CX,CY,RA*i/4,A.ring,i===4?1.5:1);if(i<4)d.mono(String(+(rNm*i/4).toFixed(2)),CX+RA*i/4*.707+3,CY+RA*i/4*.707+8,9,A.ringTxt)}
  // zona de alerta: circulo tracejado discreto em volta da casa
  if(D.alertOn&&D.aKm&&D.aKm*k<RA*2){dash([2,4]);d.drawCircle(...P(0,0),D.aKm*k,'rgba(255,77,77,.38)');dash([])}
  d.g.restore();
  // rosa dos ventos em graus; 000/090/180/270 sem numero (dariam no estado do topo e nos botoes de zoom)
  for(let g=0;g<360;g+=5){const a=g*Math.PI/180,r1=RA+(g%10?3:g%30?6:9);d.drawLine(CX+Math.sin(a)*RA,CY-Math.cos(a)*RA,CX+Math.sin(a)*r1,CY-Math.cos(a)*r1,A.rose,g%30?1:2)}
  for(let g=0;g<360;g+=30){if(g%90===0)continue;const a=g*Math.PI/180;d.mono(String(g).padStart(3,'0'),CX+Math.sin(a)*(RA+19),CY-Math.cos(a)*(RA+19),10,A.roseTxt,'center')}
  // varredura (efeito visual: os dados chegam pela internet, nao por uma antena)
  const sw=(now/SWEEP_MS*Math.PI*2)%(Math.PI*2);
  for(let q=0;q<18;q++){const a1=sw-Math.PI/2-q*.035;d.fillArc(CX,CY,0,RA,a1-.035,a1,`rgba(${A.sweep},${(.15*(1-q/18)).toFixed(3)})`)}
  d.drawLine(CX,CY,CX+Math.sin(sw)*RA,CY-Math.cos(sw)*RA,`rgba(${A.sweep},.55)`,2);
  {const[hx,hy]=P(0,0);if(Math.hypot(hx-CX,hy-CY)<RA){d.drawLine(hx-5,hy,hx+5,hy,'#3c8cff',2);d.drawLine(hx,hy-5,hx,hy+5,'#3c8cff',2)}}   // casa
  sampleTrails(planes,now);
  const shown=planes.filter(passFilter),dc=p=>Math.hypot(p.x-fx,p.y-fy);
  const inRange=shown.filter(p=>dc(p)<=rKm).sort((a,b)=>dc(a)-dc(b));
  const sel=inRange.find(p=>p.hex===selHex)||null;
  if(sel&&!fo){const[sx,sy]=P(sel.x,sel.y);dash([2,4]);d.drawLine(...P(0,0),sx,sy,'rgba(255,209,102,.4)');dash([])}   // linha casa -> selecionado
  R.blips=[];const tags=[],targets=[],nT=TRAIL[trailIdx],vs=VEC[vecIdx];
  for(const p of inRange){
    const[x,y]=P(p.x,p.y),a=Math.atan2(p.x-fx,p.y-fy),isSel=p===sel;
    const em=EMERG[p.sq],alert=!!em||(D.alertOn&&p.al),coast=posAge(p)>COAST;
    if(nT){const h=(hist.get(p.hex)||[]).slice(-nT-1,-1);h.forEach(([hx,hy],i)=>{const[qx,qy]=P(hx,hy);d.fillRect(qx-1.5,qy-1.5,3,3,`rgba(${A.hist},${(.12+.6*(i+1)/h.length).toFixed(2)})`)})}
    if(!p.ground&&!coast&&p.gs!=null&&p.trk!=null){const t=p.trk*Math.PI/180,L=p.gs*1.852/3600*vs*k;d.drawLine(x,y,x+Math.sin(t)*L,y-Math.cos(t)*L,A.lead)}
    const col=isSel?A.sel:alert?(blink?A.alert:'#ff9a9a'):p.ground?A.gnd:A.tgt;
    if(coast){d.g.strokeStyle=col;d.g.lineWidth=1.5;d.g.strokeRect(x-3,y-3,6,6)}else d.fillRect(x-3,y-3,6,6,col);
    if(isSel){d.g.strokeStyle=A.sel;d.g.lineWidth=1;d.g.strokeRect(x-7.5,y-7.5,15,15)}
    if(p.hex===follow){d.drawCircle(x,y,11,A.gold,1.5)}
    targets.push([x-5,y-5,x+5,y+5]);
    const fl=String(Math.round(p.alt/100)).padStart(3,'0'),arw=p.vr>300?'↑':p.vr<-300?'↓':' ',rt=routeOf(p),cs=csOf(p)+(p.mil?' ⚑':'');
    const full=p.ground?[cs,T('SOL')]:[em?`${cs} ${p.sq}`:cs,`${fl}${arw}${p.gs!=null?String(Math.round(p.gs)).padStart(3,'0'):'---'}`,[p.type,rt&&rt.d].filter(Boolean).join(' ')].filter(Boolean);
    // com "Etiquettes: Balayage" os dados aparecem quando a varredura passa e somem em ~1,6 s
    // (selecionado, seguido, alerta e emergencia ficam sempre); com "Fixes" ficam sempre na tela
    tags.push({hex:p.hex,x,y,full,pri:isSel||p.hex===follow?0:em?1:alert?2:3,dist:dc(p),k:isSel||alert||p.hex===follow||!sweepTags?1:sweepFade(sw,a),
      col:alert?(blink?A.alert:'#ff9a9a'):isSel?A.sel:p.ground?A.gnd:A.tag});
    R.blips.push({x,y,hex:p.hex});
  }
  // fora do alcance: setas na borda apontando para os avioes alem do zoom (agrupadas a cada 10 graus)
  const out=shown.filter(p=>dc(p)>rKm),bins=new Map();R.edge=[];
  for(const p of out){const b=Math.round(((Math.atan2(p.x-fx,p.y-fy)*180/Math.PI)+360)%360/10)%36;bins.set(b,(bins.get(b)||0)+1)}
  for(const[b,n]of bins){const a=b*10*Math.PI/180,r0=RA-4,x=CX+Math.sin(a)*r0,y=CY-Math.cos(a)*r0,s=6,px=Math.cos(a),py=Math.sin(a);
    d.fillTriangle(x+Math.sin(a)*s,y-Math.cos(a)*s,x-Math.sin(a)*s*.4+px*s*.8,y+Math.cos(a)*s*.4+py*s*.8,x-Math.sin(a)*s*.4-px*s*.8,y+Math.cos(a)*s*.4-py*s*.8,'rgba(143,188,214,.75)');
    if(n>1)d.mono(String(n),CX+Math.sin(a)*(RA-17),CY-Math.cos(a)*(RA-17),9,'rgba(143,188,214,.9)','center');
    R.edge.push({x,y})}
  // etiquetas: prioridade (selecionado, emergencia, alerta, depois os mais perto). Com muito trafego so os
  // mais perto ficam com as 3 linhas; os outros mostram so o indicativo.
  const vis=tags.filter(t=>t.k>0).sort((a,b)=>a.pri-b.pri||a.dist-b.dist),fullN=vis.length<=8?99:vis.length<=16?6:4;
  const fixed=[[R.minus[0]-26,R.minus[1]-26,R.minus[0]+26,R.minus[1]+26],[R.plus[0]-26,R.plus[1]-26,R.plus[0]+26,R.plus[1]+26],[CX-95,CY+RA-26,CX+95,CY+RA]];
  if(sel)fixed.push(CARD);
  const placed=[],ov=(a,b)=>Math.max(0,Math.min(a[2],b[2])-Math.max(a[0],b[0]))*Math.max(0,Math.min(a[3],b[3])-Math.max(a[1],b[1]));
  const fs=Math.round(11*ts),lh=Math.round(12*ts);
  d.g.font=`700 ${fs}px ui-monospace,Consolas,monospace`;
  vis.forEach((tg,n)=>{
    const lines=tg.pri<3||n<fullN?tg.full:[tg.full[0]];
    const w=Math.max(...lines.map(s=>d.g.measureText(s).width)),h=lines.length*lh,prev=tagSlot.get(tg.hex);
    let best=null;
    OFFS.forEach(([sx,sy],i)=>{
      const bx=sx>0?tg.x+12:tg.x-12-w,by=sy<0?tg.y-12-h:sy>0?tg.y+12:tg.y-h/2,box=[bx,by,bx+w,by+h];
      let cost=i*2+(prev!==undefined&&prev!==i?60:0)+(Math.hypot((box[0]+box[2])/2-CX,(box[1]+box[3])/2-CY)>RA-4?5000:0);
      for(const b of placed)cost+=ov(box,b)*3;
      for(const b of targets)cost+=ov(box,b)*2;
      for(const b of fixed)cost+=ov(box,b)*3;
      if(!best||cost<best.cost)best={cost,box,sx,sy,i};
    });
    placed.push(best.box);tagSlot.set(tg.hex,best.i);
    const[bx,by,bx2,by2]=best.box,{sx,sy}=best;
    d.g.globalAlpha=tg.k;
    d.drawLine(tg.x+sx*5,tg.y+sy*5,sx>0?bx-2:bx2+2,sy<0?by2:sy>0?by:(by+by2)/2,tg.col);
    lines.forEach((s,i)=>d.mono(s,bx,by+lh/2+i*lh,fs,tg.col));
    d.g.font=`700 ${fs}px ui-monospace,Consolas,monospace`;
  });
  d.g.globalAlpha=1;
  if(tagSlot.size>300)tagSlot.clear();
  // topo (fora do scope): estado da fonte de dados + alcance
  const[st,sc]=adsbState(),top=`${st}  ·  ${rNm} NM`,tf=Math.round(11*ts);
  d.g.font=`700 ${tf}px ui-monospace,Consolas,monospace`;const tw=d.g.measureText(top).width,x0=CX-(tw+12)/2;
  if(sc===A.ok||blink)d.fillCircle(x0+4,16,4,sc);
  d.mono(top,x0+12,16,tf,sc===A.ok?A.roseTxt:sc);
  zoomButtons(d,C);
  const clk=clockState(inRange.length,now);
  if(clk)drawClock(d,`${rNm} NM`);   // relogio antes das faixas: os avisos ficam por cima dele
  // faixas no alto do scope: alerta, aviao seguido, emergencia (squawk), dados antigos ou ESP fora do ar
  let py=50;R.followPill=null;
  if(D.alerting&&blink){pill(d,T('ALERTE'),py,A.alert,'#fff');py+=28}
  if(fo){R.followPill=pill(d,`${T('SUIVI')} ${csOf(fo)} · ${dec(fo.dist/1.852)} NM  ✕`,py,'rgba(224,181,60,.92)','#141005');py+=28}
  const em=inRange.find(p=>EMERG[p.sq]),age=dataAge();
  if(em)pill(d,`${em.sq} ${T(EMERG[em.sq])} · ${csOf(em)}`,py,blink?A.alert:'#7a1f1f','#fff');
  else if(!conn)pill(d,T('LIAISON AVEC L’ESP PERDUE'),py,'#5a1414','#ffd0d0');
  else if(D.ok!==false&&age>30)pill(d,T('DONNÉES ANCIENNES · {s} s',{s:Math.round(age)}),py,'#4a3410','#ffd9a0');
  if(clk);
  else if(sel)drawCardATC(d,sel);
  else if(!talking()){const t=new Date(),bf=Math.round(12*ts);
    d.mono(`${T('{n} AVIONS',{n:inRange.length})}${out.length?'  +'+out.length+' ↗':''}  ·  ${String(t.getUTCHours()).padStart(2,'0')}${String(t.getUTCMinutes()).padStart(2,'0')}Z`,CX,CY+RA-14,bf,A.roseTxt,'center')}
  if(D.alerting){const on=Math.floor(now/350)%2;d.drawCircle(CX,CY,229,on?A.alert:'rgba(255,77,77,.3)',6)}
}
// cartao operacional do aviao selecionado: so dados disponiveis; "—" quando falta, aviso quando a posicao e antiga
function drawCardATC(d,p){
  const CX=R.CX,inf=info[p.hex]||{loading:true},rt=routeOf(p)||{},pa=posAge(p),L=A.lbl,miss=inf.loading?'…':'—';
  d.fillRoundRect(CARD[0],CARD[1],CARD[2]-CARD[0],CARD[3]-CARD[1],22,'rgba(8,16,23,.95)');
  d.g.strokeStyle='rgba(224,181,60,.4)';d.g.lineWidth=1;d.g.beginPath();d.g.roundRect(CARD[0]+.5,CARD[1]+.5,CARD[2]-CARD[0]-1,CARD[3]-CARD[1]-1,22);d.g.stroke();
  d.text(pa>COAST?T('DONNÉES ANCIENNES · {s} s',{s:Math.round(pa)}):T('VOL SÉLECTIONNÉ'),CX,271,9,pa>COAST?A.warn:L,'center',true);
  d.fitText(csOf(p)+(EMERG[p.sq]?'  '+p.sq:'')+(p.mil?'  ⚑':''),CX,290,20,200,EMERG[p.sq]?A.alert:A.sel,true);
  d.fitText([inf.airline,inf.model||p.type].filter(Boolean).join(' · ')||(inf.loading?'…':p.reg||'—'),CX,310,11,214,'#c9d6de');
  [[T('ORIGINE'),rt.o,-62],[T('DESTINATION'),rt.d,62]].forEach(([l,v,dx])=>{d.text(l,CX+dx,329,8,L,'center',true);d.fitText(v||miss,CX+dx,346,17,96,v?'#fff':L,true)});
  d.text('→',CX,346,15,L,'center',true);
  d.drawLine(CARD[0]+20,362,CARD[2]-20,362,'rgba(255,255,255,.08)');
  const vs=p.vr>300?' ↑':p.vr<-300?' ↓':'';
  [[T('ALTITUDE'),p.ground?T('AU SOL'):`${fr(p.alt)} ft${vs}`],[T('VITESSE'),p.gs!=null?`${Math.round(p.gs)} kt`:'—'],
   [T('CAP'),p.trk!=null?`${String(Math.round(p.trk)%360).padStart(3,'0')}°`:'—'],[T('DISTANCE'),`${dec(p.dist/1.852)} NM`]]
    .forEach(([l,v],i)=>{const x=CX+(i%2?62:-62),y=375+Math.floor(i/2)*33;d.text(l,x,y,8,L,'center',true);d.fitText(v,x,y+15,15,108,i===1?A.ok:'#fff',true)});
}


// ---------- tela retangular 320x240 (CYD)
const Q={CX:120,CY:120,RMAX:110,minus:[244,206,34,28],plus:[282,206,34,28],blips:[],edge:[],d:gfx($('rect'))};
function drawRect(planes){
  const d=Q.d,{CX,CY,RMAX}=Q,uD=units==='metric'?'km':'mi',rKm=RANGES[units][rangeIdx]*KM_PER[units],str=(s,x,y,sz,c)=>{d.g.fillStyle=c;d.g.font=(sz===2?16:9)+'px monospace';d.g.textAlign='left';d.g.textBaseline='top';d.g.fillText(s,x,y)};
  d.fillRect(0,0,320,240,'#000');
  if(mapOn)drawMap(d.g,CX,CY,RMAX,rKm);
  for(let i=1;i<=3;i++)d.drawCircle(CX+.5,CY+.5,Math.round(RMAX*i/3),'#005a00');
  d.drawLine(CX,CY-RMAX,CX,CY+RMAX,'#005a00');d.drawLine(CX-RMAX,CY,CX+RMAX,CY,'#005a00');
  const sw=(performance.now()/4000*Math.PI*2)%(Math.PI*2);
  for(let k=0;k<10;k++){const a=sw-k*.03;d.drawLine(CX,CY,CX+Math.sin(a)*RMAX,CY-Math.cos(a)*RMAX,`rgba(0,255,100,${(.35-k*.033).toFixed(3)})`)}
  str(T('N'),CX-3,CY-RMAX-10,1,'#c8c8c8');str(RANGES[units][rangeIdx]+uD,CX+4,CY-RMAX+4,1,'#c8c8c8');
  d.fillTriangle(CX,CY-6,CX-5,CY+5,CX+5,CY+5,'#3c8cff');
  const inRange=planes.filter(p=>p.dist<=rKm&&passFilter(p)).sort((a,b)=>a.dist-b.dist);Q.blips=[];let sel=null;
  for(const p of inRange){const r=p.dist/rKm*RMAX,a=p.brg*Math.PI/180,x=CX+Math.round(r*Math.sin(a)),y=CY-Math.round(r*Math.cos(a)),isSel=p.hex===selHex;if(isSel)sel=p;
    const col=isSel?'#ff3c3c':p.ground?'#8a939c':'#00ff64',t=(p.trk||0)*Math.PI/180;d.drawLine(x,y,x+Math.sin(t)*7,y-Math.cos(t)*7,col);d.fillCircle(x,y,isSel?3:2,col);if(isSel)d.drawCircle(x,y,7,col);Q.blips.push({x,y,hex:p.hex})}
  d.fillRect(236,0,84,240,'#101418');str(avions(inRange.length),242,6,1,'#c8c8c8');
  if(!sel){if(!inRange.length){str(T('Aucun trafic'),242,30,1,'#c8c8c8');str(T('à portée'),242,42,1,'#c8c8c8')}}
  else{const inf=info[sel.hex]||{},rt=inf.orig||inf.dest?`${inf.orig?inf.orig.code:'?'}>${inf.dest?inf.dest.code:'?'}`:'?';
    str((sel.flight||sel.hex).slice(0,8),242,24,2,'#e0b53c');
    [[T('CIE'),inf.airline||'?'],['ALT',Math.round(sel.alt)+'ft'],[T('VIT'),sel.gs!=null?Math.round(sel.gs)+'kt':'?'],['ROUTE',rt],['DIST',dec(units==='metric'?sel.dist:sel.dist/1.60934)+uD],['TYPE',sel.type||'?'],[T('IMMAT'),sel.reg||'?']]
      .forEach(([k,v],i)=>{str(k,242,44+i*22,1,'#7a8794');str(String(v).slice(0,13),242,53+i*22,1,'#c8c8c8')})}
  d.fillRect(...Q.minus,'#1e2830');d.fillRect(...Q.plus,'#1e2830');str('-',Q.minus[0]+13,Q.minus[1]+6,2,'#c8c8c8');str('+',Q.plus[0]+12,Q.plus[1]+6,2,'#c8c8c8');
  if(D.alerting&&Math.floor(performance.now()/300)%2){d.fillRect(4,222,54,14,'#ff3c3c');str(T('ALERTE'),8,225,1,'#fff')}
}

// posicao estimada: idade da leitura no ESP + idade da posicao do aviao (seen_pos), no maximo 45 s;
// depois disso o alvo para (e aparece vazado no Controleur) em vez de seguir uma posicao inventada
const dataAge=()=>(performance.now()-recvAt)/1000,posAge=p=>dataAge()+(p.sp||0),XTRAP_MAX=45;
function live(p,dt){let x=p.dist*Math.sin(p.brg*Math.PI/180),y=p.dist*Math.cos(p.brg*Math.PI/180);
  const t0=Math.min(dt+(p.sp||0),XTRAP_MAX);
  if(!p.ground&&p.gs!=null&&p.trk!=null&&t0>0){const k=p.gs*1.852/3600*t0,t=p.trk*Math.PI/180;x+=k*Math.sin(t);y+=k*Math.cos(t)}
  return{...p,dist:Math.hypot(x,y),brg:(Math.atan2(x,y)*180/Math.PI+360)%360,x,y}}
// ---------- engrenagem + menu de reglages (desenhados na propria tela, como no aparelho)
// redonda: embaixo, simetricos em volta das 6 horas: engrenagem logo depois das 6 e mudo do radio logo antes
const GEAR={round:[210,452,10],rect:[309,12,6]};
const MUTE={round:[256,452,10],rect:[294,12,6]};   // sempre visivel
const MENU={round:{x:98,y:58,w:270,h:352,top:76,tabs:100,sub:121,row0:144,step:22.8,fs:13},
  rect:{x:28,y:2,w:264,h:236,top:11,tabs:25,sub:38,row0:52,step:15.6,fs:10}};
let menuTab='radar';
function drawMute(d,[x,y,r]){   // alto-falante; mudo = vermelho com X; radio desligado = apagado
  const c=atcMuted?'#ff6b6b':atc.on?'#7a8794':'#3a4550',g=d.g;g.fillStyle=c;g.beginPath();
  g.moveTo(x-r*.9,y-r*.35);g.lineTo(x-r*.45,y-r*.35);g.lineTo(x,y-r*.8);g.lineTo(x,y+r*.8);g.lineTo(x-r*.45,y+r*.35);g.lineTo(x-r*.9,y+r*.35);g.fill();
  if(atcMuted){d.drawLine(x+r*.25,y-r*.4,x+r*.95,y+r*.4,c,r*.2);d.drawLine(x+r*.25,y+r*.4,x+r*.95,y-r*.4,c,r*.2)}
  else{g.strokeStyle=c;g.lineWidth=r*.16;for(const k of[.45,.85]){g.beginPath();g.arc(x+r*.05,y,r*k,-.8,.8);g.stroke()}}
}
function drawGear(d,[x,y,r]){
  for(let k=0;k<8;k++){const a=k*Math.PI/4;d.drawLine(x+Math.cos(a)*r*.55,y+Math.sin(a)*r*.55,x+Math.cos(a)*r,y+Math.sin(a)*r,'#7a8794',r*.32)}
  d.fillCircle(x,y,r*.68,'#7a8794');d.fillCircle(x,y,r*.3,'#000');
}
const SRC={wifi:'Wi-Fi',ip:'IP',manuel:'manuel',secrets:'config'};
const MODES=['Désactivées','Importantes','Toutes'],LANGS={fr:'Français',pt:'Português',en:'English'};
const cycleLang=()=>{const k=Object.keys(LANGS);lang=k[(k.indexOf(lang)+1)%k.length];document.documentElement.lang=LOC[lang];savePref()};
const menuRows=()=>menuTab==='radar'?[
  [T('Son'),D.soundOn,'sw',async()=>{await fetch('/sound',{method:'POST'});poll()}],
  [T('Alertes'),T(MODES[D.aMode??1]),'val',openAlerts],
  [T('Filtres'),filtersOn()?T('actifs'):T('aucun'),'val',openFilters],
  [T('Tour de contrôle'),atc.on?feedName(atc.feed):airportFeeds().length?T('Arrêt'):feedsPending?T('recherche…'):T('aucune'),'val',openTower],
  [T('Météo'),METAR.icaoId?`${METAR.fltCat||METAR.icaoId} · ${windShort(METAR)}`:'—','val',openMeteo],
  [T('Style'),style==='atc'?T('Contrôleur'):T('Classique'),'val',()=>{style=style==='atc'?'classic':'atc';follow=null;savePref()}],
  [T('Étiquettes'),sweepTags?T('Balayage'):T('Fixes'),'val',()=>{sweepTags=!sweepTags;savePref()}],
  [T('Traînée'),T(['Aucune','Courte','Moyenne','Longue'][trailIdx]),'val',()=>{trailIdx=(trailIdx+1)%TRAIL.length;savePref()}],
  [T('Vecteur'),`${VEC[vecIdx]} s`,'val',()=>{vecIdx=(vecIdx+1)%VEC.length;savePref()}],
  [T('Unités'),units==='metric'?'km':'mi','val',()=>{units=units==='metric'?'imperial':'metric';savePref()}],
  [T('Fermer'),'','close',()=>{menuOpen=false}]]:[
  [T('Réseau Wi-Fi'),D.ap?T('à configurer'):(D.ssid||'—'),'val',openWifi],
  [T('Position'),`${D.city||'?'} (${T(SRC[D.src]||D.src||'?')})`,'val',openPosition],
  [T('Aéroport (radio)'),atcAirport?`${atcAirport.icao}${atcAirport.auto?' · auto':''}`:'—','val',openAirport],
  [T('Langue'),LANGS[lang],'val',cycleLang],
  [T('Rotation'),`${rot}°`,'val',()=>{rot=(rot+90)%360;applyRot();savePref()}],
  [T('Texte'),bigText?T('Grand'):T('Normal'),'val',()=>{bigText=!bigText;savePref()}],
  [T('Diagnostic'),'▶','val',openDiag],
  [T('Mise à jour'),'▶','val',openUpdate],
  [T('Fermer'),'','close',()=>{menuOpen=false}]];
function drawMenu(d){
  const m=MENU[mode],C={card:'rgba(8,13,19,0.96)',dim:'#7a8794',text:'#e6edf3',on:'#00c853',off:'#3a4550',accent:'#e0b53c'};
  d.fillRoundRect(m.x,m.y,m.w,m.h,18,C.card);d.g.strokeStyle='rgba(201,162,39,.25)';d.g.lineWidth=1;d.g.beginPath();d.g.roundRect(m.x+.5,m.y+.5,m.w-1,m.h-1,18);d.g.stroke();
  d.text(T('Réglages'),m.x+m.w/2,m.top,m.fs+3,C.accent,'center',true);
  // abas: Radar | Appareil
  const tw=(m.w-36)/2,th=m.fs+8;
  [['radar','Radar'],['app','Appareil']].forEach(([k,l],i)=>{const x=m.x+18+i*tw;
    d.fillRoundRect(x,m.tabs-th/2,tw-4,th,th/2,menuTab===k?C.accent:'#18212c');d.text(T(l),x+(tw-4)/2,m.tabs,m.fs-2,menuTab===k?'#111':C.text,'center',true)});
  d.fitText(conn?(D.ok?`${D.city} · ${avions(D.planes.length)} · ${T('données il y a {s} s',{s:Math.round(dataAge())})}`:D.ap?T('mode configuration · réseau {n}',{n:D.apName}):T('ESP sans données adsb.lol')):T('pas de connexion avec l’ESP32'),m.x+m.w/2,m.sub,m.fs-4,m.w-30,conn&&D.ok?C.dim:'#ff6b6b');
  const rows=menuRows();
  rows.forEach(([label,v,kind],i)=>{
    const y=m.row0+i*m.step,xl=m.x+18,xr=m.x+m.w-18;
    if(kind==='close'){d.fillRoundRect(m.x+m.w/2-50,y-m.step*.38,100,m.step*.76,m.step*.38,'#18212c');d.text(label,m.x+m.w/2,y,m.fs-1,C.text,'center',true);return}
    d.text(label,xl,y,m.fs-1,C.text);
    if(kind==='sw'){const h=m.fs+6,w=h*1.8;d.fillRoundRect(xr-w,y-h/2,w,h,h/2,v?C.on:C.off);d.fillCircle(v?xr-h/2:xr-w+h/2,y,h/2-3,'#fff')}
    else{d.g.font=`700 ${m.fs-1}px system-ui,sans-serif`;let t=String(v);while(t.length>3&&d.g.measureText(t).width>m.w*.45)t=t.slice(0,-2)+'…';
      d.text(t,xr,y,m.fs-1,kind==='info'?C.dim:C.accent,'right',true)}
    if(i<rows.length-2)d.drawLine(xl,y+m.step/2,xr,y+m.step/2,'rgba(255,255,255,.06)');
  });
}
async function menuTap(x,y){
  const m=MENU[mode];
  if(x<m.x||x>m.x+m.w||y<m.y||y>m.y+m.h){menuOpen=false;return}   // toque fora do menu fecha
  if(Math.abs(y-m.tabs)<(m.fs+8)/2+3){menuTab=x<m.x+m.w/2?'radar':'app';return}
  const r=menuRows()[Math.round((y-m.row0)/m.step)];   // cada linha traz a sua acao
  if(r&&r[3])await r[3]();
}
// ---------- torre de controle: o ESP descobre os canais da LiveATC (fica salvo na flash) e repassa o audio;
// a frequencia de radio so tem som quando alguem fala: silencio e normal.
// Aeroporto da radio: automatico (o mais perto que tem canais) ou escolhido em Reglages > Appareil > Aeroport (radio).
let atcFeeds=[],atcAirport=null,atcNear=[],feedsPending=false,feedsKey='',feedsAt=0;
const feedName=m=>{const f=atcFeeds.find(f=>f.mount===m);return f?f.title:(m||'—')};
const airportFeeds=()=>atcAirport?atcFeeds.filter(f=>f.icao===atcAirport.icao):[];
async function loadFeeds(force){
  if(!APTS.length||(!D.lat&&!D.lon))return;   // espera a lista de aeroportos (loadAirports)
  const k=Math.cos(D.lat*Math.PI/180);
  atcNear=APTS.map(a=>({icao:a[0],km:Math.hypot((a[1]-D.lat)*111,(a[2]-D.lon)*111*k)}))
    .filter(a=>a.km<=100).sort((a,b)=>a.km-b.km).slice(0,6);
  const list=atcNear.map(a=>a.icao);if(atcIcao&&!list.includes(atcIcao))list.unshift(atcIcao);   // escolhido a mao (pode ser longe)
  const key=list.join(',');
  if(!key||(!force&&((key===feedsKey&&!feedsPending)||Date.now()-feedsAt<5000)))return;   // a descoberta leva ~1 min: pergunta a cada 5 s
  feedsAt=Date.now();
  try{
    const j=await(await fetch(`/atc/feeds?icao=${key}`,{cache:'no-store'})).json();
    atcFeeds=j.feeds||[];feedsPending=!!j.pending;feedsKey=key;
    const icao=atcIcao||(atcNear.find(a=>atcFeeds.some(f=>f.icao===a.icao))||atcNear[0]||{}).icao;
    atcAirport=icao?{icao,auto:!atcIcao,km:(atcNear.find(a=>a.icao===icao)||{}).km}:null;
  }catch(e){}
}
let atcAudio=null,atcAn=null,atcBuf=null,atcGain=null,atcMuted=false;
function atcPlay(m){
  atcStop();
  try{actx=actx||new AudioContext();actx.resume()}catch(e){}
  const a=new Audio();a.crossOrigin='anonymous';a.src=`http://${location.hostname}:81/${m}`;
  a.onplaying=()=>{if(atcAudio===a)atc.live=true};
  a.onerror=a.onended=()=>{if(atcAudio!==a)return;atc.live=false;setTimeout(()=>{if(atcAudio===a)atcPlay(m)},5000)};   // caiu: reconecta
  atcAudio=a;atc={on:true,feed:m,live:false};
  // analisador para a onda de fala (o ESP libera CORS, senao o navegador entregaria silencio)
  try{const s=actx.createMediaElementSource(a);atcAn=actx.createAnalyser();atcAn.fftSize=2048;atcGain=actx.createGain();atcGain.gain.value=atcMuted?0:1;
    s.connect(atcAn);atcAn.connect(atcGain);atcGain.connect(actx.destination);atcBuf=new Float32Array(atcAn.fftSize)}catch(e){atcAn=atcGain=null;a.muted=atcMuted}
  a.play().catch(()=>{});
}
// mudo: o radio continua conectado e a onda de fala continua aparecendo, so o som e cortado
function atcMute(){atcMuted=!atcMuted;if(atcGain)atcGain.gain.value=atcMuted?0:1;else if(atcAudio)atcAudio.muted=atcMuted}
function atcStop(){
  if(atcAudio){const a=atcAudio;atcAudio=null;a.onerror=a.onended=null;a.pause();a.removeAttribute('src');a.load()}
  atcAn=null;atc={on:false,feed:null,live:false};voice.lv=[];voice.talk=false;
}

// ---------- METAR do aeroporto da radio (o ESP busca na aviationweather.gov a cada 10 min)
let METAR={},metarAt=0;
const FLTCOL={VFR:'#3ddc84',MVFR:'#4ea8ff',IFR:'#ff4d4d',LIFR:'#ff4dd8'};
const metarIcao=()=>(atcAirport&&atcAirport.icao)||(atcNear[0]&&atcNear[0].icao)||'';
async function loadMetar(force){
  const icao=metarIcao();if(!icao||(!force&&Date.now()-metarAt<300000&&METAR.icaoId===icao))return;
  metarAt=Date.now();
  try{const j=await(await fetch(`/metar?icao=${icao}`,{cache:'no-store'})).json();if(j.icaoId)METAR=j}catch(e){}
}
const windTxt=m=>m.wspd==null?'':m.wspd===0?T('vent calme'):`${T('vent')} ${m.wdir==='VRB'?T('variable'):String(m.wdir).padStart(3,'0')+'°'} ${m.wspd} kt${m.wgst?' ('+T('rafales')+' '+m.wgst+')':''}`;
const windShort=m=>m.wspd==null?'—':m.wspd===0?T('calme'):`${m.wdir==='VRB'?'VRB':String(m.wdir).padStart(3,'0')}/${String(m.wspd).padStart(2,'0')}${m.wgst?'G'+m.wgst:''}`;
const WX={RA:'pluie',DZ:'bruine',SN:'neige',SG:'neige en grains',PL:'granules de glace',GR:'grêle',GS:'grésil',UP:'précipitations',BR:'brume',FG:'brouillard',HZ:'brume sèche',FU:'fumée',SA:'sable',DU:'poussière',TS:'orage',SH:'averses',FZ:'verglaçant',BL:'chasse',VC:'au voisinage',SQ:'grains',FC:'tornade'};
const wxTxt=s=>(s||'').split(' ').filter(Boolean).map(w=>{const i=w[0]==='-'?T('faible')+' ':w[0]==='+'?T('fort')+' ':'';return i+(w.replace(/^[-+]/,'').match(/../g)||[]).map(c=>T(WX[c]||c)).join(' ')}).join(', ');

// ---------- janelas (Wi-Fi, posicao, alertas...): HTML por cima da tela, DENTRO dela
// (no quadrado inscrito no circulo ou no retangulo), girando junto com a tela
function fitDlg(){
  const el=$('dlg'),box=el.querySelector('.box');if(el.hidden||!box)return;
  const cv=mode==='round'?$('round'):$('rect'),r=cv.getBoundingClientRect(),W=cv.offsetWidth,H=cv.offsetHeight;
  // .68 do diametro: os cantos (arredondados) ficam dentro do circulo, com folga
  const w=mode==='round'?W*.68:W*.92,h=mode==='round'?H*.68:H*.92,pad=Math.round(w*.07);
  Object.assign(box.style,{left:r.left+r.width/2-w/2+'px',top:r.top+r.height/2-h/2+'px',width:w+'px',height:h+'px',
    padding:pad+'px',borderRadius:mode==='round'?Math.round(w*.16)+'px':'10px',transform:`rotate(${rot}deg)`});
}
addEventListener('resize',fitDlg);
function dlg(html){const el=$('dlg');el.innerHTML=`<div class="box">${html}</div>`;el.hidden=false;fitDlg();el.onclick=e=>{if(e.target===el||e.target.dataset.close!==undefined)el.hidden=true};return el}
const escH=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function applyRot(){for(const id of['round','rect'])$(id).style.transform=rot?`rotate(${rot}deg)`:'';fitDlg()}

// ---------- Wi-Fi: ate 3 redes salvas (o radar entra na mais forte por perto); senha pelo teclado virtual
async function openWifi(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Réseau Wi-Fi')}</h3><div id="ws"><p class="dim">${D.ap?T('Le radar est en mode configuration (réseau <b>{n}</b>). Choisissez le Wi-Fi de la maison :',{n:escH(D.apName)}):T('Connecté à <b>{n}</b>. Choisissez un autre réseau :',{n:escH(D.ssid)})}</p><ul id="wl"><li class="dim">${T('Recherche des réseaux…')}</li></ul>
    <h4>${T('Réseaux enregistrés (3 au maximum)')}</h4><ul id="wk"></ul><p class="dim">${T('Le radar se connecte au plus fort de ces réseaux et y revient tout seul quand il réapparaît.')}</p></div><div id="wf"></div><button data-close>${T('Fermer')}</button>`);
  const ws=el.querySelector('#ws'),wf=el.querySelector('#wf');
  const known=async j=>{try{j=j||await(await fetch('/wifi/list',{cache:'no-store'})).json()}catch(e){return}
    el.querySelector('#wk').innerHTML=(j.nets||[]).map(n=>`<li class="kv"><span>${escH(n)}${n===j.cur?' · <b>'+T('connecté')+'</b>':''}</span><button data-f="${escH(n)}" style="margin:0;padding:4px 10px">✕</button></li>`).join('')||`<li class="kv dim">${T('aucun')}</li>`;
    el.querySelector('#wk').onclick=async e=>{const b=e.target.closest('button[data-f]');if(!b||!confirm(T('Oublier le réseau {n} ?',{n:b.dataset.f})))return;
      try{known(await(await fetch('/wifi/forget',{method:'POST',body:new URLSearchParams({ssid:b.dataset.f})})).json())}catch(e2){}}};
  const save=async n=>{
    const wp=wf.querySelector('#wp'),b=new URLSearchParams({ssid:n.ssid,pass:wp?wp.value:''});
    try{await fetch('/wifi/save',{method:'POST',body:b})}catch(e){}
    el.querySelector('.box').innerHTML=`<h3>${T('Connexion…')}</h3><p>${T('Le radar redémarre et se connecte à <b>{n}</b>.',{n:escH(n.ssid)})}</p><p class="dim">${T('Remettez votre téléphone sur ce réseau et ouvrez <b>http://radar.local</b>. Si le mot de passe est faux, le radar revient au mode configuration (réseau {a}).',{a:escH(D.apName||'S643-XXXX')})}</p>`;
  };
  // rede escolhida: a lista some e entra o campo da senha com o teclado virtual (a tela do aparelho so tem toque)
  const ask=n=>{
    ws.hidden=true;
    wf.innerHTML=`<p>${T('Réseau')} <b>${escH(n.ssid)}</b></p>`+(n.open?`<p class="dim">${T('Réseau ouvert : pas de mot de passe.')}</p>`
      :`<div class="row"><input id="wp" type="password" autocomplete="off" spellcheck="false" placeholder="${T('mot de passe')}"><button id="we" aria-label="👁">👁</button></div><div class="vk" id="vk"></div>`)
      +`<div class="row"><button id="wb">← ${T('Réseaux')}</button><button id="wc" class="go">${T('Connecter')}</button></div>`;
    const wp=wf.querySelector('#wp');
    if(wp){vkbd(wf.querySelector('#vk'),wp,()=>save(n));wp.onkeydown=e=>{if(e.key==='Enter')save(n)};
      wf.querySelector('#we').onclick=()=>{wp.type=wp.type==='password'?'text':'password';wp.focus()};wp.focus()}
    wf.querySelector('#wb').onclick=()=>{wf.innerHTML='';ws.hidden=false};
    wf.querySelector('#wc').onclick=()=>save(n);
  };
  known();
  try{
    const list=await(await fetch('/wifi/scan',{cache:'no-store'})).json();
    const bars=r=>r>-55?'▂▄▆█':r>-67?'▂▄▆':r>-78?'▂▄':'▂';
    el.querySelector('#wl').innerHTML=list.length?list.map((n,i)=>`<li data-i="${i}"><span>${escH(n.ssid)}${n.open?'':' 🔒'}</span><span class="dim">${bars(n.rssi)}</span></li>`).join(''):`<li class="dim">${T('Aucun réseau trouvé.')}</li>`;
    el.querySelector('#wl').onclick=e=>{const li=e.target.closest('li[data-i]');if(li)ask(list[+li.dataset.i])};
  }catch(e){el.querySelector('#wl').innerHTML=`<li class="dim">${T('Le radar ne répond pas.')}</li>`}
}
// ---------- teclado virtual: a tela do aparelho e so de toque (no PC o teclado fisico tambem funciona).
// Letras (⇧ = proxima maiuscula; dois toques = trava), numeros e simbolos; ↵ confirma.
const VK={abc:[[...'qwertyuiop'],[...'asdfghjkl'],['⇧',...'zxcvbnm','⌫'],['?123','␣','.','↵']],
  num:[[...'1234567890'],[...'@#$%&*-+()'],['#+=',...'!"\':;/?','⌫'],['ABC','␣',',','↵']],
  sym:[[...'[]{}<>^~|\\'],[...'_=€£¥`°§•¬'],['?123',...'¿¡«»…·¨','⌫'],['ABC','␣','.','↵']]};
function vkbd(host,input,onEnter){
  let layer='abc',shift=0,shiftAt=0;   // shift: 0 = minusculas, 1 = so a proxima letra, 2 = travado
  input.inputMode='none';   // no celular, nao abre o teclado do sistema por cima
  const sel=()=>[input.selectionStart??input.value.length,input.selectionEnd??input.value.length];
  const edit=(a,b,s)=>{input.value=input.value.slice(0,a)+s+input.value.slice(b);try{input.setSelectionRange(a+s.length,a+s.length)}catch(e){}};
  const render=()=>{host.innerHTML=VK[layer].map(row=>`<div>${row.map(k=>{
      const fn=k.length>1||'⇧⌫↵'.includes(k),cls=k==='␣'?'sp':fn?'fn':'',up=layer==='abc'&&shift&&!fn;
      return`<button type="button" data-k="${escH(k)}" class="${cls}${k==='⇧'&&shift?' on':''}${k==='↵'?' go':''}">${k==='␣'?T('espace'):k==='⇧'&&shift===2?'⇪':escH(up?k.toUpperCase():k)}</button>`}).join('')}</div>`).join('')};
  host.onpointerdown=e=>{
    const b=e.target.closest('button[data-k]');if(!b)return;e.preventDefault();
    const k=b.dataset.k,[a,z]=sel();
    if(k==='⇧'){const now=performance.now();shift=shift===1&&now-shiftAt<400?2:shift?0:1;shiftAt=now}
    else if(k==='⌫'){if(a!==z)edit(a,z,'');else if(a>0)edit(a-1,a,'')}
    else if(k==='?123')layer='num';else if(k==='ABC')layer='abc';else if(k==='#+=')layer='sym';
    else if(k==='↵'){if(onEnter)onEnter();return}
    else{edit(a,z,k==='␣'?' ':layer==='abc'&&shift?k.toUpperCase():k);if(shift===1)shift=0}
    render();input.focus();
  };
  render();
}
// teclado virtual embaixo de um campo, criado no primeiro toque nele
function vkOnce(input,onEnter){if(input._vk)return;input._vk=1;const d=document.createElement('div');d.className='vk';(input.closest('.row')||input).after(d);vkbd(d,input,onEnter)}

// ---------- torre de controle: canais do aeroporto da radio, um toque para ouvir
function openTower(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Tour de contrôle')}</h3><div id="tw"></div><button data-close>${T('Fermer')}</button>`);
  const render=()=>{
    const box=el.querySelector('#tw');if(!box||el.hidden)return;
    const ap=atcAirport,fs=airportFeeds();
    const head=`<p class="dim">${T('Aéroport')} : <b>${escH(ap?ap.icao:'—')}</b>${ap&&ap.auto?' ('+T('le plus proche')+')':''} · <a href="#" id="tc" style="color:#e0b53c">${T('changer')}</a></p>`;
    if(!fs.length){box.innerHTML=head+`<p class="dim">${feedsPending?T('Recherche des canaux de cet aéroport…'):T('Aucun canal pour cet aéroport.')}</p>`;
      if(feedsPending)setTimeout(()=>loadFeeds(true).then(render),4000)}
    else box.innerHTML=head+(atc.on?`<p>${T('En écoute')} : <b>${escH(feedName(atc.feed))}</b></p><button id="ts" class="go">■ ${T('Arrêter le son')}</button>`:`<p class="dim">${T('Choisissez un canal pour l’écouter sur cet appareil.')}</p>`)
      +`<ul>${fs.map(f=>`<li data-m="${escH(f.mount)}"${atc.on&&atc.feed===f.mount?' class="on"':''}><span>${escH(f.title.replace(f.icao,'').trim()||f.title)}</span><span>${atc.on&&atc.feed===f.mount?(atc.live?'🔊':'…'):'▶'}</span></li>`).join('')}</ul>`
      +`<p class="dim">${T('Le silence est normal : on n’entend quelque chose que quand un pilote ou le contrôleur parle.')}</p>`;
    box.onclick=e=>{
      if(e.target.id==='tc'){e.preventDefault();openAirport();return}
      if(e.target.id==='ts'){atcStop();render();return}
      const li=e.target.closest('li[data-m]');if(!li)return;const m=li.dataset.m;
      if(atc.on&&atc.feed===m)atcStop();else atcPlay(m);   // tocar no canal que esta tocando = parar
      render()};
    if(atc.on&&!atc.live)setTimeout(render,500);   // conectando: atualiza quando o som comecar
  };
  render();
}
// aeroporto da radio: automatico ou um dos proximos; tambem aceita qualquer codigo OACI (o ESP procura os canais)
function openAirport(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Aéroport de la radio')}</h3><p class="dim">${T('Par défaut, le radar écoute l’aéroport le plus proche qui a des canaux.')}</p><ul id="al"></ul>
    <p>${T('Autre aéroport (code OACI) :')}</p><div class="row"><input id="ai" maxlength="4" autocapitalize="characters" placeholder="${T('ex. CYYZ')}"><button id="ab">OK</button></div><button data-close>${T('Fermer')}</button>`);
  const n=i=>atcFeeds.filter(f=>f.icao===i).length,cn=i=>n(i)?T(n(i)>1?'{n} canaux':'{n} canal',{n:n(i)}):feedsPending?T('recherche…'):T('aucun canal');
  const render=()=>{if(el.hidden)return;
    el.querySelector('#al').innerHTML=`<li data-i=""${atcIcao?'':' class="on"'}><span>${T('Automatique')}</span><span class="dim">${T('le plus proche')}</span></li>`
      +atcNear.map(a=>`<li data-i="${a.icao}"${atcIcao===a.icao?' class="on"':''}><span>${a.icao}</span><span class="dim">${Math.round(a.km)} km · ${cn(a.icao)}</span></li>`).join('')
      +(atcIcao&&!atcNear.some(a=>a.icao===atcIcao)?`<li data-i="${atcIcao}" class="on"><span>${atcIcao}</span><span class="dim">${cn(atcIcao)}</span></li>`:'');
    if(feedsPending)setTimeout(()=>loadFeeds(true).then(render),4000)};
  const pick=async i=>{atcIcao=i;savePref();await loadFeeds(true);loadMetar(true);render()};
  el.querySelector('#al').onclick=e=>{const li=e.target.closest('li[data-i]');if(li)pick(li.dataset.i)};
  const go=()=>{const v=el.querySelector('#ai').value.trim().toUpperCase();if(/^[A-Z0-9]{4}$/.test(v))pick(v)};
  el.querySelector('#ab').onclick=go;el.querySelector('#ai').onkeydown=e=>{if(e.key==='Enter')go()};
  el.querySelector('#ai').onfocus=e=>vkOnce(e.target,go);
  render();
}
// ---------- alertas: so o que importa (o modo "Importantes" nao bipa a cada aviao novo, bom perto de aeroporto)
const chips=(id,opts,cur,multi)=>`<div class="chips" id="${id}">${opts.map(([v,l])=>`<button data-v="${v}" class="${(multi?cur.includes(v):cur==v)?'on':''}">${l}</button>`).join('')}</div>`;
const RADII=[2,5,10],CEILS=[3000,5000,10000],VOLS=['Faible','Moyen','Fort'];
function openAlerts(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Alertes')}</h3><div id="ac"></div><h4>${T('Dernières alertes')}</h4><ul id="ah"><li class="kv dim">…</li></ul><button data-close>${T('Fermer')}</button>`);
  const send=async o=>{try{const j=await(await fetch('/alertcfg',{method:'POST',body:new URLSearchParams(o)})).json();D={...D,...j};render()}catch(e){}};
  const render=()=>{
    const m=D.aMode??1,ri=RADII.indexOf(D.aKm),ci=CEILS.indexOf(D.aFt);
    const sum=m===0?T('Aucune alerte : le radar reste silencieux.')
      :T('Proximité : avions à moins de {r} km et sous {c} ft.',{r:D.aKm,c:fr(D.aFt)})+' '+(D.aEm?T('Urgences signalées.'):'')+' '+(m===2&&D.aNew?T('Bip court pour chaque nouvel avion.'):'');
    el.querySelector('#ac').innerHTML=`<h4>${T('Mode')}</h4>`+chips('am',MODES.map((l,i)=>[i,T(l)]),m)
      +`<h4>${T('Rayon de proximité')}</h4>`+chips('ar',RADII.map((r,i)=>[i,r+' km']),ri)
      +`<h4>${T('Altitude maximale')}</h4>`+chips('ai2',CEILS.map((c,i)=>[i,fr(c)+' ft']),ci)
      +`<h4>${T('Événements')}</h4>`+chips('ae',[['e',T('Urgences (7500 / 7600 / 7700)')],['n',T('Nouvel avion (mode Toutes)')]],[D.aEm?'e':'',D.aNew?'n':''],true)
      +`<h4>${T('Volume des bips (cet appareil)')}</h4>`+chips('av',VOLS.map((l,i)=>[i,T(l)]),volIdx)
      +`<p class="note">${sum}</p><button id="at">${T('Tester le bip')}</button>`;
    const on=(id,f)=>el.querySelector('#'+id).onclick=e=>{const b=e.target.closest('button[data-v]');if(b)f(b.dataset.v)};
    on('am',v=>send({m:v}));on('ar',v=>send({r:v}));on('ai2',v=>send({c:v}));
    on('ae',v=>send(v==='e'?{e:D.aEm?0:1}:{n:D.aNew?0:1}));
    on('av',v=>{volIdx=+v;savePref();beep(true);render()});
    el.querySelector('#at').onclick=()=>{fetch('/beep',{method:'POST'});beep(true)};
  };
  render();
  fetch('/alerts',{cache:'no-store'}).then(r=>r.json()).then(l=>{if(el.hidden)return;
    el.querySelector('#ah').innerHTML=l.length?l.map(a=>`<li class="kv"><span><b>${escH(a.cs)}</b> ${a.k==='E'?'<span style="color:#ff4d4d">'+escH(a.sq||'')+' '+T(EMERG[a.sq]||'URGENCE')+'</span>':T('proximité')}</span><span class="dim">${dec(+a.km)} km · ${fr(a.alt)} ft · ${ago(a.ago)}</span></li>`).join('')
      :`<li class="kv dim">${T('Aucune alerte depuis le démarrage.')}</li>`}).catch(()=>{});
}
const ago=s=>s<60?T('il y a {n} s',{n:s}):s<3600?T('il y a {n} min',{n:Math.round(s/60)}):T('il y a {n} h',{n:Math.round(s/3600)});
// ---------- filtros
function openFilters(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Filtres')}</h3><div id="fc"></div><h4>${T('Recherche (indicatif, immatriculation, type)')}</h4><div class="row"><input id="fq" spellcheck="false" placeholder="${T('ex. ACA, C-G, A320')}"><button id="fx">✕</button></div><button data-close>${T('Fermer')}</button>`);
  const q=el.querySelector('#fq');q.value=F.q;
  const render=()=>{
    const shown=D.planes.filter(passFilter).length;
    el.querySelector('#fc').innerHTML=`<p class="note">${T('{a} avions affichés sur {b} reçus.',{a:shown,b:D.planes.length})}</p>`
      +`<h4>${T('Altitude')}</h4>`+chips('fa',ALTS.map((a,i)=>[i,T(a[2])]),F.alt)
      +`<h4>${T('Catégories (aucune choisie = toutes)')}</h4>`+chips('fk',CATS.map(c=>[c[0],T(c[1])]),F.cats,true)
      +`<h4>${T('Autres')}</h4>`+chips('fo',[['g',T('Avions au sol')],['m',T('Militaires seulement')]],[F.ground?'g':'',F.mil?'m':''],true)
      +`<p class="dim">${T('« Militaire » seulement quand la base de données de l’adsb.lol l’indique.')}</p>`;
    const on=(id,f)=>el.querySelector('#'+id).onclick=e=>{const b=e.target.closest('button[data-v]');if(b){f(b.dataset.v);savePref();render()}};
    on('fa',v=>F.alt=+v);
    on('fk',v=>{F.cats=F.cats.includes(v)?F.cats.filter(x=>x!==v):[...F.cats,v]});
    on('fo',v=>{if(v==='g')F.ground=!F.ground;else F.mil=!F.mil});
  };
  const setQ=()=>{F.q=q.value.trim();savePref();render()};
  q.oninput=setQ;q.onfocus=()=>vkOnce(q,setQ);q.onkeyup=setQ;
  el.querySelector('#fx').onclick=()=>{q.value='';setQ()};
  render();
}
// ---------- meteo (METAR decodificado)
async function openMeteo(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Météo')}</h3><div id="mc"><p class="dim">…</p></div><button data-close>${T('Fermer')}</button>`);
  await loadMetar(true);if(el.hidden)return;
  const m=METAR,box=el.querySelector('#mc');
  if(!m.icaoId){box.innerHTML=`<p class="dim">${T('METAR indisponible pour {a}.',{a:escH(metarIcao()||'—')})}</p>`;return}
  const age=Math.round((Date.now()/1000-m.obsTime)/60),vis=m.visib==null?'—':(String(m.visib).includes('+')?'10+ SM':`${m.visib} SM (${dec(parseFloat(m.visib)*1.609)} km)`);
  const cl=(m.clouds||[]).map(c=>`${c.cover}${c.base!=null?' '+fr(c.base)+' ft':''}`).join(', ')||T('ciel dégagé');
  const rows=[[T('Catégorie de vol'),`<b style="color:${FLTCOL[m.fltCat]||'#fff'}">${m.fltCat||'—'}</b>`],[T('Vent'),windTxt(m)||'—'],[T('Visibilité'),vis],
    [T('Nuages'),cl],[T('Phénomènes'),wxTxt(m.wxString)||'—'],[T('Température / point de rosée'),m.temp!=null?`${m.temp}°C / ${m.dewp}°C`:'—'],[T('Pression (QNH)'),m.altim?`${Math.round(m.altim)} hPa`:'—']];
  box.innerHTML=`<p><b>${escH(m.icaoId)}</b> · ${escH(m.name||'')}</p><p class="dim">${T('Observation {t}',{t:ago(age*60)})}</p>`
    +`<ul>${rows.map(([k,v])=>`<li class="kv"><span class="dim">${k}</span><span>${v}</span></li>`).join('')}</ul><pre>${escH(m.rawOb||'')}</pre>`;
}
// ---------- diagnostico: memoria, rede e desempenho (atualiza a cada 2 s)
const RESETS=['?','mise sous tension','externe','logiciel','panique','chien de garde','chien de garde','chien de garde','sortie de veille','baisse de tension','SDIO'];
let fps=0;
function openDiag(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Diagnostic')}</h3><ul id="dg"><li class="kv dim">…</li></ul><button data-close>${T('Fermer')}</button>`);
  const kb=n=>n<0?T('absente'):`${fr(n/1024)} KiB`,up=s=>`${Math.floor(s/86400)} j ${Math.floor(s%86400/3600)} h ${Math.floor(s%3600/60)} min`;
  const tick=async()=>{if(el.hidden||!el.querySelector('#dg'))return;
    try{const g=await(await fetch('/diag',{cache:'no-store'})).json();
      const rows=[[T('Version'),g.ver],[T('En marche depuis'),up(g.up)],[T('Dernier redémarrage'),T(RESETS[g.reset]||'?')],
        [T('Wi-Fi'),`${escH(g.ssid||'—')} · ${g.rssi} dBm`],[T('Adresse'),g.ip],
        [T('Mémoire libre'),kb(g.heap)],[T('Plus grand bloc libre'),kb(g.block)],[T('Minimum atteint'),kb(g.heapMin)],['PSRAM',kb(g.psram)],
        [T('Programme'),`${fr(g.sketch/1024)} / ${fr(g.slot/1024)} KiB`],
        [T('ADS-B : dernière requête'),`${g.pollMs} ms · ${T('rayon {r} km',{r:Math.round(g.qKm)})}`],[T('ADS-B : requêtes / échecs'),`${g.polls} / ${g.fails}${g.lastFail?' · '+T('dernier code {c}',{c:g.lastFail}):''}`],
        [T('Avions reçus'),`${g.planes} · ${T('données il y a {s} s',{s:g.age})}`],[T('Page : images/s'),String(fps)]];
      el.querySelector('#dg').innerHTML=rows.map(([k,v])=>`<li class="kv"><span class="dim">${k}</span><span>${v}</span></li>`).join('');
    }catch(e){el.querySelector('#dg').innerHTML=`<li class="kv dim">${T('Le radar ne répond pas.')}</li>`}
    setTimeout(tick,2000)};
  tick();
}
// ---------- atualizacao pelo Wi-Fi: envia o .bin compilado; o ESP confere o arquivo antes de trocar e reinicia
function openUpdate(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Mise à jour')}</h3><p class="dim" id="uv">…</p><p>${T('Choisissez le fichier .bin du nouveau micrologiciel :')}</p><input id="uf" type="file" accept=".bin">
    <div class="bar"><div id="ub"></div></div><p id="um" class="dim">${T('Ne débranchez pas le radar pendant l’envoi.')}</p><button id="ug" class="go">${T('Envoyer')}</button><button data-close>${T('Fermer')}</button>`);
  fetch('/diag').then(r=>r.json()).then(g=>{el.querySelector('#uv').textContent=T('Version actuelle : {v}',{v:g.ver})}).catch(()=>{});
  const msg=t=>el.querySelector('#um').textContent=t;
  el.querySelector('#ug').onclick=()=>{
    const f=el.querySelector('#uf').files[0];if(!f){msg(T('Choisissez un fichier .bin.'));return}
    const x=new XMLHttpRequest(),fd=new FormData();fd.append('fw',f,f.name);
    x.upload.onprogress=e=>{if(e.lengthComputable){el.querySelector('#ub').style.width=(100*e.loaded/e.total)+'%';msg(T('Envoi… {p} %',{p:Math.round(100*e.loaded/e.total)}))}};
    x.onload=()=>{let j={};try{j=JSON.parse(x.responseText)}catch(e){}
      if(j.ok){msg(T('Mise à jour réussie. Le radar redémarre…'));setTimeout(()=>location.reload(),12000)}else msg(T('Échec : {e}',{e:j.err||x.status}))};
    x.onerror=()=>msg(T('Échec de l’envoi.'));
    x.open('POST','/update');x.send(fd);el.querySelector('#ug').disabled=true;
  };
}
function openPosition(){
  menuOpen=false;
  const el=dlg(`<h3>${T('Position du radar')}</h3><p class="dim">${T('Actuelle : <b>{c}</b> ({s}). Sans GPS, le radar se situe par les réseaux Wi-Fi voisins, par l’adresse IP ou par la ville choisie ici.',{c:escH(D.city||'?'),s:T(SRC[D.src]||D.src||'?')})}${D.acc?' '+T('Précision : ± {a}.',{a:D.acc>=1000?Math.round(D.acc/1000)+' km':D.acc+' m'}):''}</p>
    <div class="row"><button id="lw" class="go">${T('Détecter par Wi-Fi')}</button><button id="li">${T('Par l’IP')}</button></div>
    <p>${T('Ou choisissez la ville :')}</p><div class="row"><input id="lq" placeholder="${T('ville ou code postal')}"><button id="ls">${T('Chercher')}</button></div><ul id="lr"></ul><p id="lm" class="dim"></p><button data-close>${T('Fermer')}</button>`);
  const msg=t=>el.querySelector('#lm').textContent=t;
  const locate=async m=>{msg(T('Recherche…'));try{const j=await(await fetch('/locate',{method:'POST',body:new URLSearchParams({method:m})})).json();
    if(j.ok===false){msg(m==='wifi'?T('Les réseaux d’ici ne sont pas encore cartographiés. Essayez par l’IP ou la ville.'):T('Échec par l’IP.'));return}
    D={...D,...j};msg(T('Position : {c}',{c:j.city}));aptsKey='';feedsKey='';loadAirports(j.lat,j.lon);loadFeeds()}catch(e){msg(T('Le radar ne répond pas.'))}};
  el.querySelector('#lw').onclick=()=>locate('wifi');el.querySelector('#li').onclick=()=>locate('ip');
  const search=async()=>{const q=el.querySelector('#lq').value.trim();if(!q)return;msg(T('Recherche…'));
    try{const j=await(await fetch(`https://geocoding-api.open-meteo.com/v1/search?count=6&language=${lang}&name=${encodeURIComponent(q)}`)).json();
      const r=j.results||[];msg(r.length?'':T('Aucune ville trouvée.'));
      el.querySelector('#lr').innerHTML=r.map((c,i)=>`<li data-i="${i}"><span>${escH(c.name)}</span><span class="dim">${escH([c.admin1,c.country].filter(Boolean).join(', '))}</span></li>`).join('');
      el.querySelector('#lr').onclick=async e=>{const li=e.target.closest('li[data-i]');if(!li)return;const c=r[+li.dataset.i];
        const j2=await(await fetch('/location',{method:'POST',body:new URLSearchParams({lat:c.latitude,lon:c.longitude,city:c.name})})).json();
        D={...D,...j2};msg(T('Position : {c}',{c:c.name}));el.querySelector('#lr').innerHTML='';aptsKey='';feedsKey='';loadAirports(c.latitude,c.longitude);loadFeeds()}}
    catch(e){msg(T('Recherche impossible (pas d’internet ?).'))}};
  el.querySelector('#ls').onclick=search;el.querySelector('#lq').onkeydown=e=>{if(e.key==='Enter')search()};
  el.querySelector('#lq').onfocus=e=>vkOnce(e.target,search);
}

// ---------- onda de fala: quando a torre estiver tocando, mostra as barras de volume
// e "TRANSMISSION" enquanto alguem fala (voz bem acima do chiado do radio, que e medido e se adapta)
const voice={talk:false,lv:[],noise:200,talkAt:0};
setInterval(()=>{   // volume (RMS, escala de 16 bits) em fatias de 100 ms
  if(!atcAn)return;
  atcAn.getFloatTimeDomainData(atcBuf);let s=0;for(const x of atcBuf)s+=x*x;
  const rms=Math.sqrt(s/atcBuf.length)*32768,nz=voice.noise;
  voice.noise=nz+(rms-nz)*(rms>nz?.002:.2);   // sobe bem devagar (fala longa nao vira "chiado"), desce rapido
  if(rms>Math.max(voice.noise*3,350))voice.talkAt=performance.now();
  voice.lv=[...voice.lv,Math.min(1,rms/12000)].slice(-30);voice.talk=performance.now()-voice.talkAt<800;
},100);
const talking=()=>atc.on&&voice.talk;
function drawVoice(d,hasSel){
  if(!talking())return;   // so aparece quando alguem esta falando; no resto do tempo fica a contagem de avioes
  const r=mode==='round',H=r?36:26,n=r?14:10,bw=r?3:2.5,gap=r?2:1.5,pad=H/2,fs1=r?11:8,fs2=r?9:7;
  const t1='TRANSMISSION',t2=feedName(atc.feed);
  d.g.font=`700 ${fs1}px system-ui,sans-serif`;const w1=d.g.measureText(t1).width;
  d.g.font=`500 ${fs2}px system-ui,sans-serif`;const w2=d.g.measureText(t2).width;
  const barsW=n*(bw+gap)-gap,W=pad+barsW+(r?10:6)+Math.max(w1,w2)+pad,x0=r?R.CX-W/2:4;
  const y0=r?(hasSel?214:style==='atc'?R.CY+RA-34:390):206;   // embaixo do radar (acima do cartao, se houver aviao tocado)
  d.fillRoundRect(x0,y0,W,H,H/2,'rgba(10,40,24,.92)');
  const bx=x0+pad,mid=y0+H/2,lv=voice.lv||[];
  for(let i=0;i<n;i++){const v=lv[lv.length-n+i]??0,h=Math.max(2,v*(H-10));d.fillRoundRect(bx+i*(bw+gap),mid-h/2,bw,h,1,'#62f59a')}
  const tx=bx+barsW+(r?10:6);
  d.text(t1,tx,mid-(r?6:4),fs1,'#62f59a','left',true);
  d.text(t2,tx,mid+(r?8:6),fs2,'#cfeedd');
}

// ---------- abertura: SENTINEL 643 (varredura, checagem da ligacao com o ESP, do Wi-Fi e do ADS-B)
const bootT0=performance.now();let bootDone=false;
function drawBoot(d){
  const t=performance.now()-bootT0,{CX,CY}=R;
  if(t>2600&&(conn&&D.planes.length||t>6000)){if(!bootDone){bootDone=true;bootEnd=performance.now()}}
  const fade=bootDone?Math.max(0,1-(performance.now()-bootEnd)/450):1;if(fade<=0)return false;
  d.g.globalAlpha=fade;
  d.fillRect(0,0,R.W,R.W,'#03070b');
  for(let i=1;i<=4;i++)d.drawCircle(CX,CY,50*i,'rgba(46,90,70,.5)');
  const sw=(t/1600*Math.PI*2)%(Math.PI*2);
  for(let q=0;q<24;q++){const a1=sw-Math.PI/2-q*.03;d.fillArc(CX,CY,0,200,a1-.03,a1,`rgba(98,245,154,${(.22*(1-q/24)).toFixed(3)})`)}
  d.drawLine(CX,CY,CX+Math.sin(sw)*200,CY-Math.cos(sw)*200,'rgba(98,245,154,.8)',2);
  // escudo com o radar
  const g=d.g;g.beginPath();g.moveTo(CX,CY-118);g.lineTo(CX+30,CY-106);g.lineTo(CX+27,CY-78);g.quadraticCurveTo(CX+20,CY-60,CX,CY-50);g.quadraticCurveTo(CX-20,CY-60,CX-27,CY-78);g.lineTo(CX-30,CY-106);g.closePath();
  g.fillStyle='#0b1a2a';g.fill();g.strokeStyle='#c9a227';g.lineWidth=2;g.stroke();
  d.drawCircle(CX,CY-84,12,'#62f59a',1.5);d.drawLine(CX,CY-84,CX+9,CY-92,'#62f59a',1.5);
  d.mono('SENTINEL 643',CX,CY-18,30,'#e0b53c','center');
  d.mono('TACTICAL AVIATION RADAR · MK I',CX,CY+8,10,'#7a96a8','center');
  d.mono(T('INITIALISATION DU SYSTÈME'),CX,CY+36,12,'#c9d6de','center');
  const steps=[[T('LIAISON ESP32'),conn],[T('WI-FI {n}',{n:(D.ssid||'').toUpperCase().slice(0,12)}),conn&&D.online!==false&&!D.ap],[T('ADS-B'),conn&&D.ok]];
  steps.forEach(([l,ok],i)=>{if(t<700+i*450)return;const y=CY+66+i*20;
    d.mono(l,CX-92,y,11,'#9fb4c3','left');d.mono(ok?'OK':t>2600?'…':'…',CX+92,y,11,ok?'#62f59a':'#e0b53c','right');
    d.drawLine(CX-92+d.g.measureText(l).width+6,y,CX+70,y,'rgba(120,150,170,.25)')});
  d.mono(T('RADAR · ADS-B · SURVEILLANCE AÉRIENNE'),CX,CY+150,9,'#5f8599','center');
  d.g.globalAlpha=1;
  return true;
}
let bootEnd=0;

let frames=0,fpsAt=performance.now();
function draw(){
  frames++;if(performance.now()-fpsAt>=1000){fps=frames;frames=0;fpsAt=performance.now()}
  const dt=dataAge(),planes=D.planes.map(p=>live(p,dt)),d=mode==='round'?R.d:Q.d;
  if(mode!=='round')drawRect(planes);else if(style==='atc')drawATC(planes);else drawRound(planes);
  drawVoice(d,!!selHex&&D.planes.some(p=>p.hex===selHex));
  drawGear(d,GEAR[mode]);
  drawMute(d,MUTE[mode]);
  if(menuOpen)drawMenu(d);
  if(mode==='round')drawBoot(d);
}

// ---------- modelo, companhia e rota: o navegador pergunta direto a adsbdb (libera CORS); sem acesso, pede ao ESP
const apOf=x=>x&&{code:x.iata_code||x.icao_code,city:x.municipality||''};
async function loadInfo(p){
  if(info[p.hex])return;info[p.hex]={loading:true};
  const cs=p.flight||'',api='https://api.adsbdb.com/v0/';
  try{
    const r=await fetch(`${api}aircraft/${p.hex}${cs?'?callsign='+encodeURIComponent(cs):''}`);
    let j=r.ok?(await r.json()).response:null;
    if((!j||!j.flightroute)&&cs){const r2=await fetch(`${api}callsign/${encodeURIComponent(cs)}`);if(r2.ok)j={...(j||{}),...(await r2.json()).response}}
    const a=j&&j.aircraft,f=j&&j.flightroute;
    info[p.hex]={model:a?`${a.manufacturer||''} ${a.type||''}`.trim():'',reg:a&&a.registration,owner:a&&a.registered_owner,
      airline:f&&f.airline&&f.airline.name,orig:f&&apOf(f.origin),dest:f&&apOf(f.destination)};
  }catch(e){
    try{info[p.hex]=await(await fetch(`/info?hex=${p.hex}&cs=${encodeURIComponent(cs)}`)).json()}catch(e2){info[p.hex]={}}
  }
}
// rota (origem/destino) dos avioes na tela para a 3a linha da etiqueta: uma consulta a cada 1,5 s, guardada na memoria
const routes=new Map(),routeQ=[];let routeBusy=false;
function queueRoutes(){for(const p of D.planes)if(p.flight&&inView&&inView.has(p.hex)&&!routes.has(p.flight)&&!routeQ.includes(p.flight)&&routeQ.length<40)routeQ.push(p.flight)}
setInterval(async()=>{
  if(routeBusy||!routeQ.length)return;routeBusy=true;const cs=routeQ.shift();
  try{const r=await fetch(`https://api.adsbdb.com/v0/callsign/${encodeURIComponent(cs)}`);
    const f=r.ok?(await r.json()).response.flightroute:null;
    routes.set(cs,f?{o:apOf(f.origin)&&apOf(f.origin).code,d:apOf(f.destination)&&apOf(f.destination).code}:null)}
  catch(e){setTimeout(()=>routeQ.push(cs),60000)}   // sem internet: tenta de novo em 1 min
  routeBusy=false},1500);

// ---------- toque: area de toque maior que o simbolo; dois alvos colados abrem uma lista; toque longo (ou no cartao) = detalhes;
// dois toques num aviao = seguir (fica no centro); setas da borda = afastar o zoom
function hitPlanes(x,y,S,isRound){const r=isRound?26:16;return S.blips.map(q=>({hex:q.hex,d:Math.hypot(q.x-x,q.y-y)})).filter(o=>o.d<=r).sort((a,b)=>a.d-b.d)}
function select(hex){selHex=hex;const p=hex&&D.planes.find(p=>p.hex===hex);if(p)loadInfo(p)}
const inCard=(x,y)=>mode==='round'&&style==='atc'&&selHex&&x>=CARD[0]&&x<=CARD[2]&&y>=CARD[1]&&y<=CARD[3];
let lastTap={hex:null,t:0};
function touch(x,y,S,isRound){
  if(!bootDone&&mode==='round'){bootDone=true;bootEnd=performance.now();return}   // toque pula a abertura
  if(menuOpen){menuTap(x,y);return}
  const mu=MUTE[mode];if(Math.hypot(x-mu[0],y-mu[1])<=mu[2]+4){atcMute();return}   // mudo vale tambem para o proximo canal
  const g=GEAR[mode];if(Math.hypot(x-g[0],y-g[1])<=g[2]+10){menuOpen=true;return}
  const hit=isRound?(([bx,by,r])=>Math.hypot(x-bx,y-by)<=r+8):(r=>x>=r[0]&&x<=r[0]+r[2]&&y>=r[1]&&y<=r[1]+r[3]);
  if(hit(S.plus)){rangeIdx=Math.max(0,rangeIdx-1);savePref();return}    // + aproxima (alcance menor)
  if(hit(S.minus)){rangeIdx=Math.min(4,rangeIdx+1);savePref();return}   // − afasta (alcance maior)
  const fp=R.followPill;if(follow&&fp&&x>=fp[0]&&x<=fp[2]&&y>=fp[1]&&y<=fp[3]){follow=null;return}
  const h=hitPlanes(x,y,S,isRound),now=performance.now();
  // segundo toque no mesmo aviao: seguir (vale mesmo se o cartao aberto pelo 1o toque ficou por cima dele)
  const again=h.find(o=>o.hex===lastTap.hex);
  if(isRound&&style==='atc'&&again&&now-lastTap.t<400){follow=follow===again.hex?null:again.hex;lastTap={hex:null,t:0};select(again.hex);return}
  if(inCard(x,y)){openDetails(selHex);return}
  if(h.length>1&&h[1].d-h[0].d<10){pickPlane(h.map(o=>o.hex));return}   // alvos colados: escolher na lista
  if(h.length){lastTap={hex:h[0].hex,t:now};select(h[0].hex);return}
  if((S.edge||[]).some(e=>Math.hypot(e.x-x,e.y-y)<18)&&rangeIdx<4){rangeIdx++;savePref();return}   // seta na borda: afasta o zoom
  select(null);   // toque fora de um aviao = desmarcar
}
function longPress(x,y,S,isRound){
  if(menuOpen)return;const h=hitPlanes(x,y,S,isRound);if(!h.length)return;
  select(h[0].hex);openDetails(h[0].hex);
}
let press=null;
for(const[id,S,isRound]of[['round',R,true],['rect',Q,false]]){
  // toque -> coordenada do canvas, desfazendo a rotacao da tela
  const cv=$(id),xy=e=>{const b=cv.getBoundingClientRect(),a=-rot*Math.PI/180,dx=e.clientX-(b.left+b.width/2),dy=e.clientY-(b.top+b.height/2);
    const rx=dx*Math.cos(a)-dy*Math.sin(a),ry=dx*Math.sin(a)+dy*Math.cos(a);return[(rx/cv.offsetWidth+.5)*cv.width,(ry/cv.offsetHeight+.5)*cv.height]};
  cv.addEventListener('pointerdown',e=>{const[x,y]=xy(e);press={x,y,long:false,timer:setTimeout(()=>{if(press){press.long=true;longPress(x,y,S,isRound)}},550)}});
  cv.addEventListener('pointermove',e=>{if(!press)return;const[x,y]=xy(e);if(Math.hypot(x-press.x,y-press.y)>12){clearTimeout(press.timer);press=null}});
  cv.addEventListener('pointerup',()=>{if(!press)return;clearTimeout(press.timer);const p=press;press=null;if(!p.long)touch(p.x,p.y,S,isRound)});
  cv.addEventListener('pointercancel',()=>{if(press)clearTimeout(press.timer);press=null});
  cv.addEventListener('contextmenu',e=>e.preventDefault());
}
function pickPlane(hexes){
  const el=dlg(`<h3>${T('Quel avion ?')}</h3><ul id="pk"></ul><button data-close>${T('Fermer')}</button>`);
  el.querySelector('#pk').innerHTML=hexes.map(h=>D.planes.find(p=>p.hex===h)).filter(Boolean).map(p=>
    `<li data-h="${p.hex}"><span><b>${escH(csOf(p))}</b> ${escH(p.type||'')}</span><span class="dim">${p.ground?T('SOL'):'FL'+String(Math.round(p.alt/100)).padStart(3,'0')} · ${dec(p.dist/1.852)} NM</span></li>`).join('');
  el.querySelector('#pk').onclick=e=>{const li=e.target.closest('li[data-h]');if(li){select(li.dataset.h);el.hidden=true}};
}
// detalhes completos do aviao (atualiza a cada 1 s enquanto a janela estiver aberta)
let detailsTok=0;
function openDetails(hex){
  menuOpen=false;const tok=++detailsTok;
  const el=dlg(`<h3 id="dh"></h3><ul id="dd"></ul><div class="row"><button id="df">${T('Suivre')}</button><button data-close>${T('Fermer')}</button></div>`);
  el.querySelector('#df').onclick=()=>{follow=follow===hex?null:hex;if(follow){style='atc';savePref()}el.hidden=true};
  const render=()=>{
    if(tok!==detailsTok||el.hidden||!el.querySelector('#dd'))return;
    el.querySelector('#df').textContent=follow===hex?T('Ne plus suivre'):T('Suivre');
    const p0=D.planes.find(p=>p.hex===hex);if(!p0){el.querySelector('#dd').innerHTML=`<li class="kv dim">${T('Cet avion a quitté le radar.')}</li>`;return}
    const p=live(p0,dataAge()),inf=info[hex]||{},o=inf.orig,de=inf.dest,rt=routeOf(p)||{},pa=posAge(p0),na='—';
    el.querySelector('#dh').textContent=csOf(p);
    const rows=[[T('Compagnie'),inf.airline||na],[T('Appareil'),inf.model||p.type||na],[T('Immatriculation'),inf.reg||p.reg||na],[T('Code OACI 24 bits'),p.hex.toUpperCase()],
      [T('Catégorie'),p.cat?p.cat+(catName(p.cat)?' · '+catName(p.cat):''):na],
      [T('Route'),o||de?`${o?o.code+(o.city?' '+o.city:''):'?'} → ${de?de.code+(de.city?' '+de.city:''):'?'}`:rt.o||rt.d?`${rt.o||'?'} → ${rt.d||'?'}`:na],
      [T('Altitude'),p.ground?T('au sol'):`${fr(p.alt)} ft (FL${String(Math.round(p.alt/100)).padStart(3,'0')})`],[T('Taux vertical'),p.vr?`${p.vr>0?'+':''}${fr(p.vr)} ft/min`:T('stable')],
      [T('Vitesse sol'),p.gs!=null?`${Math.round(p.gs)} kt (${Math.round(p.gs*1.852)} km/h)`:na],[T('Cap (route)'),p.trk!=null?`${String(Math.round(p.trk)%360).padStart(3,'0')}°`:na],
      [T('Distance'),`${dec(p.dist/1.852)} NM (${dec(p.dist)} km)`],[T('Relèvement'),`${String(Math.round(p.brg)%360).padStart(3,'0')}°`],
      [T('Transpondeur'),p.sq?p.sq+(EMERG[p.sq]?' — '+T(EMERG[p.sq]):''):na],[T('Position'),pa<2?T('à l’instant'):T('il y a {n} s',{n:Math.round(pa)})+(pa>COAST?' ('+T('ancienne')+')':'')]];
    if(inf.owner)rows.splice(3,0,[T('Propriétaire'),inf.owner]);
    if(p.mil)rows.splice(4,0,[T('Militaire'),T('oui')]);
    el.querySelector('#dd').innerHTML=rows.map(([k,v])=>`<li class="kv"><span class="dim">${k}</span><span>${escH(v)}</span></li>`).join('');
    setTimeout(render,1000);
  };
  const p=D.planes.find(p=>p.hex===hex);if(p)loadInfo(p);
  render();
}
const catName=c=>{const g=CATS.find(g=>g[2].includes(c));return g?T(g[1]):''};
function setMode(m){mode=m;savePref();$('round').hidden=m!=='round';$('rect').hidden=m==='round'}

// Som ligado por padrao. O navegador so libera audio depois do primeiro toque na pagina:
// esse primeiro toque (em qualquer lugar) ja destrava o bipe.
addEventListener('pointerdown',()=>{try{actx=actx||new AudioContext();actx.resume()}catch(e){}},{capture:true});
function beep(force,hz=880,sec=.4,vol=.15){if(!actx||!(force||D.soundOn))return;vol*=[.35,1,1.9][volIdx]||1;const o=actx.createOscillator(),v=actx.createGain();o.frequency.value=hz;v.gain.setValueAtTime(vol,actx.currentTime);v.gain.exponentialRampToValueAtTime(.001,actx.currentTime+sec);o.connect(v).connect(actx.destination);o.start();o.stop(actx.currentTime+sec)}
// aviao novo entrando no alcance da tela (modo de alertas "Toutes"): bipe curto e agudo. Na 1a leitura, ao trocar
// o zoom ou os filtros so memoriza quem ja esta la; o mesmo aviao nao bipa de novo por 5 min.
let inView=null,viewKey='';const newBeepAt=new Map(),emergBeeped=new Set();
const viewKm=()=>mode==='round'&&style==='atc'?ATC_NM[rangeIdx]*1.852:RANGES[units][rangeIdx]*KM_PER[units];
function newPlanes(alertBeep){
  const rKm=viewKm(),key=[mode,style,units,rangeIdx,JSON.stringify(F)].join();
  const now=Date.now(),cur=new Set(D.planes.filter(p=>p.dist<=rKm&&passFilter(p)).map(p=>p.hex));
  if(inView&&key===viewKey&&[...cur].some(h=>!inView.has(h)&&now-(newBeepAt.get(h)||0)>300000)){
    for(const h of cur)if(!inView.has(h))newBeepAt.set(h,now);
    if(!alertBeep&&D.aMode===2&&D.aNew)beep(false,1320,.12,.1);   // um bipe por leitura, mesmo se entrarem varios (o alerta tem prioridade)
  }
  for(const[h,t]of newBeepAt)if(now-t>300000)newBeepAt.delete(h);
  inView=cur;viewKey=key;
}
async function poll(){
  try{
    // r = alcance que esta na tela (km): o ESP ajusta o raio consultado (seguindo um aviao, soma a distancia dele)
    const fp=follow&&D.planes.find(p=>p.hex===follow),r=Math.round(viewKm()+(fp?fp.dist:0));
    D=await(await fetch(`/planes?r=${r}`,{cache:'no-store'})).json();recvAt=performance.now()-D.age*1000;conn=true;
    if(D.lat||D.lon){loadAirports(D.lat,D.lon);loadFeeds();loadMetar()}
    if(D.ap&&!apShown){apShown=true;openWifi()}   // sem Wi-Fi configurado: ja abre a lista de redes
    const alertBeep=D.alerting>lastAlert&&D.alertOn;if(alertBeep)beep();lastAlert=D.alerting;
    newPlanes(alertBeep);queueRoutes();
    for(const p of D.planes)if(EMERG[p.sq]&&!emergBeeped.has(p.hex)){emergBeeped.add(p.hex);if(D.aMode>0&&D.aEm)beep(false,660,.7,.2)}   // squawk de emergencia: bipe grave, 1 vez
    for(const h of hist.keys())if(!D.planes.some(p=>p.hex===h))hist.delete(h);
    if(follow&&!D.planes.some(p=>p.hex===follow))follow=null;   // o aviao seguido sumiu
  }catch(e){conn=false}
}
document.documentElement.lang=LOC[lang];
setMode(mode);applyRot();poll();setInterval(poll,2000);
(function loop(){draw();setTimeout(()=>requestAnimationFrame(loop),50)})();
</script></body></html>)HTML";
