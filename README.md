# db-scanner

Herramienta de línea de comandos en C++20 para escanear un esquema Oracle:
descubre las tablas de un usuario, su estructura (columnas, claves, índices),
sus relaciones de dependencia (foreign keys declaradas + inferidas por
convención de nombres) y valida la consistencia referencial de los datos
(filas huérfanas). Diseñada para esquemas grandes: checkpoint/resume, apagado
ordenado ante Ctrl+C, escaneo paralelo configurable, logging estructurado.

## Arquitectura

```
src/
  core/        tipos de dominio (TableInfo, Relationship, ScanResult) + JSON
  config/      parseo de CLI y config, resolución de password
  logging/     spdlog (consola + archivo rotativo)
  checkpoint/  estado de progreso en SQLite (resume)
  relations/   inferencia de relaciones + grafo de dependencias/ciclos
  report/      escritura de JSON + resumen en consola
  db/          ISchemaSource/IConsistencyChecker (interfaces) +
               implementación Oracle/OCCI (OracleSchemaReader, etc.)
  engine/      ThreadPool, ShutdownController, ScanEngine (orquestador)
  app/         main.cpp (solo este target + db/Oracle* requieren OCCI)
tests/         pruebas unitarias (GoogleTest), no requieren Oracle
```

`ScanEngine` sólo depende de las interfaces `ISchemaSource` /
`IConsistencyChecker`, no de OCCI directamente — por eso todo el motor,
checkpoint, inferencia de relaciones, etc. se compilan y prueban sin tener el
Oracle Instant Client SDK instalado. Sólo `db/Oracle*.cpp` y el ejecutable
`dbscanner` (`app/`) requieren `occi.h`.

## Prerrequisitos

- CMake >= 3.20, compilador con C++20 (GCC 12+/Clang equivalente).
- **Oracle Instant Client Basic + SDK** (misma versión), con `ORACLE_HOME`
  apuntando al directorio que los contiene. El paquete Basic por sí solo
  **no alcanza**: hace falta el SDK (`instantclient-sdk-linux.x64-<version>.zip`)
  para tener `sdk/include/occi.h` y `sdk/include/oci.h`. Descárgalo desde Oracle
  y extráelo dentro del mismo directorio del Instant Client Basic.
- Paquetes de desarrollo: `libsqlite3-dev`, `nlohmann-json3-dev`, `libfmt-dev`,
  `libspdlog-dev`, `libgtest-dev` (para tests). En Debian/Ubuntu:

  ```sh
  sudo apt install libsqlite3-dev nlohmann-json3-dev libfmt-dev libspdlog-dev libgtest-dev
  ```

Si `occi.h`/`oci.h` no se encuentran, CMake emite un warning claro y **omite**
los targets `dbscanner` (ejecutable) y `dbscanner_db_oracle`, pero el resto de
librerías y `dbscanner_tests` se compilan igual.

## Compilar

```sh
export ORACLE_HOME=/ruta/a/instantclient_23_26   # debe incluir sdk/include
export ORACLE_HOME=/home/miguel/oracle/instantclient_23_26
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure   # pruebas unitarias, sin Oracle
```

El binario final queda en `build/src/app/dbscanner`.

## Uso

```sh
export DBSCANNER_ORACLE_PWD='...'   # o se pedirá interactivamente (oculto)
./build/src/app/dbscanner \
  --connect myhost:1521/ORCLPDB1 \
  --user APP_USER \
  --output ./dbscanner-out \
  --threads 8 \
  --exclude 'TMP_*,*_BAK'
```

Para reanudar un run interrumpido (Ctrl+C, corte de red, etc.):

```sh
./build/src/app/dbscanner --connect myhost:1521/ORCLPDB1 --user APP_USER \
  --output ./dbscanner-out --resume
```

También se puede usar un archivo de configuración JSON (ver
`config/scanner.example.json`); los flags de CLI siempre tienen prioridad
sobre el archivo:

```sh
./build/src/app/dbscanner --config config/scanner.json --resume
```

Flags disponibles: `db-scanner --help`.

## Salidas

Dentro de `--output <dir>`:

- `schema.json` — tablas, columnas, claves, índices, relaciones (declaradas +
  inferidas) y ciclos de dependencia detectados.
- `consistency_report.json` — hallazgos de consistencia por relación (conteo
  de filas huérfanas + muestra de claves violatorias), con un resumen agregado.
- `checkpoint.db` — estado interno (SQLite) usado para `--resume`; no borrar
  mientras un run esté en curso o pendiente de reanudar.
- `logs/dbscanner.log` — log rotativo detallado (consola solo muestra
  progreso + resumen).

## Checkpoint / resume / apagado ordenado

Cada tabla escaneada y cada relación validada se marca `done` en
`checkpoint.db` **junto con su resultado**, en una transacción atómica — así
un `--resume` no vuelve a consultar Oracle por unidades ya completadas. Ante
SIGINT/SIGTERM, el proceso deja terminar la unidad en curso, no arranca
unidades nuevas, escribe el reporte parcial y sale con código 130 indicando
que es reanudable.

## Nota sobre el backend OCCI

`src/db/Oracle*.cpp` está escrito contra la API pública de OCCI pero **no se
pudo compilar en esta máquina de desarrollo** porque sólo el paquete Basic del
Instant Client estaba instalado (sin `occi.h`). Antes de considerar el
ejecutable `dbscanner` listo para producción, instala el SDK, compila
(`DBSCANNER_ORACLE_AVAILABLE` debe salir `TRUE` en el log de `cmake`) y corre
un escaneo real contra un esquema de prueba para validar el flujo completo,
incluyendo una interrupción manual (Ctrl+C) y `--resume`.
