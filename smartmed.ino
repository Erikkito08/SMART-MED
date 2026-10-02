// ----------------------------------------
// ---------------SMART MED----------------
// ----------------------------------------
// ----------------------------------------
// ----------------------------------------
// ----------------------------------------
// ----------------------------------------
// ----------------------------------------
// ----------------------------------------
// ----------------------------------------
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>

// ============================================================
//  CONFIGURAÇÕES DO TELEGRAM
// ============================================================
const char* BOT_TOKEN = "8423317542:AAHmKg_1FC2IjA9tCzdpCHsQrlnb3fpP2ic";
const int FIRMWARE_VERSION = 1;
String authorizedId = "";

// Configuração WiFi
String WIFI_SSID = "Teste45";
String WIFI_PASSWORD = "T12345678";

String WIFI_SSID2 = "Dispenser-Config";
String WIFI_PASSWORD2 = "remedios123";

// ============================================================
//  HARDWARE
// ============================================================
RTC_DS1307 rtc;
bool rtcDisponivel = false;
uint32_t internalClockStart = 0;
unsigned long internalMillisStart = 0;

Preferences preferences;
bool hasSavedWiFi = false;
bool configurado = false;

#define ende 0x27

#define buzzer 19 
#define IN1 18 
#define IN2 5
#define IN3 4 
#define IN4 2 
#define IR 15

#define delayStep 10
#define MAX_DOSES 15

LiquidCrystal_I2C lcd(ende,16,2);
WiFiClientSecure sslClient;
UniversalTelegramBot bot(BOT_TOKEN, sslClient);

// ============================================================
//  NOTIFICAÇÃO
// ============================================================
int INTERVALO_NOTIF_RETIRADA = 15;  // minutos entre notificações de não retirada

// ============================================================
//  ARRAYS DE AGENDAMENTO
// ============================================================
String horario = "", horario2 = "", horario3 = "", horario4 = "", horario5 = "", horarioF = "";
char diasSem[7][12] = {"Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta", "Sabado"};

