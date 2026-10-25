#include "communicator.h"
#include "authenticator.h"
#include "errors.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <memory>

// таймаут ожидания данных от сервера, секунд
static const int RECV_TIMEOUT = 180;

// текст системной ошибки по ее коду
static std::string errText(int err)
{
    if (err == EAGAIN || err == EWOULDBLOCK)
        return "истек таймаут ожидания данных от сервера (" + std::to_string(RECV_TIMEOUT) + " с)";
    return std::strerror(err);
}

// проверка, что строка является солью: 16 шестнадцатеричных цифр
static bool isSalt(const std::string& s)
{
    if (s.size() != 16)
        return false;
    for (char c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return false;
    }
    return true;
}

Communicator::Communicator(const std::string& serverAddress, uint16_t serverPort,
                           const Credentials& cred, const VectorData& data)
    : address(serverAddress), port(serverPort), credentials(cred), vectors(data), sock(-1)
{
}

Communicator::~Communicator()
{
    // в деструкторе исключения не возбуждаем
    if (sock != -1)
        ::close(sock);
}

// место ошибки: этап и параметры соединения (пароль не выводится)
std::string Communicator::where() const
{
    return stage + " (address=" + address + ", port=" + std::to_string(port)
           + ", login=" + credentials.login + ")";
}

void Communicator::checkConnected()
{
    if (sock == -1)
        throw NetworkError(where(), "соединение с сервером не установлено");
}

// передача всех данных (send может передать меньше, чем просили)
void Communicator::sendAll(const void* data, size_t size)
{
    const char* ptr = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < size) {
        // MSG_NOSIGNAL - чтобы при обрыве соединения программа не завершилась по сигналу SIGPIPE
        ssize_t rc = send(sock, ptr + sent, size - sent, MSG_NOSIGNAL);
        if (rc == -1) {
            int err = errno;
            throw NetworkError(where(), "ошибка при передаче данных (передано " + std::to_string(sent)
                               + " из " + std::to_string(size) + " байт): " + errText(err));
        }
        sent += rc;
    }
}

// прием ровно size байт
void Communicator::recvAll(void* data, size_t size)
{
    char* ptr = static_cast<char*>(data);
    size_t received = 0;
    while (received < size) {
        ssize_t rc = recv(sock, ptr + received, size - received, 0);
        if (rc == -1) {
            int err = errno;
            throw NetworkError(where(), "ошибка при приеме данных (принято " + std::to_string(received)
                               + " из " + std::to_string(size) + " байт): " + errText(err));
        }
        if (rc == 0)
            throw NetworkError(where(), "сервер закрыл соединение (принято "
                               + std::to_string(received) + " из " + std::to_string(size) + " байт)");
        received += rc;
    }
}

// прием короткого текстового сообщения (SALT16, OK, ERR)
std::string Communicator::recvText(size_t maxSize)
{
    std::unique_ptr<char[]> buf(new char[maxSize]);
    ssize_t rc = recv(sock, buf.get(), maxSize, 0);
    if (rc == -1) {
        int err = errno;
        throw NetworkError(where(), "ошибка при приеме ответа сервера: " + errText(err));
    }
    if (rc == 0)
        throw NetworkError(where(), "сервер закрыл соединение, не передав ответ");
    return std::string(buf.get(), rc);
}

