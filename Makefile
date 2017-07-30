
OBJS = main.o error_check.o ofmsg.o schedule.o RingBuffer.o PriorityManager.o 

HEAD = unp.h schedule.h ofmsg.h
main: $(OBJS)
	g++ -o $@ $(OBJS) -lpthread

#PriorityManager.o:  PriorityManager.cpp RingBuffer.cpp
#	g++ PriorityManager.cpp RingBuffer.cpp -o $@
%.o: %.cpp
	g++ -c $< -o $@ -std=c++11

clean: 
	rm main *.o


