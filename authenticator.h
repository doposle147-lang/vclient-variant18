// Модуль формирования хеша для аутентификации
#pragma once
#include <string>
#include <cryptopp/sha.h>

// Формирование хеша SHA-1 от соли и пароля (библиотека Crypto++)
class Authenticator {
private:
    CryptoPP::SHA1 hashFunction;  // хеш-функция SHA-1
public:
    // Вычислить HASH = SHA-1(SALT16 || PASSWORD)
    std::string makeHash(const std::string& salt16, const std::string& password);
};