void Communicator::connect()
{
    // создаем сокет
    stage = "Communicator::connect: создание сокета";
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        int err = errno;
        throw NetworkError(where(), std::string("не удалось создать сокет: ") + std::strerror(err));
    }

    // таймаут для блокирующего приема
    stage = "Communicator::connect: установка таймаута";
    timeval timeout {RECV_TIMEOUT, 0};
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout) == -1) {
        int err = errno;
        throw NetworkError(where(), std::string("не удалось установить таймаут: ") + std::strerror(err));
    }

    // свой адрес: порт и адрес выбираются системой
    stage = "Communicator::connect: привязка своего адреса";
    std::unique_ptr<sockaddr_in> self_addr(new sockaddr_in());
    self_addr->sin_family = AF_INET;
    self_addr->sin_port = 0;
    self_addr->sin_addr.s_addr = 0;
    int rc = ::bind(sock, reinterpret_cast<const sockaddr*>(self_addr.get()), sizeof(sockaddr_in));
    if (rc == -1) {
        int err = errno;
        throw NetworkError(where(), std::string("не удалось привязать адрес к сокету: ") + std::strerror(err));
    }

    // адрес сервера
    stage = "Communicator::connect: подготовка адреса сервера";
    std::unique_ptr<sockaddr_in> serv_addr(new sockaddr_in());
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &serv_addr->sin_addr) != 1)
        throw NetworkError(where(), "'" + address + "' не является корректным IPv4-адресом");

    // соединяемся
    stage = "Communicator::connect: установка соединения";
    rc = ::connect(sock, reinterpret_cast<const sockaddr*>(serv_addr.get()), sizeof(sockaddr_in));
    if (rc == -1) {
        int err = errno;
        throw NetworkError(where(), std::string("не удалось установить соединение: ") + std::strerror(err));
    }
}

void Communicator::authenticate()
{
    stage = "Communicator::authenticate";
    checkConnected();

    // 1. передаем логин
    stage = "Communicator::authenticate: передача LOGIN";
    sendAll(credentials.login.data(), credentials.login.size());

    // 2. получаем соль или ERR
    stage = "Communicator::authenticate: прием SALT16";
    std::string answer = recvText(64);
    if (answer == "ERR")
        throw AuthError(where(), "сервер ответил ERR: логин '" + credentials.login + "' не найден");
    if (!isSalt(answer))
        throw AuthError(where(), "вместо соли (16 шестнадцатеричных цифр) получено: '" + answer + "'");
    std::string salt16 = answer;

    // 3. считаем хеш и передаем его серверу
    Authenticator authenticator;
    std::string hash = authenticator.makeHash(salt16, credentials.password);
    stage = "Communicator::authenticate: передача HASH";
    sendAll(hash.data(), hash.size());

    // 4. ждем результат
    stage = "Communicator::authenticate: прием результата аутентификации";
    answer = recvText(64);
    if (answer == "OK")
        return;
    if (answer == "ERR")
        throw AuthError(where(), "сервер ответил ERR: пароль неверный (SALT16=" + salt16
                                 + ", HASH=" + hash + ")");
    throw AuthError(where(), "неожиданный ответ сервера вместо OK/ERR: '" + answer + "'");
}

std::vector<uint64_t> Communicator::exchange()
{
    stage = "Communicator::exchange";
    checkConnected();

    std::vector<uint64_t> results;

    // количество векторов
    stage = "Communicator::exchange: передача количества векторов";
    uint32_t count = vectors.size();
    sendAll(&count, sizeof count);

    for (size_t i = 0; i < vectors.size(); i++) {
        std::string num = "вектор " + std::to_string(i + 1) + " из " + std::to_string(vectors.size());

        // размер вектора
        stage = "Communicator::exchange: " + num + ", передача размера";
        uint32_t size = vectors[i].size();
        sendAll(&size, sizeof size);

        // значения вектора одним блоком
        stage = "Communicator::exchange: " + num + ", передача значений (размер " + std::to_string(size) + ")";
        if (size > 0)
            sendAll(vectors[i].data(), size * sizeof vectors[i][0]);

        // результат по вектору
        stage = "Communicator::exchange: " + num + ", прием результата";
        uint64_t result;
        recvAll(&result, sizeof result);
        results.push_back(result);
    }
    return results;
}

void Communicator::close()
{
    if (sock == -1)
        return;
    stage = "Communicator::close: закрытие сокета";
    int rc = ::close(sock);
    int err = errno;
    sock = -1;
    if (rc == -1)
        throw NetworkError(where(), std::string("не удалось закрыть сокет: ") + std::strerror(err));
}
