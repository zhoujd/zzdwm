# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
TARGET = dag_demo
OBJS = main.o dag.o dag_operations.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c dag.h dag_operations.h
	$(CC) $(CFLAGS) -c main.c

dag.o: dag.c dag.h
	$(CC) $(CFLAGS) -c dag.c

dag_operations.o: dag_operations.c dag_operations.h dag.h
	$(CC) $(CFLAGS) -c dag_operations.c

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full ./$(TARGET)

.PHONY: all clean run valgrind
