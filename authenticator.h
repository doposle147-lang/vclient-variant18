/**
 * @file authenticator.h
 * @brief Модуль формирования хеша для аутентификации
 */
#pragma once
#include <string>
#include <cryptopp/sha.h>

/**
 * @brief Формирование хеша SHA-1 от соли и пароля (библиотека Crypto++)
 */
class Authenticator {
private:
    CryptoPP::SHA1 hashFunction;  ///< хеш-функция SHA-1
public:
    /**
     * @brief Вычислить HASH = SHA-1(SALT16 || PASSWORD)
     * @param [in] salt16 соль в виде строки из 16 шестнадцатеричных цифр
     * @param [in] password пароль клиента
     * @return хеш в виде строки шестнадцатеричных цифр (0-9, A-F)
     */
    std::string makeHash(const std::string& salt16, const std::string& password);
};
