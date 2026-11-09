// Модуль чтения исходных данных из текстового файла
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Набор числовых векторов (значения типа uint64_t)
typedef std::vector<std::vector<uint64_t>> VectorData;

// Чтение числовых векторов из текстового файла
// Формат файла: в первой строке количество векторов, далее в каждой
// строке размер вектора и его значения. Числа разделены пробелами.
class DataReader {
public:
    // Прочитать векторы из файла
    VectorData read(const std::string& path);
};
