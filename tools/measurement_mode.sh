#!/usr/bin/env bash
# tools/measurement_mode.sh — poner el host en (o sacarlo de) modo medición.
#
# El esquema del manifiesto de la tesis codifica las condiciones de validez de
# la campaña como criterios de anulación. Dos de ellos son estado del sistema y
# no del arnés:
#
#   governor        debe ser "performance"           (enum del esquema, duro)
#   deviation_pct   frecuencia media dentro del 1 %  (turbo la rompe)
#
# Este script los conmuta y —más importante— los **verifica**. Fijar un valor y
# suponer que quedó fijado es cómo una campaña entera acaba anulada por un
# sysfs que rechazó la escritura en silencio.
#
# No es permanente a propósito. `off` devuelve la máquina a su estado normal,
# porque es también la máquina de las demostraciones y dejarla capada a la
# frecuencia base todo el tiempo no tiene sentido. Se llama antes de una
# campaña y después de ella.
#
# Lo que este script NO hace: isolcpus. Eso exige editar GRUB y reiniciar, es
# una decisión del autor y no algo que un script deba hacer por su cuenta.
#
# Uso:
#   tools/measurement_mode.sh on       # gobernador performance, turbo apagado
#   tools/measurement_mode.sh off      # devuelve el estado por defecto
#   tools/measurement_mode.sh status   # solo informa, no cambia nada
#   tools/measurement_mode.sh check    # sale 0 solo si el host es válido para medir

set -euo pipefail

NO_TURBO=/sys/devices/system/cpu/intel_pstate/no_turbo
BOOST=/sys/devices/system/cpu/cpufreq/boost           # AMD y algunos ARM
GOV_GLOB='/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor'

# Gobernador por defecto al que volver con `off`. powersave en Intel pstate,
# schedutil en casi todo lo demás; se detecta en vez de asumirse.
default_governor() {
    if [ -d /sys/devices/system/cpu/intel_pstate ]; then echo powersave; else echo schedutil; fi
}

governors()      { cat $GOV_GLOB 2>/dev/null | sort -u | tr '\n' ' '; }
turbo_state() {
    if [ -f "$NO_TURBO" ]; then
        [ "$(cat "$NO_TURBO")" = "1" ] && echo apagado || echo encendido
    elif [ -f "$BOOST" ]; then
        [ "$(cat "$BOOST")" = "0" ] && echo apagado || echo encendido
    else
        echo "no-controlable"
    fi
}
isolcpus_state() {
    tr ' ' '\n' < /proc/cmdline | grep '^isolcpus=' || echo "(ausente)"
}

set_governor() {
    sudo cpupower frequency-set -g "$1" >/dev/null 2>&1 \
        || for f in $GOV_GLOB; do echo "$1" | sudo tee "$f" >/dev/null; done
}

set_turbo() {   # $1 = on|off
    local want_no_turbo want_boost
    if [ "$1" = off ]; then want_no_turbo=1; want_boost=0; else want_no_turbo=0; want_boost=1; fi
    if [ -f "$NO_TURBO" ]; then
        echo "$want_no_turbo" | sudo tee "$NO_TURBO" >/dev/null
    elif [ -f "$BOOST" ]; then
        echo "$want_boost" | sudo tee "$BOOST" >/dev/null
    fi
}

# Estados C de inactividad.
#
# El gobernador `performance` NO basta. Medido en arc-cednav: con el gobernador
# en performance y el turbo apagado, un nucleo ocupado marca 2400 MHz y uno
# ocioso baja a 445. El esquema anula la corrida si la frecuencia media se
# desvia mas del 1 % del nominal, asi que TODA corrida de carga baja --la
# normativa ofrece seis sentencias por segundo y deja casi todo ocioso-- se
# anularia por inactividad y no por throttling, que es justo lo que ese
# criterio NO quiere decir.
#
# Desactivando los estados de inactividad los nucleos se quedan en 2399-2400 MHz
# aunque no tengan trabajo, y entonces una desviacion significa de verdad que el
# procesador bajo la frecuencia.
#
# Cuesta consumo y calor, y por eso `off` los vuelve a activar.
set_cstates() {   # $1 = on|off  (on = estados permitidos, o sea modo normal)
    local want; [ "$1" = off ] && want=1 || want=0
    for s in /sys/devices/system/cpu/cpu*/cpuidle/state[1-9]; do
        [ -f "$s/disable" ] && echo "$want" | sudo tee "$s/disable" >/dev/null 2>&1
    done
}

