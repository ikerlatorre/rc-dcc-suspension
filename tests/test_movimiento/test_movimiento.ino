/**
 * @file test_movimiento.ino
 * @brief Prueba rápida y segura de movimiento (tracción ESC + dirección L298N) para Nodo 1.
 *
 * INSTRUCCIONES DE SEGURIDAD:
 *  1. Coloca el coche con las RUEDAS EN EL AIRE (sobre un soporte).
 *  2. Alimenta el ESP32-WROOM-32 desde el BEC 5V del ESC (o por USB con GND común).
 *     NUNCA conectes los 9.6V directos al ESP32.
 *  3. La secuencia se ejecuta UNA SOLA VEZ al arrancar y luego queda en neutro/reposo.
 */

#include <Arduino.h>

// Pines oficiales según AGENTS.md
constexpr uint8_t PIN_ESC_PWM   = 18;
constexpr uint8_t PIN_STEER_ENA = 25;
constexpr uint8_t PIN_STEER_IN1 = 26;
constexpr uint8_t PIN_STEER_IN2 = 27;

// Parámetros ESC LRP (50 Hz, resolución 16 bits -> 0..65535)
constexpr uint32_t ESC_FREQ_HZ   = 50;
constexpr uint8_t  ESC_RES_BITS  = 16;
constexpr uint16_t PULSE_NEUTRAL = 1500;
constexpr uint16_t PULSE_FWD     = 1620; // Avance suave
constexpr uint16_t PULSE_REV     = 1380; // Marcha atrás suave

// Parámetros dirección L298N (protección contra topes)
constexpr uint8_t  STEER_TEST_PWM = 160; // ~62% duty cycle
constexpr uint32_t STEER_PULSE_MS = 350; // Pulso corto para no forzar topes

void setEscPulseUs(uint16_t pulseUs) {
    uint32_t duty = (static_cast<uint32_t>(pulseUs) * 65535UL) / 20000UL;
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(PIN_ESC_PWM, duty);
#else
    ledcWrite(0, duty);
#endif
}

void steerStop() {
    analogWrite(PIN_STEER_ENA, 0);
    digitalWrite(PIN_STEER_IN1, LOW);
    digitalWrite(PIN_STEER_IN2, LOW);
}

void steerLeft(uint8_t pwm) {
    digitalWrite(PIN_STEER_IN1, HIGH);
    digitalWrite(PIN_STEER_IN2, LOW);
    analogWrite(PIN_STEER_ENA, pwm);
}

void steerRight(uint8_t pwm) {
    digitalWrite(PIN_STEER_IN1, LOW);
    digitalWrite(PIN_STEER_IN2, HIGH);
    analogWrite(PIN_STEER_ENA, pwm);
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println(F("\n=== TEST RAPIDO DE MOVIMIENTO (RUEDAS EN EL AIRE) ==="));

    // Configurar dirección
    pinMode(PIN_STEER_ENA, OUTPUT);
    pinMode(PIN_STEER_IN1, OUTPUT);
    pinMode(PIN_STEER_IN2, OUTPUT);
    steerStop();

    // Configurar PWM 50 Hz para ESC
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcAttach(PIN_ESC_PWM, ESC_FREQ_HZ, ESC_RES_BITS);
#else
    ledcSetup(0, ESC_FREQ_HZ, ESC_RES_BITS);
    ledcAttachPin(PIN_ESC_PWM, 0);
#endif

    // 1. Armar ESC en neutro (obligatorio)
    Serial.println(F("[1/5] Armando ESC en neutro (1500 us) durante 2.5 s..."));
    setEscPulseUs(PULSE_NEUTRAL);
    delay(2500);

    // 2. Probar dirección izquierda
    Serial.println(F("[2/5] Direccion IZQUIERDA (350 ms)..."));
    steerLeft(STEER_TEST_PWM);
    delay(STEER_PULSE_MS);
    steerStop();
    delay(500);

    // 3. Probar dirección derecha
    Serial.println(F("[3/5] Direccion DERECHA (350 ms)..."));
    steerRight(STEER_TEST_PWM);
    delay(STEER_PULSE_MS);
    steerStop();
    delay(500);

    // 4. Probar tracción hacia adelante
    Serial.println(F("[4/5] Traccion ADELANTE suave (1620 us, 1 s)..."));
    setEscPulseUs(PULSE_FWD);
    delay(1000);
    setEscPulseUs(PULSE_NEUTRAL);
    delay(1000);

    // 5. Probar marcha atrás
    Serial.println(F("[5/5] Traccion ATRAS suave (1380 us, 1 s)..."));
    setEscPulseUs(PULSE_REV);
    delay(1000);
    setEscPulseUs(PULSE_NEUTRAL);

    Serial.println(F("=== TEST FINALIZADO (Motor en neutro, direccion parada) ==="));
}

void loop() {
    // Reposo seguro; pulsa el botón EN/RST del ESP32 para repetir el test.
    delay(1000);
}
