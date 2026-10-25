#include "data_reader.h"
#include "errors.h"
#include <fstream>
#include <sstream>

// максимальное значение для 32-битных полей (количество и размер векторов)
static const uint64_t MAX_UINT32 = 4294967295ULL;

// возвращает текст с местом ошибки
static std::string where(const std::string& path, size_t lineNo)
{
    return "DataReader::read(path=" + path + ", строка=" + std::to_string(lineNo) + ")";
}

// разбивает строку на слова по пробельным символам
static std::vector<std::string> split(const std::string& line)
{
    std::istringstream ss(line);
    std::vector<std::string> words;
    std::string word;
    while (ss >> word)
        words.push_back(word);
    return words;
}

// читает следующую непустую строку и разбивает ее на слова,
// возвращает false если файл закончился
static bool nextLine(std::istream& f, std::vector<std::string>& words, size_t& lineNo)
{
    std::string line;
    while (std::getline(f, line)) {
        lineNo++;
        // строка считается пустой, если в ней нет ни одного слова
        // (в том числе если в ней только пробелы, табуляции, \r и т. п.)
        words = split(line);
        if (!words.empty())
            return true;
    }
    return false;
}

// переводит слово в число, допускаются только цифры (число без знака)
static uint64_t toNumber(const std::string& word, const std::string& path, size_t lineNo)
{
    bool ok = !word.empty();
    for (char c : word) {
        if (c < '0' || c > '9')
            ok = false;
    }
    if (!ok)
        throw DataFormatError(where(path, lineNo),
                              "'" + word + "' не является целым числом без знака");
    try {
        return std::stoull(word);
    } catch (const std::out_of_range&) {
        throw DataFormatError(where(path, lineNo),
                              "число '" + word + "' не помещается в тип uint64_t");
    }
}

VectorData DataReader::read(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw FileError("DataReader::read(path=" + path + ")",
                        "не удалось открыть файл для чтения");

    std::vector<std::string> words;
    size_t lineNo = 0;

    // первая строка - количество векторов
    if (!nextLine(file, words, lineNo))
        throw DataFormatError(where(path, lineNo), "файл пустой, нет количества векторов");
    if (words.size() != 1)
        throw DataFormatError(where(path, lineNo),
                              "в первой строке должно быть одно число (количество векторов), а найдено слов: "
                              + std::to_string(words.size()));
    uint64_t count = toNumber(words[0], path, lineNo);
    if (count > MAX_UINT32)
        throw DataFormatError(where(path, lineNo),
                              "количество векторов " + words[0] + " больше максимального (4294967295)");
    if (count == 0)
        throw DataFormatError(where(path, lineNo), "количество векторов равно 0, нечего передавать на сервер");

    // остальные строки - векторы
    VectorData data;
    for (uint64_t i = 0; i < count; i++) {
        if (!nextLine(file, words, lineNo))
            throw DataFormatError(where(path, lineNo),
                                  "ожидалось векторов: " + std::to_string(count)
                                  + ", а в файле только " + std::to_string(i));
        uint64_t size = toNumber(words[0], path, lineNo);
        if (size > MAX_UINT32)
            throw DataFormatError(where(path, lineNo),
                                  "размер вектора " + words[0] + " больше максимального (4294967295)");
        if (size == 0)
            throw DataFormatError(where(path, lineNo), "размер вектора равен 0, вектор должен содержать значения");
        if (words.size() - 1 != size)
            throw DataFormatError(where(path, lineNo),
                                  "размер вектора " + std::to_string(size) + ", а значений в строке: "
                                  + std::to_string(words.size() - 1));
        std::vector<uint64_t> vect;
        for (size_t j = 1; j < words.size(); j++)
            vect.push_back(toNumber(words[j], path, lineNo));
        data.push_back(vect);
    }

    // после последнего вектора ничего быть не должно
    if (nextLine(file, words, lineNo))
        throw DataFormatError(where(path, lineNo),
                              "лишние данные после последнего вектора");
    return data;
}