int ano[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int mes[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int dia[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int hora[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int minuto[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
String nomeCompartimento[MAX_DOSES];

// ============================================================
//  ESTADO DO DISPENSER
// ============================================================
struct EstadoDispenser {
  bool aguardandoRetirada;
  bool gavetaAberta;
  bool gavetaAbandonada;
  int notificacoesEnviadas;
  int notificacoesGaveta;
  int compartimentoAtual;
  unsigned long tempoUltimaNotif;
  unsigned long tempoGavetaAberta;
};

EstadoDispenser estado = {false, false, false, 0, 0, 0, 0, 0};

// ============================================================
//  VARIÁVEIS DE CONTROLE
// ============================================================
int momen[6] = {0, 0, 0, 0, 0, 0};
int coleta[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int coletav[MAX_DOSES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int seq2[4][4] = {
  {1, 0, 0, 0},
  {0, 1, 0, 0},
  {0, 0, 1, 0},
  {0, 0, 0, 1}
};
bool remedioPresente = false;

int vez = 0, steps = 0, liberado = 0;
int minT, minT2, limpa;

int inici = 1;
int momseg = 0;
int mommin = -1;
unsigned long irHighStart = 0;

unsigned long ultimoPoll = 0;
unsigned long ultimaVerifDose = 0;
unsigned long ultimaVerifGaveta = 0;

String menuAtivo = "";
int tempHora = -1;
int tempMin = -1;
int tempDia = -1;
int tempMes = -1;
int tempAno = -1;

bool wifiConectado = false;
bool wifiConectado2 = false;
bool wificonectar2 = false;
bool estadoer = false;
bool estadosr = false;
bool init4 = true;
int missedDosesCycle = 0;
bool missedDoseFlag[MAX_DOSES] = {false};
bool alertaProximoHorarioDesligado = false;
unsigned long ultimaTentativaWiFi1 = 0;

bool lcdShowingConfirm = false;
unsigned long lcdConfirmEndTime = 0;
bool lcdShowingUncollected = false;
unsigned long lcdUncollectedEndTime = 0;

bool conectarWiFi();
void cmdAlterarWiFi(String chat_id, String params);
void carregarCredenciaisWiFi();
void salvarCredenciaisWiFi();
void carregarConfiguracao();
void salvarConfiguracao();
bool hasHorariosSalvos();
int findNextScheduleIndex();
void cmdRenomearCompartimento(String chat_id, String params);
void cmdAgendarCompartimento(String chat_id, String params);
void cmdResetHorarios(String chat_id);
void cmdReset(String chat_id);
void cmdConfirmarReset(String chat_id);
void cmdDispensar(String chat_id);
void cmdConfirmarDispensar(String chat_id);
void processarMenuConfirmarDispensar(String chat_id, String texto);
void processarMenuConfirmarReset(String chat_id, String texto);
void processarMenuAgendarLote(String chat_id, String texto);
void processarMenuIntervaloNotif(String chat_id, String texto);
void playWarningBeep();
void lcdClearLine(int line);
void lcdPrintLine(int line, String text);
void showLcdConfirmation(String line1, String line2);
void atualizarLCDStatus();
void wifiteste();

// ============================================================
//  SETUP
// ============================================================
void setup() {
  pinMode(buzzer, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(IR, OUTPUT); 

  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  if (!rtc.begin()) {
    rtcDisponivel = false;
    internalClockStart = DateTime(F(__DATE__), F(__TIME__)).unixtime();
    internalMillisStart = millis();
    Serial.println("RTC não encontrado: usando relógio interno");
    lcd.setCursor(0, 0);
    lcd.print("RTC nao");
    lcd.setCursor(0, 1);
    lcd.print("encontrado");
  } else {
    rtcDisponivel = true;
    if (!rtc.isrunning()) {
      Serial.println("Ajustando RTC para horário de compilação");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ajustando RTC");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  lcd.clear();

  carregarCredenciaisWiFi();
  carregarConfiguracao();
  conectarWiFi();

  if (wifiConectado||wifiConectado2) {
    sslClient.setInsecure();
    Serial.println("Aguardando pareamento via /start no Telegram...");

    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 8000)) {
      DateTime ntpNow(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      );
      if (rtcDisponivel) {
        rtc.adjust(ntpNow);
        Serial.println("RTC ajustado via NTP");
      }
    } else {
      Serial.println("Falha ao obter hora NTP");
    }
  }

  digitalWrite(IR, HIGH);
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  unsigned long agora = millis();

  if(wificonectar2 == true){
    wifiteste();
  }

  if (wifiConectado2&&estadoer) {
    sslClient.setInsecure();
    Serial.println("Aguardando pareamento via /start no Telegram...");

    configTime(-3 * 3600, 0, "pool.ntp.org", "time.google.com");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 8000)) {
      DateTime ntpNow(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec
      );
      if (rtcDisponivel) {
        rtc.adjust(ntpNow);
        Serial.println("RTC ajustado via NTP");
      }
    } else {
      Serial.println("Falha ao obter hora NTP");
    }
    estadoer = false;
  }

  if(estadosr){
    WiFi.disconnect(true, true);
    conectarWiFi();
    estadosr = false;
  }

  Time(0);

  if (lcdShowingConfirm && millis() >= lcdConfirmEndTime) {
    lcdShowingConfirm = false;
    atualizarLCDStatus();
  }

  if (configurado && authorizedId.length() > 0) {
    DateTime nowDt = DateTime(momen[0], momen[1], momen[2], momen[3], momen[4], 0);
    long nowSeconds = nowDt.unixtime();

    int nextIndex = findNextScheduleIndex();
    if (liberado == 0) {
      if (nextIndex != -1) {
        vez = nextIndex;
      }
    } else if (nextIndex != -1 && getScheduleDateTime(nextIndex).unixtime() <= nowSeconds) {
      int missedIndex = estado.compartimentoAtual - 1;
      if (missedIndex >= 0 && missedIndex < MAX_DOSES && !missedDoseFlag[missedIndex]) {
        missedDoseFlag[missedIndex] = true;
        missedDosesCycle++;
      }
      estado.aguardandoRetirada = false;
      liberado = 0;
      vez = nextIndex;
      if (authorizedId != "") {
        String msg = "⚠️ O remédio " + getNomeCompartimento(missedIndex) + " não foi coletado. Liberando próxima dose.";
        bot.sendMessage(authorizedId, msg, "");
      }
    }
    acao();
  }
  momseg = momen[5];
  mommin = momen[4];

  if (wifiConectado || wifiConectado2) {
    if (agora - ultimoPoll > 5000) {
      ultimoPoll = agora;
      int msgs = bot.getUpdates(bot.last_message_received + 1);
      for (int i = 0; i < msgs; i++) {     
       processarMensagem(bot.messages[i]); 
      }
    }

    if (configurado && authorizedId.length() > 0 && agora - ultimaVerifDose > 30000) {
      ultimaVerifDose = agora;
      verificarDoses();
    }

    if (configurado && authorizedId.length() > 0 && estado.aguardandoRetirada && (agora - ultimaVerifGaveta > 1000)) {
      ultimaVerifGaveta = agora;
      gerenciarRetirada();
    }
  }

  delay(100);
}

// ============================================================
//  WIFI
// ============================================================
bool conectarWiFi() {
  if (WIFI_SSID.length() == 0) {
    Serial.println("WiFi não configurado. Configure WIFI_SSID e WIFI_PASSWORD");
    lcd.setCursor(0, 0);
    lcd.print("WiFi nao config");
    return false;
  }

  Serial.print("Conectando WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect(true, true);
  delay(500);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiConectado = true;
    return true;
  } else {
    Serial.println("\nFalha ao conectar WiFi");
    wifiConectado = false;
    wificonectar2 = true;
    return false;
  }
}

void wifiteste() {

  if (!wifiConectado2) {

    if (init4) {
      WiFi.disconnect(true, true);
      delay(500);

      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID2.c_str(), WIFI_PASSWORD2.c_str());

      init4 = false;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi backup conectado!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      wifiConectado2 = true;
      estadoer = true;
    }

    return;
  }

  if (!hasSavedWiFi) {
    return;
  }

  if (millis() - ultimaTentativaWiFi1 < 30000) {
    return;
  }

  ultimaTentativaWiFi1 = millis();

  Serial.println("Tentando voltar para o WiFi principal...");

  WiFi.disconnect(true, true);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  int tentativas = 0;

  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("\nWiFi principal conectado!");

    wifiConectado = true;
    wifiConectado2 = false;
    wificonectar2 = false;
    init4 = true;

  } else {

    Serial.println("\nFalha no WiFi principal.");

    WiFi.disconnect(true, true);
    delay(500);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID2.c_str(), WIFI_PASSWORD2.c_str());

    wifiConectado = false;
    wifiConectado2 = false;
  }
}

void carregarCredenciaisWiFi() {
  preferences.begin("wifi", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid.length() > 0) {
    WIFI_SSID = ssid;
    WIFI_PASSWORD = pass;
    hasSavedWiFi = true;
    Serial.print("Credenciais WiFi carregadas: ");
    Serial.println(WIFI_SSID);
  } else {
    hasSavedWiFi = false;
    Serial.println("Nenhuma credencial WiFi persistente encontrada.");
  }
}

void salvarCredenciaisWiFi() {
  preferences.begin("wifi", false);
  preferences.putString("ssid", WIFI_SSID);
  preferences.putString("pass", WIFI_PASSWORD);
  preferences.end();
  Serial.print("Credenciais WiFi salvas: ");
  Serial.println(WIFI_SSID);
}

void carregarConfiguracao() {
  preferences.begin("config", true);
  String savedBuildTime = preferences.getString("buildTime", "");
  authorizedId = preferences.getString("auth", "");
  configurado = preferences.getUInt("configurado", 0) == 1;

  String currentBuild = String(__DATE__) + " " + String(__TIME__);
  if (savedBuildTime != currentBuild) {
    preferences.end();
    preferences.begin("config", false);
    preferences.putString("buildTime", currentBuild);

    for (int i = 0; i < MAX_DOSES; i++) {
      ano[i] = 0;
      mes[i] = 0;
      dia[i] = 0;
      hora[i] = 0;
      minuto[i] = 0;
      coleta[i] = 0;
      coletav[i] = 0;
    }

    authorizedId = "";
    configurado = false;

    preferences.putString("auth", authorizedId);
    preferences.putBytes("ano", ano, sizeof(ano));
    preferences.putBytes("mes", mes, sizeof(mes));
    preferences.putBytes("dia", dia, sizeof(dia));
    preferences.putBytes("hora", hora, sizeof(hora));
    preferences.putBytes("minuto", minuto, sizeof(minuto));
    preferences.putUInt("configurado", 0);
    preferences.end();
    Serial.println("Build mudou ou firmware re-enviado. Horários e pareamento resetados.");
  } else {
    preferences.getBytes("ano", ano, sizeof(ano));
    preferences.getBytes("mes", mes, sizeof(mes));
    preferences.getBytes("dia", dia, sizeof(dia));
    preferences.getBytes("hora", hora, sizeof(hora));
    preferences.getBytes("minuto", minuto, sizeof(minuto));
    for (int i = 0; i < MAX_DOSES; i++) {
      String key = "nome" + String(i);
      nomeCompartimento[i] = preferences.getString(key.c_str(), "");
      if (nomeCompartimento[i].length() == 0) {
        nomeCompartimento[i] = "Compartimento " + String(i + 1);
      }
    }
    preferences.end();
  }

  if (configurado && !hasHorariosSalvos()) {
    configurado = false;
  }

  Serial.print("Configuração carregada - pareado: ");
  Serial.print(authorizedId.length() > 0 ? "sim" : "nao");
  Serial.print(" | configurado: ");
  Serial.println(configurado ? "sim" : "nao");
}

void salvarConfiguracao() {
  preferences.begin("config", false);
  String currentBuild = String(__DATE__) + " " + String(__TIME__);
  preferences.putString("buildTime", currentBuild);
  preferences.putString("auth", authorizedId);
  preferences.putUInt("configurado", configurado ? 1 : 0);
  preferences.putBytes("ano", ano, sizeof(ano));
  preferences.putBytes("mes", mes, sizeof(mes));
  preferences.putBytes("dia", dia, sizeof(dia));
  preferences.putBytes("hora", hora, sizeof(hora));
  preferences.putBytes("minuto", minuto, sizeof(minuto));
  for (int i = 0; i < MAX_DOSES; i++) {
    String key = "nome" + String(i);
    preferences.putString(key.c_str(), nomeCompartimento[i]);
  }
  preferences.end();
  Serial.println("Configuração persistente salva.");
}

bool hasHorariosSalvos() {
  for (int i = 0; i < MAX_DOSES; i++) {
    if (ano[i] != 0 || mes[i] != 0 || dia[i] != 0 || hora[i] != 0 || minuto[i] != 0) return true;
  }
  return false;
}

String getNomeCompartimento(int index) {
  if (index < 0 || index >= MAX_DOSES) return "Compartimento ?";
  if (nomeCompartimento[index].length() == 0) {
    return "Compartimento " + String(index + 1);
  }
  return nomeCompartimento[index];
}

String formatHorario(int h, int m) {
  String texto = "";
  if (h < 10) texto += "0";
  texto += String(h);
  texto += ":";
  if (m < 10) texto += "0";
  texto += String(m);
  return texto;
}

bool tempoValidoParaCompartimento(int index, int newHour, int newMin) {
  if (index < 0 || index >= MAX_DOSES) return false;
  if (!scheduleSlotUsed(index)) return false;

  DateTime novoHorario = DateTime(ano[index], mes[index], dia[index], newHour, newMin, 0);
  for (int i = index - 1; i >= 0; i--) {
    if (!scheduleSlotUsed(i)) continue;
    DateTime anterior = getScheduleDateTime(i);
    if (novoHorario.unixtime() <= anterior.unixtime()) return false;
    break;
  }
  for (int i = index + 1; i < MAX_DOSES; i++) {
    if (!scheduleSlotUsed(i)) continue;
    DateTime proximo = getScheduleDateTime(i);
    if (novoHorario.unixtime() >= proximo.unixtime()) return false;
    break;
  }
  return true;
}

int getNextPendingScheduleIndex() {
  int bestIndex = -1;
  DateTime nowDt = DateTime(momen[0], momen[1], momen[2], momen[3], momen[4], 0);
  long nowSeconds = nowDt.unixtime();
  bool overdueFound = false;

  for (int i = 0; i < MAX_DOSES; i++) {
    if (!scheduleSlotUsed(i) || coleta[i] == 1) continue;
    long scheduleSeconds = getScheduleDateTime(i).unixtime();
    if (scheduleSeconds <= nowSeconds) {
      if (!overdueFound || scheduleSeconds < getScheduleDateTime(bestIndex).unixtime()) {
        bestIndex = i;
        overdueFound = true;
      }
    } else if (!overdueFound) {
      if (bestIndex == -1 || scheduleSeconds < getScheduleDateTime(bestIndex).unixtime()) {
        bestIndex = i;
      }
    }
  }
  return bestIndex;
}

int findNextScheduleIndex() {
  int bestIndex = -1;
  DateTime nowDt = DateTime(momen[0], momen[1], momen[2], momen[3], momen[4], 0);
  long nowSeconds = nowDt.unixtime();
  bool overdueFound = false;

  for (int i = 0; i < MAX_DOSES; i++) {
    if (coleta[i] == 1) continue;
    if (!scheduleSlotUsed(i)) continue;
    long scheduleSeconds = getScheduleDateTime(i).unixtime();
    if (scheduleSeconds <= nowSeconds) {
      if (!overdueFound || scheduleSeconds < getScheduleDateTime(bestIndex).unixtime()) {
        bestIndex = i;
        overdueFound = true;
      }
    } else if (!overdueFound) {
      if (bestIndex == -1 || scheduleSeconds < getScheduleDateTime(bestIndex).unixtime()) {
        bestIndex = i;
      }
    }
  }

  if (bestIndex != -1) {
    return bestIndex;
  }

  if (liberado == 1 || estado.aguardandoRetirada) {
    return -1;
  }

  advanceScheduleCycle();

  bestIndex = -1;
  overdueFound = false;
  for (int i = 0; i < MAX_DOSES; i++) {
    if (coleta[i] == 1) continue;
    if (!scheduleSlotUsed(i)) continue;
    long scheduleSeconds = getScheduleDateTime(i).unixtime();
    if (scheduleSeconds <= nowSeconds) {
      if (!overdueFound || scheduleSeconds < getScheduleDateTime(bestIndex).unixtime()) {
        bestIndex = i;
        overdueFound = true;
      }
    } else if (!overdueFound) {
      if (bestIndex == -1 || scheduleSeconds < getScheduleDateTime(bestIndex).unixtime()) {
        bestIndex = i;
      }
    }
  }

  return bestIndex;
}

// ============================================================
//  BOT TELEGRAM
// ============================================================
void processarMensagem(telegramMessage& msg) {
  String chat_id = msg.chat_id;
  String texto = msg.text;
  texto.trim();

  if (texto == "/start") {
    if (authorizedId == "") {
      authorizedId = chat_id;
      salvarConfiguracao();
      Serial.print("Dono pareado! Chat ID: ");
      Serial.println(authorizedId);
      String msg_boas_vindas = "✅ *Dispenser pareado com sucesso!*\n\n";
      msg_boas_vindas += "Você é o dono deste dispositivo.\n";
      msg_boas_vindas += "Use /ajuda para ver os comandos disponíveis.\n";
      msg_boas_vindas += "Defina o intervalo de alerta com /alterar\\_intervalo retirada N.";
      bot.sendMessage(chat_id, msg_boas_vindas, "Markdown");
    } else if (chat_id == authorizedId) {
      cmdAjuda(chat_id);
    } else {
      bot.sendMessage(chat_id, "⛔ Este dispenser já está pareado com outro usuário.", "");
    }
    return;
  }

  if (chat_id != authorizedId) {
    if (authorizedId == "") {
      bot.sendMessage(chat_id, "⚠️ Dispenser não pareado. Envie /start para se tornar o dono.", "");
    } else {
      bot.sendMessage(chat_id, "⛔ Acesso não autorizado.", "");
    }
    return;
  }

  Serial.print("Mensagem recebida: ");
  Serial.println(texto);

  if (menuAtivo == "agendar_lote") {
  processarMenuAgendarLote(chat_id, texto);
  return;
}
  if (menuAtivo == "agendar_hora") {
    processarMenuHora(chat_id, texto);
    return;
  }
  if (menuAtivo == "agendar_compartimento") {
    processarMenuCompartimento(chat_id, texto);
    return;
  }
  if (menuAtivo == "agendar_data") {
    processarMenuData(chat_id, texto);
    return;
  }
  if (menuAtivo == "intervalo_notif") {
    processarMenuIntervaloNotif(chat_id, texto);
    return;
  }
  if (menuAtivo == "confirm_dispensar") {
    processarMenuConfirmarDispensar(chat_id, texto);
    return;
  }
  if (menuAtivo == "confirm_reset") {
    processarMenuConfirmarReset(chat_id, texto);
    return;
  }

  // Comandos
  if (texto == "/ajuda") {
    cmdAjuda(chat_id);
  }
  else if (texto == "/horarios") {
    cmdListarHorarios(chat_id);
  }
  else if (texto == "/agendar" || texto.startsWith("/agendar ")) {

  if (texto == "/agendar") {
    cmdIniciarAgendamento(chat_id); 
  } else {

    String params = texto.substring(9);

    int espaco = params.indexOf(' ');

    if (espaco == -1) {
      bot.sendMessage(
        chat_id,
        "⚠️ Use:\n/agendar DD/MM/YYYY HH:MM\n\nExemplo:\n/agendar 25/06/2026 08:30",
        ""
      );
      return;
    }

    String data = params.substring(0, espaco);
    String horario = params.substring(espaco + 1);

    // Processa data
    int barra1 = data.indexOf('/');
    int barra2 = data.lastIndexOf('/');

    if (barra1 == -1 || barra2 == -1 || barra1 == barra2) {
      bot.sendMessage(chat_id, "⚠️ Data inválida. Use DD/MM/YYYY", "");
      return;
    }

    tempDia = data.substring(0, barra1).toInt();
    tempMes = data.substring(barra1 + 1, barra2).toInt();
    tempAno = data.substring(barra2 + 1).toInt();

    if (tempDia < 1 || tempDia > 31 || tempMes < 1 || tempMes > 12) {
      bot.sendMessage(chat_id, "⚠️ Data inválida.", "");
      return;
    }

    processarMenuHora(chat_id, horario);
  }
}
  else if (texto == "/wifi" || texto.startsWith("/wifi ")) {
    if (texto == "/wifi") {
      bot.sendMessage(chat_id, "⚠️ Use: /wifi SSID SENHA\nExemplo: /wifi MinhaRede MinhaSenha", "");
    } else {
      cmdAlterarWiFi(chat_id, texto.substring(6));
    }
  }
  else if (texto == "/status") {
    cmdStatus(chat_id);
  }
  else if (texto == "/remover" || texto.startsWith("/remover ")) {
    if (texto == "/remover") {
      bot.sendMessage(chat_id, "🗑️ Utilize /remover HH:MM para remover um agendamento.\nExemplo: /remover 08:00", "");
    } else {
      cmdRemoverHorario(chat_id, texto.substring(9));
    }
  }
  else if (texto == "/intervalos") {
    cmdMostrarIntervalos(chat_id);
  }
  else if (texto == "/alterar_intervalo" || texto.startsWith("/alterar_intervalo ")) {
    if (texto == "/alterar_intervalo") {
      bot.sendMessage(chat_id, "⏱️ Use: /alterar_intervalo retirada MINUTOS\nExemplo:\n/alterar_intervalo retirada 20", "");
    } else {
      cmdAlterarIntervalo(chat_id, texto.substring(19));
    }
  }
  else if (texto == "/reset" || texto == "/reset_horarios" || texto.startsWith("/reset ") || texto.startsWith("/reset_horarios ")) {
    if (texto == "/reset" || texto == "/reset_horarios") {
      cmdReset(chat_id);
    } else {
      cmdReset(chat_id);
    }
  }
  else if (texto == "/dispensar") {
    cmdDispensar(chat_id);
  }
  else if (texto == "/confirmar_dispensar") {
    cmdConfirmarDispensar(chat_id);
  }
  else if (texto == "/renomear_compartimento" || texto.startsWith("/renomear_compartimento ")) {
    if (texto == "/renomear_compartimento") {
      bot.sendMessage(chat_id, "✏️ Use: /renomear_compartimento ÍNDICE NOME\nExemplo: /renomear_compartimento 2 Café", "");
    } else {
      int pos = texto.indexOf(' ');
      cmdRenomearCompartimento(chat_id, texto.substring(pos + 1));
    }
  }
  else if (texto == "/agendar_compartimento" || texto.startsWith("/agendar_compartimento ")) {
    if (texto == "/agendar_compartimento") {
      bot.sendMessage(chat_id, "⏰ Use: /agendar_compartimento ÍNDICE HH:MM\nExemplo: /agendar_compartimento 4 14:30", "");
    } else {
      int pos = texto.indexOf(' ');
      cmdAgendarCompartimento(chat_id, texto.substring(pos + 1));
    }
  }
  else if (texto == "/desparear") {
    cmdDesparear(chat_id);
  }
  else {
    bot.sendMessage(chat_id, "❓ Comando não reconhecido. Use /ajuda para ver os comandos disponíveis.", "");
  }
}

// ============================================================
//  COMANDOS TELEGRAM
// ============================================================
void cmdAjuda(String chat_id) {
  String msg = "💊 *Dispenser de Remédios - ESP32*\n\n";
  msg += "📋 *Comandos:*\n";
  msg += "/agendar — agendar dose\n";
  msg += "/horarios — listar agendamentos\n";
  msg += "/remover HH:MM — remover dose\n";
  msg += "/renomear_compartimento ÍNDICE NOME — nomear dose\n";
  msg += "/agendar_compartimento ÍNDICE HH:MM — alterar horário de dose\n";
  msg += "/reset — apagar todos os agendamentos com confirmação\n";
  msg += "/dispensar — liberar próximo remédio com confirmação\n";
  msg += "/status — estado do dispenser\n";
  msg += "/wifi SSID SENHA — alterar rede WiFi\n";
  msg += "/intervalos — ver tempos de notificação\n";
  msg += "/alterar\\_intervalo — mudar tempos\n";
  msg += "/desparear — desvincular usuário";
  bot.sendMessage(chat_id, msg, "Markdown");
}

void cmdListarHorarios(String chat_id) {
  int total = 0;
  for (int i = 0; i < MAX_DOSES; i++) {
    if (scheduleSlotUsed(i)) total++;
  }

  if (total == 0) {
    bot.sendMessage(chat_id, "📭 Nenhuma dose agendada.", "");
    return;
  }

  String msg = "📋 *Doses agendadas:*\n\n";
  for (int i = 0; i < MAX_DOSES; i++) {
    if (scheduleSlotUsed(i)) {
      msg += String(i + 1) + ". " + getNomeCompartimento(i) + "\n";
      msg += "   ⏰ " + formatHorario(hora[i], minuto[i]);
      msg += " — " + String(dia[i]) + "/" + String(mes[i]) + "/" + String(ano[i]) + "\n";
    }
  }
  bot.sendMessage(chat_id, msg, "Markdown");
}

void cmdIniciarAgendamento(String chat_id) {

  menuAtivo = "agendar_lote";

  bot.sendMessage(
    chat_id,
    "📅 Envie os horários no formato:\n\n"
    "ÍNDICE DD/MM/YYYY HH:MM\n\n"
    "Exemplo:\n"
    "1 24/06/2026 16:00\n"
    "2 24/06/2026 18:00\n"
    "3 24/06/2026 20:00\n\n"
    "Máximo: 15 doses\n"
    "Use /cancelar para sair.",
    ""
  );
}

void processarMenuData(String chat_id, String texto) {
  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Agendamento cancelado.", "");
    return;
  }

  int barra1 = texto.indexOf('/');
  int barra2 = texto.lastIndexOf('/');

  if (barra1 == -1 || barra2 == -1 || barra1 == barra2) {
    bot.sendMessage(chat_id, "⚠️ Formato inválido. Use DD/MM/YYYY\nOu /cancelar para sair.", "");
    return;
  }

  int d = texto.substring(0, barra1).toInt();
  int m = texto.substring(barra1 + 1, barra2).toInt();
  int a = texto.substring(barra2 + 1).toInt();

  if (d < 1 || d > 31 || m < 1 || m > 12) {
    bot.sendMessage(chat_id, "⚠️ Data inválida. Use DD/MM/YYYY\nOu /cancelar para sair.", "");
    return;
  }

  tempDia = d;
  tempMes = m;
  tempAno = a;

  menuAtivo = "agendar_hora";
  bot.sendMessage(chat_id, "⏰ Digite o horário no formato HH:MM\n\nExemplo: 08:30", "");
}

void processarMenuHora(String chat_id, String texto) {
  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Agendamento cancelado.", "");
    return;
  }

  int idx = texto.indexOf(':');
  if (idx == -1) {
    bot.sendMessage(chat_id, "⚠️ Formato inválido. Use HH:MM\nOu /cancelar para sair.", "");
    return;
  }

  int h = texto.substring(0, idx).toInt();
  int m = texto.substring(idx + 1).toInt();

  if (h < 0 || h > 23 || m < 0 || m > 59) {
    bot.sendMessage(chat_id, "⚠️ Horário inválido. Use HH:MM (00-23:00-59)\nOu /cancelar para sair.", "");
    return;
  }

  tempHora = h;
  tempMin = m;
  menuAtivo = "";

  for (int i = 0; i < MAX_DOSES; i++) {
    if (!scheduleSlotUsed(i)) continue;
    if (ano[i] == tempAno && mes[i] == tempMes && dia[i] == tempDia && hora[i] == h && minuto[i] == m) {
      bot.sendMessage(chat_id, "⚠️ Este horário já foi agendado. Escolha outro horário.", "");
      return;
    }
  }

  for (int i = 0; i < MAX_DOSES; i++) {
    if (ano[i] == 0 && mes[i] == 0 && dia[i] == 0 && hora[i] == 0 && minuto[i] == 0) {
      hora[i] = h;
      minuto[i] = m;
      dia[i] = tempDia;
      mes[i] = tempMes;
      ano[i] = tempAno;
      coleta[i] = 0;
      coletav[i] = 0;

      String msg = "✅ *Dose agendada!*\n";
      msg += "📅 Data: " + String(dia[i]) + "/" + String(mes[i]) + "/" + String(ano[i]) + "\n";
      msg += "⏰ Horário: ";
      if (h < 10) msg += "0";
      msg += String(h) + ":";
      if (m < 10) msg += "0";
      msg += String(m);
      bot.sendMessage(chat_id, msg, "Markdown");
      configurado = true;
      salvarConfiguracao();
      return;
    }
  }

  bot.sendMessage(chat_id, "⚠️ Limite de agendamentos atingido.", "");
}

void processarMenuCompartimento(String chat_id, String texto) {
  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Cancelado.", "");
    return;
  }
  menuAtivo = "";
}

