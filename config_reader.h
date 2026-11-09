// Модуль чтения логина и пароля из файла конфигурации
#pragma once
#include <string>

// Учетные данные клиента
struct Credentials {
    std::string login;     // идентификатор клиента
    std::string password;  // пароль клиента
};

// Чтение логина и пароля из файла
// В файле должна быть строка вида LOGIN:PASSWORD
class ConfigReader {
public:
    // Прочитать учетные данные
    Credentials read(const std::string& path);
};
