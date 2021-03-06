NAME = cLitterBin

CC = clang
OPT = -O2
VERSION = -std=c11

FLAGS = -Wall -Wextra -Werror -pedantic -pedantic-errors -Wpedantic
WARNINGS = 
UNUSED_WARNINGS = -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-extra-semi
REMOVE_WARNINGS = 
LIBS = -c

INPUT = src/$(NAME)/*.c
OUTPUT = $(NAME).so

#TESTFILES = ../src/$(NAME)/*.c
TESTINPUT = $(NAME).spec.c
TESTOUTPUT = spec_results

all: default

default:
	$(CC) $(OPT) $(VERSION) $(FLAGS) $(WARNINGS) $(REMOVE_WARNINGS) $(UNUSED_WARNINGS) $(LIBS) $(INPUT)
	ar -rcs $(OUTPUT) *.o
	$(RM) -r export && mkdir export && mkdir export/$(NAME) && mkdir export/$(NAME)/headers
	cp src/$(NAME).h export/
	cp  src/$(NAME)/headers/* export/$(NAME)/headers/
	mv $(OUTPUT) export/
	$(RM) ./*.o

lib: default

test:
	cd spec && $(CC) $(OPT) $(VERSION) $(HEADERS) $(FLAGS) $(WARNINGS) $(REMOVE_WARNINGS) $(UNUSED_WARNINGS) -o $(TESTOUTPUT) $(TESTFILES) $(TESTINPUT)
	@echo
	./spec/$(TESTOUTPUT)

spec: test

clean:
	$(RM) -r spec/$(TESTOUTPUT)
	$(RM) -r export