void processarMenuAgendarLote(String chat_id, String texto) {

  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Agendamento cancelado.", "");
    return;
  }

  int adicionados = 0;

  while (texto.length()) {

    int quebra = texto.indexOf('\n');

    String linha;

    if (quebra == -1) {
      linha = texto;
      texto = "";
    } else {
      linha = texto.substring(0, quebra);
      texto = texto.substring(quebra + 1);
    }

    linha.trim();

    if (linha.length() == 0)
      continue;

    int e1 = linha.indexOf(' ');
    int e2 = linha.lastIndexOf(' ');

    if (e1 == -1 || e2 == -1 || e1 == e2) {
      continue;
    }

    int indice = linha.substring(0, e1).toInt() - 1;

    if (indice < 0 || indice >= MAX_DOSES)
      continue;

    String data = linha.substring(e1 + 1, e2);
    String horaTxt = linha.substring(e2 + 1);

    int b1 = data.indexOf('/');
    int b2 = data.lastIndexOf('/');

    int c = horaTxt.indexOf(':');

    if (b1 == -1 || b2 == -1 || c == -1)
      continue;

    int d = data.substring(0, b1).toInt();
    int mesTemp = data.substring(b1 + 1, b2).toInt();
    int a = data.substring(b2 + 1).toInt();

    int h = horaTxt.substring(0, c).toInt();
    int min = horaTxt.substring(c + 1).toInt();

    if (
      d < 1 || d > 31 ||
      mesTemp < 1 || mesTemp > 12 ||
      h < 0 || h > 23 ||
      min < 0 || min > 59
    ) {
      continue;
    }

    ano[indice] = a;
    mes[indice] = mesTemp;
    dia[indice] = d;

    hora[indice] = h;
    minuto[indice] = min;

    coleta[indice] = 0;
    coletav[indice] = 0;

    adicionados++;
  }

  menuAtivo = "";

  if (adicionados == 0) {
    bot.sendMessage(
      chat_id,
      "⚠️ Nenhum horário válido encontrado.",
      ""
    );
    return;
  }

  configurado = true;

  salvarConfiguracao();

  bot.sendMessage(
    chat_id,
    "✅ " + String(adicionados) +
    " horários adicionados com sucesso.",
    ""
  );
}

