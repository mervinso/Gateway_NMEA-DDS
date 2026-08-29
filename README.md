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

**Obligatorios** (bastan para el núcleo y los arneses headless):

- Linux
- CMake ≥ 3.20 y compilador con soporte **C++23**
- **Fast DDS 3** (fastdds, fastcdr)

**Opcionales**, cada uno detrás de su propia opción de construcción:

| Dependencia | Opción | Habilita |
|---|---|---|
| **Qt 6** (Widgets, SerialPort, Network) | `GATEWAY_BUILD_UI` | `gateway_app` y los arneses de UI |
| **ROS 2** (`rclcpp`, `sensor_msgs`) | `GATEWAY_ROS_BRIDGE` | puente ROS, `verify_gps_ros` |
| **GoogleTest** | `GATEWAY_BUILD_TESTS` | la suite de `ctest` |

## Compilación

```bash
# Con el entorno ROS 2 cargado (provee además Fast DDS):
source /opt/ros/<distro>/setup.bash

cmake -B build -S .
cmake --build build -j$(nproc)
```

Sin ROS 2, indique dónde está Fast DDS:

```bash
cmake -B build -S . -DGATEWAY_FASTDDS_ROOT=/ruta/al/prefijo
```

### Opciones de construcción

Cada componente opcional tiene tres estados:

| Valor | Comportamiento |
|---|---|
| `AUTO` (por defecto) | se construye si sus dependencias están presentes; si no, se omite con un aviso |
| `ON` | se exige; la configuración **falla** si faltan dependencias |
| `OFF` | se excluye aunque las dependencias existan |

| Opción | Por defecto |
|---|---|
| `GATEWAY_ROS_BRIDGE` | `AUTO` |
| `GATEWAY_BUILD_UI` | `AUTO` |
| `GATEWAY_BUILD_TESTS` | `AUTO` |
| `GATEWAY_FASTDDS_ROOT` | vacío (se busca por `CMAKE_PREFIX_PATH`) |
| `CMAKE_BUILD_TYPE` | `Release` si no se especifica |

Construcción mínima, sólo núcleo y arneses headless:

```bash
cmake -B build -S . -DGATEWAY_FASTDDS_ROOT=/ruta/al/prefijo \
      -DGATEWAY_ROS_BRIDGE=OFF -DGATEWAY_BUILD_UI=OFF
```

> Para corridas de **medición**, fije `ON` u `OFF` explícitamente. Con `AUTO` el
> manifiesto registra el resultado de una detección, no una decisión, y la
> configuración deja de ser reproducible entre máquinas.

### Manifiesto de construcción

Cada configuración escribe `build/gateway_build_manifest.txt` con el commit
(`git describe`), versiones de CMake, compilador, Fast DDS, ROS y Qt, el
`CMAKE_BUILD_TYPE` y los flags efectivos. Adjúntelo a los datos de cualquier
medición.

## Ejecución

```bash
./build/gateway_app
```

`gateway_app` requiere Qt 6 **y** el puente ROS; no se construye si alguno de
los dos está desactivado. Sin interfaz, use los arneses headless de `tools/`.

Flujo típico en la UI: seleccionar la **fuente** en el panel de interfaces → verificar el
**dispositivo** detectado y asignarle un `device_id` → revisar la previsualización IDL y el
perfil **QoS** recomendado → lanzar la **conversión**. Cada conversión corre en su propio
pipeline y puede editarse su QoS en vivo.

## Pruebas

```bash
ctest --test-dir build
```

La suite cubre parser, registro, captura, mapper, pipeline, plan de publicación,
recomendador de QoS y publicador ROS. `ros_publisher_test` sólo existe si el
puente ROS se construyó. Si GoogleTest no está instalado, la configuración omite
las pruebas en lugar de fallar.

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

## Licencia

Distribuido bajo la **Licencia Apache 2.0**; véase [`LICENSE`](LICENSE). Las
dependencias de terceros conservan sus propias licencias, listadas en
[`NOTICE`](NOTICE) — en particular Qt 6 bajo LGPL-3.0, que impone condiciones
sobre la redistribución del ejecutable de interfaz. El núcleo y los arneses
headless se construyen sin Qt (`-DGATEWAY_BUILD_UI=OFF`) y no quedan afectados.

## Contexto académico

Este repositorio es el artefacto de la tesis de maestría *"Diseño e
implementación de un middleware adaptador basado en DDS para la integración
interoperable de sensores legacy bajo IEC 61162-1 / NMEA 0183"*, Maestría en
Ingeniería de Sistemas de Cómputo, Universidad Tecnológica de Bolívar
(Cartagena, Colombia).

El repositorio sigue evolucionando. Los resultados publicados en la tesis se
refieren a una **etiqueta congelada**, no a `main`; cite esa etiqueta.
