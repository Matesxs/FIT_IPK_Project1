BINARY_NAME=hinfosvc

OUTPUT_FOLDER=output
OBJECT_FOLDER=obj
SOURCE_FOLDER=src

CC=g++
CFLAGS=-Wall -Wextra -Werror -std=c++17
SUFFIX=cpp
HEADER_SUFF=hpp

ADDITIONAL_CLEANU= output
RM=rm -rf

BINARY_PATH=$(OUTPUT_FOLDER)/$(BINARY_NAME)

SRC_SUBFOLDERS=$(shell find $(SOURCE_FOLDER) -type d)
$(CC)=$(CC) $(foreach DIR, $(SRC_SUBFOLDERS),-I $(DIR))
vpath %.$(SUFFIX) $(SRC_SUBFOLDERS)
vpath %.h $(SRC_SUBFOLDERS)

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC = $(call rwildcard,$(SOURCE_FOLDER),*.$(SUFFIX))
HDR = $(call rwildcard,$(SOURCE_FOLDER),*.$(HEADER_SUFF))
OBJ = $(patsubst $(SOURCE_FOLDER)/%.$(SUFFIX), $(OBJECT_FOLDER)/%.o, $(SRC))

$(BINARY_PATH) : $(OBJ)
	@echo LINKING
	@mkdir -p $(@D)
	@$(CC) $(OBJ) -o $@ $(CFLAGS)

$(OBJECT_FOLDER)/%.o: %.$(SUFFIX) $(HDR)
	@echo COMPILING $<
	@mkdir -p $(@D)
	@$(CC)  $< -c -o $@ $(CFLAGS)

.PHONY:  all build clean zip tar docs
.SILENT: docs clean zip tar

all: docs build

docs: $(SRC) $(HDR)
	doxygen Doxyfile

build: $(BINARY_PATH)

clean:
	$(RM) $(OBJECT_FOLDER)
	$(RM) $(BINARY_PATH)
	$(RM) $(BINARY_NAME).zip
	$(RM) $(BINARY_NAME).tar.gz
	$(RM) $(ADDITIONAL_CLEANU)

zip: clean
	zip -r -9 $(BINARY_NAME).zip *

tar: clean
	tar -czvf $(BINARY_NAME).tar.gz *