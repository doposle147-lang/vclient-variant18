/**
 * @file communicator.h
 * @brief Модуль сетевого взаимодействия с сервером
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "config_reader.h"
#include "data_reader.h"

/**
 * @brief Сеанс обмена данными с сервером по протоколу TCP
 * @details Порядок работы: connect(), authenticate(), exchange(), close().
 * Аутентификация выполняется в текстовом виде, обмен данными - в двоичном.
 */
class Communicator {
private:
    std::string address;        ///< IPv4-адрес сервера
    uint16_t port;              ///< порт сервера
    Credentials credentials;    ///< логин и пароль клиента
    const VectorData& vectors;  ///< векторы для передачи на сервер
    int sock;                   ///< идентификатор сокета (-1 если соединения нет)
    std::string stage;          ///< текущий этап работы, используется в сообщениях об ошибках

    std::string where() const;
    void checkConnected();
    void sendAll(const void* data, size_t size);
    void recvAll(void* data, size_t size);
    std::string recvText(size_t maxSize);
public:
    Communicator() = delete;
    /**
     * @param [in] serverAddress IPv4-адрес сервера
     * @param [in] serverPort порт сервера
     * @param [in] cred логин и пароль клиента
     * @param [in] data векторы для передачи (объект должен существовать, пока жив Communicator)
     */
    Communicator(const std::string& serverAddress, uint16_t serverPort,
                 const Credentials& cred, const VectorData& data);
    ~Communicator();
    Communicator(const Communicator&) = delete;
    Communicator& operator=(const Communicator&) = delete;

    /**
     * @brief Установить соединение с сервером
     * @throw NetworkError при ошибках создания сокета, подготовки адресов и соединения
     */
    void connect();

    /**
     * @brief Выполнить аутентификацию на сервере
     * @details Клиент отправляет LOGIN, получает SALT16, отправляет
     * HASH = SHA-1(SALT16 || PASSWORD) и ждет ответа OK.
     * @throw AuthError если сервер ответил ERR или ответ неверный
     * @throw NetworkError при сетевых ошибках
     */
    void authenticate();

    /**
     * @brief Передать векторы на сервер и получить результаты вычислений
     * @return результаты по каждому вектору
     * @throw NetworkError при сетевых ошибках
     */
    std::vector<uint64_t> exchange();

    /**
     * @brief Завершить соединение
     * @throw NetworkError если сокет не удалось закрыть
     */
    void close();
};
