include Make_linux.inc
#include Make_msys2.inc
#include Make_osx.inc

CXXFLAGS = -std=c++17 -fopenmp  # Ajouté -fopenmp pour OpenMP
ifdef DEBUG
CXXFLAGS += -g -O0 -Wall -fbounds-check -pedantic -D_GLIBCXX_DEBUG
CXXFLAGS2 = $(CXXFLAGS)
else
CXXFLAGS2 = $(CXXFLAGS) -O2 -march=native -Wall 
CXXFLAGS += -O3 -march=native -Wall
endif

# Ajout des bibliothèques OpenSSL
LDFLAGS = -lssl -lcrypto

ALL = simulation.exe 

default: help

all: $(ALL)

comp:
	$(CXX) $(CXXFLAGS2) -c simulation.cpp -o simulation.o
	$(CXX) $(CXXFLAGS2) -c model.cpp -o model.o
	$(CXX) $(CXXFLAGS2) -c display.cpp -o display.o
	$(CXX) $(CXXFLAGS2) simulation.o model.o display.o -o simulation.exe $(LDFLAGS) $(LIB)

clean:
	@rm -fr *.o *.exe *~

.cpp.o:
	$(CXX) $(CXXFLAGS2) -c $< -o $@	

simulation.exe: display.o display.hpp model.o model.hpp simulation.o
	$(CXX) $(CXXFLAGS2) $^ -o $@ $(LDFLAGS) $(LIB)	

help:
	@echo "Available targets : "
	@echo "    all            : compile all executables"
	@echo "    comp           : compile object files and link them"
	@echo "Add DEBUG=yes to compile in debug"
	@echo "Configuration :"
	@echo "    CXX      :    $(CXX)"
	@echo "    CXXFLAGS :    $(CXXFLAGS)"
	@echo "    LDFLAGS  :    $(LDFLAGS)"

%.html: %.md
	pandoc -s --toc $< --css=./github-pandoc.css --metadata pagetitle="OS202 - TD1" -o $@