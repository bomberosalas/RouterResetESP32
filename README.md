# 📡 ESP32 Router Monitor & Auto-Reset V2

Este proyecto utiliza un **ESP32** para monitorear la estabilidad de la conexión a Internet y realizar un ciclo de encendido (power cycle) al router de forma automática mediante un relé en caso de falla. 

Además, incluye funciones avanzadas de gestión remota como un panel web, notificaciones por Telegram y Wake-on-LAN.

## ✨ Características Principales

* **Monitoreo Inteligente:** Chequeo programado cada 1 hora. Realiza 3 pruebas de conectividad antes de decidir un reinicio.
* **Auto-Reset Robusto:** Si detecta caída, espera 60 segundos (permitiendo cancelación manual) y apaga el router por 25 segundos.
* **Servidor NTP Local:** Sincroniza la hora mediante un servidor NTP local (compatible con GPS) para mantener logs precisos incluso sin internet.
* **Dashboard Web:** Interfaz en tiempo real para visualizar estados, logs y realizar reinicios manuales.
* **Notificaciones Telegram:** Alertas instantáneas sobre el estado del WiFi, caídas de internet y reinicios ejecutados.
* **Watchdog (WDT):** Sistema de seguridad por hardware que reinicia el ESP32 si el código se bloquea.
* **Wake-on-LAN (WOL):** Permite encender computadoras de la red local desde el panel web.

## 🛠️ Hardware Necesario

1.  **ESP32** (Cualquier variante compatible).
2.  **Módulo Relé** (Para controlar la alimentación del router).
3.  **LED de Estado** (Indicador visual de reinicio).
4.  **Fuente de poder 5V** para el ESP32.

## 🚀 Configuración e Instalación

1.  Abre el código en el IDE de Arduino o VS Code (PlatformIO).
2.  Instala las librerías necesarias (vienen por defecto en el core de ESP32).
3.  Modifica las variables de configuración en el código:
    * `ssid` y `password` de tu red.
    * `BOT_TOKEN` y `CHAT_ID` de tu Bot de Telegram.
    * `ntpServer` con la IP de tu servidor de hora.
    * `WOL_MAC` con la dirección física del dispositivo a despertar.
4.  Configura los pines `RELAY_PIN` y `LED_PIN` según tu conexión física.

## 🖥️ Panel Web
El monitor crea un servidor web en la IP fija asignada. Desde allí puedes:
* Ver el historial de los últimos 50 eventos con marca de tiempo.
* Monitorear la potencia de la señal WiFi (RSSI).
* Cancelar un reinicio automático inminente.
* Enviar un paquete "Magic Packet" (WOL).

## Diagrama de conexiones

graph TD
    subgraph "Alimentación"
        USB[Adaptador AC a USB] -- "Cable USB-C" --> ESP32[ESP32-C3 Super Mini]
    end

    subgraph "Control"
        ESP32 -- "GPIO 4 (Signal)" --> Relay[Módulo Relé]
        ESP32 -- "5V (VCC)" --> Relay
        ESP32 -- "GND" --> Relay
        ESP32 -- "Internal LED" --> LED[Pin 8 - Status]
    end

    subgraph "Carga (Router)"
        Power[Fuente 12V Router] -- "Cable (+)" --> COM[Relé: PIN COM]
        NC[Relé: PIN NC] -- "Cable (+)" --> Router[Entrada Router]
        Power -- "Cable (-)" --> Router
    end

    style ESP32 fill:#2d333b,stroke:#3081f7,color:#fff
    style Relay fill:#2d333b,stroke:#f44336,color:#fff
    style Router fill:#111,stroke:#4caf50,color:#fff
---
*Desarrollado por Carlos Salas - 2026*
