NAME = cLitterBin

CC = clang
OPT = -O2
VERSION = -std=c11

FLAGS = -Wall -Wextra -Werror -pedantic -pedantic-errors -Wpedantic
WARNINGS = -Wno-incompatible-pointer-types
UNUSED_WARNINGS = -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-extra-semi
REMOVE_WARNINGS = -Wno-int-conversion
NIX_LIBS = -shared -fPIC
OSX_LIBS = -c
DEPS = $(shell find ./export -name "*.*o") $(shell find ./libs -name "*.*o")

INPUTFILES = src/$(NAME)/*.c
INPUT = src/$(NAME).c
OUTPUT = $(NAME)

TESTFILES = src/$(NAME)/*.c
TESTINPUT = spec/$(NAME).spec.c
TESTOUTPUT = spec_results

all: default

make_export:
	$(RM) -r export && mkdir export

copy_headers:
	mkdir export/$(NAME) && mkdir export/$(NAME)/headers
	cp -r src/$(NAME)/headers/* export/$(NAME)/headers/ >/dev/null 2>&1 || true
	cp src/$(NAME).h export/ >/dev/null 2>&1 || true

default: make_export
	$(CC) $(OPT) $(VERSION) $(FLAGS) $(WARNINGS) $(REMOVE_WARNINGS) $(UNUSED_WARNINGS) $(LIBS) -o $(OUTPUT) $(INPUT) $(INPUTFILES) $(DEPS)
	mv $(OUTPUT) export/ >/dev/null 2>&1 || true

lib: $(shell uname)
	cp $(shell find ./libs -name "*.*o") export/ >/dev/null 2>&1 || true

Darwin: make_export copy_headers
	$(CC) $(OPT) $(VERSION) $(FLAGS) $(WARNINGS) $(REMOVE_WARNINGS) $(UNUSED_WARNINGS) $(OSX_LIBS) $(INPUTFILES)
	mv *.o export/ >/dev/null 2>&1 || true

Linux: make_export copy_headers
	$(CC) $(OPT) $(VERSION) $(FLAGS) $(WARNINGS) $(REMOVE_WARNINGS) $(UNUSED_WARNINGS) $(NIX_LIBS) -o $(OUTPUT).so $(INPUTFILES)
	mv $(OUTPUT).so export/ >/dev/null 2>&1 || true

test:
	mkdir export; $(CC) $(OPT) $(VERSION) $(HEADERS) $(FLAGS) $(WARNINGS) $(REMOVE_WARNINGS) $(UNUSED_WARNINGS) -Wno-implicit-function-declaration $(LIBS) -o $(TESTOUTPUT) $(DEPS) $(TESTFILES) $(TESTINPUT)
	@echo
	./$(TESTOUTPUT)

spec: test

clean:
	$(RM) -r $(TESTOUTPUT)
	$(RM) -r export

