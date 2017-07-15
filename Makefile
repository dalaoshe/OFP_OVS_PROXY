
OBJS = main.o error_check.o
HEAD = unp.h
main: $(OBJS) $(HEAD)
	g++ -o $@ $(OBJS) -lpthread

%.o: %.cpp
	g++ -c $< -o $@

clean: 
	rm main *.o


