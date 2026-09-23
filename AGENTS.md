# Gateway NMEA 0183 → DDS — reglas del repositorio

> **Este archivo es el contexto canónico del repositorio y es agnóstico a la
> herramienta:** vale igual para Claude Code, Codex, Copilot, Gemini, Cursor o cualquier
> agente LLM. `CLAUDE.md` es solo una capa de compatibilidad que importa este archivo.
> Su espejo del lado de la tesis es `dds-own/AGENTS.md`; el estado vivo del proyecto es
> `dds-own/STATUS.md`; el contrato entre los dos repositorios es `THESIS_CONTRACT.md`
> (aquí).

Este repositorio **es el artefacto** de una tesis de maestría, no un proyecto paralelo.

> Maestría en Ingeniería de Sistemas de Cómputo, Universidad Tecnológica de Bolívar.
> *Design and Implementation of a DDS-Based Middleware Adapter for the Interoperable
> Integration of Legacy Sensors under IEC 61162-1 / NMEA 0183.*
> Autor: Mervin Jesús Sosa Borrero.

Bajo el paradigma **Design Science Research**, el artefacto *es* la contribución. Este
código se publica, se cita con DOI y se congela en un punto concreto que la tesis mide. No
es andamiaje descartable.

---

## Qué máquina es esta, y qué se hace aquí

El trabajo vive en **dos máquinas con roles distintos**. No son dos copias de lo mismo.

| Máquina | Repositorio | Rol | Qué se produce |
|---|---|---|---|
| Windows | `dds-own` | **diseño** — el ideal | Especificación, metodología, pre-registro, análisis, texto de la tesis. Decide *qué* hay que medir y *por qué*. |
| **Ubuntu — estás aquí** | `Gateway_NMEA-DDS` | **implementación y prueba de software** | Construye contra esa especificación, corre los tests y los arneses headless, y prepara el binario que después mide el hardware. |

**El objetivo de esta máquina no es "programar el gateway".** Es llevarlo a un estado en
que las **pruebas físicas** puedan ejecutarse: banco real, dos plataformas (x86 y ARM64),
400 corridas, 13,7 h por plataforma. Todo lo que se hace aquí se juzga contra esa meta.

Regla de propiedad, para que dos agentes trabajando en paralelo no se pisen (sea cual sea la herramienta LLM): **esta máquina no
edita `thesis/`; la de la tesis no edita este código.** Si al implementar descubres que la
especificación del Cap. 8 está equivocada, dilo — se corrige el capítulo primero.

---

## Ruta hasta las pruebas físicas

Este es el camino crítico visto desde aquí. El detalle y la justificación de cada ítem están
en `dds-own/STATUS.md` y en `thesis/ch8_design_implementation.tex`.

1. **Reproducir el build de referencia** en el host de medición y **fijar las versiones de
   dependencias**. Hoy Fast DDS es "lo que traiga la distro de ROS" y no está registrado en
   ningún lado. Es la obligación A3 y bloquea todo lo demás.
2. **Trabajo pendiente antes del freeze** — la tabla de abajo: 2a, 2b, A5, A4, 4b, y la
   decisión de topología de participantes.
3. **Compuertas 1 y 2** — equivalencia CDR entre el brazo estático y el dinámico, y piso de
   ruido de la instrumentación. Si no pasan, no se mide nada.
4. **Piloto de varianza** (18 corridas, `pilot_schedule.csv`) → sale la SD residual, que fija
   `R_main` y `R_arity`. **Es el último bloqueo por hardware del freeze**: sin él no se puede
   regenerar `schedule.csv` con el `R` final.
5. **Corridas de shakedown**, luego el **freeze**, luego la campaña completa.

Los pasos 1–3 no necesitan que la tesis decida nada más. El 4 sí necesita hardware.

---

## Antes de tocar nada: lee `THESIS_CONTRACT.md`

Dice si la pre-registración del experimento está **congelada**. Esa única palabra cambia lo
que puedes hacer:

