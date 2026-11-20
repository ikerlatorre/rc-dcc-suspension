/**
 * @file Nodo2_BancoDCC.ino
 * @brief Firmware del Nodo 2 (Banco Exterior Amortiguador VW DCC) - Proyecto Suspensión Semiactiva
 *
 * Funciones principales:
 *  1. Recepción de paquetes de telemetría (`TelemetryPacket`) vía ESP-NOW desde el Nodo 1.
 *  2. Modulación PWM por hardware LEDC (100-250 Hz) del amortiguador real VW DCC (12V, 2 pines)
 *     mediante etapa de potencia con MOSFET lógico IRLZ44N (R_gate=220 ohm, pull-down=10k, diodo flyback).
 *  3. Visualización en pantalla 2.8" TFT LCD Shield (lógica 3.3V) del duty cycle PWM, modo activo
 *     y diagrama de fuerzas G (ax, ay).
 *  4. Failsafe por pérdida de comunicación ESP-NOW (corte de PWM si se supera el timeout).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Inclusión del protocolo compartido entre ambos nodos
#include "../shared/esp_now_protocol.h"

// ============================================================================
// PINOUT OFICIAL NODO 2 (Sincronizado con AGENTS.md)
// ============================================================================
constexpr uint8_t PIN_DCC_MOSFET_GATE = 32; // Salida PWM LEDC hacia Gate del IRLZ44N (vía 220 ohm)

// Pines SPI para Pantalla 2.8" TFT LCD Shield (ILI9341 - Lógica 3.3V)
constexpr uint8_t PIN_TFT_MOSI        = 23; // SPI MOSI (SDI)
constexpr uint8_t PIN_TFT_MISO        = 19; // SPI MISO (SDO)
constexpr uint8_t PIN_TFT_SCK         = 18; // SPI Clock (SCK)
constexpr uint8_t PIN_TFT_CS          = 15; // TFT Chip Select
constexpr uint8_t PIN_TFT_DC          = 2;  // TFT Data / Command
constexpr uint8_t PIN_TFT_RST         = 4;  // TFT Reset

// ============================================================================
// CONFIGURACIÓN PWM LEDC PARA VÁLVULA VW DCC (100 - 250 Hz)
// ============================================================================
constexpr uint32_t DCC_PWM_FREQ_HZ     = 150; // Frecuencia nominal dentro del rango 100-250 Hz
constexpr uint8_t  DCC_PWM_RESOLUTION  = 8;   // Resolución de 8 bits (0 - 255)
constexpr uint8_t  DCC_PWM_MAX_SAFE    = 220; // Límite térmico en banco estático (~86% duty cycle)
constexpr uint32_t ESP_NOW_TIMEOUT_MS  = 500; // Failsafe: apaga bobina si no hay paquetes en 500 ms
constexpr uint32_t UI_REFRESH_PERIOD_MS = 50; // Refresco de interfaz TFT a 20 Hz

// Variables globales de recepción y estado
volatile bool   newPacketAvailable = false;
volatile uint32_t lastPacketRxMs   = 0;
TelemetryPacket telemetryRx;

uint8_t  appliedPwmDuty = 0;
uint32_t lastUiUpdateMs = 0;

// ============================================================================
// FUNCIÓN AUXILIAR: APLICAR PWM AL AMORTIGUADOR VW DCC
// ============================================================================
void setDccValvePwm(uint8_t duty) {
    if (duty > DCC_PWM_MAX_SAFE) {
        duty = DCC_PWM_MAX_SAFE;
    }
    appliedPwmDuty = duty;
    analogWrite(PIN_DCC_MOSFET_GATE, appliedPwmDuty);
}

// ============================================================================
// CALLBACK DE RECEPCIÓN ESP-NOW
// ============================================================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    (void)recv_info;
#else
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    (void)mac_addr;
#endif
    if (len == sizeof(TelemetryPacket)) {
        memcpy(&telemetryRx, incomingData, sizeof(TelemetryPacket));
        lastPacketRxMs     = millis();
        newPacketAvailable = true;
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // 1. Inicialización del bus serie a 115200 baudios
    Serial.begin(115200);
    delay(200);
    Serial.println(F("===================================================="));
    Serial.println(F(" [Nodo 2] Inicializando Banco Amortiguador VW DCC   "));
    Serial.println(F("===================================================="));

    // 2. Configuración de la etapa de potencia PWM (LEDC) para el MOSFET IRLZ44N
    pinMode(PIN_DCC_MOSFET_GATE, OUTPUT);
    analogWriteFrequency(PIN_DCC_MOSFET_GATE, DCC_PWM_FREQ_HZ);
    analogWriteResolution(PIN_DCC_MOSFET_GATE, DCC_PWM_RESOLUTION);
    setDccValvePwm(0); // Estado seguro inicial: bobina desenergizada (0% duty cycle)

    // 3. Inicialización de la pantalla 2.8" TFT LCD Shield
    pinMode(PIN_TFT_CS, OUTPUT);
    pinMode(PIN_TFT_DC, OUTPUT);
    pinMode(PIN_TFT_RST, OUTPUT);
    // TODO: Inicializar instancia de librería TFT (Adafruit_ILI9341 / TFT_eSPI) y dibujar layout base

    // 4. Inicialización de WiFi en modo estación y protocolo ESP-NOW
    WiFi.mode(WIFI_STA);
    Serial.print(F("[INFO] MAC Address Nodo 2 (Banco DCC): "));
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println(F("[ERROR] Fallo al inicializar ESP-NOW en Nodo 2"));
    } else {
        esp_now_register_recv_cb(onDataRecv);
        Serial.println(F("[OK] Receptor ESP-NOW activo y esperando telemetria."));
    }
}

// ============================================================================
// LOOP PRINCIPAL
// ============================================================================
void loop() {
    const uint32_t now = millis();

    // 1. Procesamiento de paquetes entrantes y cálculo de ley de control semiactiva
    if (newPacketAvailable) {
        newPacketAvailable = false;

        // Selección del duty cycle según el modo activo recibido o calculado
        uint8_t targetDuty = telemetryRx.target_pwm;
        switch (telemetryRx.mode) {
            case MODE_MANUAL_COMFORT:
                targetDuty = 51;  // ~20% duty cycle
                break;
            case MODE_MANUAL_NORMAL:
                targetDuty = 128; // ~50% duty cycle
                break;
            case MODE_MANUAL_SPORT:
                targetDuty = 204; // ~80% duty cycle
                break;
            case MODE_AUTO_REACTIVE:
            default:
                // TODO: Ajustar dinámicamente con ax, ay si se delega el cálculo al Nodo 2
                targetDuty = telemetryRx.target_pwm;
                break;
        }

        setDccValvePwm(targetDuty);
    }

    // 2. Supervisión de Failsafe por pérdida de enlace inalámbrico
    if ((lastPacketRxMs > 0) && (now - lastPacketRxMs > ESP_NOW_TIMEOUT_MS)) {
        if (appliedPwmDuty != 0) {
            setDccValvePwm(0);
            Serial.println(F("[WARN] Timeout ESP-NOW: Bobina VW DCC desenergizada por seguridad."));
        }
    }

    // 3. Actualización no bloqueante de la pantalla TFT 2.8" y telemetría por puerto serie
    if (now - lastUiUpdateMs >= UI_REFRESH_PERIOD_MS) {
        lastUiUpdateMs = now;
        // TODO: Actualizar barra de Duty Cycle PWM, etiqueta de Modo y punto (ax, ay) en diana G
    }
}
