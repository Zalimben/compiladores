#!/bin/sh

set -u

MINIC=${1:-./minic}
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/minic-tests.XXXXXX") || exit 1
PASSED=0
FAILED=0

cleanup() {
    rm -rf "$TEST_ROOT"
}
trap cleanup EXIT HUP INT TERM

pass() {
    PASSED=$((PASSED + 1))
    printf 'ok - %s\n' "$1"
}

fail() {
    FAILED=$((FAILED + 1))
    printf 'not ok - %s\n' "$1"
}

expect_status() {
    expected=$1
    name=$2
    shift 2

    "$@" >"$TEST_ROOT/stdout" 2>"$TEST_ROOT/stderr"
    actual=$?
    if [ "$actual" -eq "$expected" ]; then
        pass "$name"
    else
        fail "$name (se esperaba $expected, se obtuvo $actual)"
    fi
}

cat >"$TEST_ROOT/return_2.c" <<'EOF'
#define RESULTADO 2
int main(void) {
    return RESULTADO;
}
EOF

expect_status 0 "muestra la ayuda" "$MINIC" --help
expect_status 0 "muestra la versión" "$MINIC" --version

expect_status 0 "preprocesa con -E" \
    "$MINIC" -E "$TEST_ROOT/return_2.c"
if [ -f "$TEST_ROOT/return_2.i" ] &&
   grep -q "return 2;" "$TEST_ROOT/return_2.i"; then
    pass "genera la salida .i predeterminada"
else
    fail "genera la salida .i predeterminada"
fi

rm -f "$TEST_ROOT/return_2.i"
expect_status 0 "-E conserva los marcadores por defecto" \
    "$MINIC" -E "$TEST_ROOT/return_2.c"
if [ -f "$TEST_ROOT/return_2.i" ] &&
   grep -q '^# ' "$TEST_ROOT/return_2.i"; then
    pass "sin -P conserva los marcadores de línea"
else
    fail "sin -P conserva los marcadores de línea"
fi

expect_status 0 "-P elimina los marcadores de línea" \
    "$MINIC" -E -P "$TEST_ROOT/return_2.c" -o "$TEST_ROOT/sin_marcadores.i"
if [ -f "$TEST_ROOT/sin_marcadores.i" ] &&
   ! grep -q '^# ' "$TEST_ROOT/sin_marcadores.i"; then
    pass "la salida de -P no contiene marcadores de línea"
else
    fail "la salida de -P no contiene marcadores de línea"
fi

expect_status 0 "acepta -o en Entrega 1" \
    "$MINIC" -E "$TEST_ROOT/return_2.c" -o "$TEST_ROOT/personalizado.i"
if [ -f "$TEST_ROOT/personalizado.i" ]; then
    pass "-o cambia el archivo de salida"
else
    fail "-o cambia el archivo de salida"
fi

mkdir "$TEST_ROOT/ruta con espacios"
cp "$TEST_ROOT/return_2.c" "$TEST_ROOT/ruta con espacios/programa.c"
expect_status 0 "procesa rutas con espacios sin usar un shell" \
    "$MINIC" -E "$TEST_ROOT/ruta con espacios/programa.c"
if [ -f "$TEST_ROOT/ruta con espacios/programa.i" ]; then
    pass "genera la salida en una ruta con espacios"
else
    fail "genera la salida en una ruta con espacios"
fi

# Casos de integración de la Entrega 2.
rm -f "$TEST_ROOT/return_2.i" "$TEST_ROOT/return_2.s"
expect_status 0 "-S genera ensamblador" \
    "$MINIC" -S "$TEST_ROOT/return_2.c"
if [ -f "$TEST_ROOT/return_2.s" ] &&
   grep -Eq 'movl[[:space:]]+\$2, %eax' "$TEST_ROOT/return_2.s"; then
    pass "el ensamblador retorna la constante esperada"
else
    fail "el ensamblador retorna la constante esperada"
fi
if [ ! -e "$TEST_ROOT/return_2.i" ]; then
    pass "-S elimina el preprocesado temporal"
else
    fail "-S elimina el preprocesado temporal"
fi