| Estado | Regla |
|---|---|
| **No congelada** | Arregla el artefacto. Los defectos se corrigen, no se documentan como limitaciones. |
| **Congelada** | Describe el artefacto. Cualquier cambio en la ruta de publicación invalida las mediciones ya tomadas. |

La campaña cuesta **13,7 horas por plataforma**. Un cambio bienintencionado en el camino
caliente después del freeze obliga a repetirla entera, o —peor— pasa inadvertido y la tesis
reporta números de un binario que ya no existe.

---

## El repositorio de la tesis

`https://github.com/mervinso/dds-own` (privado). Clónalo si necesitas consultar el diseño.

- **`STATUS.md`** — el estado: fase actual, qué bloquea, camino crítico. Es el handoff.
- **`thesis/ch8_design_implementation.tex`** — el capítulo que *especifica* este artefacto.
  Cuando el diseño de un cambio ya está escrito ahí, **constrúyelo contra esa especificación**,
  no desde cero. Las secciones se citan abajo.
- **`docs/science-superpowers/preregistrations/`** — el experimento que mide este código.

**Dirección del flujo, y no es simétrica:**

```
diseño      tesis  →  gateway     (Cap. 8 especifica; aquí se implementa)
medición    gateway →  tesis      (los manifiestos de corrida alimentan el análisis)
```

El gateway no inventa diseño que la tesis tenga después que racionalizar. Si al implementar
encuentras que la especificación del Cap. 8 está equivocada, **dilo** — se corrige el
capítulo y luego se implementa. No al revés.

---

## Reglas no negociables

### 1. Nunca enmarcar la contribución como mejora de throughput

IEC 61162-1 fija el enlace serie en **4 800 bps (~480 caracteres/s)**. El enlace legacy es
el cuello de botella por órdenes de magnitud; el lado DDS está sobredimensionado. Un
argumento de "más rápido" es indefendible contra un techo de ingreso de 480 char/s.

Los tres ejes reales, en orden de prioridad:

1. **Escalabilidad 1→N suscriptores.** Un talker NMEA sirve a un listener; el adaptador
   sirve a N sin tocar el sensor.
2. **Mapeo semántico sentencia NMEA → IDL / XTypes.** ASCII sin tipo → tópicos DDS
   fuertemente tipados y evolucionables.
3. **Mapeo de políticas QoS** sobre un flujo serie que no ofrece ninguna garantía.

### 2. El alcance normativo es IEC 61162-1 Edición 4.0 (2010)

Las ediciones 5.0 y 6.0 se citan solo como contexto de evolución: su texto normativo es de
pago y no se posee. Es una delimitación declarada, no un descuido. Nota relacionada: IEC
61162-1 omite la construcción TAG, que vive en IEC 61162-450 (Ethernet).

### 3. El registro tiene 40 formatters

`grep -cE '^\s*\{\s*"[A-Z0-9]+"\s*,' src/registry/Registry.cpp` devuelve 40. No los cuentes
a ojo; esta cifra ya se corrigió dos veces.

### 4. Terminología

`CONTEXT.md` es el glosario del proyecto (fuente, dispositivo, `device_id`, talker ID…).
Respétalo en código, comentarios, GUI y commits. `device_id` es la `@key` de la instancia
DDS y **no** se deriva del contenido de la sentencia.

---

## Trabajo pendiente antes del freeze

Cada ítem tiene su diseño ya escrito y argumentado en la tesis. Construye contra él.

