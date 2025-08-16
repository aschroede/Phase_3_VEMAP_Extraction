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
DAI_DATE="July 17, 2015 - or later"

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
TARGETS:=lib examples

# Define conditional build targets
NAMES:=graph dag bipgraph varset daialg alldai clustergraph factor factorgraph properties regiongraph cobwebgraph util weightedgraph exceptions exactinf evidence emalg io
NAMES:=$(NAMES) bp fbp trwbp mf hak lc treeep jtree mr gibbs bbp cbp bp_dual decmap glc map logger

# Define standard libDAI header dependencies, source file names and object file names
HEADERS=$(foreach name,graph dag bipgraph index var factor varset smallset prob daialg properties alldai enum exceptions util,$(INC)/$(name).h)
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

# Setup final command for MEX
ifdef NEW_MATLAB
  MEXFLAGS:=$(MEXFLAGS) -largeArrayDims
else
  MEXFLAGS:=$(MEXFLAGS) -DSMALLMEM
endif
MEX:=$(MEX) $(MEXINC) $(MEXFLAGS) $(WITHFLAGS) $(MEXLIBS) $(MEXLIB)


# META TARGETS
###############

all : $(TARGETS)
	@echo
	@echo VEMAP built successfully!

# Only one example that we need... maybe this is overkill?
EXAMPLES=$(foreach name,example_map,examples/$(name)$(EE))

# Rule to build the examples
examples : $(EXAMPLES)

utils : utils/createfg$(EE) utils/fg2dot$(EE) utils/fginfo$(EE) utils/uai2fg$(EE)

lib: $(LIB)/libdai$(LE)


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

# bbp.o is explicitly handled by this rule and overrules the first implicit rule
$(OBJ_DIR)/bbp$(OE) : $(SRC)/bbp.cpp $(INC)/bbp.h $(INC)/bp_dual.h $(HEADERS) | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/cbp$(OE) : $(SRC)/cbp.cpp $(INC)/cbp.h $(INC)/bbp.h $(INC)/bp_dual.h $(HEADERS) | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/hak$(OE) : $(SRC)/hak.cpp $(INC)/hak.h $(HEADERS) $(INC)/regiongraph.h | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/jtree$(OE) : $(SRC)/jtree.cpp $(INC)/jtree.h $(HEADERS) $(INC)/weightedgraph.h $(INC)/clustergraph.h $(INC)/regiongraph.h | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/treeep$(OE) : $(SRC)/treeep.cpp $(INC)/treeep.h $(HEADERS) $(INC)/weightedgraph.h $(INC)/clustergraph.h $(INC)/regiongraph.h $(INC)/jtree.h | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/emalg$(OE) : $(SRC)/emalg.cpp $(INC)/emalg.h $(INC)/evidence.h $(HEADERS) | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/decmap$(OE) : $(SRC)/decmap.cpp $(INC)/decmap.h $(HEADERS) | $(OBJ_DIR)
	$(CC) -c $< -o $@

$(OBJ_DIR)/glc$(OE) : $(SRC)/glc.cpp $(INC)/glc.h $(HEADERS) $(INC)/cobwebgraph.h | $(OBJ_DIR)
	$(CC) -c $< -o $@

# EXAMPLES
###########

examples/example_map$(EE) : examples/example_map.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) $(CCO)$@ $< $(LIBS)


# UTILS
########
# TODO: Not sure if we need these utils tbh.

utils/createfg$(EE) : utils/createfg.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) $(CCO)$@ $< $(LIBS) $(BOOSTLIBS_PO)

utils/fg2dot$(EE) : utils/fg2dot.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) $(CCO)$@ $< $(LIBS)

utils/fginfo$(EE) : utils/fginfo.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) $(CCO)$@ $< $(LIBS)

utils/uai2fg$(EE) : utils/uai2fg.cpp $(HEADERS) $(LIB)/libdai$(LE)
	$(CC) $(CCO)$@ $< $(LIBS)


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

#doc : $(INC)/*.h $(SRC)/*.cpp examples/*.cpp doxygen.conf
#	doxygen doxygen.conf
#
#README : doc scripts/makeREADME Makefile
#	DAI_VERSION=$(DAI_VERSION) DAI_DATE=$(DAI_DATE) scripts/makeREADME
#
#TAGS :
#	etags src/*.cpp include/dai/*.h tests/*.cpp utils/*.cpp
#	ctags src/*.cpp include/dai/*.h tests/*.cpp utils/*.cpp


# CLEAN
########

.PHONY : clean
ifneq ($(OS),WINDOWS)
clean :
	-rm -rf $(OBJ_DIR)
	-rm matlab/*$(ME)
	-rm examples/example$(EE) examples/example_bipgraph$(EE) examples/example_varset$(EE) examples/example_permute$(EE) examples/example_sprinkler$(EE) examples/example_sprinkler_gibbs$(EE) examples/example_sprinkler_em$(EE) examples/example_imagesegmentation$(EE)
	-rm tests/testdai$(EE) tests/testem/testem$(EE) tests/testbbp$(EE)
	-rm tests/unit/var_test$(EE) tests/unit/smallset_test$(EE) tests/unit/varset_test$(EE) tests/unit/graph_test$(EE) tests/unit/dag_test$(EE) tests/unit/bipgraph_test$(EE) tests/unit/weightedgraph_test$(EE) tests/unit/enum_test$(EE) tests/unit/util_test$(EE) tests/unit/exceptions_test$(EE) tests/unit/properties_test$(EE) tests/unit/index_test$(EE) tests/unit/prob_test$(EE) tests/unit/factor_test$(EE) tests/unit/factorgraph_test$(EE) tests/unit/clustergraph_test$(EE) tests/unit/regiongraph_test$(EE) tests/unit/daialg_test$(EE) tests/unit/alldai_test$(EE)
	-rm factorgraph_test.fg alldai_test.aliases
	-rm utils/fg2dot$(EE) utils/createfg$(EE) utils/fginfo$(EE) utils/uai2fg$(EE)
	-rm -R doc
	-rm -R lib
else
clean :
	-del *.obj
	-del *.ilk
	-del *.pdb
	-del matlab\*$(ME)
	-del examples\*$(EE)
	-del examples\*$(EE).manifest
	-del examples\*.ilk
	-del examples\*.pdb
	-del tests\*$(EE)
	-del tests\*$(EE).manifest
	-del tests\*.pdb
	-del tests\*.ilk
	-del tests\testem\*$(EE)
	-del tests\testem\*$(EE).manifest
	-del tests\testem\*.pdb
	-del tests\testem\*.ilk
	-del utils\*$(EE)
	-del utils\*$(EE).manifest
	-del utils\*.pdb
	-del utils\*.ilk
	-del tests\unit\*_test$(EE)
	-del tests\unit\*_test$(EE).manifest
	-del tests\unit\*_test.pdb
	-del tests\unit\*_test.ilk
	-del factorgraph_test.fg
	-del alldai_test.aliases
	-del $(LIB)\libdai$(LE)
	-rmdir lib
endif