void processarMenuIntervaloNotif(String chat_id, String texto) {
  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Cancelado.", "");
    return;
  }

  int minutos = texto.toInt();
  if (minutos < 1 || minutos > 60) {
    bot.sendMessage(chat_id, "⚠️ Informe um valor entre 1 e 60 minutos.\nOu envie /cancelar.", "");
    return;
  }

  INTERVALO_NOTIF_RETIRADA = minutos;
  menuAtivo = "";
  salvarConfiguracao();
  bot.sendMessage(chat_id, "✅ Intervalo de alerta de retirada definido para " + String(minutos) + " min.", "");
}

void liberarRemedioIndice(int index, bool forcado = false) {
  if (index < 0 || index >= MAX_DOSES) return;

  vez = index;
  lcd.clear();
  for (int i = 0; i < 32; i++) {
    passoFrente();
    Serial.println("Liberando remédio");
    lcd.setCursor(0, 0);
    lcd.print("Liberando");
    lcd.setCursor(0, 1);
    lcd.print("remedio");
    steps = steps + 4;
  }

  Serial.print("Remédio ");
  Serial.print(vez + 1);
  Serial.println(" liberado");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Remedio ");
  lcd.print(vez + 1);
  lcd.setCursor(0, 1);
  lcd.print("liberado");

  liberado = 1;
  coleta[vez] = 1;
  limpa = 1;
  estado.aguardandoRetirada = true;
  estado.gavetaAberta = false;
  alertaProximoHorarioDesligado = false;
  estado.gavetaAbandonada = false;
  estado.notificacoesEnviadas = 0;
  estado.notificacoesGaveta = 0;
  estado.compartimentoAtual = vez + 1;
  estado.tempoUltimaNotif = millis();
  estado.tempoGavetaAberta = 0;

  digitalWrite(buzzer, HIGH);
  delay(1000);
  digitalWrite(buzzer, LOW);

  if (authorizedId != "") {
    String msg = "💊 *Remédio liberado!*\n";
    msg += getNomeCompartimento(vez) + "\n";
    if (forcado) msg += "⚠️ Dispensamento forçado.\n";
    msg += "Por favor, retire seu remédio.";
    bot.sendMessage(authorizedId, msg, "Markdown");
  }
}

