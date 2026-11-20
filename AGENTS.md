# AGENTS.md — Arquitectura y Contexto Maestro del Proyecto

## 1. Descripción General del Sistema

Este repositorio contiene el firmware y las especificaciones de arquitectura para un banco de pruebas de **suspensión semiactiva VW DCC (Dynamic Chassis Control)** acoplado en tiempo real a la dinámica de un **coche RC**, controlado de forma distribuida mediante dos microcontroladores **ESP32** comunicados inalámbricamente a baja latencia por el protocolo **ESP-NOW**.

---

## 2. Arquitectura Distribuida (Dos Nodos)

### Nodo 1 (`Nodo1_CocheRC`) — Unidad Embarcada en el Vehículo RC
- **Microcontrolador:** ESP32-WROOM-32 (DevKit V1).
- **Alimentación y Distribución de Potencia:**
  - Batería principal del chasis: **9.6 V NiMH / LiFe**.
  - El ESP32 se alimenta **exclusivamente desde la salida BEC de 5 V** integrada en el ESC LRP AI Runner Reverse V2.09 (conectada al pin `VIN` / `5V` del ESP32).
  - ⚠️ **REGLA CRÍTICA DE SEGURIDAD ELÉCTRICA:** **NUNCA** conectar los 9.6 V de la batería principal directamente al ESP32 ni a periféricos de lógica de 3.3 V / 5 V. Todas las masas (`GND`) del ESC, puente H L298N y ESP32 deben estar unidas en estrella.
- **Subsistema de Tracción Trasera:**
  - Controlado mediante el variador electrónico **ESC LRP AI Runner Reverse V2.09**.
  - Señal PWM estándar de servomotor a **50 Hz** (período de 20 ms):
    - `1000 µs`: Freno / Marcha atrás máxima.
    - `1500 µs`: Punto neutro (reposo / calibración inicial obligatoria en `setup()`).
    - `2000 µs`: Tracción delantera máxima.
- **Subsistema de Dirección Delantera:**
  - Actuador original del chasis basado en **motor DC de 2 cables** (sin potenciómetro de realimentación integrado).
  - Etapa de potencia: **Puente H L298N** (`IN1`, `IN2` para sentido de giro y `ENA` con modulación PWM para control de par/velocidad y protección contra bloqueo en topes mecánicos).
- **Subsistema de Adquisición Inercial (IMU):**
  - Sensor **GY-521 (MPU-6050)** conectado por bus **I2C** alimentado a **3.3 V**.
  - Lectura de aceleraciones longitudinal ($a_x$), lateral ($a_y$), vertical ($a_z$) y ángulos de inclinación (cabeceo/pitch y balanceo/roll).

---

### Nodo 2 (`Nodo2_BancoDCC`) — Banco Exterior de Suspensión Semiactiva VW DCC
- **Microcontrolador:** ESP32-WROOM-32 (DevKit V1).
- **Comunicación Inalámbrica:**
  - Recepción de paquetes de telemetría (`TelemetryPacket` / `struct_message`) vía **ESP-NOW** desde el Nodo 1.
- **Etapa de Potencia para Amortiguador Real VW DCC:**
  - Actuador: Electro-válvula proporcional interna de un amortiguador **VW DCC de 2 pines** alimentada desde fuente externa de **12 V DC nominales** (corriente de trabajo ~0.2 A – 1.8 A).
  - Conmutación low-side mediante **MOSFET Canal N de nivel lógico IRLZ44N**:
    - **Resistencia de puerta (Gate):** `220 Ω` entre el GPIO del ESP32 y el terminal Gate del IRLZ44N para limitar picos de corriente de carga capacitiva.
    - **Resistencia Pull-Down:** `10 kΩ` entre Gate y Source (`GND`) para asegurar corte seguro durante el arranque o reinicio del ESP32.
    - **Protección Inductiva:** **Diodo Flyback** rápido / Schottky (p. ej. 1N5822 o UF4007) en antiparalelo con los 2 pines del amortiguador VW DCC (cátodo a +12 V, ánodo al Drain del MOSFET) para disipar la fuerza contraelectromotriz de la bobina.
    - **Masa común (`GND`):** El terminal negativo de la fuente de 12 V, el Source del IRLZ44N y el pin `GND` del ESP32 **deben compartir masa común**.
  - Modulación PWM mediante periférico hardware **LEDC del ESP32** a frecuencia portadora de **100 Hz a 250 Hz** (recomendado `150 Hz`, resolución de 8 o 10 bits).
- **Interfaz Gráfica (HMI):**
  - **Pantalla 2.8" TFT LCD Shield** (controlador ILI9341, bus SPI con lógica de **3.3 V en líneas de datos**).
  - Visualización en tiempo real de:
    1. Duty cycle PWM aplicado a la válvula DCC (`0–100 %` / `0–255`).
    2. Modo de control activo (`MANUAL: Comfort / Normal / Sport` o `AUTO: Reactivo`).
    3. Diagrama polar / diana de fuerzas G ($a_x, a_y$).

---

## 3. Tabla de Pinout Oficial