if gcc -c "$TEST_ROOT/return_2.s" -o "$TEST_ROOT/return_2.o" \
    >"$TEST_ROOT/stdout" 2>"$TEST_ROOT/stderr"; then
    pass "la salida .s es ensamblador válido"
else
    fail "la salida .s es ensamblador válido"
fi

expect_status 0 "-S acepta una salida personalizada" \
    "$MINIC" -S "$TEST_ROOT/return_2.c" \
    -o "$TEST_ROOT/personalizado.s"
if [ -f "$TEST_ROOT/personalizado.s" ]; then
    pass "-o cambia el nombre del ensamblador"
else
    fail "-o cambia el nombre del ensamblador"
fi

expect_status 0 "-v muestra las etapas ejecutadas" \
    "$MINIC" -v -S "$TEST_ROOT/return_2.c" \
    -o "$TEST_ROOT/detallado.s"
if grep -q "preprocesamiento:" "$TEST_ROOT/stderr" &&
   grep -q "compilador simulado con GCC:" "$TEST_ROOT/stderr" &&
   [ "$(grep -c "^minic: comando:" "$TEST_ROOT/stderr")" -eq 2 ]; then
    pass "-v informa los comandos de las dos etapas"
else
    fail "-v informa los comandos de las dos etapas"
fi

rm -f "$TEST_ROOT/return_2.i"
expect_status 0 "--keep-temp conserva el archivo .i" \
    "$MINIC" --keep-temp -S "$TEST_ROOT/return_2.c" \
    -o "$TEST_ROOT/con_temporal.s"
if [ -f "$TEST_ROOT/return_2.i" ]; then
    pass "--keep-temp publica el archivo preprocesado"
else
    fail "--keep-temp publica el archivo preprocesado"
fi

cat >"$TEST_ROOT/return_42.c" <<'EOF'
int main(void) {
    return 42;
}
EOF
expect_status 0 "compila otras constantes enteras" \
    "$MINIC" -S "$TEST_ROOT/return_42.c"
if grep -Eq 'movl[[:space:]]+\$42, %eax' "$TEST_ROOT/return_42.s"; then
    pass "el mock de GCC genera la constante esperada"
else
    fail "el mock de GCC genera la constante esperada"
fi

cat >"$TEST_ROOT/sin_punto_y_coma.c" <<'EOF'
int main(void) {
    return 2
}
EOF
expect_status 4 "un error del compilador detiene el pipeline" \
    "$MINIC" -S "$TEST_ROOT/sin_punto_y_coma.c"
if [ ! -e "$TEST_ROOT/sin_punto_y_coma.s" ] &&
   grep -q "error:" "$TEST_ROOT/stderr"; then
    pass "GCC diagnostica el error del mock"
else
    fail "GCC diagnostica el error del mock"
fi

# Modos internos simulados para el futuro compilador.
cp "$TEST_ROOT/return_2.c" "$TEST_ROOT/modos_internos.c"
rm -f \
    "$TEST_ROOT/modos_internos.i" \
    "$TEST_ROOT/modos_internos.s" \
    "$TEST_ROOT/modos_internos.o" \
    "$TEST_ROOT/modos_internos"
expect_status 0 "--lex ejecuta el mock y se detiene" \
    "$MINIC" --lex "$TEST_ROOT/modos_internos.c"
expect_status 0 "--parse ejecuta el mock y se detiene" \
    "$MINIC" --parse "$TEST_ROOT/modos_internos.c"
expect_status 0 "--codegen ejecuta el mock y se detiene" \
    "$MINIC" --codegen "$TEST_ROOT/modos_internos.c"
if [ ! -e "$TEST_ROOT/modos_internos.i" ] &&
   [ ! -e "$TEST_ROOT/modos_internos.s" ] &&
   [ ! -e "$TEST_ROOT/modos_internos.o" ] &&
   [ ! -e "$TEST_ROOT/modos_internos" ]; then
    pass "los modos internos no publican productos"
else
    fail "los modos internos no publican productos"
fi

expect_status 0 "--keep-temp conserva el .i de un modo interno" \
    "$MINIC" --keep-temp --parse "$TEST_ROOT/modos_internos.c"
