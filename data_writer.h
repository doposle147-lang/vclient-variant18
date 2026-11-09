// Модуль записи результатов в двоичный файл
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Запись результатов вычислений в двоичный файл
// Формат файла: первое поле - количество результатов (uint32_t),
// далее результаты (uint64_t).
class DataWriter {
public:
    // Записать результаты в файл
    void write(const std::string& path, const std::vector<uint64_t>& results);
};