| # | Tarea | Especificación |
|---|---|---|
| 2a | **Miembros opcionales para datos ausentes.** Hoy un campo NMEA nulo se vuelve `0.0`/`0`/`'\0'` (`parse_f64("")` en `Mapper.cpp`) y `populate()` itera sobre `std::min(recibidos, declarados)`, dejando los miembros finales en su valor por defecto. Ambos confunden "no reportado" con "reportado como cero" — justo el fallo que el tipado fuerte debe impedir. Los tres miembros de cabecera siguen obligatorios; cada campo *de sensor* pasa a opcional. | tesis §8.5.3 |
| 2b | **Sacar dos costos de la región medida.** `resolve_formatter()` corre dos veces por sentencia (`Pipeline`, y otra vez dentro de `Mapper::populate`), y `ctx.reconcile(plan->active_formatters())` corre una vez por *chunk* leído en lugar de al cambiar. No amenazan la validez —los dos brazos los pagan— pero inflan el denominador de toda comparación relativa. | tesis §8.6.1 |
| A5 | **Brazo estático como backend seleccionable** (`static` \| `dynamic`): un tipo generado por `fastddsgen` detrás de la misma interfaz de publicación que usan `Mapper`/`Pipeline`. **Debe llevar `@optional`** para casar con 2a, o falla la compuerta de equivalencia CDR. | tesis Cap. 8 |
| A4 | **Instrumentación de la ruta de publicación**, in-tree, detrás de una opción de build por defecto **OFF**. Retorno del parser → retorno de `write()`, con `CLOCK_MONOTONIC_RAW`. El binario medido debe ser el binario publicado, salvo un flag. | tesis Cap. 8 |
| 4b | **Emitir un `manifest.json` conforme.** El esquema está en `dds-own/experiments/testbed/manifest.schema.json` (`additionalProperties: false`), con ejemplo válido en `manifest.example.json`. Tres campos no tienen código de recolección todavía: frecuencia de CPU muestreada durante la ventana, `discovery_frames_in_window` vía `tshark`, y la lista de transportes leída de vuelta tras crear el participante. **El arnés escribe `{"is_void": false, "criteria": []}` como marcador y nunca juzga su propia corrida.** | `dds-own/experiments/testbed/` |

**Decisión abierta, añadida 2026-08-31 — topología de participantes DDS.**

`Pipeline.cpp:178`: cada `Pipeline` crea **su propio `DomainParticipant`**, su propio
`Publisher` y su propio `Mapper`. Con M fuentes eso son **M participantes en un mismo
dominio**, cada uno con su descubrimiento SPDP/SEDP, sus recursos de transporte y sus hilos;
además la caché de `DynamicType` es por fuente, así que M fuentes con el mismo formatter
pagan M arranques en frío.

La encuesta de literatura de la tesis (Fase 2 de RQ3, 2026-08-31) encontró:

- **Ninguna fuente recomienda ni analiza un participante por fuente de datos.**
- Dalkıran et al. 2021 (*Defence Technology* 17(2):657–670, doi `10.1016/j.dt.2020.01.005`)
  hace lo contrario a propósito: **un solo `DomainParticipant`** gestiona muchos publishers,
  *explícitamente* para evitar el metatráfico SPDP redundante de uno por cliente.