if [ -f "$TEST_ROOT/modos_internos.i" ] &&
   [ ! -e "$TEST_ROOT/modos_internos.s" ]; then
    pass "--keep-temp solo conserva la entrada preprocesada"
else
    fail "--keep-temp solo conserva la entrada preprocesada"
fi

expect_status 0 "-v describe el mock de --codegen" \
    "$MINIC" -v --codegen "$TEST_ROOT/modos_internos.c"
if grep -q "preprocesamiento:" "$TEST_ROOT/stderr" &&
   grep -q "mock de generación de código:" "$TEST_ROOT/stderr" &&
   [ "$(grep -c "^minic: comando:" "$TEST_ROOT/stderr")" -eq 2 ]; then
    pass "-v muestra preprocesamiento y modo interno"
else
    fail "-v muestra preprocesamiento y modo interno"
fi

expect_status 4 "--parse propaga un error del mock" \
    "$MINIC" --parse "$TEST_ROOT/sin_punto_y_coma.c"
expect_status 1 "rechaza dos modos internos simultáneos" \
    "$MINIC" --lex --parse "$TEST_ROOT/modos_internos.c"
expect_status 1 "rechaza un modo interno junto con -S" \
    "$MINIC" --lex -S "$TEST_ROOT/modos_internos.c"
expect_status 1 "rechaza -o en los modos internos" \
    "$MINIC" --codegen "$TEST_ROOT/modos_internos.c" \
    -o "$TEST_ROOT/no_permitido.s"

# Casos de integración de la Entrega 3.
rm -f \
    "$TEST_ROOT/return_2.i" \
    "$TEST_ROOT/return_2.s" \
    "$TEST_ROOT/return_2.o" \
    "$TEST_ROOT/return_2"
expect_status 0 "-c genera un archivo objeto" \
    "$MINIC" -c "$TEST_ROOT/return_2.c"
if [ -f "$TEST_ROOT/return_2.o" ]; then
    pass "-c conserva el producto .o"
else
    fail "-c conserva el producto .o"
fi
if [ ! -e "$TEST_ROOT/return_2.i" ] &&
   [ ! -e "$TEST_ROOT/return_2.s" ] &&
   [ ! -e "$TEST_ROOT/return_2" ]; then
    pass "-c elimina intermedios y no enlaza"
else
    fail "-c elimina intermedios y no enlaza"
fi

expect_status 0 "-c acepta una salida personalizada" \
    "$MINIC" -c "$TEST_ROOT/return_2.c" \
    -o "$TEST_ROOT/personalizado.o"
if [ -f "$TEST_ROOT/personalizado.o" ]; then
    pass "-o cambia el nombre del archivo objeto"
else
    fail "-o cambia el nombre del archivo objeto"
fi

rm -f \
    "$TEST_ROOT/return_2.i" \
    "$TEST_ROOT/return_2.s" \
    "$TEST_ROOT/return_2.o" \
    "$TEST_ROOT/return_2"
expect_status 0 "sin opción de etapa genera un ejecutable" \
    "$MINIC" "$TEST_ROOT/return_2.c"
if [ -x "$TEST_ROOT/return_2" ]; then
    pass "genera el ejecutable con el nombre predeterminado"
else
    fail "genera el ejecutable con el nombre predeterminado"
fi

"$TEST_ROOT/return_2" >"$TEST_ROOT/stdout" 2>"$TEST_ROOT/stderr"
program_status=$?
if [ "$program_status" -eq 2 ]; then
    pass "el ejecutable retorna el valor esperado"
else
    fail "el ejecutable retorna 2 (se obtuvo $program_status)"
fi

if [ ! -e "$TEST_ROOT/return_2.i" ] &&
   [ ! -e "$TEST_ROOT/return_2.s" ] &&
   [ ! -e "$TEST_ROOT/return_2.o" ]; then
    pass "el enlace completo elimina todos los intermedios"
else
    fail "el enlace completo elimina todos los intermedios"
fi

expect_status 0 "el enlace acepta una salida personalizada" \
    "$MINIC" "$TEST_ROOT/return_2.c" -o "$TEST_ROOT/aplicacion"
