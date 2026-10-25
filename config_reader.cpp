#include "config_reader.h"
#include "errors.h"
#include <fstream>

Credentials ConfigReader::read(const std::string& path)
{
    // пароль в сообщения об ошибках не выводим
    std::string where = "ConfigReader::read(path=" + path + ")";

    std::ifstream file(path);
    if (!file.is_open())
        throw FileError(where, "не удалось открыть файл с логином и паролем");

    // ищем первую непустую строку
    std::string line;
    bool found = false;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (!line.empty()) {
            found = true;
            break;
        }
    }
    if (!found)
        throw DataFormatError(where, "файл пустой, ожидалась строка вида LOGIN:PASSWORD");

    size_t pos = line.find(':');
    if (pos == std::string::npos)
        throw DataFormatError(where, "в строке нет разделителя ':', ожидался формат LOGIN:PASSWORD");

    Credentials cred;
    cred.login = line.substr(0, pos);
    cred.password = line.substr(pos + 1);
    if (cred.login.empty())
        throw DataFormatError(where, "логин пустой");
    if (cred.password.empty())
        throw DataFormatError(where, "пароль пустой (логин: " + cred.login + ")");
    return cred;
}