void processarMenuConfirmarDispensar(String chat_id, String texto) {
  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Dispensa cancelada.", "");
    return;
  }

  if (texto.equalsIgnoreCase("SIM") || texto.equalsIgnoreCase("S") || texto == "/confirmar_dispensar") {
    menuAtivo = "";
    cmdConfirmarDispensar(chat_id);
    return;
  }

  bot.sendMessage(chat_id, "⚠️ Digite SIM para confirmar ou /cancelar para cancelar.", "");
}

DateTime getScheduleDateTime(int index) {
  return DateTime(ano[index], mes[index], dia[index], hora[index], minuto[index], 0);
}

bool scheduleSlotUsed(int index) {
  return ano[index] != 0 || mes[index] != 0 || dia[index] != 0 || hora[index] != 0 || minuto[index] != 0;
}

long getNextScheduleTimestamp(long currentSeconds) {
  long bestTimestamp = -1;
  int firstIndex = -1;
  int lastIndex = -1;
  long firstTimestamp = 0;
  long lastTimestamp = 0;

  for (int i = 0; i < MAX_DOSES; i++) {
    if (!scheduleSlotUsed(i)) continue;
    DateTime dt = getScheduleDateTime(i);
    long ts = dt.unixtime();

    if (ts > currentSeconds && (bestTimestamp == -1 || ts < bestTimestamp)) {
      bestTimestamp = ts;
    }

    if (firstIndex == -1 || ts < firstTimestamp) {
      firstIndex = i;
      firstTimestamp = ts;
    }
    if (lastIndex == -1 || ts > lastTimestamp) {
      lastIndex = i;
      lastTimestamp = ts;
    }
  }

  if (bestTimestamp != -1) {
    return bestTimestamp;
  }

  if (firstIndex == -1 || lastIndex == -1) {
    return -1;
  }

  DateTime lastDt = getScheduleDateTime(lastIndex);
  DateTime nextFirst = DateTime(lastDt.year(), lastDt.month(), lastDt.day(), hora[firstIndex], minuto[firstIndex], 0);
  if (nextFirst.unixtime() <= lastDt.unixtime()) {
    nextFirst = nextFirst + TimeSpan(1, 0, 0, 0);
  }

  return nextFirst.unixtime();
}

void advanceScheduleCycle() {
  if (liberado == 1 || estado.aguardandoRetirada) return;

  int firstIndex = -1;
  int lastIndex = -1;
  long firstTimestamp = 0;
  long lastTimestamp = 0;

  for (int i = 0; i < MAX_DOSES; i++) {
    if (!scheduleSlotUsed(i)) continue;
    DateTime dt = getScheduleDateTime(i);
    long ts = dt.unixtime();

    if (firstIndex == -1 || ts < firstTimestamp) {
      firstIndex = i;
      firstTimestamp = ts;
    }
    if (lastIndex == -1 || ts > lastTimestamp) {
      lastIndex = i;
      lastTimestamp = ts;
    }
  }

  if (firstIndex == -1 || lastIndex == -1) return;

  DateTime lastDt = getScheduleDateTime(lastIndex);
  DateTime nextFirst = DateTime(lastDt.year(), lastDt.month(), lastDt.day(), hora[firstIndex], minuto[firstIndex], 0);
  if (nextFirst.unixtime() <= lastDt.unixtime()) {
    nextFirst = nextFirst + TimeSpan(1, 0, 0, 0);
  }

  long cycleShift = nextFirst.unixtime() - firstTimestamp;
  if (cycleShift <= 0) return;

  int missedCount = 0;
  for (int i = 0; i < MAX_DOSES; i++) {
    if (scheduleSlotUsed(i) && coleta[i] == 1 && coletav[i] == 0) {
      missedCount++;
    }
  }

  for (int i = 0; i < MAX_DOSES; i++) {
    if (!scheduleSlotUsed(i)) continue;
    DateTime dt = getScheduleDateTime(i) + TimeSpan(cycleShift);
    ano[i] = dt.year();
    mes[i] = dt.month();
    dia[i] = dt.day();
    hora[i] = dt.hour();
    minuto[i] = dt.minute();
    coleta[i] = 0;
    coletav[i] = 0;
  }

  salvarConfiguracao();

  if (authorizedId != "") {
    String msg;
    if (missedCount > 0) {
      msg = "⚠️ Alguns remédios não foram coletados no horário correto. ";
      msg += "Reabasteça o disco e verifique os horários.\n";
      msg += "Doses não coletadas: " + String(missedCount);
    } else {
      msg = "✅ Todos os remédios foram liberados corretamente. ";
      msg += "Reabasteça o disco para o próximo ciclo.";
    }
    bot.sendMessage(authorizedId, msg, "Markdown");
  }
}

