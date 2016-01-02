#SIM_SRC  = harp_emulator.cpp 
SIM_SRC  = fetch.cpp decoder.cpp execute.cpp harp_emulator.cpp 
SIM_OBJS = $(SIM_SRC:.cpp=.o)

all: $(SIM_SRC) harp_emulator

%.o: %.cpp 
	#g++ -std=c++11 -g -c -o $@ $<  
	g++ -c -g -o $@ $<  

harp_emulator: $(SIM_OBJS) 
	g++ -g -o $@ $^
	#g++ -std=c++11 -g -o $@ $^

clean: 
	rm harp_emulator *.o *.log
	#rm harp_emulator *.log
