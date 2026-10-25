CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
# -MMD -MP: компилятор сам записывает зависимости .cpp от .h в файлы *.d,
# поэтому после изменения заголовка make пересоберет все нужные модули
DEPFLAGS = -MMD -MP
LIBS = -lboost_program_options -lcryptopp
OBJS = main.o interface.o communicator.o authenticator.o data_reader.o config_reader.o data_writer.o

vclient: $(OBJS)
	$(CXX) $(CXXFLAGS) -o vclient $(OBJS) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

clean:
	rm -f *.o *.d vclient

.PHONY: clean

-include $(OBJS:.o=.d)
