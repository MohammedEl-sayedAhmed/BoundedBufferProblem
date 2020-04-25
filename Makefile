build:
	gcc producer.c -pthread -o producer.out 
	gcc consumer.c -pthread -o consumer.out

clean:
	rm -f *.out  

all: clean build

run:
	./producer.out
