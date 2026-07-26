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

expect_status 0 "preprocesa sin -E" "$MINIC" "$TEST_ROOT/return_2.c"
if [ -f "$TEST_ROOT/return_2.i" ] &&
   grep -q "return 2;" "$TEST_ROOT/return_2.i"; then
    pass "genera la salida .i predeterminada"
else
    fail "genera la salida .i predeterminada"
fi

rm -f "$TEST_ROOT/return_2.i"
expect_status 0 "preprocesa con -E" "$MINIC" -E "$TEST_ROOT/return_2.c"
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
    "$MINIC" "$TEST_ROOT/return_2.c" -o "$TEST_ROOT/personalizado.i"
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

printf '\n%d pruebas correctas; %d pruebas fallidas\n' "$PASSED" "$FAILED"
[ "$FAILED" -eq 0 ]
