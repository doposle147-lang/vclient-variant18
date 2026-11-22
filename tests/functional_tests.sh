#!/bin/bash
# Функциональное (приемочное) тестирование программы vclient
#
# Запуск из корня проекта (после make):
#     bash tests/functional_tests.sh [путь к тестовому серверу]
# По умолчанию используется сервер ~/timp/server/server
# (https://gitlab.com/winwood/timp, каталог server).
#
# Скрипт сам запускает тестовый сервер с параметрами варианта 18
# (-T uint64_t -H SHA1 -S server), готовит входные файлы, выполняет тесты
# и записывает вывод программы по каждому тесту в файл functional_tests.log.

export LC_ALL=C.UTF-8          # одинаковый язык системных сообщений и подсчет символов
PROJECT="$(pwd)"
CLIENT="$PROJECT/vclient"
SERVER="${1:-$HOME/timp/server/server}"
LOG="$PROJECT/functional_tests.log"
WORK="$(mktemp -d)"            # временный каталог для файлов тестов
PORT_NO_SERVER=33334           # порт, на котором сервер не запущен
PORT_FAKE=33335                # порт для имитации неисправного сервера

PASSED=0
FAILED=0

if [ ! -x "$CLIENT" ]; then
    echo "Нет программы $CLIENT, сначала выполните make"
    exit 1
fi
if [ ! -x "$SERVER" ]; then
    echo "Нет тестового сервера $SERVER"
    echo "Укажите путь: bash tests/functional_tests.sh ПУТЬ_К_СЕРВЕРУ"
    exit 1
fi

# запуск программы-клиента
vclient() {
    "$CLIENT" "$@"
}

# запуск клиента и вывод содержимого файла результатов (имя после -o)
vclient_and_results() {
    "$CLIENT" "$@" || return $?
    local out=""
    while [ $# -gt 0 ]; do
        { [ "$1" = "-o" ] || [ "$1" = "--output" ]; } && out="$2"
        shift
    done
    echo "Количество результатов: $(od -A n -t u4 -N4 "$out" | xargs)"
    echo "Результаты: $(od -A n -t u8 -j4 "$out" | xargs)"
}

# то же, но файл с логином и паролем берется по умолчанию
# (~/.config/vclient.conf, домашний каталог подменяется на временный)
vclient_default_conf() {
    HOME="$WORK/home" vclient_and_results "$@"
}

# имитация неисправного сервера (режимы: close, garbage, drop)
#   close   - закрыть соединение сразу после получения LOGIN
#   garbage - вместо SALT16 передать строку HELLO
#   drop    - пройти аутентификацию и закрыть соединение при обмене данными
fake_server() {
    python3 - "$1" "$PORT_FAKE" <<'PY' &
import socket, sys
mode, port = sys.argv[1], int(sys.argv[2])
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port))
s.listen(1)
c, _ = s.accept()
c.recv(1024)                       # LOGIN
if mode == "garbage":
    c.send(b"HELLO")
elif mode == "drop":
    c.send(b"0123456789ABCDEF")    # SALT16
    c.recv(1024)                   # HASH
    c.send(b"OK")
    c.recv(4)                      # количество векторов
c.close()
s.close()
PY
    sleep 1
}