void cmdRemoverHorario(String chat_id, String horarioStr) {
  int idx = horarioStr.indexOf(':');
  if (idx == -1) {
    bot.sendMessage(chat_id, "⚠️ Formato inválido. Use /remover HH:MM", "");
    return;
  }

  int h = horarioStr.substring(0, idx).toInt();
  int m = horarioStr.substring(idx + 1).toInt();

  for (int i = 0; i < MAX_DOSES; i++) {
    if (hora[i] == h && minuto[i] == m) {
      hora[i] = 0;
      minuto[i] = 0;
      dia[i] = 0;
      mes[i] = 0;
      ano[i] = 0;
      coleta[i] = 0;
      coletav[i] = 0;

      String msg = "🗑️ Dose das ";
      if (h < 10) msg += "0";
      msg += String(h) + ":";
      if (m < 10) msg += "0";
      msg += String(m) + " removida.";
      bot.sendMessage(chat_id, msg, "");
      configurado = hasHorariosSalvos();
      salvarConfiguracao();
      return;
    }
  }

  bot.sendMessage(chat_id, "⚠️ Nenhuma dose encontrada neste horário.", "");
}

void cmdStatus(String chat_id) {
  String msg = "📊 *Status do Dispenser*\n\n";
  msg += "🕐 Hora atual: ";
  if (momen[3] < 10) msg += "0";
  msg += String(momen[3]) + ":";
  if (momen[4] < 10) msg += "0";
  msg += String(momen[4]) + "\n";

  int total = 0;
  int uncollected = 0;
  for (int i = 0; i < MAX_DOSES; i++) {
    if (scheduleSlotUsed(i)) total++;
    if (scheduleSlotUsed(i) && coleta[i] == 1 && coletav[i] == 0) uncollected++;
  }

  msg += "💊 Doses agendadas: " + String(total) + "/" + String(MAX_DOSES) + "\n";
  if (uncollected > 0) {
    msg += "⚠️ Não coletadas: " + String(uncollected) + "\n";
  }

  if (estado.aguardandoRetirada) {
    msg += "📦 Aguardando retirada: " + getNomeCompartimento(estado.compartimentoAtual - 1) + "\n";
  } else {
    msg += "📦 Estado: Aguardando próxima dose\n";
  }

  int nextIndex = getNextPendingScheduleIndex();
  if (nextIndex != -1) {
    msg += "⏰ Próximo horário: " + getNomeCompartimento(nextIndex) + " às ";
    if (hora[nextIndex] < 10) msg += "0";
    msg += String(hora[nextIndex]) + ":";
    if (minuto[nextIndex] < 10) msg += "0";
    msg += String(minuto[nextIndex]);
    msg += " — " + String(dia[nextIndex]) + "/" + String(mes[nextIndex]) + "/" + String(ano[nextIndex]) + "\n";
  } else {
    msg += "⏰ Próximo horário: nenhum agendado\n";
  }

  bot.sendMessage(chat_id, msg, "Markdown");
}

void cmdMostrarIntervalos(String chat_id) {
  String msg = "⏱️ *Tempos de Notificação:*\n\n";
  msg += "⏰ Retirada não realizada: " + String(INTERVALO_NOTIF_RETIRADA) + " min\n\n";
  msg += "Use /alterar\\_intervalo para mudar.";
  bot.sendMessage(chat_id, msg, "Markdown");
}

void cmdAlterarIntervalo(String chat_id, String params) {
  int espaco = params.indexOf(' ');
  if (espaco == -1) {
    bot.sendMessage(chat_id, "⚠️ Formato: /alterar_intervalo retirada MINUTOS", "");
    return;
  }

  String tipo = params.substring(0, espaco);
  int minutos = params.substring(espaco + 1).toInt();

  if (minutos < 1 || minutos > 60) {
    bot.sendMessage(chat_id, "⚠️ Use um valor entre 1 e 60 minutos.", "");
    return;
  }

  if (tipo == "retirada") {
    INTERVALO_NOTIF_RETIRADA = minutos;
    bot.sendMessage(chat_id, "✅ Intervalo de retirada alterado para " + String(minutos) + " min.", "");
  }else {
    bot.sendMessage(chat_id, "⚠️ Tipo inválido. Use: retirada, gaveta ou abandon.", "");
  }
}

void cmdAlterarWiFi(String chat_id, String params) {
  int espaco = params.indexOf(' ');
  if (espaco == -1) {
    bot.sendMessage(chat_id, "⚠️ Use: /wifi SSID SENHA\nExemplo: /wifi MinhaRede MinhaSenha", "");
    return;
  }

  String novoSSID = params.substring(0, espaco);
  String novaSenha = params.substring(espaco + 1);

  if (novoSSID.length() == 0 || novaSenha.length() == 0) {
    bot.sendMessage(chat_id, "⚠️ SSID ou senha inválidos.", "");
    return;
  }

  WIFI_SSID = novoSSID;
  WIFI_PASSWORD = novaSenha;
  salvarCredenciaisWiFi();

  wifiConectado = false;
  wifiConectado2 = false;
  wificonectar2 = true;
  ultimaTentativaWiFi1 = 0;
  estadosr = true;

  bot.sendMessage(
    chat_id,
    "✅ Credenciais salvas. Tentando migrar para a nova rede.",
    ""
  );
}

void cmdRenomearCompartimento(String chat_id, String params) {
  int espaco = params.indexOf(' ');
  if (espaco == -1) {
    bot.sendMessage(chat_id, "✏️ Use: /renomear_compartimento ÍNDICE NOME\nExemplo: /renomear_compartimento 2 Café", "");
    return;
  }

  int index = params.substring(0, espaco).toInt() - 1;
  String nome = params.substring(espaco + 1);
  nome.trim();

  if (index < 0 || index >= MAX_DOSES || nome.length() == 0) {
    bot.sendMessage(chat_id, "⚠️ Índice inválido ou nome vazio. Use: /renomear_compartimento ÍNDICE NOME", "");
    return;
  }

  nomeCompartimento[index] = nome;
  salvarConfiguracao();
  bot.sendMessage(chat_id, "✅ Compartimento " + String(index + 1) + " renomeado para: " + nome, "");
}

void cmdAgendarCompartimento(String chat_id, String params) {
  int espaco = params.indexOf(' ');
  if (espaco == -1) {
    bot.sendMessage(chat_id, "⏰ Use: /agendar_compartimento ÍNDICE HH:MM\nExemplo: /agendar_compartimento 4 14:30", "");
    return;
  }

  int index = params.substring(0, espaco).toInt() - 1;
  String horarioStr = params.substring(espaco + 1);
  int idx = horarioStr.indexOf(':');

  if (index < 0 || index >= MAX_DOSES || idx == -1) {
    bot.sendMessage(chat_id, "⚠️ Formato inválido. Use: /agendar_compartimento ÍNDICE HH:MM", "");
    return;
  }

  int h = horarioStr.substring(0, idx).toInt();
  int m = horarioStr.substring(idx + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) {
    bot.sendMessage(chat_id, "⚠️ Horário inválido. Use HH:MM (00-23:00-59)", "");
    return;
  }

  if (!scheduleSlotUsed(index)) {
    bot.sendMessage(chat_id, "⚠️ Não há agendamento para este compartimento. Crie um horário primeiro com /agendar.", "");
    return;
  }

  if (!tempoValidoParaCompartimento(index, h, m)) {
    bot.sendMessage(chat_id, "⚠️ Horário fora dos limites do compartimento. O novo horário deve ficar entre o horário anterior e o próximo.", "");
    return;
  }

  hora[index] = h;
  minuto[index] = m;
  salvarConfiguracao();

  String msg = "✅ Horário atualizado para " + getNomeCompartimento(index) + "\n";
  msg += "⏰ Novo horário: " + formatHorario(h, m) + "\n";
  msg += "Compartimento: " + String(index + 1);
  bot.sendMessage(chat_id, msg, "Markdown");
}

