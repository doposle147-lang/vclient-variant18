#include "data_writer.h"
#include "errors.h"
#include <fstream>

void DataWriter::write(const std::string& path, const std::vector<uint64_t>& results)
{
    std::string where = "DataWriter::write(path=" + path
                        + ", количество результатов=" + std::to_string(results.size()) + ")";

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        throw FileError(where, "не удалось открыть файл для записи");

    // количество результатов
    uint32_t count = results.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof count);

    // сами результаты
    for (uint64_t value : results)
        file.write(reinterpret_cast<const char*>(&value), sizeof value);

    file.close();
    if (file.fail())
        throw FileError(where, "ошибка при записи данных в файл");
}