if [ -x "$TEST_ROOT/aplicacion" ]; then
    pass "-o cambia el nombre del ejecutable"
else
    fail "-o cambia el nombre del ejecutable"
fi

rm -f \
    "$TEST_ROOT/return_2.i" \
    "$TEST_ROOT/return_2.s" \
    "$TEST_ROOT/return_2.o"
expect_status 0 "--keep-temp conserva todos los intermedios" \
    "$MINIC" --keep-temp "$TEST_ROOT/return_2.c" \
    -o "$TEST_ROOT/con_intermedios"
if [ -f "$TEST_ROOT/return_2.i" ] &&
   [ -f "$TEST_ROOT/return_2.s" ] &&
   [ -f "$TEST_ROOT/return_2.o" ] &&
   [ -x "$TEST_ROOT/con_intermedios" ]; then
    pass "--keep-temp conserva .i, .s y .o"
else
    fail "--keep-temp conserva .i, .s y .o"
fi

cat >"$TEST_ROOT/detallado.c" <<'EOF'
int main(void) {
    return 2;
}
EOF
expect_status 0 "-v muestra el pipeline completo" \
    "$MINIC" -v "$TEST_ROOT/detallado.c"
if grep -q "preprocesamiento:" "$TEST_ROOT/stderr" &&
   grep -q "compilador simulado con GCC:" "$TEST_ROOT/stderr" &&
   grep -q "ensamblado:" "$TEST_ROOT/stderr" &&
   grep -q "enlace:" "$TEST_ROOT/stderr" &&
   [ "$(grep -c "^minic: comando:" "$TEST_ROOT/stderr")" -eq 4 ]; then
    pass "-v informa las cuatro etapas y comandos"
else
    fail "-v informa las cuatro etapas y comandos"
fi

rm -f \
    "$TEST_ROOT/ruta con espacios/programa.i" \
    "$TEST_ROOT/ruta con espacios/programa.s" \
    "$TEST_ROOT/ruta con espacios/programa.o" \
    "$TEST_ROOT/ruta con espacios/programa"
expect_status 0 "el pipeline completo procesa rutas con espacios" \
    "$MINIC" "$TEST_ROOT/ruta con espacios/programa.c"
"$TEST_ROOT/ruta con espacios/programa" \
    >"$TEST_ROOT/stdout" 2>"$TEST_ROOT/stderr"
space_program_status=$?
if [ "$space_program_status" -eq 2 ]; then
    pass "el ejecutable ubicado en una ruta con espacios funciona"
else
    fail "el ejecutable ubicado en una ruta con espacios funciona"
fi

# GCC controlado para simular fallos de etapas posteriores.
REAL_GCC=$(command -v gcc)
export REAL_GCC
mkdir "$TEST_ROOT/fake-bin"
cat >"$TEST_ROOT/fake-bin/gcc" <<'EOF'
#!/bin/sh

stage=link
for argument in "$@"; do
    case "$argument" in
        -E) stage=preprocess ;;
        -S) stage=compile ;;
        -c) stage=assemble ;;
    esac
done

if [ -n "${MINIC_TEST_LOG:-}" ]; then
    printf '%s\n' "$stage" >>"$MINIC_TEST_LOG"
fi

if [ "${MINIC_TEST_FAIL_STAGE:-}" = "$stage" ]; then
    exit 23
fi

exec "$REAL_GCC" "$@"
EOF
chmod +x "$TEST_ROOT/fake-bin/gcc"

cp "$TEST_ROOT/return_2.c" "$TEST_ROOT/fallo_ensamblador.c"
: >"$TEST_ROOT/etapas_ensamblador.log"
expect_status 5 "propaga el fallo del ensamblador" \
    env \
    "PATH=$TEST_ROOT/fake-bin:$PATH" \
    MINIC_TEST_FAIL_STAGE=assemble \
    "MINIC_TEST_LOG=$TEST_ROOT/etapas_ensamblador.log" \
    "$MINIC" -c "$TEST_ROOT/fallo_ensamblador.c"
