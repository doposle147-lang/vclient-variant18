// Модульные тесты программы vclient (библиотека UnitTest++)
// Запуск из корня проекта: make unit_tests && ./unit_tests
#include <UnitTest++/UnitTest++.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include "authenticator.h"
#include "communicator.h"
#include "config_reader.h"
#include "data_reader.h"
#include "data_writer.h"
#include "errors.h"
#include "interface.h"

// имена временных файлов, которые создают тесты
#define DATA_FILE "test_tmp_input.txt"
#define CONF_FILE "test_tmp_vclient.conf"
#define OUT_FILE  "test_tmp_out.bin"
#define NO_FILE   "test_tmp_notfound.txt"

// создать текстовый файл с заданным содержимым
static void makeFile(const char* name, const std::string& text)
{
    std::ofstream f(name, std::ios::binary);
    f << text;
}

// ---------------------------------------------------------------------------
// 1. Authenticator
// Ожидаемые значения получены командой:
//   echo -n 'СОЛЬПАРОЛЬ' | openssl sha1 | tr a-z A-Z
// ---------------------------------------------------------------------------
SUITE(AuthenticatorTest)
{
    TEST(ZeroSalt) {
        Authenticator a;
        CHECK_EQUAL("992A7F537CB6CB041303FA3205E7C784C364C0AE",
                    a.makeHash("0000000000000000", "password"));
    }
    TEST(TestServerPassword) {
        Authenticator a;
        CHECK_EQUAL("FE78BDB4183B677A55E711B17E9CB6CCC2AE7315",
                    a.makeHash("0123456789ABCDEF", "P@ssW0rd"));
    }
    TEST(SaltWithLeadingZeros) {
        Authenticator a;
        CHECK_EQUAL("7B98F0D28DBD04BB4DFC0959A8DC0FA37535C8D4",
                    a.makeHash("00000000000000FF", "P@ssW0rd"));
    }
    TEST(EmptyPassword) {
        Authenticator a;
        CHECK_EQUAL("CE27CB141098FEB00714E758646BE3E99C185B71",
                    a.makeHash("0123456789ABCDEF", ""));
    }
    TEST(HashFormat) {
        Authenticator a;
        std::string h = a.makeHash("0123456789ABCDEF", "P@ssW0rd");
        REQUIRE CHECK_EQUAL(40u, h.size());
        bool ok = true;
        for (char c : h)
            if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
                ok = false;
        CHECK(ok);
    }
    TEST(DifferentSalts) {
        Authenticator a;
        CHECK(a.makeHash("0000000000000001", "P@ssW0rd")
              != a.makeHash("0000000000000002", "P@ssW0rd"));
    }
    TEST(SameDataTwice) {
        Authenticator a;
        CHECK_EQUAL(a.makeHash("0123456789ABCDEF", "P@ssW0rd"),
                    a.makeHash("0123456789ABCDEF", "P@ssW0rd"));
    }
}

// ---------------------------------------------------------------------------
// 2. ConfigReader
// ---------------------------------------------------------------------------
struct ConfFile {
    ~ConfFile() { std::remove(CONF_FILE); }
};

SUITE(ConfigReaderTest)
{
    TEST_FIXTURE(ConfFile, CorrectFile) {
        makeFile(CONF_FILE, "user:P@ssW0rd\n");
        ConfigReader r;
        Credentials c = r.read(CONF_FILE);
        CHECK_EQUAL("user", c.login);
        CHECK_EQUAL("P@ssW0rd", c.password);
    }
    TEST_FIXTURE(ConfFile, ColonInPassword) {
        makeFile(CONF_FILE, "user:pa:ss\n");
        ConfigReader r;
        Credentials c = r.read(CONF_FILE);
        CHECK_EQUAL("user", c.login);
        CHECK_EQUAL("pa:ss", c.password);
    }
    TEST_FIXTURE(ConfFile, WindowsLineEnd) {
        makeFile(CONF_FILE, "user:P@ssW0rd\r\n");
        ConfigReader r;
        CHECK_EQUAL("P@ssW0rd", r.read(CONF_FILE).password);
    }
    TEST_FIXTURE(ConfFile, EmptyLinesBefore) {
        makeFile(CONF_FILE, "\n\nuser:P@ssW0rd\n");
        ConfigReader r;
        CHECK_EQUAL("user", r.read(CONF_FILE).login);
    }
    TEST(NoFile) {
        ConfigReader r;
        CHECK_THROW(r.read(NO_FILE), FileError);
    }
    TEST_FIXTURE(ConfFile, EmptyFile) {
        makeFile(CONF_FILE, "");
        ConfigReader r;
        CHECK_THROW(r.read(CONF_FILE), DataFormatError);
    }
    TEST_FIXTURE(ConfFile, NoColon) {
        makeFile(CONF_FILE, "userP@ssW0rd\n");
        ConfigReader r;
        CHECK_THROW(r.read(CONF_FILE), DataFormatError);
    }
    TEST_FIXTURE(ConfFile, EmptyLogin) {
        makeFile(CONF_FILE, ":P@ssW0rd\n");
        ConfigReader r;
        CHECK_THROW(r.read(CONF_FILE), DataFormatError);
    }
    TEST_FIXTURE(ConfFile, EmptyPassword) {
        makeFile(CONF_FILE, "user:\n");
        ConfigReader r;
        CHECK_THROW(r.read(CONF_FILE), DataFormatError);
    }
}

