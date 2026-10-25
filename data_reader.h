/**
 * @file data_reader.h
 * @brief Модуль чтения исходных данных из текстового файла
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

/// Набор числовых векторов (значения типа uint64_t)
typedef std::vector<std::vector<uint64_t>> VectorData;

/**
 * @brief Чтение числовых векторов из текстового файла
 * @details Формат файла: в первой строке количество векторов, далее в каждой
 * строке размер вектора и его значения. Числа разделены пробелами.
 */
class DataReader {
public:
    /**
     * @brief Прочитать векторы из файла
     * @param [in] path имя файла с исходными данными
     * @return прочитанные векторы
     * @throw FileError если файл не удалось открыть
     * @throw DataFormatError если структура файла нарушена, количество векторов
     * или размер какого-либо вектора равен 0
     */
    VectorData read(const std::string& path);
};
