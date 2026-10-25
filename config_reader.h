/**
 * @file config_reader.h
 * @brief Модуль чтения логина и пароля из файла конфигурации
 */
#pragma once
#include <string>

/// Учетные данные клиента
struct Credentials {
    std::string login;     ///< идентификатор клиента
    std::string password;  ///< пароль клиента
};

/**
 * @brief Чтение логина и пароля из файла
 * @details В файле должна быть строка вида LOGIN:PASSWORD
 */
class ConfigReader {
public:
    /**
     * @brief Прочитать учетные данные
     * @param [in] path имя файла конфигурации
     * @return логин и пароль
     * @throw FileError если файл не удалось открыть
     * @throw DataFormatError если формат строки неверный
     */
    Credentials read(const std::string& path);
};