> **[!IMPORTANT] AVISO PARA EL EQUIPO:**
> Esta tabla es la **ÚNICA FUENTE DE VERDAD** para el conexionado hardware. **Actualizad esta tabla inmediatamente tras cualquier cambio físico en la protoboard o cableado**, y sincronizad las constantes `PIN_*` en los archivos `.ino`.

### 3.1 Pinout Nodo 1 (`Nodo1_CocheRC`)

| Subsistema | Componente / Señal | Pin ESP32 (GPIO) | Nivel Lógico / Tensión | Notas Técnicas |
| :--- | :--- | :---: | :---: | :--- |
| **Alimentación** | Salida BEC ESC (+5V) | `VIN` (5V) | 5.0 V DC | **¡Prohibido conectar 9.6 V directos!** |
| **Alimentación** | Masa Común | `GND` | 0 V | Unir GND de ESC, L298N, IMU y ESP32 |
| **IMU (GY-521)** | Alimentación Sensor (`VCC`) | `3V3` | 3.3 V DC | Bus I2C a 3.3 V |
| **IMU (GY-521)** | Datos I2C (`SDA`) | `GPIO 21` | 3.3 V | I2C Estándar ESP32 |
| **IMU (GY-521)** | Reloj I2C (`SCL`) | `GPIO 22` | 3.3 V | I2C Estándar ESP32 |
| **Tracción (ESC)** | Señal PWM ESC LRP | `GPIO 18` | 3.3 V (50 Hz) | Pulso 1000–2000 µs (Neutro: 1500 µs) |
| **Dirección (L298N)**| Habilitación / PWM (`ENA`) | `GPIO 25` | 3.3 V (PWM) | Limitar duty cycle para proteger topes |
| **Dirección (L298N)**| Sentido Giro Izquierda (`IN1`)| `GPIO 26` | 3.3 V | Control dirección puente H |
| **Dirección (L298N)**| Sentido Giro Derecha (`IN2`) | `GPIO 27` | 3.3 V | Control dirección puente H |

### 3.2 Pinout Nodo 2 (`Nodo2_BancoDCC`)

| Subsistema | Componente / Señal | Pin ESP32 (GPIO) | Nivel Lógico / Tensión | Notas Técnicas |
| :--- | :--- | :---: | :---: | :--- |
| **Potencia DCC** | Gate MOSFET `IRLZ44N` | `GPIO 32` | 3.3 V (LEDC PWM) | Vía R=220 Ω + Pull-down 10 kΩ a GND |
| **Potencia DCC** | Masa de Potencia / Lógica | `GND` | 0 V | Masa común Fuente 12 V + Source + ESP32 |
| **Pantalla TFT 2.8"**| SPI MOSI (`SDI`) | `GPIO 23` | 3.3 V | Bus SPI Hardware VSPI |
| **Pantalla TFT 2.8"**| SPI MISO (`SDO`) | `GPIO 19` | 3.3 V | Bus SPI Hardware VSPI |
| **Pantalla TFT 2.8"**| SPI Clock (`SCK`) | `GPIO 18` | 3.3 V | Bus SPI Hardware VSPI |
| **Pantalla TFT 2.8"**| TFT Chip Select (`CS`) | `GPIO 15` | 3.3 V | Selección de dispositivo TFT |
| **Pantalla TFT 2.8"**| TFT Data/Command (`DC`) | `GPIO 2` | 3.3 V | Control Comando/Dato |
| **Pantalla TFT 2.8"**| TFT Reset (`RST`) | `GPIO 4` | 3.3 V | Reset por hardware |

---

## 4. Reglas Estrictas de Desarrollo (Directivas para Agentes y Desarrolladores)

1. **Sincronización Estricta del Protocolo ESP-NOW (`TelemetryPacket` / `struct_message`):**
   - La estructura de datos intercambiada por ESP-NOW está definida en `shared/esp_now_protocol.h`.
   - **Prohibido duplicar o redefinir la estructura localmente** en `Nodo1_CocheRC.ino` o `Nodo2_BancoDCC.ino` con campos distintos. Cualquier cambio en la trama debe realizarse en `shared/esp_now_protocol.h` garantizando alineación de memoria (`__attribute__((packed))`) e idéntico tamaño `sizeof(TelemetryPacket)` en ambos nodos.
2. **Filtrado Digital Obligatorio en la IMU GY-521 (MPU-6050):**
   - Las vibraciones mecánicas del chasis RC introducen ruido de alta frecuencia en el acelerómetro.
   - **Nunca enviar ni utilizar valores crudos directos del registro I2C** para calcular el PWM del amortiguador. Se debe aplicar siempre un **filtro paso bajo exponencial (EMA)** o **media móvil** sobre $a_x$ y $a_y$ antes de empaquetar la telemetría.
3. **Protección del Motor DC de Dirección contra Bloqueo Mecánico:**
   - Al carecer de final de carrera en la cremallera de dirección delantera, el puente H L298N nunca debe mantener un PWM del 100 % de forma indefinida cuando la dirección alcanza el tope físico.
   - Implementar siempre:
     - **Tiempo máximo de actuación continua (timeout de dirección)** tras el cual se reduce el PWM a un nivel de mantenimiento bajo o se corta (`ENA = 0`).
     - **Límite máximo de duty cycle** en `ENA` para evitar sobrecorrientes que dañen el bobinado del motor DC o sobrecalienten el L298N.
