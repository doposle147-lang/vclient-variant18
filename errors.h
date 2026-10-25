/**
 * @file errors.h
 * @brief Классы ошибок программы
 */
#pragma once
#include <exception>
#include <string>

/**
 * @brief Базовый класс для всех ошибок программы
 * @details В тексте ошибки указывается ее тип, место возникновения
 * (функция и значения параметров) и причина.
 */
class ProgramError : public std::exception {
protected:
    std::string message;   ///< причина ошибки
    std::string context;   ///< функция и значения параметров, при которых возникла ошибка
    std::string fullText;  ///< итоговый текст сообщения
public:
    /**
     * @param type тип ошибки
     * @param ctx функция и значения параметров
     * @param msg причина ошибки
     */
    ProgramError(const std::string& type, const std::string& ctx, const std::string& msg)
        : message(msg), context(ctx)
    {
        fullText = "Тип ошибки: " + type + "\n"
                   "Где произошла: " + context + "\n"
                   "Причина: " + message;
    }
    const char* what() const noexcept override { return fullText.c_str(); }
};

/// Ошибка параметров командной строки
class InterfaceError : public ProgramError {
public:
    InterfaceError(const std::string& ctx, const std::string& msg)
        : ProgramError("InterfaceError (ошибка интерфейса)", ctx, msg) {}
};

/// Ошибка работы с файлом (открытие, чтение, запись)
class FileError : public ProgramError {
public:
    FileError(const std::string& ctx, const std::string& msg)
        : ProgramError("FileError (ошибка работы с файлом)", ctx, msg) {}
};

/// Ошибка формата данных в файле
class DataFormatError : public ProgramError {
public:
    DataFormatError(const std::string& ctx, const std::string& msg)
        : ProgramError("DataFormatError (неверный формат данных)", ctx, msg) {}
};

/// Ошибка сетевого взаимодействия
class NetworkError : public ProgramError {
public:
    NetworkError(const std::string& ctx, const std::string& msg)
        : ProgramError("NetworkError (сетевая ошибка)", ctx, msg) {}
};

/// Ошибка аутентификации на сервере
class AuthError : public ProgramError {
public:
    AuthError(const std::string& ctx, const std::string& msg)
        : ProgramError("AuthError (ошибка аутентификации)", ctx, msg) {}
};
