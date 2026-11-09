CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
# -MMD -MP: компилятор сам записывает зависимости .cpp от .h в файлы *.d,
# поэтому после изменения заголовка make пересоберет все нужные модули
DEPFLAGS = -MMD -MP
LIBS = -lboost_program_options -lcryptopp
OBJS = main.o interface.o communicator.o authenticator.o data_reader.o config_reader.o data_writer.o

# модульные тесты: все модули программы, кроме main.o, и файл с тестами
TEST_OBJS = tests/unit_tests.o $(filter-out main.o,$(OBJS))

vclient: $(OBJS)
	$(CXX) $(CXXFLAGS) -o vclient $(OBJS) $(LIBS)

unit_tests: $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o unit_tests $(TEST_OBJS) $(LIBS) -lUnitTest++

# собрать и запустить модульные тесты
test: unit_tests
	./unit_tests

tests/unit_tests.o: tests/unit_tests.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -I. -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f *.o *.d tests/*.o tests/*.d vclient unit_tests

.PHONY: clean test

-include $(OBJS:.o=.d) tests/unit_tests.d
