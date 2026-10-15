// Copie este arquivo para secrets.h e preencha com os seus dados.
// O secrets.h esta no .gitignore: a senha do Wi-Fi nunca vai para o repositorio.
#pragma once

#define WIFI_SSID "nome-da-sua-rede"
#define WIFI_PASS "senha-da-sua-rede"

// Casa (centro do radar). Deixe 0 e 0 para o ESP32 descobrir pelo IP (precisao de cidade).
#define HOME_LAT  0.0
#define HOME_LON  0.0
#define HOME_CITY "casa"

// PC na mesma rede rodando radar/server.py (audio da torre). Deixe "" se nao for usar.
#define PC_URL "http://192.168.1.10:8080"
