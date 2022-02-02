CC=g++
FLAGS=-Wall -std=c++14

shell: shell.o Command.o Pipeline.o read_command.o utility.o history.o autocomplete.o signal_handlers.o
	$(CC) $(FLAGS) shell.o Command.o Pipeline.o read_command.o utility.o history.o autocomplete.o signal_handlers.o -o shell

shell.o: shell.cpp header.h
	$(CC) $(FLAGS) -c shell.cpp -o shell.o

Command.o: Command.cpp header.h
	$(CC) $(FLAGS) -c Command.cpp -o Command.o

Pipeline.o: Pipeline.cpp header.h
	$(CC) $(FLAGS) -c Pipeline.cpp -o Pipeline.o

read_command.o: read_command.cpp header.h
	$(CC) $(FLAGS) -c read_command.cpp -o read_command.o

utility.o: utility.cpp header.h
	$(CC) $(FLAGS) -c utility.cpp -o utility.o

history.o: history.cpp header.h
	$(CC) $(FLAGS) -c history.cpp -o history.o

autocomplete.o: autocomplete.cpp header.h
	$(CC) $(FLAGS) -c autocomplete.cpp -o autocomplete.o

signal_handlers.o: signal_handlers.cpp header.h
	$(CC) $(FLAGS) -c signal_handlers.cpp -o signal_handlers.o

clean:
	rm *.o shell