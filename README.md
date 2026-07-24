# Gateway NMEA 0183 → DDS

Gateway que traduce las sentencias de sensores marinos legacy **NMEA 0183** al espacio de
datos global de **OMG DDS** (Fast DDS), preservando y configurando la calidad de servicio
(QoS) por cada flujo de datos. Incluye un puente opcional hacia **ROS 2**
(`sensor_msgs/Imu` y `sensor_msgs/NavSatFix`) y una interfaz gráfica Qt para operar las
conversiones.

## Características

- **Ingesta multi-interfaz**: puerto serie (con autodetección de baudios), TCP y UDP.
- **Parser NMEA 0183 determinista**: hot path sin excepciones ni asignaciones de heap.
- **Registro de sentencias dirigido por datos**: catálogo extensible que define por
  formatter su categoría (GPS, Heading, Inertial, Weather, …) y sus campos; amplía el
  soporte de sentencias sin recompilar.
- **Tipos dinámicos DDS (XTypes)**: los tipos DDS se construyen en tiempo de ejecución a
  partir del registro; los suscriptores los descubren sin compartir IDL
  (ver [ADR 0001](docs/adr/0001-tipos-dinamicos-xtypes.md)).
- **Tópico de respaldo `RawSentence`**: las sentencias fuera del registro se publican
  crudas; el gateway nunca descarta datos.
- **Perfiles QoS con recomendador**: propuesta automática de perfil según categoría y tasa
  medida, con edición en vivo sin reiniciar el pipeline.
- **Monitor DDS**: participante de solo-descubrimiento que inspecciona dominios y reporta
  tópicos, tipos y endpoints.
- **Puente ROS 2**: publicación de `Imu` y `NavSatFix` con conversión de unidades
  (DDMM.mmmm → grados decimales, etc.).
- **UI Qt 6 (Widgets)**: paneles de interfaces, dispositivos, previsualización IDL, QoS,
  conversiones y monitor DDS, con indicadores de recepción por trama/conversión.

## Arquitectura

```
Fuente (serie/TCP/UDP)
   │  capture/           lectura + autodetección de baudios
   ▼
Parser NMEA 0183         parser/    hot path sin excepciones
   ▼
Registro de sentencias   registry/  categoría + campos por formatter
   ▼
Mapper → DynamicData     mapper/    XTypes en tiempo de ejecución
   ▼
Pipeline                 pipeline/  hilo read→parse→map→publish, QoS por flujo
   ├──► DDS (Fast DDS)              tópicos dinámicos + RawSentence
   └──► ROS 2 (opcional) ros/       sensor_msgs/Imu, sensor_msgs/NavSatFix
UI Qt 6                  ui/        control de conversiones y monitor DDS
```

El vocabulario del dominio está en [`CONTEXT.md`](CONTEXT.md).

## Requisitos

- Linux
- CMake ≥ 3.20 y compilador con soporte **C++23**
- **Fast DDS 3** (fastdds, fastcdr)
- **Qt 6** (Widgets, SerialPort, Network)
- **ROS 2** (`rclcpp`, `sensor_msgs`) para el puente ROS

## Compilación

```bash
# Con el entorno ROS 2 cargado (provee además Fast DDS):
source /opt/ros/<distro>/setup.bash

cmake -B build -S .
cmake --build build -j$(nproc)
```

## Ejecución

```bash
./build/gateway_app
```

Flujo típico en la UI: seleccionar la **fuente** en el panel de interfaces → verificar el
**dispositivo** detectado y asignarle un `device_id` → revisar la previsualización IDL y el
perfil **QoS** recomendado → lanzar la **conversión**. Cada conversión corre en su propio
pipeline y puede editarse su QoS en vivo.

## Pruebas

```bash
ctest --test-dir build
```

La suite cubre parser, registro, captura, mapper, pipeline, plan de publicación,
recomendador de QoS y publicador ROS.

## Herramientas de validación (`tools/`)

| Herramienta | Propósito |
|---|---|
| `dds_dynamic_sub` | Suscriptor DDS independiente que descubre tipos sin IDL del gateway (interoperabilidad) |
| `verify_gps_ros` | Validación headless GPS → `NavSatFix` por la ruta de producción |
| `verify_live` | Verificación headless del pipeline contra datos en vivo |
| `pub_hold` | Publicador de larga duración con datos reales para pruebas de interop |
| `test_multi_iface`, `test_serial`, `test_qos` | Arneses headless de multi-interfaz, lectura serial y edición de QoS en vivo |
| `rehearse_demos`, `verify_stale_ui` | Ensayo de demos y verificación del indicador de recepción en la UI |

## Documentación

- [`CONTEXT.md`](CONTEXT.md) — lenguaje ubicuo del dominio
- [`docs/adr/`](docs/adr/) — registros de decisiones de arquitectura
