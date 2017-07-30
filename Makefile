
OBJS = main.o error_check.o ofmsg.o schedule.o PriorityManager.o RingBuffer.o
HEAD = unp.h schedule.h ofmsg.h
main: $(OBJS) $(HEAD)
	g++ -o $@ $(OBJS) -lpthread

%.o: %.cpp
	g++ -c $< -o $@ -std=c++11

clean: 
	rm main *.o


