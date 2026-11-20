#ifndef ESP_NOW_PROTOCOL_H
#define ESP_NOW_PROTOCOL_H

#include <stdint.h>

/**
 * @file esp_now_protocol.h
 * @brief Definición común del protocolo de telemetría ESP-NOW entre
 *        Nodo1_CocheRC (emisor en chasis RC) y Nodo2_BancoDCC (receptor banco VW DCC).
 *
 * REGLA ESTRICTA: Ambos nodos DEBEN incluir este archivo sin modificar localmente
 * la estructura para garantizar compatibilidad binaria exacta.
 */

// Modos de operación del sistema de suspensión semiactiva VW DCC
enum SuspensionMode : uint8_t {
    MODE_AUTO_REACTIVE  = 0, // Control automático reactivo a fuerzas G (ax, ay) y mandos
    MODE_MANUAL_COMFORT = 1, // Modo manual suave (~20% PWM)
    MODE_MANUAL_NORMAL  = 2, // Modo manual medio (~50% PWM)
    MODE_MANUAL_SPORT   = 3  // Modo manual firme (~80% PWM)
};

/**
 * @brief Trama de telemetría enviada por ESP-NOW desde Nodo 1 hacia Nodo 2.
 *        Empaquetada a nivel de byte para evitar diferencias de padding del compilador.
 */
typedef struct __attribute__((packed)) {
    float    ax;          // Aceleración longitudinal filtrada [g] (+ aceleración, - frenada)
    float    ay;          // Aceleración lateral filtrada [g] (+ curva izq/der)
    int16_t  throttle;    // Consigna de tracción ESC en microsegundos [1000..2000 us, neutro 1500]
    int16_t  steering;    // Consigna de dirección [-255..255] (<0 izquierda, 0 centro, >0 derecha)
    uint8_t  target_pwm;  // Duty cycle PWM objetivo para válvula VW DCC [0..255]
    uint8_t  mode;        // Modo activo (ver enum SuspensionMode)
    uint32_t timestamp;   // Marca de tiempo del Nodo 1 en milisegundos (millis())
} TelemetryPacket;

// Alias compatible con la nomenclatura estándar de ejemplos ESP-NOW (struct_message)
typedef TelemetryPacket struct_message;

#endif // ESP_NOW_PROTOCOL_H
