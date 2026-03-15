#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// CONFIGURACIÓN
// =====================================================
#define SIGNAL_PIN 18
#define AP_SSID "ESP32-FREQ"
#define AP_PASS "12345678"

#define QUEUE_SIZE 128
#define WEB_REFRESH_MS 500   // refresco de la web cada 0.5 s

// =====================================================
// SERVIDOR WEB
// =====================================================
WebServer server(80);

// =====================================================
// TIMER HARDWARE
// Se usa como base de tiempo en microsegundos
// =====================================================
hw_timer_t *timer = NULL;

// =====================================================
// COLA CIRCULAR DE PERIODOS
// Cada elemento guarda el tiempo entre dos flancos
// =====================================================
volatile uint32_t periodQueue[QUEUE_SIZE];
volatile uint16_t queueHead = 0;
volatile uint16_t queueTail = 0;
volatile uint16_t queueCount = 0;

// =====================================================
// VARIABLES COMPARTIDAS
// =====================================================
volatile uint64_t lastTimeUs = 0;
volatile bool firstEdge = true;
volatile uint32_t lostSamples = 0;

// Protección de acceso entre ISR y loop
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// =====================================================
// ESTADÍSTICAS DE FRECUENCIA
// =====================================================
float fMin = 0.0f;
float fMax = 0.0f;
float fMed = 0.0f;

double sumFreq = 0.0;
uint32_t totalSamples = 0;

// =====================================================
// RESET DE ESTADÍSTICAS
// =====================================================
void resetStats() {
  portENTER_CRITICAL(&mux);

  queueHead = 0;
  queueTail = 0;
  queueCount = 0;
  firstEdge = true;
  lastTimeUs = 0;
  lostSamples = 0;

  portEXIT_CRITICAL(&mux);

  fMin = 0.0f;
  fMax = 0.0f;
  fMed = 0.0f;
  sumFreq = 0.0;
  totalSamples = 0;
}

// =====================================================
// ISR DE LA SEÑAL
// Se ejecuta en cada flanco detectado
// =====================================================
void IRAM_ATTR onSignalInterrupt() {
  uint64_t nowUs = timerRead(timer);   // 1 tick = 1 us

  if (firstEdge) {
    lastTimeUs = nowUs;
    firstEdge = false;
    return;
  }

  uint32_t dt = (uint32_t)(nowUs - lastTimeUs);
  lastTimeUs = nowUs;

  // Evitar valores inválidos
  if (dt == 0) return;

  portENTER_CRITICAL_ISR(&mux);

  if (queueCount < QUEUE_SIZE) {
    periodQueue[queueHead] = dt;
    queueHead = (queueHead + 1) % QUEUE_SIZE;
    queueCount++;
  } else {
    lostSamples++;
  }

  portEXIT_CRITICAL_ISR(&mux);
}

// =====================================================
// PROCESAR LA COLA
// Convierte periodos en frecuencias y actualiza Fmin/Fmed/Fmax
// =====================================================
void processQueue() {
  while (true) {
    uint32_t dt = 0;

    portENTER_CRITICAL(&mux);

    if (queueCount == 0) {
      portEXIT_CRITICAL(&mux);
      break;
    }

    dt = periodQueue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    queueCount--;

    portEXIT_CRITICAL(&mux);

    if (dt > 0) {
      float freq = 1000000.0f / (float)dt;   // dt en microsegundos

      if (totalSamples == 0) {
        fMin = freq;
        fMax = freq;
      } else {
        if (freq < fMin) fMin = freq;
        if (freq > fMax) fMax = freq;
      }

      sumFreq += freq;
      totalSamples++;
      fMed = (float)(sumFreq / totalSamples);
    }
  }
}

