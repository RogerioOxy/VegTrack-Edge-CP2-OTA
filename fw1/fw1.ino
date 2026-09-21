// VegTrack - Edge Computing CP2
// Rogério Deligi Ferreira Filho - RM561942
// Maria Fernanda Garavelli Dantas - RM562686
// Ciências da Computação - 4º semestre
#include <Arduino.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_arduino_version.h>
#include <cJSON.h>
#include <time.h>

static const char *const FW_VERSION = "1.0";
static const char *const MANIFEST_URL =
    "https://raw.githubusercontent.com/RogerioOxy/VegTrack-Edge-CP2-OTA/main/version.json";
static const uint8_t LED_R = 25, LED_G = 26, LED_B = 27;
static const uint32_t READING_INTERVAL_MS = 2000;
static const uint32_t SESSION_INTERVAL_MS = 48000;
static uint32_t sessionStartedAt, firstSessionAt;
static unsigned readingIndex = 0, completedSessions = 0;
static int readings[5];
static volatile bool otaRunning = false;

// Coleção de CAs incluída no core ESP32 3.3.11 instalado.
// Os certificados e o nome do servidor são verificados; não usamos setInsecure.
extern const uint8_t caBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_x509_crt_bundle_end");

void setRgb(bool red, bool green, bool blue) {
  digitalWrite(LED_R, red);
  digitalWrite(LED_G, green);
  digitalWrite(LED_B, blue);
}

// BEGIN PURE FUNCTIONS - usados também pelos testes locais.
float meanFive(const int *values) {
  int sum = 0;
  for (int i = 0; i < 5; i++) sum += values[i];
  return sum / 5.0f;
}

bool parseVersion(const char *text, unsigned &major, unsigned &minor) {
  if (!text || !*text) return false;
  unsigned parts[2] = {0, 0};
  for (int part = 0; part < 2; part++) {
    unsigned digits = 0;
    while (*text >= '0' && *text <= '9') {
      if (++digits > 5) return false;
      parts[part] = parts[part] * 10 + (*text++ - '0');
      if (parts[part] > 65535) return false;
    }
    if (digits == 0) return false;
    if (part == 0 && *text++ != '.') return false;
  }
  if (*text != '\0') return false;
  major = parts[0];
  minor = parts[1];
  return true;
}

bool isNewer(unsigned major, unsigned minor, unsigned installedMajor,
             unsigned installedMinor) {
  return major > installedMajor ||
         (major == installedMajor && minor > installedMinor);
}
// END PURE FUNCTIONS

void printVector(const char *label, const int *values) {
  Serial.print(label);
  for (int i = 0; i < 5; i++) Serial.printf("%d%s", values[i], i == 4 ? "\n" : " ");
}

void printFlashProof() {
  const esp_partition_t *partition = esp_ota_get_running_partition();
  Serial.printf("PARTIÇÃO atual: %s offset=0x%lx tamanho=0x%lx\n",
                partition ? partition->label : "?",
                partition ? (unsigned long)partition->address : 0UL,
                partition ? (unsigned long)partition->size : 0UL);
  Serial.printf("IMAGEM FW=%s bytes=%u MD5=%s compilada=%s %s\n", FW_VERSION,
                ESP.getSketchSize(), ESP.getSketchMD5().c_str(), __DATE__, __TIME__);
}

bool parseManifest(const String &body, String &version, String &url) {
  // cJSON valida o JSON completo; não buscamos campos por posição no texto.
  const char *end = nullptr;
  cJSON *document = cJSON_ParseWithOpts(body.c_str(), &end, true);
  if (!document) return false;
  const cJSON *versionField = cJSON_GetObjectItemCaseSensitive(document, "version");
  const cJSON *urlField = cJSON_GetObjectItemCaseSensitive(document, "url");
  unsigned major, minor;
  bool valid = cJSON_IsObject(document) && cJSON_GetArraySize(document) == 2 &&
               cJSON_IsString(versionField) && cJSON_IsString(urlField) &&
               parseVersion(versionField->valuestring, major, minor);
  if (valid) {
    version = versionField->valuestring;
    url = urlField->valuestring;
    valid = url.startsWith("https://raw.githubusercontent.com/RogerioOxy/") &&
            url.endsWith(".bin") && url.indexOf(' ') < 0;
  }
  cJSON_Delete(document);
  return valid;
}

bool connectWiFi() {
  Serial.println("Wi-Fi: conectando à rede Wokwi-GUEST...");
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin("Wokwi-GUEST", "", 6);
  }
  uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 10000) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERRO 1: sem conexão Wi-Fi. As medições continuam.");
    return false;
  }
  Serial.printf("Wi-Fi conectado; IP=%s\n", WiFi.localIP().toString().c_str());
  // TLS precisa de data correta para verificar a validade dos certificados.
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  started = millis();
  while (time(nullptr) < 1704067200 && millis() - started < 10000) delay(100);
  if (time(nullptr) < 1704067200) {
    Serial.println("ERRO 2: relógio não sincronizado; consulta HTTPS cancelada.");
    return false;
  }
  Serial.printf("TLS: relógio UTC=%lld; coleção de CAs=%u bytes\n",
                (long long)time(nullptr), (unsigned)(caBundleEnd - caBundleStart));
  return true;
}

