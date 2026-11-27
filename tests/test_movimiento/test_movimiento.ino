/*
 * Prueba rápida y sencilla de movimiento (Ruedas en el aire)
 * Pines oficiales según AGENTS.md:
 *   - ESC (Tracción): GPIO 18
 *   - L298N (Dirección): ENA = 25, IN1 = 26, IN2 = 27
 */

// 1. Pines con #define (fácil de leer y cambiar)
#define PIN_ESC       18
#define PIN_STEER_ENA 25
#define PIN_STEER_IN1 26
#define PIN_STEER_IN2 27

// 2. Velocidades del coche (en microsegundos)
#define NEUTRO        1500  // Coche quieto
#define ADELANTE      1620  // Avance suave
#define ATRAS         1380  // Marcha atrás suave

// 3. Fuerza y tiempo de giro (protegido para no forzar topes)
#define FUERZA_GIRO   160   // De 0 a 255
#define TIEMPO_GIRO   350   // Milisegundos

// Envía pulsos de 50 Hz al ESC durante "segundos * 50" veces
void moverTraccion(int pulso_us, int repeticiones) {
  for (int i = 0; i < repeticiones; i++) {
    digitalWrite(PIN_ESC, HIGH);
    delayMicroseconds(pulso_us);
    digitalWrite(PIN_ESC, LOW);
    delay(18); // Pausa de ~20 ms entre pulsos (50 Hz)
  }
}

void pararDireccion() {
  analogWrite(PIN_STEER_ENA, 0);
  digitalWrite(PIN_STEER_IN1, LOW);
  digitalWrite(PIN_STEER_IN2, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== INICIANDO PRUEBA DEL COCHE ===");

  pinMode(PIN_ESC, OUTPUT);
  pinMode(PIN_STEER_ENA, OUTPUT);
  pinMode(PIN_STEER_IN1, OUTPUT);
  pinMode(PIN_STEER_IN2, OUTPUT);
  pararDireccion();

  // PASO 1: Armar el ESC (2.5 segundos en neutro = 125 pulsos)
  Serial.println("1. Armando ESC (neutro)...");
  moverTraccion(NEUTRO, 125);

  // PASO 2: Girar a la izquierda
  Serial.println("2. Girando IZQUIERDA...");
  digitalWrite(PIN_STEER_IN1, HIGH);
  digitalWrite(PIN_STEER_IN2, LOW);
  analogWrite(PIN_STEER_ENA, FUERZA_GIRO);
  delay(TIEMPO_GIRO);
  pararDireccion();
  delay(500);

  // PASO 3: Girar a la derecha
  Serial.println("3. Girando DERECHA...");
  digitalWrite(PIN_STEER_IN1, LOW);
  digitalWrite(PIN_STEER_IN2, HIGH);
  analogWrite(PIN_STEER_ENA, FUERZA_GIRO);
  delay(TIEMPO_GIRO);
  pararDireccion();
  delay(500);

  // PASO 4: Avanzar 1 segundo (50 pulsos) y volver a neutro
  Serial.println("4. Avanzando...");
  moverTraccion(ADELANTE, 50);
  moverTraccion(NEUTRO, 50);

  // PASO 5: Marcha atrás 1 segundo (50 pulsos) y volver a neutro
  Serial.println("5. Marcha atras...");
  moverTraccion(ATRAS, 50);
  moverTraccion(NEUTRO, 50);

  Serial.println("=== PRUEBA TERMINADA ===");
}

void loop() {
  // No hace nada más. Pulsa el botón RST del ESP32 para repetir.
}