if [ ! -e "$TEST_ROOT/fallo_ensamblador.o" ] &&
   [ ! -e "$TEST_ROOT/fallo_ensamblador.i" ] &&
   [ ! -e "$TEST_ROOT/fallo_ensamblador.s" ] &&
   ! grep -q '^link$' "$TEST_ROOT/etapas_ensamblador.log"; then
    pass "un fallo del ensamblador limpia y evita el enlace"
else
    fail "un fallo del ensamblador limpia y evita el enlace"
fi

cp "$TEST_ROOT/return_2.c" "$TEST_ROOT/fallo_enlazador.c"
expect_status 6 "propaga el fallo del enlazador" \
    env \
    "PATH=$TEST_ROOT/fake-bin:$PATH" \
    MINIC_TEST_FAIL_STAGE=link \
    "$MINIC" --keep-temp "$TEST_ROOT/fallo_enlazador.c"
if [ ! -e "$TEST_ROOT/fallo_enlazador" ] &&
   [ -f "$TEST_ROOT/fallo_enlazador.i" ] &&
   [ -f "$TEST_ROOT/fallo_enlazador.s" ] &&
   [ -f "$TEST_ROOT/fallo_enlazador.o" ]; then
    pass "un fallo del enlace conserva intermedios solicitados"
else
    fail "un fallo del enlace conserva intermedios solicitados"
fi

expect_status 1 "rechaza la ausencia de entrada" "$MINIC"
expect_status 2 "rechaza un archivo inexistente" \
    "$MINIC" "$TEST_ROOT/inexistente.c"

touch "$TEST_ROOT/entrada.txt"
expect_status 1 "rechaza una extensión no permitida" \
    "$MINIC" "$TEST_ROOT/entrada.txt"
expect_status 1 "rechaza opciones desconocidas" \
    "$MINIC" --desconocida "$TEST_ROOT/return_2.c"
expect_status 1 "rechaza -P repetida" \
    "$MINIC" -P -P "$TEST_ROOT/return_2.c"
expect_status 1 "rechaza etapas incompatibles" \
    "$MINIC" -E -S "$TEST_ROOT/return_2.c"
expect_status 1 "rechaza -S y -c simultáneas" \
    "$MINIC" -S -c "$TEST_ROOT/return_2.c"
expect_status 1 "detecta el argumento ausente de -o" \
    "$MINIC" "$TEST_ROOT/return_2.c" -o
expect_status 1 "rechaza varias entradas" \
    "$MINIC" "$TEST_ROOT/return_2.c" "$TEST_ROOT/otra.c"

cat >"$TEST_ROOT/error.c" <<'EOF'
#error fallo solicitado por la prueba
EOF
expect_status 3 "propaga el fallo del preprocesador" \
    "$MINIC" -E "$TEST_ROOT/error.c"
if [ ! -e "$TEST_ROOT/error.i" ]; then
    pass "elimina la salida parcial después de un fallo"
else
    fail "elimina la salida parcial después de un fallo"
fi

printf 'salida anterior\n' >"$TEST_ROOT/anterior.i"
expect_status 3 "falla sin reemplazar una salida previa" \
    "$MINIC" -E "$TEST_ROOT/error.c" -o "$TEST_ROOT/anterior.i"
if grep -q "salida anterior" "$TEST_ROOT/anterior.i"; then
    pass "conserva una salida previa después de un fallo"
else
    fail "conserva una salida previa después de un fallo"
fi

expect_status 3 "un fallo al preprocesar impide compilar" \
    "$MINIC" -S "$TEST_ROOT/error.c"
if [ ! -e "$TEST_ROOT/error.s" ]; then
    pass "no genera ensamblador después de fallar el preprocesador"
else
    fail "no genera ensamblador después de fallar el preprocesador"
fi

expect_status 3 "un fallo al preprocesar impide el enlace" \
    "$MINIC" "$TEST_ROOT/error.c"
if [ ! -e "$TEST_ROOT/error" ]; then
    pass "no genera ejecutable después de fallar el preprocesador"
else
    fail "no genera ejecutable después de fallar el preprocesador"
fi

printf '\n%d pruebas correctas; %d pruebas fallidas\n' "$PASSED" "$FAILED"
[ "$FAILED" -eq 0 ]
