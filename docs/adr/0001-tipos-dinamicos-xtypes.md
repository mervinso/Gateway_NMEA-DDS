# Tipos DDS vía DynamicData/XTypes en runtime (no codegen)

Status: accepted

El gateway debe crear tipos DDS para sentencias NMEA decididas en tiempo de ejecución —
incluido un catálogo extensible y sentencias propietarias (`$VN...`) añadidas sin
recompilar. Decidimos construir los tipos con **DDS-XTypes DynamicTypeBuilder/DynamicData**
y publicarlos con `DynamicPubSubType`, en lugar de generar C++ con `fastddsgen` y compilarlo.
El archivo `.idl` se sigue generando, pero como **artefacto** (documentación, interop,
codegen estático offline opcional), no como paso de compilación: "crear IDL y ejecutar" =
cargar IDL → construir `DynamicType` → crear `DataWriter`.

## Considered Options

- **fastddsgen + compilar en runtime**: máximo rendimiento y tipado estático, pero exige un
  toolchain de C++ en la OrangePi e invocar `dlopen` por cada tipo nuevo — pesado y frágil.
- **Catálogo estático precompilado**: simple, pero añadir una sentencia nueva (especialmente
  propietaria) obliga a recompilar, lo que contradice el requisito de extensibilidad.

## Consequences

- Costo de rendimiento de DynamicData frente a tipos estáticos: irrelevante a tasas NMEA
  (pocos Hz a ~100 Hz).
- El `TypeObject` viaja por el cable vía XTypes, así que suscriptores externos (incluido
  ROS 2 vía `rmw_fastrtps`) descubren los tipos sin tener los stubs compilados.
- El registro de sentencias dirigido por datos pasa a ser la fuente de verdad del tipo.