int getCurrentCompartimentoPos() {
  if (estado.compartimentoAtual > 0) {
    return estado.compartimentoAtual;
  }
  return vez + 1;
}

void girarParaInicio(int passosCompartimento) {
  for (int i = 0; i < passosCompartimento * 32; i++) {
    passoTras();
  }
}

void cmdReset(String chat_id) {
  if (!configurado || !hasHorariosSalvos()) {
    bot.sendMessage(chat_id, "⚠️ Nenhum horário agendado para resetar.", "");
    return;
  }

  menuAtivo = "confirm_reset";
  bot.sendMessage(chat_id, "⚠️ Confirmar reset de todos os horários? Responda SIM para confirmar ou /cancelar para cancelar.", "");
}

void cmdConfirmarReset(String chat_id) {
  int currentPos = getCurrentCompartimentoPos();
  if (currentPos > 0) {
    girarParaInicio(currentPos);
  }

  for (int i = 0; i < MAX_DOSES; i++) {
    ano[i] = 0;
    mes[i] = 0;
    dia[i] = 0;
    hora[i] = 0;
    minuto[i] = 0;
    coleta[i] = 0;
    coletav[i] = 0;
    missedDoseFlag[i] = false;
    nomeCompartimento[i] = "Compartimento " + String(i + 1);
  }
  configurado = false;
  liberado = 0;
  missedDosesCycle = 0;
  estado.aguardandoRetirada = false;
  estado.gavetaAberta = false;
  estado.gavetaAbandonada = false;
  estado.compartimentoAtual = 0;
  estado.notificacoesEnviadas = 0;
  estado.notificacoesGaveta = 0;
  estado.tempoUltimaNotif = 0;
  estado.tempoGavetaAberta = 0;
  irHighStart = 0;
  alertaProximoHorarioDesligado = false;
  salvarConfiguracao();
  bot.sendMessage(chat_id, "✅ Todos os horários foram apagados e o disco retornou à posição inicial.", "");
  menuAtivo = "intervalo_notif";
  bot.sendMessage(chat_id, "⌛ Agora digite o intervalo de alerta de retirada em minutos (1-60).\nOu /cancelar para cancelar.", "");
}

void processarMenuConfirmarReset(String chat_id, String texto) {
  if (texto == "/cancelar") {
    menuAtivo = "";
    bot.sendMessage(chat_id, "❌ Reset cancelado.", "");
    return;
  }

  if (texto.equalsIgnoreCase("SIM") || texto.equalsIgnoreCase("S") || texto == "/confirmar_reset") {
    menuAtivo = "";
    cmdConfirmarReset(chat_id);
    return;
  }

  bot.sendMessage(chat_id, "⚠️ Digite SIM para confirmar ou /cancelar para cancelar.", "");
}

void cmdDispensar(String chat_id) {
  if (!configurado || !hasHorariosSalvos()) {
    bot.sendMessage(chat_id, "⚠️ Nenhum horário agendado para dispensar.", "");
    return;
  }

  menuAtivo = "confirm_dispensar";
  bot.sendMessage(chat_id, "⚠️ Confirmar liberação do próximo remédio? Responda SIM para confirmar ou /cancelar para cancelar.", "");
}

void cmdConfirmarDispensar(String chat_id) {
  int nextIndex = findNextScheduleIndex();
  if (nextIndex == -1) {
    bot.sendMessage(chat_id, "⚠️ Não há próximo remédio agendado para dispensar.", "");
    return;
  }

  if (estado.aguardandoRetirada) {
    bot.sendMessage(chat_id, "⚠️ Há um remédio anterior não coletado. A dispensa forçada será realizada mesmo assim.", "");
  }

  liberarRemedioIndice(nextIndex, true);
  bot.sendMessage(chat_id, "✅ Remédio liberado à força.", "");
}

void cmdDesparear(String chat_id) {
  authorizedId = "";
  salvarConfiguracao();
  bot.sendMessage(chat_id, "🔓 Dispenser despareado.", "");
  Serial.println("Dispenser despareado.");
}

// ============================================================
//  DOSES E RETIRADA
// ============================================================
void verificarDoses() {
  for (int i = 0; i < MAX_DOSES; i++) {
    if (coleta[i] == 1) continue;
    if (hora[i] != momen[3] || minuto[i] != momen[4]) continue;
    if (dia[i] != momen[2] || mes[i] != momen[1] || ano[i] != momen[0]) continue;

    if (!estado.aguardandoRetirada && liberado == 0) {
      vez = i;
      break;
    }
  }
}

void dispensarDose(int compartimento) {
  estado.aguardandoRetirada = true;
  estado.gavetaAberta = false;
  estado.gavetaAbandonada = false;
  estado.notificacoesEnviadas = 0;
  estado.notificacoesGaveta = 0;
  estado.compartimentoAtual = compartimento;
  estado.tempoUltimaNotif = millis();
  estado.tempoGavetaAberta = 0;

  String msg = "💊 *Dose disponível!*\n";
  msg += "Compartimento: " + getNomeCompartimento(compartimento - 1) + "\n";
  msg += "Por favor, retire seu remédio.";
  
  if (authorizedId != "") {
    bot.sendMessage(authorizedId, msg, "Markdown");
  }
}

void gerenciarRetirada() {
  Serial.println(analogRead(33));
  Serial.println(analogRead(34));
  Serial.println(analogRead(35));
  if((analogRead(33) < 50 ||analogRead(34) < 50 || analogRead(35) < 50)){
      remedioPresente = true;
    }else{
      remedioPresente = false;
    }
  unsigned long agora = millis();

  if (!remedioPresente) {
    if (irHighStart == 0) {
      irHighStart = agora;
    }
  } else {
    irHighStart = 0;
  }

  // Se o sensor permanecer em HIGH por 5 segundos, considera coletado
  if (!remedioPresente && irHighStart != 0 && agora - irHighStart >= 5000) {
    if (estado.compartimentoAtual > 0 && estado.compartimentoAtual - 1 < MAX_DOSES) {
      int idx = estado.compartimentoAtual - 1;
      coletav[idx] = 1;
      if (missedDoseFlag[idx]) {
        missedDoseFlag[idx] = false;
        if (missedDosesCycle > 0) missedDosesCycle--;
      }
    }
    estado.aguardandoRetirada = false;
    liberado = 0;
    irHighStart = 0;
    showLcdConfirmation("OK Remedio", "Coletado!");
    if (authorizedId != "") {
      bot.sendMessage(authorizedId, "✅ Remédio coletado com sucesso!", "");
    }
    return;
  }

  if (estado.aguardandoRetirada) {
    int nextIndex = getNextPendingScheduleIndex();
    if (nextIndex != -1) {
      DateTime agoraDt = DateTime(momen[0], momen[1], momen[2], momen[3], momen[4], 0);
      long agoraSeconds = agoraDt.unixtime();
      long proximoSeconds = getScheduleDateTime(nextIndex).unixtime();
      if (proximoSeconds - agoraSeconds <= 60 && !alertaProximoHorarioDesligado) {
        alertaProximoHorarioDesligado = true;
        int currentIndex = estado.compartimentoAtual - 1;
        if (currentIndex >= 0 && currentIndex < MAX_DOSES && !missedDoseFlag[currentIndex]) {
          missedDoseFlag[currentIndex] = true;
          missedDosesCycle++;
        }
        if (authorizedId != "") {
          String msg = "⚠️ O remédio anterior " + getNomeCompartimento(currentIndex) + " não foi coletado. ";
          msg += "O próximo horário é " + formatHorario(hora[nextIndex], minuto[nextIndex]) + ".";
          bot.sendMessage(authorizedId, msg, "");
          playWarningBeep();
        }
        lcdShowingUncollected = true;
        lcdUncollectedEndTime = agora + 10000;
        estado.tempoUltimaNotif = agora;
        estado.notificacoesEnviadas++;
      }
    }

    unsigned long intervalMs = (unsigned long)INTERVALO_NOTIF_RETIRADA * 60000UL;
    if (agora - estado.tempoUltimaNotif >= intervalMs && estado.notificacoesEnviadas < 3) {
      int currentIndex = estado.compartimentoAtual - 1;
      if (currentIndex >= 0 && currentIndex < MAX_DOSES && !missedDoseFlag[currentIndex]) {
        missedDoseFlag[currentIndex] = true;
        missedDosesCycle++;
      }
      if (authorizedId != "") {
        String msg = "⚠️ O remédio " + getNomeCompartimento(currentIndex) + " não foi coletado.\n";
        msg += "Retire-o agora.";
        bot.sendMessage(authorizedId, msg, "");
        playWarningBeep();
      }
      estado.tempoUltimaNotif = agora;
      estado.notificacoesEnviadas++;
    }
  }
}