cstates_state() {
    local d=0 total=0
    for s in /sys/devices/system/cpu/cpu*/cpuidle/state[1-9]; do
        [ -f "$s/disable" ] || continue
        total=$((total + 1))
        [ "$(cat "$s/disable")" = "1" ] && d=$((d + 1))
    done
    [ "$total" = 0 ] && { echo "no-controlables"; return; }
    [ "$d" = "$total" ] && echo apagados || echo "encendidos ($d/$total apagados)"
}

# Temperatura desde sysfs y no desde `sensors`. En arc-cednav el modulo
# coretemp no esta cargado, asi que `sensors` no expone la temperatura del
# paquete de CPU y un parseo de su salida devuelve el umbral de otro sensor.
# El manifiesto exige thermal_c_start / thermal_c_end, asi que el arnes necesita
# una fuente fiable: es esta.
cpu_temp_c() {
    for z in /sys/class/thermal/thermal_zone*; do
        case "$(cat "$z/type" 2>/dev/null)" in
            TCPU|x86_pkg_temp|cpu-thermal|soc-thermal|acpitz)
                echo $(( $(cat "$z/temp" 2>/dev/null || echo 0) / 1000 )); return ;;
        esac
    done
    echo "desconocida"
}

# Un portatil midiendo con bateria hace throttling y la campana no seria
# comparable con una hecha con corriente. Se comprueba porque este host ES un
# portatil, no porque el diseno lo pidiera.
on_ac_power() {
    for s in /sys/class/power_supply/A{C,DP}*/online /sys/class/power_supply/*/online; do
        [ -f "$s" ] && { [ "$(cat "$s")" = "1" ] && echo si || echo NO; return; }
    done
    echo "sin-bateria"
}

report() {
    echo "  gobernador : $(governors)"
    echo "  turbo      : $(turbo_state)"
    echo "  estados C  : $(cstates_state)"
    echo "  isolcpus   : $(isolcpus_state)"
    echo "  nucleos    : $(nproc) en linea"
    echo "  temp CPU   : $(cpu_temp_c) C"
    echo "  corriente  : $(on_ac_power)"
}

# Sale 0 solo si el host satisface lo que el esquema exige. isolcpus se reporta
# como advertencia y no como fallo: su ausencia anula las corridas, pero es una
# decisión abierta del autor y este script no la toma.
check() {
    local ok=0
    local gov; gov=$(governors)
    if [ "$gov" != "performance " ]; then
        echo "  FALLA  gobernador es '${gov% }', el esquema exige 'performance'"; ok=1
    else
        echo "  ok     gobernador performance en todas las CPU"
    fi
    local t; t=$(turbo_state)
    if [ "$t" = encendido ]; then
        echo "  FALLA  turbo encendido; la frecuencia media se saldra del 1 % del nominal"; ok=1
    else
        echo "  ok     turbo $t"
    fi
    local cs; cs=$(cstates_state)
    if [ "$cs" != apagados ] && [ "$cs" != no-controlables ]; then
        echo "  FALLA  estados C activos ($cs): un nucleo ocioso baja a ~445 MHz y"
        echo "         la desviacion de frecuencia anularia toda corrida de carga baja"; ok=1
    else
        echo "  ok     estados C $cs"
    fi

    local ac; ac=$(on_ac_power)
    if [ "$ac" = "NO" ]; then
        echo "  FALLA  el host corre con bateria; hara throttling y las corridas no"
        echo "         seran comparables con las hechas con corriente"; ok=1
    elif [ "$ac" = si ]; then
        echo "  ok     con corriente"
    fi

    local iso; iso=$(isolcpus_state)
    if [ "$iso" = "(ausente)" ]; then
        echo "  AVISO  isolcpus ausente: el esquema anula la corrida. Exige editar GRUB"
        echo "         y reiniciar; es decision del autor y este script no la toma."
    else
        echo "  ok     $iso"
    fi
    return $ok
}

case "${1:-status}" in
    on)
        set_governor performance
        set_turbo off
        set_cstates off
        echo "modo medicion ACTIVADO"; report; echo; check || true
        ;;
    off)
        set_governor "$(default_governor)"
        set_turbo on
        set_cstates on
        echo "modo medicion DESACTIVADO (estado normal)"; report
        ;;
    status)
        echo "estado actual"; report
        ;;
    check)
        echo "validez como host de medicion"; check
        ;;
    *)
        echo "uso: $0 {on|off|status|check}" >&2; exit 2
        ;;
esac
