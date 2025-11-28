# This file is part of libDAI - http://www.libdai.org/
#
# Copyright (c) 2006-2011, The libDAI authors. All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

# DEFINITIONS
##############
# Load the platform independent build configuration file
include MakefileConfigs/Makefile.ALL

# Load the local configuration from Makefile.conf
include MakefileConfigs/Makefile.conf

# Set version and date
DAI_VERSION="git HEAD"
DAI_DATE="Nov 15, 2025"

# Directories of libDAI sources
# Location of libDAI headers
INC=include/dai
# Location of libDAI source files
SRC=src
# Destination directory of libDAI library
LIB=lib
# Destination directory for the object files
OBJ_DIR=obj

# Set final compiler flags
ifdef DEBUG
  CCFLAGS:=$(CCFLAGS) $(CCDEBUGFLAGS)
else
  CCFLAGS:=$(CCFLAGS) $(CCNODEBUGFLAGS)
endif

# Define build targets
TARGETS:=lib examples unittests

ifdef WITH_DOC
	TARGETS:=$(TARGETS) doc
endif

# Define conditional build targets
NAMES:=varset factor exceptions bipgraph regiongraph util weightedgraph factorgraph clustergraph graph
NAMES:=$(NAMES) jtree map logger

# Define standard libDAI header dependencies, source file names and object file names
HEADERS=$(foreach name,varset factor exceptions bipgraph regiongraph util weightedgraph factorgraph clustergraph graph,$(INC)/$(name).h)

SOURCES:=$(foreach name,$(NAMES),$(SRC)/$(name).cpp)

# This generates the object files in the root directory. It is responsible for the mess
# OLD: OBJECTS:=$(foreach name,$(NAMES),$(name)$(OE))
OBJECTS:=$(patsubst $(SRC)/%.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# Setup final command for C++ compiler
# If we are not compiling on Windows (Linux, MAC)
ifneq ($(OS),WINDOWS)
  CC:=$(CC) $(CCINC) $(CCFLAGS) $(WITHFLAGS) $(CCLIB)
# Else if we are compiling on Windows
else
  CC:=$(CC) $(CCINC) $(CCFLAGS) $(WITHFLAGS)
  LIBS:=$(LIBS) $(CCLIB)
endif


# META TARGETS
###############

all : $(TARGETS)
	@echo
	@echo VEMAP built successfully!

EXAMPLES=examples/example_map$(EE)
examples : $(EXAMPLES)

lib: $(LIB)/libdai$(LE)

unittests : unitTests/map_test$(EE)
	@echo Running unit tests...
	@echo
	./unitTests/map_test$(EE)


# OBJECTS
##########
# This section builds all the object files that have associated .cpp and .h files. TODO: We may not need them all!

# Add a rule to create the object directory
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# To make foo.o we must have foo.cpp and foo.h (and the stuff in headers -> I wonder why we need HEADERS?)
# To build foo.o we compile the first prereq (denoted by $<) which is foo.cpp into foo.o
# The pipe | designates the OBJ_DIR as an order-only prereq, ensuring it gets made before we build the obj files.
# But the OBJ_DIR does not trigger rebuilds.
$(OBJ_DIR)/%$(OE) : $(SRC)/%.cpp $(INC)/%.h $(HEADERS) | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/jtree$(OE) : $(SRC)/jtree.cpp $(INC)/jtree.h $(HEADERS) $(INC)/weightedgraph.h $(INC)/clustergraph.h $(INC)/regiongraph.h | $(OBJ_DIR)
	$(CC) -c $< -o $@

# EXAMPLES
###########

examples/example_map$(EE) : examples/example_map.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) $(CCO)$@ $< $(LIBS)

# UNIT TESTS
#############
unitTests/%$(EE) : unitTests/%.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) -DBOOST_TEST_DYN_LINK $(CCO)$@ $< $(LIBS) $(BOOSTLIBS_UTF)

# LIBRARY
##########

ifneq ($(OS),WINDOWS)
$(LIB)/libdai$(LE) : $(OBJECTS)
	-mkdir -p lib
	ar rcus $(LIB)/libdai$(LE) $(OBJECTS)
else
$(LIB)/libdai$(LE) : $(OBJECTS)
	-mkdir lib
	lib /out:$(LIB)/libdai$(LE) $(OBJECTS)
endif

# DOCUMENTATION
################

doc : $(INC)/*.h $(SRC)/*.cpp examples/*.cpp doxygen.conf
	doxygen doxygen.conf

README : doc scripts/makeREADME Makefile
	DAI_VERSION=$(DAI_VERSION) DAI_DATE=$(DAI_DATE) scripts/makeREADME

TAGS :
	etags src/*.cpp include/dai/*.h tests/*.cpp utils/*.cpp
	ctags src/*.cpp include/dai/*.h tests/*.cpp utils/*.cpp


# CLEAN
########

.PHONY : clean
clean :
	-rm -rf $(OBJ_DIR)
	-rm examples/example_map$(EE)
	-rm unitTests/map_test$(EE)
	-rm lib/libdai$(LE)
	-rm -R doc
