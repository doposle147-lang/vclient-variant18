#include "authenticator.h"
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>

namespace CPP = CryptoPP;

std::string Authenticator::makeHash(const std::string& salt16, const std::string& password)
{
    std::string hash16;  // сюда запишется хеш в виде шестнадцатеричной строки

    // соль + пароль -> хеш -> шестнадцатеричная строка
    CPP::StringSource src(salt16 + password, true,
        new CPP::HashFilter(hashFunction,
            new CPP::HexEncoder(
                new CPP::StringSink(hash16))));
    return hash16;
}