- Bode et al. 2023 (ACM Middleware '23, doi `10.1145/3590140.3629118`) mide **Fast DDS
  incapaz de entregar más datos a partir de tres publicadores concurrentes** en nodos ARM de
  600 MHz, fallando por expiración del *lease* de liveliness, no por throughput.
- Alaerjan 2023 (doi `10.3390/electronics12102246`): **100 % de CPU** en un barrido de 1→30
  participantes en Raspberry Pi 4.

**Por qué importa aquí:** si es costo no intencionado, el experimento de escalabilidad (RQ3)
reportaría como propiedad *del enfoque DDS* un límite causado por *esta implementación*. Es
justo el caso que la regla "antes del freeze se arregla el artefacto" existe para evitar.

**No afecta a RQ1**, que corre una sola fuente, así que no bloquea ese freeze. Toca creación
de participante y publisher, **no la región cronometrada**, así que tampoco perturba la
instrumentación.

**Decide el autor.** Si un participante por pipeline es deliberado —aislamiento, fallo y
reinicio independientes por fuente— **escríbelo como ADR en `docs/adr/`** para que RQ3 lo
reporte como compromiso deliberado y no como accidente. Lo que no se sostiene es dejarlo sin
decidir hasta después del freeze.

**Gatillados por una afirmación, no por el freeze:**

| # | Tarea | Especificación |
|---|---|---|
| A6 | **Cargador de definiciones del registro.** El README y el ADR 0001 ya lo prometen; no existe. El esquema espeja `SentenceDef`; los builtins cargan primero y el archivo los sustituye **en bloque, nunca fusiona**; categoría o tipo desconocido, nombre de campo duplicado, formatter duplicado o lista de campos vacía **rechazan la entrada ruidosamente** — un modelo semántico que carga a medias es peor que uno que se niega. Habilita medir RQ2. | tesis §8.4.1 |
| A-QoS | **Mover `QoSRecommender` fuera de `src/ui/` a la ruta de publicación.** Su único llamador es `MainWindow.cpp:124`; un gateway headless no deriva QoS de la semántica NMEA, así que el eje 3 es hoy una función de la GUI. Derivar por defecto, el override explícito del operador gana y se registra junto al valor derivado, y la estimación de tasa debe estabilizarse dentro de una tolerancia con banda de histéresis — si no, una estimación ruidosa dispara repetidamente el camino destruir/recrear del writer, porque *reliability* y *durability* son inmutables. | tesis §8.7.1 |

**Deuda del README:** anuncia un perfil QoS `critical_reliable` que `QoSRecommender` nunca
devuelve. Decidido: el conjunto son los dos que sí retorna — bórralo del README en lugar de
dejarlo como promesa.

---

## Build

CMake. Fast DDS 3 · Fast CDR · C++23 · Linux. Qt 6 solo para la UI, que es un ejecutable
aparte; el núcleo son bibliotecas estáticas.

- **Opciones tri-estado `AUTO | ON | OFF`** para Qt 6, el puente ROS 2 y GoogleTest. `AUTO`
  omite lo ausente; `ON` falla con mensaje accionable.
- **`CMAKE_BUILD_TYPE` por defecto `Release`.** Estuvo sin fijar, con lo que GCC compilaba a
  `-O0`: cualquier medición sobre ese build no vale nada y no deja rastro visible. No lo
  vuelvas a dejar sin definir.
- Fast DDS se localiza por el mecanismo estándar de CMake (`CMAKE_PREFIX_PATH`, que el
  entorno ROS 2 ya aporta). **No** vuelvas a poner rutas absolutas tipo `/opt/ros/<distro>`.
- La variable es **`GATEWAY_FASTDDS_ROOT`**, no `FASTDDS_ROOT`: esta última choca con la
  convención reservada `<PKG>_ROOT` de CMake y `find_package` la ignora en silencio.
- Cada configuración escribe **`gateway_build_manifest.txt`** (`git describe`, versiones de
  toolchain y dependencias, opciones resueltas, tipo de build y flags efectivos). Es el
  registro de entorno por corrida que el plan de análisis exige.
- 8 suites GoogleTest en `tests/`, 8 arneses headless en `tools/`.

**Sigue abierto:** fijar las versiones de las dependencias. Hoy Fast DDS es "lo que traiga
la distro de ROS" y eso no está registrado en ninguna parte (obligación A3 de la tesis).

---

## Licencia y autoría

Apache-2.0 (`LICENSE`), con `NOTICE` registrando autoría, la atribución a la tesis y las
licencias de terceros. La LGPL-3.0 de Qt 6 afecta **solo** al ejecutable de la UI. No
introduzcas dependencias con licencias incompatibles sin decirlo.

---

## Qué no hacer

- No editar `thesis/` del repositorio de la tesis desde aquí. Cada repo tiene un dueño.
- No borrar ni reescribir `docs/adr/0001-tipos-dinamicos-xtypes.md`. Afirma que el costo de
  `DynamicData` es *"irrelevante a tasas NMEA"* sin haberlo medido, y **esa afirmación es el
  objeto del experimento de la tesis**. Es evidencia histórica: se anota, no se corrige.
- No cambiar los valores por defecto de QoS `{reliable=false, transient_local=false,
  deadline_ms=0, lifespan_ms=0}` sin avisar: son el valor mantenido constante del
  experimento.
- No optimizar la ruta caliente después del freeze. Ver `THESIS_CONTRACT.md`.
