/*
 * Prueba rápida y sencilla de movimiento (Ruedas en el aire)
 * Adaptada a la instalación física del Nodo 1 (ESP32-DevKitC 38 pines):
 *   - ESC LRP AI Runner Reverse V2.09 (Tracción): GPIO 18 (cable Blanco)
 *       * Rojo  -> 5V del ESP32 (SOLO sin USB conectado; desconectar si usas USB)
 *       * Negro -> GND común del ESP32
 *   - L298N (Dirección con jumper ENA puesto, solo 2 señales):
 *       * IN1 -> GPIO 26 (giro izquierda por PWM directo)
 *       * IN2 -> GPIO 27 (giro derecha por PWM directo)
 *       * Jumper ENA colocado en el L298N (no se usa GPIO 25)
 *
 * ADVERTENCIA DE SEGURIDAD:
 *   - Coloca el coche con las RUEDAS EN EL AIRE antes de encender la batería.
 *   - NUNCA conectes los 9.6 V de la batería directamente al ESP32.
 *   - Une todas las masas (GND batería 9.6 V, cable Negro ESC, GND L298N y GND ESP32).
 */

#include <Arduino.h>

// 1. Pines de nuestra instalación
#define PIN_ESC       18  // Cable BLANCO del ESC LRP (PWM hardware 50 Hz)
#define PIN_STEER_IN1 26  // Entrada IN1 del puente H L298N (Izquierda)
#define PIN_STEER_IN2 27  // Entrada IN2 del puente H L298N (Derecha)

// 2. Configuración PWM hardware (LEDC) del ESC LRP (50 Hz continuos)
#define ESC_FREQ_HZ   50
#define ESC_RES_BITS  14  // Resolución de 14 bits (0..16383)
#define ESC_LEDC_CH   6   // Canal 6 (Timer 3 en core 2.x; evita colisión con analogWrite)

// 3. Velocidades del coche (en microsegundos, rango 1000-2000 us)
#define NEUTRO        1500  // Coche quieto / armado obligatorio
#define ADELANTE      1620  // Avance suave
#define ATRAS         1380  // Marcha atrás suave

// 4. Fuerza y tiempo de giro (protegido para no forzar topes mecánicos)
#define FUERZA_GIRO   160   // De 0 a 255 (< 200 límite seguro en IN1/IN2)
#define TIEMPO_GIRO   350   // Milisegundos (< 600 ms límite máximo continuo)

// Configura el periférico LEDC para enviar pulsos continuos de 50 Hz al ESC
void configurarESC() {
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(PIN_ESC, ESC_FREQ_HZ, ESC_RES_BITS);
#else
  ledcSetup(ESC_LEDC_CH, ESC_FREQ_HZ, ESC_RES_BITS);
  ledcAttachPin(PIN_ESC, ESC_LEDC_CH);
#endif
}

// Actualiza el ancho de pulso del ESC (1000-2000 us) mantenido de forma continua
void fijarESC(int pulso_us) {
  pulso_us = constrain(pulso_us, 1000, 2000);
  uint32_t duty = (uint32_t)pulso_us * ((1UL << ESC_RES_BITS) - 1UL) / 20000UL;
#if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_ESC, duty);
#else
  ledcWrite(ESC_LEDC_CH, duty);
#endif
}

// Control de dirección por 2 pines con PWM directo (siempre analogWrite en ambos)
void pararDireccion() {
  analogWrite(PIN_STEER_IN1, 0);
  analogWrite(PIN_STEER_IN2, 0);
}

void girarIzquierda(uint8_t pwm) {
  analogWrite(PIN_STEER_IN1, pwm);
  analogWrite(PIN_STEER_IN2, 0);
}

void girarDerecha(uint8_t pwm) {
  analogWrite(PIN_STEER_IN1, 0);
  analogWrite(PIN_STEER_IN2, pwm);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== INICIANDO PRUEBA DEL COCHE (RUEDAS EN EL AIRE) ===");

  // Configurar dirección en reposo seguro
  pinMode(PIN_STEER_IN1, OUTPUT);
  pinMode(PIN_STEER_IN2, OUTPUT);
  pararDireccion();

  // Configurar PWM hardware continuo del ESC e inicializar en neutro
  configurarESC();
  fijarESC(NEUTRO);

  // PASO 1: Armar el ESC (2.5 segundos en neutro continuo a 50 Hz)
  Serial.println("1. Armando ESC en neutro (1500 us, 2.5 s)...");
  delay(2500);

  // PASO 2: Girar a la izquierda (el ESC sigue recibiendo 1500 us por hardware)
  Serial.println("2. Girando IZQUIERDA...");
  girarIzquierda(FUERZA_GIRO);
  delay(TIEMPO_GIRO);
  pararDireccion();
  delay(500);

  // PASO 3: Girar a la derecha
  Serial.println("3. Girando DERECHA...");
  girarDerecha(FUERZA_GIRO);
  delay(TIEMPO_GIRO);
  pararDireccion();
  delay(500);

  // PASO 4: Avanzar 1 segundo y volver a neutro
  Serial.println("4. Avanzando suave...");
  fijarESC(ADELANTE);
  delay(1000);
  fijarESC(NEUTRO);
  delay(1000);

  // PASO 5: Marcha atrás con doble toque suave (liberar freno -> neutro -> reversa)
  Serial.println("5. Marcha atras suave (doble toque LRP)...");
  fijarESC(ATRAS);
  delay(250);
  fijarESC(NEUTRO);
  delay(250);
  fijarESC(ATRAS);
  delay(1000);
  fijarESC(NEUTRO);

  Serial.println("=== PRUEBA TERMINADA (ESC en neutro seguro, direccion parada) ===");
}

void loop() {
  // Reposo seguro; LEDC sigue enviando 1500 us a 50 Hz al ESC.
  // Pulsa el botón EN/RST del ESP32 para repetir la prueba.
  delay(1000);
}
