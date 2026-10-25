/**
 * @file data_writer.h
 * @brief Модуль записи результатов в двоичный файл
 */
#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Запись результатов вычислений в двоичный файл
 * @details Формат файла: первое поле - количество результатов (uint32_t),
 * далее результаты (uint64_t).
 */
class DataWriter {
public:
    /**
     * @brief Записать результаты в файл
     * @param [in] path имя файла для результатов
     * @param [in] results результаты вычислений
     * @throw FileError если файл не удалось открыть или записать
     */
    void write(const std::string& path, const std::vector<uint64_t>& results);
};
