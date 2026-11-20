---
name: rc-cdc-suspension-lab
description: Especificaciones eléctricas, diseño de etapa de potencia para amortiguador VW DCC de 12V, estrategia de control semiactivo y guía de prevención de fallos para el proyecto de coche RC con suspensión semiactiva ESP32 + ESP-NOW.
---

# Skill Técnica: Laboratorio de Suspensión Semiactiva RC + VW DCC (`rc-cdc-suspension-lab`)

Utiliza esta skill siempre que diseñes, revises o modifiques código de control, asignación de pines, adquisición IMU o modulación PWM para el sistema de dos nodos ESP32 (`Nodo1_CocheRC` y `Nodo2_BancoDCC`).

---

## 1. Especificaciones Eléctricas por Componente

### 1.1 Subsistema Embarcado (`Nodo1_CocheRC`)
- **Batería Principal del Vehículo:** `9.6 V` nominales (pack NiMH / LiFe de alta descarga).
  - Alimenta únicamente la entrada de potencia del variador ESC LRP y el borne `VMS` (alimentación de motores) del puente H L298N.
- **Variador ESC LRP AI Runner Reverse V2.09:**
  - **BEC Integrado:** Salida regulada de `5.0 V DC` en el cable rojo del conector receptor JR/Futaba. Alimenta el pin `VIN` (5V) del ESP32 del Nodo 1.
  - **Señal de Control:** PWM de `50 Hz` (`20 ms` de período).
    - `1000 µs`: Freno / Reversa máxima.
    - `1500 µs`: Neutro (el ESC requiere recibir `1500 µs` estables durante al menos 1.5–2 segundos al encender para armar la etapa de tracción).
    - `2000 µs`: Avance máximo.
- **Puente H L298N (Dirección Delantera DC de 2 hilos):**
  - Tensión lógica: `5 V` (entradas `IN1`, `IN2`, `ENA` compatibles con salida TTL `3.3 V` del ESP32).
  - Caída de tensión interna (~1.8 V – 2.5 V en sus transistores BJT), lo cual ayuda a amortiguar la tensión efectiva sobre el motor DC de dirección.
  - Requiere limitación por PWM en `ENA` y temporizador de corte (`steering timeout`) para evitar quemar el motor en los topes mecánicos.
- **IMU GY-521 (MPU-6050):**
  - Alimentación: `3.3 V` desde el regulador del ESP32.
  - Interfaz: Bus I2C (`SDA = GPIO 21`, `SCL = GPIO 22`) a `400 kHz` (Fast Mode).
  - Escala recomendada del acelerómetro: `±4 g` o `±8 g` para evitar saturación ante impactos del chasis RC.

### 1.2 Subsistema de Banco de Suspensión (`Nodo2_BancoDCC`)
- **Amortiguador Real VW DCC (Dynamic Chassis Control - Monroe / Tenneco CES):**
  - Conector de 2 pines (bobina solenoide proporcional interna, sin polaridad estricta en la bobina, pero **con polaridad estricta respecto al diodo flyback externo**).
  - Tensión nominal de alimentación: `12 V DC` (fuente externa capaz de suministrar al menos `3 A` continuos).
  - Resistencia típica de bobina en frío: `~5 Ω – 7 Ω`.
  - Rango de corriente operativa:
    - `0.0 A` (Válvula en estado base / modo seguro por defecto según diseño hidráulico interno).
    - `0.25 A – 0.40 A`: Amortiguación suave (**Comfort**).
    - `0.70 A – 1.00 A`: Amortiguación media (**Normal**).
    - `1.40 A – 1.80 A`: Amortiguación firme (**Sport / Hard**).
- **Pantalla 2.8" TFT LCD Shield (ILI9341):**
  - Alimentación lógica y bus SPI estrictamente a `3.3 V`.

---

## 2. Precauciones de Aislamiento y Protección Flyback en la Bobina DCC (12 V)

La electro-válvula del amortiguador VW DCC es una **carga altamente inductiva**. Al conmutar mediante PWM entre `100 Hz` y `250 Hz`, cada flanco de apagado del MOSFET (`TON -> TOFF`) genera un pico de tensión inversa ($V = -L \frac{di}{dt}$) que puede superar los `80 V – 150 V` en microsegundos.

### Esquema Eléctrico Obligatorio de la Etapa de Potencia
```text
Fuente +12V DC ──────┬───────────────────────┐
                     │                       │
                [Bobina DCC]            [Diodo Flyback]
                 (2 pines)              (Cátodo a +12V,
                     │                   Ánodo a Drain)
                     ├───────────────────────┘
                     │
                  (Drain)
GPIO 32 ──[220 Ω]──┬─(Gate)  IRLZ44N (Logic-Level N-MOSFET)
                   │  (Source)
          [10 kΩ]  │     │
                   ┴─────┴───────────────────┴─── GND Común (ESP32 + Fuente 12V)
```

1. **Diodo Flyback Obligatorio:**
   - Colocar un diodo rectificador rápido o Schottky de al menos `3 A / 40 V` (ej. **1N5822**, **SB360** o **UF4007**) lo más cerca posible de los bornes de la bobina DCC.
   - **Cátodo (franja)** conectado a `+12 V`; **Ánodo** conectado al `Drain` del MOSFET IRLZ44N.
   - Sin este diodo, el pico inductivo perforará la unión Drain-Source del IRLZ44N y puede acoplarse por puerta destruyendo el `GPIO 32` del ESP32.
2. **Transistor MOSFET de Nivel Lógico (`IRLZ44N`):**
   - **No usar IRF540N ni IRFZ44N estándar** (requieren $V_{GS} = 10\text{ V}$ para saturar y con los `3.3 V` del ESP32 trabajarían en zona lineal disipando calor extremo). Usar siempre **IRLZ44N** (serie `L` = Logic Level, baja $R_{DS(on)}$ a $V_{GS} \ge 3.3\text{ V}$) o incluir un driver de puerta (`TC4427` / transistor NPN previo).