// ---------------------------------------------------------------------------
// 3. DataReader
// ---------------------------------------------------------------------------
struct DataFile {
    ~DataFile() { std::remove(DATA_FILE); }
};

SUITE(DataReaderTest)
{
    TEST_FIXTURE(DataFile, CorrectFile) {
        makeFile(DATA_FILE, "3\n3 1 2 3\n2 10 20\n1 42\n");
        DataReader r;
        VectorData expected {{1, 2, 3}, {10, 20}, {42}};
        CHECK(expected == r.read(DATA_FILE));
    }
    TEST_FIXTURE(DataFile, MaxValue) {
        makeFile(DATA_FILE, "1\n2 18446744073709551615 0\n");
        DataReader r;
        VectorData expected {{18446744073709551615ULL, 0}};
        CHECK(expected == r.read(DATA_FILE));
    }
    TEST_FIXTURE(DataFile, SpacesAndEmptyLines) {
        makeFile(DATA_FILE, "  2\n\n\t2   5\t6  \n\n1 7\n\n");
        DataReader r;
        VectorData expected {{5, 6}, {7}};
        CHECK(expected == r.read(DATA_FILE));
    }
    TEST_FIXTURE(DataFile, WindowsLineEnd) {
        makeFile(DATA_FILE, "1\r\n2 5 6\r\n");
        DataReader r;
        VectorData expected {{5, 6}};
        CHECK(expected == r.read(DATA_FILE));
    }
    TEST(NoFile) {
        DataReader r;
        CHECK_THROW(r.read(NO_FILE), FileError);
    }
    TEST_FIXTURE(DataFile, EmptyFile) {
        makeFile(DATA_FILE, "");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, NotANumber) {
        makeFile(DATA_FILE, "1\n2 5 abc\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, NegativeNumber) {
        makeFile(DATA_FILE, "1\n2 5 -6\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, TooBigNumber) {
        makeFile(DATA_FILE, "1\n1 18446744073709551616\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, TwoNumbersInFirstLine) {
        makeFile(DATA_FILE, "1 2\n1 5\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, ZeroCount) {
        makeFile(DATA_FILE, "0\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, ZeroSize) {
        makeFile(DATA_FILE, "1\n0\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, SizeMismatch) {
        makeFile(DATA_FILE, "1\n3 1 2\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, NotEnoughVectors) {
        makeFile(DATA_FILE, "3\n1 5\n1 6\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
    TEST_FIXTURE(DataFile, ExtraData) {
        makeFile(DATA_FILE, "1\n1 5\n1 6\n");
        DataReader r;
        CHECK_THROW(r.read(DATA_FILE), DataFormatError);
    }
}

// ---------------------------------------------------------------------------
// 4. DataWriter
// Данные читаются из файла прямо в тесте, после записи
// ---------------------------------------------------------------------------
struct OutFile {
    ~OutFile() { std::remove(OUT_FILE); }
};

// прочитать файл результатов: количество и значения
static bool readResults(uint32_t& count, std::vector<uint64_t>& values)
{
    std::ifstream f(OUT_FILE, std::ios::binary);
    if (!f.read(reinterpret_cast<char*>(&count), sizeof count))
        return false;
    values.clear();
    uint64_t v;
    while (f.read(reinterpret_cast<char*>(&v), sizeof v))
        values.push_back(v);
    return true;
}

// размер файла в байтах
static long fileSize(const char* name)
{
    std::ifstream f(name, std::ios::binary | std::ios::ate);
    return static_cast<long>(f.tellg());
}

SUITE(DataWriterTest)
{
    TEST_FIXTURE(OutFile, ThreeResults) {
        std::vector<uint64_t> written {6, 18446744073709551615ULL, 42};
        DataWriter w;
        w.write(OUT_FILE, written);
        uint32_t count = 0;
        std::vector<uint64_t> readed;
        REQUIRE CHECK(readResults(count, readed));
        CHECK_EQUAL(3u, count);
        CHECK(written == readed);
    }
    TEST_FIXTURE(OutFile, FileSize) {
        DataWriter w;
        w.write(OUT_FILE, {1, 2, 3, 4, 5});
        CHECK_EQUAL(4 + 5 * 8, fileSize(OUT_FILE));
    }
    TEST_FIXTURE(OutFile, NoResults) {
        DataWriter w;
        w.write(OUT_FILE, {});
        uint32_t count = 1;
        std::vector<uint64_t> readed;
        REQUIRE CHECK(readResults(count, readed));
        CHECK_EQUAL(0u, count);
        CHECK_EQUAL(4, fileSize(OUT_FILE));
    }
    TEST_FIXTURE(OutFile, Overwrite) {
        DataWriter w;
        w.write(OUT_FILE, {1, 2, 3, 4, 5});
        w.write(OUT_FILE, {7});
        uint32_t count = 0;
        std::vector<uint64_t> readed;
        REQUIRE CHECK(readResults(count, readed));
        CHECK_EQUAL(1u, count);
        CHECK(std::vector<uint64_t>{7} == readed);
    }
    TEST(BadPath) {
        DataWriter w;
        CHECK_THROW(w.write("test_tmp_no_dir/out.bin", {1}), FileError);
    }
}

// ---------------------------------------------------------------------------
// 5. UserInterface
// argv формируется как массив строк, последний указатель нулевой
// ---------------------------------------------------------------------------
SUITE(UserInterfaceTest)
{
    TEST(NoParams) {
        UserInterface ui;
        const char* argv[] = {"vclient", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK(ui.parseCommandLine(argc, argv).help);
    }
    TEST(ShortHelp) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-h", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK(ui.parseCommandLine(argc, argv).help);
    }
    TEST(LongHelp) {
        UserInterface ui;
        const char* argv[] = {"vclient", "--help", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK(ui.parseCommandLine(argc, argv).help);
    }
    TEST(HelpWithOtherParams) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-h", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK(ui.parseCommandLine(argc, argv).help);
    }
    TEST(RequiredParamsAndDefaults) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", "-o", "out.bin", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        ProgramOptions opt = ui.parseCommandLine(argc, argv);
        CHECK(!opt.help);
        CHECK_EQUAL("127.0.0.1", opt.address);
        CHECK_EQUAL("in.txt", opt.inputFile);
        CHECK_EQUAL("out.bin", opt.outputFile);
        CHECK_EQUAL(33333, opt.port);
        const char* home = getenv("HOME");
        REQUIRE CHECK(home != nullptr);
        CHECK_EQUAL(std::string(home) + "/.config/vclient.conf", opt.configFile);
    }
    TEST(AllParamsLong) {
        UserInterface ui;
        const char* argv[] = {"vclient", "--address", "10.0.0.5", "--port", "44444",
                              "--input", "a.txt", "--output", "b.bin", "--config", "my.conf", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        ProgramOptions opt = ui.parseCommandLine(argc, argv);
        CHECK_EQUAL("10.0.0.5", opt.address);
        CHECK_EQUAL(44444, opt.port);
        CHECK_EQUAL("a.txt", opt.inputFile);
        CHECK_EQUAL("b.bin", opt.outputFile);
        CHECK_EQUAL("my.conf", opt.configFile);
    }
    TEST(ParamsInOtherOrder) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-o", "b.bin", "-c", "my.conf", "-i", "a.txt",
                              "-p", "65535", "-a", "10.0.0.5", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        ProgramOptions opt = ui.parseCommandLine(argc, argv);
        CHECK_EQUAL("10.0.0.5", opt.address);
        CHECK_EQUAL(65535, opt.port);
        CHECK_EQUAL("a.txt", opt.inputFile);
        CHECK_EQUAL("b.bin", opt.outputFile);
        CHECK_EQUAL("my.conf", opt.configFile);
    }
    TEST(NoAddress) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-i", "in.txt", "-o", "out.bin", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(NoInput) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-o", "out.bin", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(NoOutput) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(UnknownParam) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", "-o", "out.bin",
                              "--wrong", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(ParamWithoutValue) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-i", "in.txt", "-o", "out.bin", "-a", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(ZeroPort) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", "-o", "out.bin",
                              "-p", "0", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(TooBigPort) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", "-o", "out.bin",
                              "-p", "70000", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(NegativePort) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", "-o", "out.bin",
                              "-p", "-1", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(PortNotANumber) {
        UserInterface ui;
        const char* argv[] = {"vclient", "-a", "127.0.0.1", "-i", "in.txt", "-o", "out.bin",
                              "-p", "abc", nullptr};
        int argc = sizeof argv / sizeof nullptr - 1;
        CHECK_THROW(ui.parseCommandLine(argc, argv), InterfaceError);
    }
    TEST(HelpText) {
        UserInterface ui;
        std::string text = ui.getHelpText();
        CHECK(!text.empty());
        CHECK(text.find("--address") != std::string::npos);
        CHECK(text.find("--input") != std::string::npos);
        CHECK(text.find("--output") != std::string::npos);
    }
}

// ---------------------------------------------------------------------------
// 6. Communicator
// Обмен с сервером проверяется при функциональном тестировании,
// здесь проверяются ошибки соединения и порядок вызова методов
// ---------------------------------------------------------------------------

// фикстура: локальный сокет в режиме прослушивания (вместо сервера),
// порт выбирает система
struct Listener {
    int sock;
    uint16_t port;
    Listener() : sock(-1), port(0) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        listen(sock, 5);
        socklen_t len = sizeof addr;
        getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len);
        port = ntohs(addr.sin_port);
    }
    ~Listener() { if (sock != -1) ::close(sock); }
};

// фикстура: порт, на котором заведомо никто не слушает
struct ClosedPort {
    uint16_t port;
    ClosedPort() : port(0) {
        Listener l;
        port = l.port;
    }   // сокет закрыт в деструкторе Listener, порт освободился
};

SUITE(CommunicatorTest)
{
    Credentials cred {"user", "P@ssW0rd"};
    VectorData data {{1, 2, 3}};

    TEST_FIXTURE(Listener, ConnectOk) {
        Communicator c("127.0.0.1", port, cred, data);
        CHECK(sock != -1);
        c.connect();
        c.close();
    }
    TEST(BadAddress) {
        Communicator c("abc", 33333, cred, data);
        CHECK_THROW(c.connect(), NetworkError);
    }
    TEST(BadAddressOctet) {
        Communicator c("127.0.0.256", 33333, cred, data);
        CHECK_THROW(c.connect(), NetworkError);
    }
    TEST_FIXTURE(ClosedPort, ConnectionRefused) {
        Communicator c("127.0.0.1", port, cred, data);
        CHECK_THROW(c.connect(), NetworkError);
    }
    TEST(AuthenticateWithoutConnect) {
        Communicator c("127.0.0.1", 33333, cred, data);
        CHECK_THROW(c.authenticate(), NetworkError);
    }
    TEST(ExchangeWithoutConnect) {
        Communicator c("127.0.0.1", 33333, cred, data);
        CHECK_THROW(c.exchange(), NetworkError);
    }
    TEST(CloseWithoutConnect) {
        Communicator c("127.0.0.1", 33333, cred, data);
        c.close();
        CHECK(true);
    }
    TEST_FIXTURE(Listener, CloseTwice) {
        Communicator c("127.0.0.1", port, cred, data);
        c.connect();
        c.close();
        c.close();
        CHECK(true);
    }
    TEST_FIXTURE(Listener, ExchangeAfterClose) {
        Communicator c("127.0.0.1", port, cred, data);
        c.connect();
        c.close();
        CHECK_THROW(c.exchange(), NetworkError);
    }
}

// ---------------------------------------------------------------------------
// 7. Классы ошибок
// ---------------------------------------------------------------------------
SUITE(ErrorsTest)
{
    TEST(MessageText) {
        FileError e("DataReader::read(path=a.txt)", "не удалось открыть файл");
        std::string text = e.what();
        CHECK(text.find("FileError") != std::string::npos);
        CHECK(text.find("DataReader::read(path=a.txt)") != std::string::npos);
        CHECK(text.find("не удалось открыть файл") != std::string::npos);
    }
    TEST(CatchAsProgramError) {
        CHECK_THROW(throw AuthError("f()", "ERR"), ProgramError);
        CHECK_THROW(throw NetworkError("f()", "ERR"), ProgramError);
        CHECK_THROW(throw DataFormatError("f()", "ERR"), ProgramError);
        CHECK_THROW(throw InterfaceError("f()", "ERR"), ProgramError);
    }
    TEST(CatchAsStdException) {
        CHECK_THROW(throw FileError("f()", "ERR"), std::exception);
    }
}

int main()
{
    return UnitTest::RunAllTests();
}
