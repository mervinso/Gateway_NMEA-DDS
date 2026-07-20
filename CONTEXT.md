# Gateway NMEA 0183 → DDS

Gateway que traduce las sentencias de sensores marinos legacy NMEA 0183 al espacio de
datos global de OMG DDS (Fast DDS), preservando y configurando la calidad de servicio (QoS)
por cada flujo de datos.

## Language

**Fuente** (source):
El origen físico desde el que entran datos NMEA: un puerto serie (`ttyUSBx` + baud) o un
endpoint de red (`tcp://host:port`). Una fuente es exclusiva: una sola conversión la posee.
_Avoid_: canal, entrada, interfaz.
_Nota GUI_: el panel ① se rotula "Interfaces" por familiaridad para el operador; en prosa
técnica y en el resto de la documentación el término sigue siendo **fuente**.

**Dispositivo** (device):
Un sensor lógico identificado de forma única por su **device_id**, ligado a una **fuente**.
Un mismo equipo físico puede emitir varias **categorías** a la vez (p. ej. GPS + rumbo).
_Avoid_: sensor (úsese coloquialmente), equipo.

**device_id**:
Alias estable asignado por el usuario que identifica unívocamente a un **dispositivo**. Es
la clave (`@key`) de la instancia DDS. No se deriva del contenido de la sentencia.
_Avoid_: id, talker id (no es identidad).

**Talker ID**:
Los dos caracteres tras el `$` de una sentencia estándar (p. ej. `GP`, `HD`). Es un
**metadato descriptivo**, nunca identidad — dos equipos iguales comparten talker.
_Avoid_: id de dispositivo.

**Formatter**:
El código de tipo de sentencia (p. ej. `GGA`, `RMC`, `MWV`). Las sentencias propietarias
(`$VNYMR`) no siguen la estructura talker+formatter estándar.
_Avoid_: tipo de sentencia.

**Categoría** (category):
Clasificación funcional de un **formatter** según el **registro** (GPS, Weather, Heading,
Radar, Sounder, Velocity, Attitude, Inertial). El "tipo de dispositivo" detectado es el
conjunto multi-etiqueta de categorías observadas en una fuente.
_Avoid_: tipo de dispositivo (es el conjunto de categorías, no una sola).

**Registro de sentencias** (sentence registry):
Catálogo extensible, dirigido por datos, que define por **formatter**: su **categoría** y
sus campos (nombre + tipo semántico + unidad). Fuente de verdad para parseo, detección y
previsualización de IDL. Ampliable sin recompilar.
_Avoid_: tabla de sentencias, esquema.

**Conversión** (conversion):
Una traducción activa de una **fuente** NMEA a tópicos DDS, con su perfil **QoS** y su
estado (ejecución / parado / error). Se configura de a una; varias corren en paralelo.
_Avoid_: sesión, job, tarea.

**Pipeline**:
La realización en ejecución de una **conversión**: el camino lectura → parser → mapeo →
publicación que corre en su propio hilo.
_Avoid_: flujo, hilo (es más que el hilo).

**RawSentence**:
Tópico/tipo DDS de respaldo (talker + formatter + payload crudo) para sentencias que no
están en el **registro**, de modo que el gateway nunca descarta datos.
_Avoid_: genérico, fallback.

**Perfil QoS** (QoS profile):
Conjunto nombrado de políticas DDS (`telemetry_fast`, `state_latched`, `critical_reliable`,
...) que el **recomendador** propone a partir de la tasa medida y la **categoría**, y que el
usuario puede sobre-escribir antes de lanzar una **conversión**.
_Avoid_: configuración, ajustes.

**Monitor** (DDS monitor):
Participante DDS de solo-descubrimiento que inspecciona un dominio (o barre un rango) y
reporta sus tópicos, tipos y endpoints, más diagnósticos de red (multicast, firewall,
perfiles).
_Avoid_: spy, sniffer, escáner.

## Example dialogue

— "El usuario conecta dos GPS idénticos al mismo OrangePi. ¿Cómo los distingues?"
— "Por **device_id**: cada uno es un **dispositivo** distinto porque está en una **fuente**
distinta (`ttyUSB0` y `ttyUSB1`) y el usuario les dio alias `gps_proa` y `gps_popa`. El
**Talker ID** de ambos es `GP` — por eso no sirve como identidad."

— "Ese GPS también manda HDT. ¿Es un dispositivo de rumbo entonces?"
— "Es el mismo **dispositivo**, pero su 'tipo' es multi-etiqueta: el **registro** mapea `GGA`
a la **categoría** GPS y `HDT` a Heading, así que la fuente muestra ambas **categorías**."

— "¿Y si llega una `$PXYZ` que no conozco?"
— "Va al tópico **RawSentence**: no está en el **registro**, así que no se tipa, pero no se
pierde. Si luego la añades al **registro**, pasa a tener su propio tópico tipado."

— "Lancé la **conversión** del primer GPS y configuré el segundo. ¿Se paró el primero?"
— "No. La configuración es de a uno, pero cada **conversión** es un **pipeline** independiente;
el primero sigue en estado 'ejecución' en la lista mientras configuras el segundo."