// ============================================================
//  DISPENSAR
// ============================================================
void acao() {
  if (vez >= 0 && vez < MAX_DOSES && scheduleSlotUsed(vez) && liberado == 0) {
    DateTime nowDt = DateTime(momen[0], momen[1], momen[2], momen[3], momen[4], 0);
    if (getScheduleDateTime(vez).unixtime() <= nowDt.unixtime()) {
      liberarRemedioIndice(vez);
    }
  }

  if (limpa == 1) {
    lcd.clear();
    limpa = 0;
  }

  if (coleta[vez] == 1) {
    if((analogRead(33) < 50||analogRead(34) < 50 || analogRead(35) < 50)){
      remedioPresente = true;
    }else{
      remedioPresente = false;
    }
    unsigned long agora = millis();

    if (remedioPresente) {
      irHighStart = 0;
      lcdClearLine(0);
      lcdClearLine(1);
      lcdPrintLine(0, "Remedio nao");
      lcdPrintLine(1, "coletado");
      digitalWrite(buzzer, LOW);
    } else {
      if (irHighStart == 0) {
        irHighStart = agora;
      }
      unsigned long elapsed = agora - irHighStart;
      lcdClearLine(0);
      lcdClearLine(1);
      if (elapsed < 5000) {
        unsigned long secRestantes = 5 - elapsed / 1000;
        lcdPrintLine(0, "Confirmando...");
        lcdPrintLine(1, "Falta: " + String(secRestantes) + "s");
      } else {
        lcdPrintLine(0, "Confirmando...");
        lcdPrintLine(1, "Aguardando");
      }
    }
  }

  if (vez == MAX_DOSES) {
    for (int i = 0; i < 32; i++) {
      passoFrente();
      steps = steps + 4;
    }
    steps = 0;
    vez = 0;

    if (missedDosesCycle > 1) {
      Serial.println("Alguns remédios não foram coletados no horário correto");
    } else if (missedDosesCycle == 1) {
      Serial.println("Um remédio não foi coletado no horário correto");
    }
    if (missedDosesCycle > 0 && authorizedId != "") {
      String msg = "⚠️ Alguns remédios não foram coletados no horário correto. Doses não coletadas neste ciclo: ";
      msg += String(missedDosesCycle) + ".";
      bot.sendMessage(authorizedId, msg, "Markdown");
    }
    missedDosesCycle = 0;
    for (int i = 0; i < MAX_DOSES; i++) {
      missedDoseFlag[i] = false;
    }
    Serial.println("Todos os remédios foram liberados, favor reabastecer o disco");
  }
}

void passoFrente() {
  for (int j = 0; j < 4; j++) {
    digitalWrite(IN1, seq2[j][0]);
    digitalWrite(IN2, seq2[j][1]);
    digitalWrite(IN3, seq2[j][2]);
    digitalWrite(IN4, seq2[j][3]);
    delay(delayStep);
  }
}

void passoTras() {
  for (int j = 3; j >= 0; j--) {
    digitalWrite(IN1, seq2[j][0]);
    digitalWrite(IN2, seq2[j][1]);
    digitalWrite(IN3, seq2[j][2]);
    digitalWrite(IN4, seq2[j][3]);
    delay(delayStep);
  }
}

void lcdClearLine(int line) {
  lcd.setCursor(0, line);
  for (int i = 0; i < 16; i++) {
    lcd.print(' ');
  }
  lcd.setCursor(0, line);
}

void lcdPrintLine(int line, String text) {
  if (text.length() > 16) {
    text = text.substring(0, 16);
  }
  lcd.setCursor(0, line);
  lcd.print(text);
  int remaining = 16 - text.length();
  if (remaining < 0) remaining = 0;
  for (int i = 0; i < remaining; i++) {
    lcd.print(' ');
  }
}

void showLcdConfirmation(String line1, String line2) {
  lcd.clear();
  lcdPrintLine(0, line1);
  lcdPrintLine(1, line2);
  lcdShowingConfirm = true;
  lcdConfirmEndTime = millis() + 5000;
}

void atualizarLCDStatus() {
  if (!configurado) {
    lcd.clear();
    lcdPrintLine(0, "Aguardando");
    lcdPrintLine(1, "configuracao");
    return;
  }

  unsigned long agora = millis();
  
  if (lcdShowingUncollected && agora >= lcdUncollectedEndTime) {
    lcdShowingUncollected = false;
  }

  int nextIndex = getNextPendingScheduleIndex();
  String horaAgora = formatHorario(momen[3], momen[4]);

  lcd.clear();
  lcdPrintLine(0, "Agora " + horaAgora);
  
  if (lcdShowingUncollected) {
    int currentIndex = estado.compartimentoAtual - 1;
    if (currentIndex >= 0 && currentIndex < MAX_DOSES) {
      lcdPrintLine(1, "Nao coletado!");
    }
    return;
  }
  
  if (estado.aguardandoRetirada) {
    lcdPrintLine(1, "Retirar: " + getNomeCompartimento(estado.compartimentoAtual - 1));
    return;
  }

  if (nextIndex != -1) {
    String proxHora = formatHorario(hora[nextIndex], minuto[nextIndex]);
    lcdPrintLine(1, "Prox " + proxHora);
  } else {
    lcdPrintLine(1, "Sem proximo");
  }
}

void playWarningBeep() {
  digitalWrite(buzzer, HIGH);
  delay(150);
  digitalWrite(buzzer, LOW);
  delay(50);
  digitalWrite(buzzer, HIGH);
  delay(150);
  digitalWrite(buzzer, LOW);
}

void Time(int tipo) {
  DateTime now;
  if (rtcDisponivel) {
    now = rtc.now();
  } else {
    unsigned long elapsedSeconds = (millis() - internalMillisStart) / 1000UL;
    now = DateTime(internalClockStart + elapsedSeconds);
  }

  String a = String(now.year(), DEC);
  String m = (now.month() < 10 ? "0" : "") + String(now.month(), DEC);
  String d = (now.day() < 10 ? "0" : "") + String(now.day(), DEC);
  String h = (now.hour() < 10 ? "0" : "") + String(now.hour(), DEC);
  String min = (now.minute() < 10 ? "0" : "") + String(now.minute(), DEC);
  String s = (now.second() < 10 ? "0" : "") + String(now.second(), DEC);
  String diaSem = diasSem[now.dayOfTheWeek()];

  horario = h + ":" + min + ":" + s;
  horario2 = d + "/" + m + "/" + a;
  horario3 = h + ":" + min;
  horario4 = d + "/" + m;

  horario5 = diaSem;

  if (tipo == 0) {
    horarioF = horario3;
  } else if (tipo == 1) {
    horarioF = horario2;
  } else if (tipo == 2) {
    horarioF = horario3;
  } else if (tipo == 3) {
    horarioF = horario4;
  } else if (tipo == 4) {
    horarioF = horario5;
  }

  momen[0] = a.toInt();
  momen[1] = m.toInt();
  momen[2] = d.toInt();
  momen[3] = h.toInt();
  momen[4] = min.toInt();
  momen[5] = s.toInt();

  if (!configurado) {
    if (mommin != momen[4] && !lcdShowingConfirm) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Aguardando");
      lcd.setCursor(0, 1);
      lcd.print("configuracao");
    }
    return;
  }

  minT = momen[4] + 60 * momen[3];

  minT = momen[4] + 60 * momen[3];
  minT2 = minuto[vez] + 60 * hora[vez];

  if (mommin != momen[4] && !lcdShowingConfirm) {
    atualizarLCDStatus();
  }
}


