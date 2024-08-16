NAME = test

CC = clang
OPT = -O2
VERSION = -std=c89

FLAGS = -Wall -Wextra -Werror -pedantic -pedantic-errors -Wpedantic
WARNINGS =
UNUSED_WARNINGS = -Wno-unused-function
REMOVE_WARNINGS =

INPUT = $(NAME).c $(shell find ../export -name "*.o") $(shell find ../libs -name "*.o")
OUTPUT = a.out

all: default

default:
	cd .. && em build lib release
	$(CC) $(OPT) $(VERSION) $(FLAGS) $(WARNINGS) $(UNUSED_WARNINGS) $(REMOVE_WARNINGS) -o $(OUTPUT) $(INPUT)
	./$(OUTPUT)

clean:
	cd .. && em clean
	$(RM) -r $(OUTPUT)