# выполнение одного теста
#   run_test НОМЕР "НАЗВАНИЕ" ОЖИДАЕМЫЙ_КОД_ВОЗВРАТА "ОЖИДАЕМЫЙ_ТЕКСТ" КОМАНДА...
run_test() {
    local id="$1" name="$2" code="$3" text="$4"
    shift 4
    local out rc result
    out=$("$@" 2>&1)
    rc=$?
    if [ "$rc" -eq "$code" ] && grep -qF -- "$text" <<< "$out"; then
        result="ПРОЙДЕН"
        PASSED=$((PASSED + 1))
    else
        result="НЕ ПРОЙДЕН"
        FAILED=$((FAILED + 1))
    fi
    {
        echo "=========================================================="
        echo "Тест $id: $name"
        echo "Команда: $(sed -E 's/^vclient_(and_results|default_conf)/vclient/' <<< "$*")"
        echo "Код возврата: $rc (ожидался $code)"
        echo "Вывод программы:"
        echo "$out"
        echo "Итог: $result"
    } >> "$LOG"
    local dots=$((62 - ${#name}))
    [ $dots -lt 3 ] && dots=3
    printf "%-5s %s %s %s\n" "$id" "$name" "$(printf '%*s' $dots '' | tr ' ' '.')" "$result"
}

# ---------------------------------------------------------------------------
# подготовка
# ---------------------------------------------------------------------------
: > "$LOG"
echo "Функциональное тестирование vclient, $(date '+%d.%m.%Y %H:%M')" >> "$LOG"
cd "$WORK" || exit 1

"$SERVER" -T uint64_t -H SHA1 -S server > "$WORK/server.out" 2>&1 &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null; cd "$PROJECT"; rm -rf "$WORK"' EXIT
sleep 1

echo "user:P@ssW0rd" > vclient.conf
echo "admin:P@ssW0rd" > bad_login.conf
echo "user:wrong" > bad_pass.conf
echo "userP@ssW0rd" > no_colon.conf
printf '3\n3 1 2 3\n2 10 20\n1 42\n' > data.txt

# ---------------------------------------------------------------------------
echo "Сценарий 1. Справка"
# ---------------------------------------------------------------------------
run_test 1.1 "Запуск без параметров" 0 "Использование: vclient" vclient
run_test 1.2 "Параметр -h" 0 "Использование: vclient" vclient -h
run_test 1.3 "Параметр --help" 0 "Использование: vclient" vclient --help
run_test 1.4 "Параметр -h вместе с другими параметрами" 0 "Использование: vclient" \
    vclient -a 127.0.0.1 -i data.txt -h

# ---------------------------------------------------------------------------
echo "Сценарий 2. Неверные параметры"
# ---------------------------------------------------------------------------
run_test 2.1 "Нет адреса сервера" 1 "'--address' is required" \
    vclient -i data.txt -o out.bin -c vclient.conf
run_test 2.2 "Нет файла исходных данных" 1 "'--input' is required" \
    vclient -a 127.0.0.1 -o out.bin -c vclient.conf
run_test 2.3 "Нет файла результатов" 1 "'--output' is required" \
    vclient -a 127.0.0.1 -i data.txt -c vclient.conf
run_test 2.4 "Неизвестный параметр" 1 "unrecognised option '-x'" \
    vclient -a 127.0.0.1 -i data.txt -o out.bin -x
run_test 2.5 "Параметр без значения" 1 "InterfaceError" \
    vclient -i data.txt -o out.bin -a
run_test 2.6 "Порт 0" 1 "вне диапазона 1-65535" \
    vclient -a 127.0.0.1 -p 0 -i data.txt -o out.bin
run_test 2.7 "Порт больше 65535" 1 "вне диапазона 1-65535" \
    vclient -a 127.0.0.1 -p 70000 -i data.txt -o out.bin
run_test 2.8 "Отрицательный порт" 1 "должен быть целым числом от 1 до 65535" \
    vclient -a 127.0.0.1 -p -1 -i data.txt -o out.bin
run_test 2.9 "Порт не число" 1 "должен быть целым числом от 1 до 65535" \
    vclient -a 127.0.0.1 -p abc -i data.txt -o out.bin

# ---------------------------------------------------------------------------
echo "Сценарий 3. Ошибки чтения и записи данных"
# ---------------------------------------------------------------------------
: > empty.txt
printf '1\n2 5 abc\n' > not_number.txt
printf '1\n2 5 -6\n' > negative.txt
printf '1\n1 18446744073709551616\n' > too_big.txt
printf '1\n3 1 2\n' > bad_size.txt
printf '3\n1 5\n1 6\n' > few_vectors.txt
printf '0\n' > zero_count.txt

run_test 3.1 "Нет файла исходных данных" 1 "не удалось открыть файл для чтения" \
    vclient -a 127.0.0.1 -i nofile.txt -o out.bin -c vclient.conf
run_test 3.2 "Пустой файл исходных данных" 1 "файл пустой, нет количества векторов" \
    vclient -a 127.0.0.1 -i empty.txt -o out.bin -c vclient.conf
run_test 3.3 "Значение не является числом" 1 "'abc' не является целым числом без знака" \
    vclient -a 127.0.0.1 -i not_number.txt -o out.bin -c vclient.conf
run_test 3.4 "Отрицательное значение" 1 "'-6' не является целым числом без знака" \
    vclient -a 127.0.0.1 -i negative.txt -o out.bin -c vclient.conf
run_test 3.5 "Значение больше максимального для uint64_t" 1 "не помещается в тип uint64_t" \
    vclient -a 127.0.0.1 -i too_big.txt -o out.bin -c vclient.conf
run_test 3.6 "Размер вектора не совпадает с числом значений" 1 "размер вектора 3, а значений в строке: 2" \
    vclient -a 127.0.0.1 -i bad_size.txt -o out.bin -c vclient.conf
run_test 3.7 "Векторов меньше, чем указано" 1 "ожидалось векторов: 3, а в файле только 2" \
    vclient -a 127.0.0.1 -i few_vectors.txt -o out.bin -c vclient.conf
run_test 3.8 "Количество векторов 0" 1 "количество векторов равно 0" \
    vclient -a 127.0.0.1 -i zero_count.txt -o out.bin -c vclient.conf
run_test 3.9 "Нет файла с логином и паролем" 1 "не удалось открыть файл с логином и паролем" \
    vclient -a 127.0.0.1 -i data.txt -o out.bin -c noconf.conf
run_test 3.10 "Неверный формат файла с логином и паролем" 1 "в строке нет разделителя ':'" \
    vclient -a 127.0.0.1 -i data.txt -o out.bin -c no_colon.conf
run_test 3.11 "Нельзя создать файл результатов" 1 "не удалось открыть файл для записи" \
    vclient -a 127.0.0.1 -i data.txt -o nodir/out.bin -c vclient.conf

# ---------------------------------------------------------------------------
echo "Сценарий 4. Ошибки соединения, аутентификации, приема и передачи"
# ---------------------------------------------------------------------------
run_test 4.1 "Сервер не запущен" 1 "не удалось установить соединение" \
    vclient -a 127.0.0.1 -p $PORT_NO_SERVER -i data.txt -o out.bin -c vclient.conf
run_test 4.2 "Некорректный адрес сервера" 1 "не является корректным IPv4-адресом" \
    vclient -a 127.0.0.256 -i data.txt -o out.bin -c vclient.conf
run_test 4.3 "Неверный логин" 1 "логин 'admin' не найден" \
    vclient -a 127.0.0.1 -i data.txt -o out.bin -c bad_login.conf
run_test 4.4 "Неверный пароль" 1 "пароль неверный" \
    vclient -a 127.0.0.1 -i data.txt -o out.bin -c bad_pass.conf
fake_server close
run_test 4.5 "Сервер разорвал соединение до передачи соли" 1 "сервер закрыл соединение, не передав ответ" \
    vclient -a 127.0.0.1 -p $PORT_FAKE -i data.txt -o out.bin -c vclient.conf
fake_server garbage
run_test 4.6 "Сервер передал некорректную соль" 1 "получено: 'HELLO'" \
    vclient -a 127.0.0.1 -p $PORT_FAKE -i data.txt -o out.bin -c vclient.conf
fake_server drop
run_test 4.7 "Сервер разорвал соединение при обмене данными" 1 "NetworkError" \
    vclient -a 127.0.0.1 -p $PORT_FAKE -i data.txt -o out.bin -c vclient.conf

# ---------------------------------------------------------------------------
echo "Сценарий 5. Работа программы при корректных входных данных"
# ---------------------------------------------------------------------------
printf '2\n2 18446744073709551615 5\n1 7\n' > overflow.txt
printf '1\n2 18446744073709551614 1\n' > max_sum.txt
printf '  2 \r\n\r\n\t3   1  2\t3 \r\n\r\n1 100\r\n' > spaces.txt
{ echo 1; echo "100 $(seq -s ' ' 1 100)"; } > big_vector.txt
mkdir -p home/.config && cp vclient.conf home/.config/vclient.conf
printf '1\n1 99\n' > one.txt

run_test 5.1 "Обработка трех векторов" 0 "Результаты: 6 30 42" \
    vclient_and_results -a 127.0.0.1 -i data.txt -o out.bin -c vclient.conf
run_test 5.2 "Переполнение суммы" 0 "Результаты: 18446744073709551615 7" \
    vclient_and_results -a 127.0.0.1 -i overflow.txt -o out.bin -c vclient.conf
run_test 5.3 "Максимальная сумма без переполнения" 0 "Результаты: 18446744073709551615" \
    vclient_and_results -a 127.0.0.1 -i max_sum.txt -o out.bin -c vclient.conf
run_test 5.4 "Длинные имена параметров и явный порт" 0 "Результаты: 6 30 42" \
    vclient_and_results --address 127.0.0.1 --port 33333 --input data.txt --output out.bin --config vclient.conf
run_test 5.5 "Файл конфигурации по умолчанию" 0 "Результаты: 6 30 42" \
    vclient_default_conf -a 127.0.0.1 -i data.txt -o out.bin
run_test 5.6 "Лишние пробелы, пустые строки, окончания строк Windows" 0 "Результаты: 6 100" \
    vclient_and_results -a 127.0.0.1 -i spaces.txt -o out.bin -c vclient.conf
run_test 5.7 "Вектор из 100 значений" 0 "Результаты: 5050" \
    vclient_and_results -a 127.0.0.1 -i big_vector.txt -o out.bin -c vclient.conf
"$CLIENT" -a 127.0.0.1 -i data.txt -o out.bin -c vclient.conf > /dev/null   # в out.bin 3 результата
run_test 5.8 "Перезапись существующего файла результатов" 0 "Результаты: 99" \
    vclient_and_results -a 127.0.0.1 -i one.txt -o out.bin -c vclient.conf

# ---------------------------------------------------------------------------
echo "=========================================================="
echo "Пройдено тестов: $PASSED из $((PASSED + FAILED))"
{
    echo "=========================================================="
    echo "Пройдено тестов: $PASSED из $((PASSED + FAILED))"
} >> "$LOG"
echo "Подробный вывод программы по каждому тесту: functional_tests.log"
[ "$FAILED" -eq 0 ]