void configureHttps(NetworkClientSecure &client, HTTPClient &http) {
  client.setCACertBundle(caBundleStart, caBundleEnd - caBundleStart);
  client.setHandshakeTimeout(12);
  client.setTimeout(8000);
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  // São URLs diretas HTTPS: uma resposta 301/302 é registrada como falha.
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
}

bool fetchManifest(const char *manifestUrl, String &version, String &url) {
  NetworkClientSecure client;
  HTTPClient http;
  configureHttps(client, http);
  Serial.printf("Consulta: %s\n", manifestUrl);
  if (!http.begin(client, manifestUrl)) {
    Serial.println("ERRO 2: manifesto não pode ser acessado.");
    return false;
  }
  int status = http.GET();
  int size = http.getSize();
  Serial.printf("Manifesto HTTP=%d bytes=%d\n", status, size);
  if (status != HTTP_CODE_OK || size <= 0 || size > 1024) {
    Serial.println("ERRO 2: manifesto inacessível ou tamanho inválido.");
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  Serial.printf("Manifesto recebido: %s\n", body.c_str());
  if (body.length() != (unsigned)size || !parseManifest(body, version, url)) {
    Serial.println("ERRO 2: manifesto inválido; esperado JSON com version e url HTTPS.");
    return false;
  }
  return true;
}

bool downloadAndUpdate(const String &url) {
  NetworkClientSecure client;
  HTTPClient http;
  configureHttps(client, http);
  Serial.printf("Download remoto: %s\n", url.c_str());
  if (!http.begin(client, url)) {
    Serial.println("ERRO 4: arquivo de firmware não pode ser baixado.");
    return false;
  }
  int status = http.GET();
  int size = http.getSize();
  Serial.printf("Firmware HTTP=%d bytes=%d\n", status, size);
  if (status != HTTP_CODE_OK || size <= 0) {
    Serial.println("ERRO 4: download indisponível ou tamanho não informado.");
    http.end();
    return false;
  }
  const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
  if (!target || size > (int)target->size || !Update.begin(size, U_FLASH)) {
    Serial.printf("ERRO 5: não foi possível iniciar OTA; código=%u\n", Update.getError());
    Update.printError(Serial);
    http.end();
    return false;
  }
  Serial.printf("OTA: gravando partição=%s offset=0x%lx\n", target->label,
                (unsigned long)target->address);
  uint8_t buffer[2048];
  size_t written = 0, lastReport = 0;
  uint32_t started = millis(), lastData = started;
  NetworkClient *stream = http.getStreamPtr();
  while (written < (size_t)size) {
    if (millis() - started > 120000 || millis() - lastData > 10000) {
      Serial.println("ERRO 4: download interrompido ou tempo limite excedido.");
      Update.abort();
      http.end();
      return false;
    }
    int available = stream->available();
    if (available <= 0) { delay(1); continue; }
    size_t wanted = min(sizeof(buffer), (size_t)available);
    wanted = min(wanted, (size_t)size - written);
    int count = stream->read(buffer, wanted);
    if (count <= 0) { delay(1); continue; }
    lastData = millis();
    if (Update.write(buffer, count) != (size_t)count) {
      Serial.printf("ERRO 5: gravação OTA falhou; código=%u\n", Update.getError());
      Update.printError(Serial);
      Update.abort();
      http.end();
      return false;
    }
    written += count;
    if (written - lastReport >= 65536 || written == (size_t)size) {
      Serial.printf("OTA bytes=%u/%d\n", (unsigned)written, size);
      lastReport = written;
    }
    delay(1);
  }
  http.end();
  if (!Update.end() || !Update.isFinished()) {
    Serial.printf("ERRO 5: finalização OTA falhou; código=%u\n", Update.getError());
    Update.printError(Serial);
    return false;
  }
  Serial.println("OTA: imagem validada, partição de boot selecionada. Reiniciando...");
  Serial.flush();
  ESP.restart();
  return true;
}

void otaTask(void *argument) {
  char command = (char)(uintptr_t)argument;
  // Os comandos numerados provocam falhas reais com entradas de diagnóstico.
  // Eles não representam leituras ou uma atualização bem-sucedida.
  if (command == '1') {
    WiFi.disconnect(false);
    delay(100);
    if (WiFi.status() != WL_CONNECTED)
      Serial.println("ERRO 1: sem conexão Wi-Fi. Diagnóstico após desconectar a rede.");
  } else if (connectWiFi()) {
    String version, url;
    const char *manifestUrl = command == '2'
      ? "https://raw.githubusercontent.com/RogerioOxy/VegTrack-Edge-CP2-OTA/main/manifesto-inexistente.json"
      : MANIFEST_URL;
    if (command == '4' || command == '5') {
      // 404 comprova falha no download; JSON (não é imagem ESP32) falha na gravação.
      const char *testUrl = command == '4'
        ? "https://raw.githubusercontent.com/RogerioOxy/VegTrack-Edge-CP2-OTA/main/firmware-inexistente.bin"
        : MANIFEST_URL;
      downloadAndUpdate(testUrl);
    } else if (fetchManifest(manifestUrl, version, url)) {
      unsigned major, minor, installedMajor, installedMinor;
      if (parseVersion(version.c_str(), major, minor) &&
          parseVersion(FW_VERSION, installedMajor, installedMinor)) {
        Serial.printf("Versão instalada=%s disponível=%s\n", FW_VERSION, version.c_str());
        if (isNewer(major, minor, installedMajor, installedMinor)) {
          Serial.println("OTA: atualização disponível após três ciclos completos.");
          downloadAndUpdate(url);
        } else {
          Serial.println("INFO 3: versão instalada já é a mais recente; nenhuma gravação.");
        }
      }
    }
  }
  Serial.println("Consulta OTA encerrada; medições continuam até parar a simulação.");
  otaRunning = false;
  vTaskDelete(nullptr);
}

void startOtaTask(char command) {
  if (completedSessions < 3 || otaRunning) {
    Serial.println("Consulta adiada: aguarde três ciclos completos e o fim da tarefa atual.");
    return;
  }
  otaRunning = true;
  if (xTaskCreate(otaTask, "consulta-ota", 12288, (void *)(uintptr_t)command,
                  1, nullptr) != pdPASS) {
    otaRunning = false;
    Serial.println("ERRO 5: não foi possível criar a tarefa OTA.");
  }
}


void finishSession() {
  printVector("Ordem original: ", readings);
  Serial.printf("Média da sessão: %.1f cm\n", meanFive(readings));

  completedSessions++;
  Serial.printf("Ciclo %u completo em t=%lums; próximo início previsto=%lums\n",
                completedSessions, (unsigned long)(millis() - firstSessionAt),
                (unsigned long)(completedSessions * SESSION_INTERVAL_MS));
  if (completedSessions == 3) {
    Serial.println("Três ciclos completos. Iniciando tarefa de consulta OTA.");
    startOtaTask('m');
  }
}

void startSession() {
  readingIndex = 0;
  Serial.printf("Nova sessão: real=%lums previsto=%lums; próxima em +48000ms\n",
                (unsigned long)(millis() - firstSessionAt),
                (unsigned long)(sessionStartedAt - firstSessionAt));
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  randomSeed(esp_random());
  Serial.printf("\n========================================\n"
                "MONITORAMENTO DE VEGETAÇÃO - FW %s\n"
                "========================================\n", FW_VERSION);
  Serial.printf("CORE Arduino ESP32: %s\n", ESP_ARDUINO_VERSION_STR);
  printFlashProof();
  Serial.println("Comandos após 3 ciclos: m=consultar; 1=sem Wi-Fi; 2=manifesto 404;");
  Serial.println("4=binário 404; 5=imagem inválida. p=prova da partição.");
  setRgb(false, false, true);
  Serial.println("LED azul: Firmware 1.0.");
  firstSessionAt = sessionStartedAt = millis();
  startSession();
}

void loop() {
  if (Serial.available()) {
    char command = Serial.read();
    if (command == 'p') printFlashProof();
    if (command == 'm' || command == '1' || command == '2' ||
        command == '4' || command == '5') {
      Serial.printf("COMANDO %c: %s\n", command,
                    command == 'm' ? "consulta normal" : "DIAGNÓSTICO de erro");
      startOtaTask(command);
    }
  }
  uint32_t now = millis();
  if (now - sessionStartedAt >= SESSION_INTERVAL_MS) {
    // Soma 48 s à referência anterior; não soma 48 s à quinta leitura.
    sessionStartedAt += SESSION_INTERVAL_MS;
    startSession();
  }
  if (readingIndex < 5 &&
      now - sessionStartedAt >= readingIndex * READING_INTERVAL_MS) {
    readings[readingIndex] = random(10, 21); // limite superior exclusivo
    Serial.printf("Leitura %u: %d cm; real=+%lums previsto=+%lums\n",
                  readingIndex + 1, readings[readingIndex],
                  (unsigned long)(millis() - sessionStartedAt),
                  (unsigned long)(readingIndex * READING_INTERVAL_MS));
    readingIndex++;
    if (readingIndex == 5) finishSession();
  }
  delay(1); // libera a CPU; não representa a espera entre sessões
}
