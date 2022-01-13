CC = g++
FLAGS = -std=c++17 -O3

all:
	${CC} ${FLAGS} dmsh.cpp -o dmsh
	./dmsh

# dmsh: dmsh.cpp
# 	${CC} ${FLAGS} dmsh.cpp -o dmsh

.PHONY: clean
clean:
	rm -rf *.o dmsh
