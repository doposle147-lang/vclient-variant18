#include "interface.h"
#include "communicator.h"
#include "config_reader.h"
#include "data_reader.h"
#include "data_writer.h"
#include "errors.h"
#include <cstdlib>
#include <iostream>

// значения параметров по умолчанию
static const char* DEFAULT_PORT = "33333";
static const char* DEFAULT_CONFIG = "~/.config/vclient.conf";

// заменяет ~ в начале пути на домашний каталог
static std::string expandHome(const std::string& path)
{
    if (path.empty() || path[0] != '~')
        return path;
    if (path.size() > 1 && path[1] != '/')
        return path;
    const char* home = getenv("HOME");
    if (home == nullptr)
        throw InterfaceError("expandHome(path=" + path + ")",
                             "переменная окружения HOME не задана, нельзя определить домашний каталог");
    return std::string(home) + path.substr(1);
}

// ширина справки: Boost считает длину строки в байтах, а русская буква занимает
// 2 байта в UTF-8, поэтому при ширине по умолчанию (80) строки переносились
// посреди буквы и в справке появлялись "битые" символы
static const unsigned HELP_LINE_LENGTH = 160;
static const unsigned HELP_DESCRIPTION_LENGTH = 100;

UserInterface::UserInterface() : desc("Параметры", HELP_LINE_LENGTH, HELP_DESCRIPTION_LENGTH)
{
    // добавление параметров в парсер командной строки
    desc.add_options()
        ("help,h", "Показать справку")
        ("address,a", po::value<std::string>()->required(),
            "IPv4-адрес сервера (обязательный)")
        ("port,p", po::value<std::string>()->default_value(DEFAULT_PORT),
            "Порт сервера")
        ("input,i", po::value<std::string>()->required(),
            "Файл с исходными данными (обязательный)")
        ("output,o", po::value<std::string>()->required(),
            "Файл для сохранения результатов (обязательный)")
        ("config,c", po::value<std::string>()->default_value(DEFAULT_CONFIG),
            "Файл с логином и паролем (строка LOGIN:PASSWORD)");
}

ProgramOptions UserInterface::parseCommandLine(int argc, const char** argv)
{
    ProgramOptions opt;

    // запуск без параметров - выдаем справку
    if (argc < 2) {
        opt.help = true;
        return opt;
    }

    // командная строка целиком, для сообщений об ошибках
    std::string cmdLine;
    for (int i = 1; i < argc; i++)
        cmdLine += std::string(argv[i]) + " ";
    std::string where = "UserInterface::parseCommandLine(параметры: " + cmdLine + ")";

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        // --help обрабатываем до notify, иначе будет ошибка об обязательных параметрах
        if (vm.count("help")) {
            opt.help = true;
            return opt;
        }
        po::notify(vm);
    } catch (const po::error& e) {
        throw InterfaceError(where, e.what());
    }

    opt.address = vm["address"].as<std::string>();
    opt.inputFile = vm["input"].as<std::string>();
    opt.outputFile = vm["output"].as<std::string>();
    opt.configFile = expandHome(vm["config"].as<std::string>());

    // порт проверяем вручную: Boost при разборе unsigned принимает отрицательные
    // числа и молча превращает, например, -1 в 4294967295
    std::string portStr = vm["port"].as<std::string>();
    bool digitsOnly = !portStr.empty() && portStr.size() <= 5;
    for (char c : portStr) {
        if (c < '0' || c > '9')
            digitsOnly = false;
    }
    if (!digitsOnly)
        throw InterfaceError(where, "порт '" + portStr + "' должен быть целым числом от 1 до 65535");
    unsigned long port = std::stoul(portStr);
    if (port < 1 || port > 65535)
        throw InterfaceError(where, "порт " + portStr + " вне диапазона 1-65535");
    opt.port = static_cast<uint16_t>(port);

    return opt;
}

void UserInterface::printHelp()
{
    std::cout << "Использование: vclient -a АДРЕС -i ВХОДНОЙ_ФАЙЛ -o ВЫХОДНОЙ_ФАЙЛ "
              << "[-p ПОРТ] [-c КОНФИГ]\n\n";
    std::cout << desc << std::endl;
}

int UserInterface::run(int argc, const char** argv)
{
    options = parseCommandLine(argc, argv);
    if (options.help) {
        printHelp();
        return 0;
    }

    // логин и пароль
    ConfigReader configReader;
    Credentials credentials = configReader.read(options.configFile);

    // исходные данные
    DataReader dataReader;
    VectorData vectors = dataReader.read(options.inputFile);

    // сеанс обмена с сервером
    std::vector<uint64_t> results;
    {
        Communicator communicator(options.address, options.port, credentials, vectors);
        communicator.connect();
        communicator.authenticate();
        results = communicator.exchange();
        communicator.close();
    }

    // результаты
    DataWriter dataWriter;
    dataWriter.write(options.outputFile, results);

    std::cout << "Обработано векторов: " << results.size()
              << ", результаты записаны в файл " << options.outputFile << std::endl;
    return 0;
}
