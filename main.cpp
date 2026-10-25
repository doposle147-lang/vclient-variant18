/**
 * @file main.cpp
 * @brief Клиент для системы обработки числовых векторов
 * @details Читает векторы из текстового файла, передает их на сервер
 * (аутентификация SHA-1, обмен в двоичном виде) и сохраняет результаты
 * в двоичный файл.
 */
#include <iostream>
#include "errors.h"
#include "interface.h"

int main(int argc, const char** argv)
{
    try {
        UserInterface ui;
        return ui.run(argc, argv);
    } catch (const ProgramError& e) {
        // ошибки нашей программы
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        // прочие ошибки стандартной библиотеки
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 2;
    } catch (...) {
        std::cerr << "Исключение неизвестного типа!" << std::endl;
        return 3;
    }
}