3. **Red de Puerta (`220 Ω` + `10 kΩ`):**
   - La resistencia serie de `220 Ω` amortigua oscilaciones parásitas (ringing) y protege la etapa de salida del GPIO del ESP32.
   - La resistencia pull-down de `10 kΩ` entre Gate y GND evita que la puerta quede flotante mientras el ESP32 está en reset o booteando.

---

## 3. Estrategia de Control Semiactivo

El Nodo 2 soporta dos filosofías de operación seleccionables o comandadas desde la trama ESP-NOW (`mode`):

### 3.1 Modo Manual (`MODE_MANUAL_*`)
Fija un ciclo de trabajo (duty cycle PWM de 8 bits: `0–255`) constante para caracterizar el comportamiento hidráulico del amortiguador en el banco:
- **`MODE_MANUAL_COMFORT` (1):** Duty cycle bajo (~`20 %` -> `PWM ≈ 51/255`). Mínima restricción hidráulica, prioriza absorción de baches.
- **`MODE_MANUAL_NORMAL` (2):** Duty cycle intermedio (~`50 %` -> `PWM ≈ 128/255`). Compromiso equilibrado entre confort y control de carrocería.
- **`MODE_MANUAL_SPORT` (3):** Duty cycle alto (~`80 %` -> `PWM ≈ 204/255`). Máxima firmeza, válvula cerrada para minimizar cabeceo y balanceo.

### 3.2 Modo Automático Reactivo (`MODE_AUTO_REACTIVE` = 0)
Modula dinámicamente el PWM entre un umbral base (`PWM_BASE_COMFORT ≈ 45`) y un máximo seguro (`PWM_MAX_SAFE ≈ 220`) en función de la **transferencia de pesos longitudinal y lateral** medida por la IMU del coche RC:

1. **Filtrado de Señal (en Nodo 1):**
   $$a_{x,\text{filt}}[k] = \alpha \cdot a_{x,\text{raw}}[k] + (1 - \alpha) \cdot a_{x,\text{filt}}[k-1] \quad (\alpha \in [0.15, 0.30])$$
   $$a_{y,\text{filt}}[k] = \alpha \cdot a_{y,\text{raw}}[k] + (1 - \alpha) \cdot a_{y,\text{filt}}[k-1]$$

2. **Reacción ante Frenada / Aceleración Longitudinal ($|a_x|$ — Anti-Dive / Anti-Squat):**
   - Durante una frenada fuerte o aceleración brusca, $|a_x|$ supera la zona muerta (`DEADBAND_G ≈ 0.12 g`).
   - Incremento proporcional de rigidez: $\Delta \text{PWM}_{\text{long}} = K_x \cdot \max(0, |a_x| - a_{\text{deadband}})$.
   - Anticipación feed-forward opcional: Si se detecta un cambio brusco en la consigna de `throttle` (paso rápido a freno), elevar preventivamente el duty cycle antes de que el chasis termine de cabecear.

3. **Reacción ante Paso por Curva ($|a_y|$ — Anti-Roll):**
   - En curva, la aceleración centrífuga lateral $|a_y|$ induce balanceo de carrocería.
   - Incremento proporcional de rigidez: $\Delta \text{PWM}_{\text{lat}} = K_y \cdot \max(0, |a_y| - a_{\text{deadband}})$.

4. **Ley de Control Combinada y Saturación:**
   $$\text{PWM}_{\text{target}} = \text{clamp}\left(\text{PWM}_{\text{base}} + \sqrt{(\Delta \text{PWM}_{\text{long}})^2 + (\Delta \text{PWM}_{\text{lat}})^2},\ \text{PWM}_{\text{min}},\ \text{PWM}_{\text{max}}\right)$$
   - **Failsafe por Pérdida de Enlace ESP-NOW:** Si el Nodo 2 no recibe paquetes válidos durante más de `500 ms` (`ESP_NOW_TIMEOUT_MS`), el duty cycle debe reducirse automáticamente a `0` para evitar sobrecalentar la bobina del amortiguador en estático.

---

## 4. Errores Comunes a Evitar

1. **Conectar los 9.6 V de la batería al ESP32:** Destruye instantáneamente el regulador LDO AMS1117 de la placa ESP32. Usar únicamente los `5 V` del BEC del ESC LRP.
2. **Olvidar unir las masas (`GND`) entre la fuente de 12 V y el ESP32 del Nodo 2:** Sin referencia de `GND` común entre el Source del IRLZ44N y el ESP32, la tensión $V_{GS}$ queda flotante y el MOSFET no conmuta o entra en conducción parcial térmica.
3. **Mantener `PWM = 100 %` (12 V continuos) en el amortiguador DCC en parado:** En un vehículo real el amortiguador disipa calor con el flujo de aire y movimiento del aceite. En banco estático, limitar el duty cycle continuo máximo al `85 %` (`~216/255`) e implementar timeout de inactividad.
4. **Desincronizar `TelemetryPacket` entre ambos nodos:** Añadir o reordenar un campo en uno solo de los `.ino` corrompe toda la deserialización binaria de ESP-NOW. Siempre incluir `shared/esp_now_protocol.h`.
5. **Bloquear el `loop()` con `delay()` largos o refrescos completos de pantalla TFT:** Borrar toda la pantalla (`fillScreen()`) en cada ciclo SPI congela la recepción y el lazo de control. Redibujar únicamente las regiones de texto o el punto móvil de la diana de fuerzas G a una tasa fija (ej. `15–20 Hz` mediante `millis()`).
