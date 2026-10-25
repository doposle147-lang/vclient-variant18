/**
 * @file interface.h
 * @brief Модуль пользовательского интерфейса (командная строка)
 */
#pragma once
#include <boost/program_options.hpp>
#include <cstdint>
#include <string>

namespace po = boost::program_options;

/// Параметры, полученные из командной строки
struct ProgramOptions {
    bool help = false;        ///< нужно ли выдать справку
    std::string address;      ///< адрес сервера
    uint16_t port = 33333;    ///< порт сервера
    std::string inputFile;    ///< файл с исходными данными
    std::string outputFile;   ///< файл для результатов
    std::string configFile;   ///< файл с логином и паролем
};

/**
 * @brief Пользовательский интерфейс программы
 * @details Разбирает командную строку и управляет работой остальных классов:
 * ConfigReader, DataReader, Communicator, DataWriter.
 */
class UserInterface {
private:
    po::options_description desc;  ///< описание параметров командной строки
    ProgramOptions options;        ///< полученные параметры

    /**
     * @brief Разобрать параметры командной строки
     * @details Если параметров нет или указан -h, в результате help = true.
     * @throw InterfaceError при неверных параметрах
     */
    ProgramOptions parseCommandLine(int argc, const char** argv);

    /// Вывести справку
    void printHelp();
public:
    UserInterface();

    /**
     * @brief Выполнить программу
     * @param argc количество параметров командной строки
     * @param argv параметры командной строки
     * @return код возврата (0 - успешно)
     * @throw ProgramError при ошибках
     */
    int run(int argc, const char** argv);
};
