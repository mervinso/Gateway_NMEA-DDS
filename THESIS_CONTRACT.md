# Contrato con la tesis

Este archivo existe porque el artefacto y la tesis se desarrollan en **dos máquinas
distintas**, y porque un cambio inocente aquí puede invalidar días de medición allá.

Es corto a propósito. Actualízalo; no lo dejes envejecer.

---

## 1. Estado del freeze

> ## `NO CONGELADA`
>
> **Última verificación:** 2026-08-31
> **Verificado en:** `dds-own`, `STATUS.md` → sección "Phase 4"
> **Commit de la tesis que corresponde a este estado:** `33ebc84`

**Mientras diga `NO CONGELADA`:**
arregla el artefacto. Un defecto encontrado ahora se corrige. **No** lo escribas como
limitación de la tesis — eso es convertir un bug en un hallazgo, y no lo es.

**En cuanto diga `CONGELADA <fecha> <sha-del-freeze>`:**
describe el artefacto. Todo cambio en la ruta de publicación, en el mapeo, en el registro o
en los valores de QoS invalida las mediciones ya tomadas. Si algo *tiene* que cambiar,
detente y escríbelo en `dds-own/STATUS.md` antes de tocar el código: la desviación se
declara en el reporte y vuelve **exploratorio** el análisis afectado.

La campaña completa cuesta **13,7 h por plataforma**. Repetirla no es gratis.

---

## 2. Cambios que exigen avisar a la tesis

Aviso = anotarlo en `dds-own/STATUS.md`. No hace falta permiso mientras no esté congelada;
hace falta que quede escrito.

| Área | Por qué |
|---|---|
| `Mapper::populate()`, `Mapper::type_for()` | Son el mecanismo bajo prueba: caché de `DynamicType`, `create_data()` en heap, `get_member_id_by_name()` por campo. H1 y H4 se enuncian sobre esta ruta. |
| Número o tipo de campos de cualquier formatter | La escalera de aridad **ROT (2) · RSA (4) · DBT (6) · VHW (8)** está congelada y cada nivel añade exactamente un par *(numérico, carácter)*. Cambiar un conteo de campos rompe el diseño y reintroduce el confuso aridad↔composición. Los conteos están verificados dos veces: contra `Registry.cpp` y contra el texto normativo de IEC 61162-1 Ed. 4.0 (§8.3.71, §8.3.73, §8.3.21, §8.3.94). |
| Miembros de cabecera (`device_id` `@key`, `talker`, `recv_timestamp`) | Las búsquedas por corrida son **campos + 3**. Añadir o quitar un miembro de cabecera desplaza el intercepto de todos los modelos. |
| Valores por defecto de QoS | `{reliable=false, transient_local=false, deadline_ms=0, lifespan_ms=0}` es el valor mantenido constante. Cambiarlo cambia el experimento. |
| Transportes / memoria compartida | El diseño exige `["UDPv4"]` y SHM deshabilitada; es criterio de anulación de corrida (`v3_transport`). |
| Cualquier cosa dentro de la región cronometrada | Retorno del parser → retorno de `write()`. |
| Versión de Fast DDS, toolchain, flags de build | Van al manifiesto de corrida. Un cambio de versión a mitad de campaña hace incomparables las corridas. |

**No exigen aviso:** la UI, el puente ROS 2, `tools/`, los tests, el README, los comentarios.
El puente ROS 2 queda explícitamente fuera de los tres ejes de contribución: se describe en
el capítulo del artefacto y se excluye de las afirmaciones empíricas.

---

## 3. El commit que la tesis mide

Cuando llegue el freeze, la tesis cita **un SHA**, no "el gateway". `main` sigue avanzando;
las mediciones no.

| Campo | Valor |
|---|---|
| Commit medido | *(pendiente — se fija en el freeze)* |
| Tag citable | *(pendiente — obligación A2)* |
| DOI Zenodo | *(pendiente — obligación A2)* |

---

## 4. Ritual de sincronía

Los dos repos son privados/públicos por separado y **no** se fusionan.

