// Модуль пользовательского интерфейса (командная строка)
#pragma once
#include <boost/program_options.hpp>
#include <cstdint>
#include <string>

namespace po = boost::program_options;

// Параметры, полученные из командной строки
struct ProgramOptions {
    bool help = false;        // нужно ли выдать справку
    std::string address;      // адрес сервера
    uint16_t port = 33333;    // порт сервера
    std::string inputFile;    // файл с исходными данными
    std::string outputFile;   // файл для результатов
    std::string configFile;   // файл с логином и паролем
};

// Пользовательский интерфейс программы
// Разбирает командную строку и управляет работой остальных классов:
// ConfigReader, DataReader, Communicator, DataWriter.
class UserInterface {
private:
    po::options_description desc;  // описание параметров командной строки
    ProgramOptions options;        // полученные параметры

    // Вывести справку
    void printHelp();
public:
    UserInterface();

    // Разобрать параметры командной строки
    // Если параметров нет или указан -h, в результате help = true.
    // исключение: InterfaceError при неверных параметрах
    // (метод публичный, чтобы его можно было проверить модульными тестами)
    ProgramOptions parseCommandLine(int argc, const char** argv);

    // Текст справки (используется в printHelp и в модульных тестах)
    std::string getHelpText() const;

    // Выполнить программу
    int run(int argc, const char** argv);
};