// =====================================================
// HTML
// =====================================================
String htmlPage() {
  String page;
  page += "<!DOCTYPE html><html lang='es'><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "<title>Frecuencimetro ESP32</title>";
  page += "<style>";
  page += "body{font-family:Arial,sans-serif;background:#f2f2f2;text-align:center;padding:25px;}";
  page += ".card{max-width:600px;margin:auto;background:#fff;padding:24px;border-radius:16px;box-shadow:0 2px 12px rgba(0,0,0,0.15);}";
  page += "h1{margin-bottom:6px;}";
  page += ".sub{color:#666;margin-bottom:20px;}";
  page += ".label{font-size:1rem;color:#555;margin-top:16px;}";
  page += ".value{font-size:2rem;font-weight:bold;color:#0a7d5a;}";
  page += ".small{margin-top:14px;color:#666;font-size:0.95rem;}";
  page += "button{margin-top:18px;padding:10px 18px;font-size:1rem;border:none;border-radius:10px;cursor:pointer;background:#0a7d5a;color:white;}";
  page += "button:hover{opacity:0.9;}";
  page += "</style>";
  page += "</head><body>";

  page += "<div class='card'>";
  page += "<h1>Frecuencimetro ESP32</h1>";
  page += "<div class='sub'>Medida por interrupciones + timer hardware + cola circular</div>";

  page += "<div class='label'>Fmin</div>";
  page += "<div class='value' id='fmin'>0.00 Hz</div>";

  page += "<div class='label'>Fmed</div>";
  page += "<div class='value' id='fmed'>0.00 Hz</div>";

  page += "<div class='label'>Fmax</div>";
  page += "<div class='value' id='fmax'>0.00 Hz</div>";

  page += "<div class='small'>Muestras procesadas: <span id='samples'>0</span></div>";
  page += "<div class='small'>Muestras perdidas: <span id='lost'>0</span></div>";

  page += "<button onclick='resetData()'>Reset estadísticas</button>";
  page += "</div>";

  page += "<script>";
  page += "async function updateData(){";
  page += "  const response = await fetch('/data');";
  page += "  const data = await response.json();";
  page += "  document.getElementById('fmin').textContent = data.fmin.toFixed(2) + ' Hz';";
  page += "  document.getElementById('fmed').textContent = data.fmed.toFixed(2) + ' Hz';";
  page += "  document.getElementById('fmax').textContent = data.fmax.toFixed(2) + ' Hz';";
  page += "  document.getElementById('samples').textContent = data.samples;";
  page += "  document.getElementById('lost').textContent = data.lost;";
  page += "}";
  page += "async function resetData(){";
  page += "  await fetch('/reset');";
  page += "  updateData();";
  page += "}";
  page += "setInterval(updateData, " + String(WEB_REFRESH_MS) + ");";
  page += "updateData();";
  page += "</script>";

  page += "</body></html>";
  return page;
}

// =====================================================
// HANDLERS WEB
// =====================================================
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

void handleData() {
  String json = "{";
  json += "\"fmin\":" + String(fMin, 2) + ",";
  json += "\"fmed\":" + String(fMed, 2) + ",";
  json += "\"fmax\":" + String(fMax, 2) + ",";
  json += "\"samples\":" + String(totalSamples) + ",";
  json += "\"lost\":" + String(lostSamples);
  json += "}";

  server.send(200, "application/json", json);
}

void handleReset() {
  resetStats();
  server.send(200, "text/plain", "OK");
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Timer hardware:
  // 80 MHz / 80 = 1 MHz -> 1 tick = 1 microsegundo
  timer = timerBegin(0, 80, true);

  // Entrada de señal
  pinMode(SIGNAL_PIN, INPUT_PULLUP);

  // Interrupción por flanco de subida
  attachInterrupt(SIGNAL_PIN, onSignalInterrupt, RISING);

  // WiFi modo Access Point
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.println();
  Serial.println("Frecuencimetro iniciado");
  Serial.print("Red WiFi: ");
  Serial.println(AP_SSID);
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Abre http://192.168.4.1");

  // Servidor web
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/reset", handleReset);
  server.begin();

  Serial.println("Servidor web iniciado");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  processQueue();
  server.handleClient();
}