```
Gateway_NMEA-DDS   https://github.com/mervinso/Gateway_NMEA-DDS   (público, Apache-2.0)
                   Ubuntu — implementación y prueba de software
dds-own            https://github.com/mervinso/dds-own            (privado)
                   Windows — diseño, metodología, análisis, tesis
```

**El gesto que sostiene todo esto es `git push`.** Sin él las dos máquinas divergen en
silencio y el esfuerzo de una no le sirve a la otra.

**Al empezar a trabajar, en cualquiera de las dos:**

```bash
git pull
```

**Al terminar, en cualquiera de las dos:**

```bash
git add -A && git commit && git push
```

**Además, al empezar en la tesis:** anotar el SHA de `HEAD` del gateway en `STATUS.md`, y
comprobar que la sección 1 de este archivo sigue diciendo lo correcto.

**Regla de propiedad:** la tesis no edita código del gateway; el gateway no edita `thesis/`.
Dos instancias de Claude Code no deben tocar el mismo archivo. Si algo necesita cambiar del
otro lado, se anota, no se edita a distancia.

**Advertencia — hay un tercer clon.** La máquina Windows tiene una copia local de
`Gateway_NMEA-DDS` que existe solo para que la tesis pueda consultar el código sin adivinar.
**Esa copia es de solo lectura: `git pull` y nada más.** Todo commit de código sale de
Ubuntu. La única excepción fue la creación inicial de `CLAUDE.md` y este archivo.

---

## 5. Qué hay al otro lado que te sirve

En `dds-own`, listo para usar y ya validado:

| Ruta | Qué es |
|---|---|
| `experiments/testbed/manifest.schema.json` | El contrato exacto del `manifest.json` que debe emitir el arnés. `additionalProperties: false`. |
| `experiments/testbed/manifest.example.json` | Ejemplo válido contra el cual construir. |
| `experiments/testbed/validate_manifest.py` | Valida, cruza los factores contra el schedule y adjudica anulaciones. `--self-test` prueba que cada criterio de anulación es capaz de dispararse. |
| `experiments/testbed/schedule.csv` | Las 400 corridas en orden aleatorizado. **Se regenera** con el `R` final que salga del piloto. |
| `experiments/testbed/pilot_schedule.csv` | Las 18 corridas del piloto de varianza. |
| `requirements.txt` | Stack de análisis fijado. Verificado en Windows; **no** verificado todavía en los hosts Linux x86/ARM64. |
| `thesis/ch8_design_implementation.tex` | La especificación de todo lo pendiente. |
| `docs/science-superpowers/questions/2026-08-31-integrated-bridge-fanout-headroom.md` | RQ3, el estudio de escalabilidad. Contiene el hallazgo sobre topología de participantes. |
| `docs/science-superpowers/prior-work/2026-08-31-rq3-prior-work-note.md` | La evidencia publicada detrás de esa decisión, con DOIs verificados. |

---

## 6. Qué falta para poder medir

En orden. Los tres primeros no dependen de que la tesis decida nada más.

- [ ] **A3 — reproducir el build de referencia y fijar versiones.** Fast DDS es hoy "lo que
      traiga la distro de ROS" y no está registrado. Bloquea todo lo demás.
- [ ] **2a** miembros opcionales · **2b** sacar dos costos de la región medida ·
      **A5** brazo estático · **A4** instrumentación · **4b** emisor de `manifest.json`.
- [ ] **Decidir la topología de participantes** (§ correspondiente en `CLAUDE.md`).
      Es del autor; si es deliberada, va como ADR.
- [ ] **Compuerta 1** — equivalencia CDR entre brazo estático y dinámico.
- [ ] **Compuerta 2** — piso de ruido de la instrumentación.
- [ ] **Piloto de varianza**, 18 corridas (`pilot_schedule.csv`). Necesita hardware. Sale la
      SD residual → fija `R_main` y `R_arity` → permite regenerar `schedule.csv`.
- [ ] **Shakedown**, luego **freeze**, luego la campaña: 400 corridas, 13,7 h por plataforma.

Cuando el freeze ocurra, la sección 1 de este archivo cambia de palabra y con ella cambian
las reglas.
