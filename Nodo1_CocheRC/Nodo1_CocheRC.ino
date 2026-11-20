/**
 * @file Nodo1_CocheRC.ino
 * @brief Firmware del Nodo 1 (Chasis Coche RC) - Proyecto Suspensión Semiactiva VW DCC
 *
 * Funciones principales:
 *  1. Lectura y filtrado digital (EMA) de aceleraciones (ax, ay) desde IMU GY-521 (MPU-6050) por I2C.
 *  2. Control de tracción trasera mediante PWM de servo (50 Hz, 1000-2000 us, neutro 1500 us) al ESC LRP.
 *  3. Control de dirección delantera DC (2 cables) con puente H L298N y protección contra bloqueo en topes.
 *  4. Envío de telemetría en tiempo real al Nodo 2 (Banco DCC) mediante ESP-NOW.
 *
 * ADVERTENCIA ELÉCTRICA:
 *  - Alimentar el ESP32 EXCLUSIVAMENTE desde el BEC de 5V del ESC LRP AI Runner Reverse V2.09.
 *  - NUNCA conectar la batería de 9.6V directamente al ESP32.
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>

// Inclusión del protocolo compartido entre ambos nodos
#include "../shared/esp_now_protocol.h"

// ============================================================================
// PINOUT OFICIAL NODO 1 (Sincronizado con AGENTS.md)
// ============================================================================
constexpr uint8_t PIN_IMU_SDA       = 21; // I2C SDA para GY-521 (MPU-6050, lógica 3.3V)
constexpr uint8_t PIN_IMU_SCL       = 22; // I2C SCL para GY-521 (MPU-6050, lógica 3.3V)
constexpr uint8_t PIN_ESC_PWM       = 18; // Señal PWM 50 Hz para ESC LRP AI Runner Reverse V2.09
constexpr uint8_t PIN_STEER_ENA     = 25; // PWM velocidad/par dirección en puente H L298N (ENA)
constexpr uint8_t PIN_STEER_IN1     = 26; // Sentido giro izquierda en puente H L298N (IN1)
constexpr uint8_t PIN_STEER_IN2     = 27; // Sentido giro derecha en puente H L298N (IN2)

// ============================================================================
// CONSTANTES DE CONTROL Y PROTECCIÓN
// ============================================================================
constexpr int16_t  ESC_PULSE_MIN_US     = 1000;
constexpr int16_t  ESC_PULSE_NEUTRAL_US = 1500;
constexpr int16_t  ESC_PULSE_MAX_US     = 2000;

constexpr uint8_t  STEER_MAX_PWM        = 200;  // Límite de duty cycle (0-255) para proteger topes
constexpr uint32_t STEER_MAX_ON_TIME_MS = 600;  // Tiempo máximo continuo antes de reducir par en tope

constexpr float    IMU_EMA_ALPHA        = 0.20f; // Coeficiente filtro paso bajo exponencial (0.0 - 1.0)
constexpr uint32_t TELEMETRY_PERIOD_MS  = 20;    // Frecuencia de envío ESP-NOW: 50 Hz (cada 20 ms)

// Dirección MAC del Nodo 2 (Banco DCC) - Actualizar con la MAC real del ESP32 receptor
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Instancia global de la trama de telemetría sincronizada
TelemetryPacket telemetryTx;

// Variables de estado interno
float    filteredAx       = 0.0f;
float    filteredAy       = 0.0f;
uint32_t lastTelemetryMs  = 0;

// ============================================================================
// CALLBACKS ESP-NOW
// ============================================================================
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    // Callback opcional de diagnóstico de transmisión ESP-NOW
    (void)mac_addr;
    (void)status;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // 1. Inicialización del bus serie para depuración
    Serial.begin(115200);
    delay(200);
    Serial.println(F("===================================================="));
    Serial.println(F(" [Nodo 1] Inicializando Coche RC - Telemetria + ESC "));
    Serial.println(F("===================================================="));

    // 2. Configuración de pines del puente H L298N (Dirección delantera)
    pinMode(PIN_STEER_ENA, OUTPUT);
    pinMode(PIN_STEER_IN1, OUTPUT);
    pinMode(PIN_STEER_IN2, OUTPUT);
    digitalWrite(PIN_STEER_IN1, LOW);
    digitalWrite(PIN_STEER_IN2, LOW);
    analogWrite(PIN_STEER_ENA, 0); // Motor de dirección inicialmente en reposo

    // 3. Configuración inicial del ESC LRP en punto neutro obligatorio (1500 us)
    pinMode(PIN_ESC_PWM, OUTPUT);
    // TODO: Configurar canal LEDC a 50 Hz (16 bits) o librería ESP32Servo y enviar 1500 us durante 2 s

    // 4. Inicialización del bus I2C y sensor IMU GY-521 (MPU-6050)
    Wire.begin(PIN_IMU_SDA, PIN_IMU_SCL, 400000);
    // TODO: Despertar MPU-6050 (registro 0x6B = 0x00) y calibrar offsets iniciales en reposo

    // 5. Inicialización de WiFi en modo estación y protocolo ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[ERROR] Fallo al inicializar ESP-NOW en Nodo 1"));
    } else {
        esp_now_register_send_cb(onDataSent);

        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastAddress, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;

        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println(F("[ERROR] No se pudo registrar el peer Nodo 2"));
        } else {
            Serial.println(F("[OK] ESP-NOW listo para transmitir telemetria."));
        }
    }

    // Inicialización de valores por defecto de la estructura
    telemetryTx.ax         = 0.0f;
    telemetryTx.ay         = 0.0f;
    telemetryTx.throttle   = ESC_PULSE_NEUTRAL_US;
    telemetryTx.steering   = 0;
    telemetryTx.target_pwm = 50;
    telemetryTx.mode       = MODE_AUTO_REACTIVE;
    telemetryTx.timestamp  = millis();
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    const uint32_t now = millis();

    // 1. Lectura y filtrado paso bajo (EMA) de la IMU GY-521
    // TODO: Leer aceleraciones crudas ax_raw, ay_raw por I2C y aplicar:
    // filteredAx = (IMU_EMA_ALPHA * ax_raw) + ((1.0f - IMU_EMA_ALPHA) * filteredAx);
    // filteredAy = (IMU_EMA_ALPHA * ay_raw) + ((1.0f - IMU_EMA_ALPHA) * filteredAy);

    // 2. Actualización de consignas de tracción (ESC LRP) y dirección (L298N con timeout de topes)
    // TODO: Aplicar control de tracción y supervisar que la dirección no supere STEER_MAX_ON_TIME_MS

    // 3. Cálculo del PWM objetivo y envío periódico por ESP-NOW (50 Hz)
    if (now - lastTelemetryMs >= TELEMETRY_PERIOD_MS) {
        lastTelemetryMs = now;

        telemetryTx.ax        = filteredAx;
        telemetryTx.ay        = filteredAy;
        telemetryTx.timestamp = now;

        esp_now_send(broadcastAddress, reinterpret_cast<const uint8_t *>(&telemetryTx), sizeof(telemetryTx));
    }
}
