

CXXFLAGS+=-O3 -g
CXXFLAGS+=-std=c++17

#### Cross compilation stuff ####
CXX=arm-linux-gnueabihf-g++.exe
# This needs to contain includes and binaries for:
# - alsa
# - wxwidgets
ROOT=C:\\users\\c_r_o\\Desktop\\pi
#### ----------------------- ####

CXXFLAGS+=-I$(ROOT)

# got from wx-config --cxxflags
CXXFLAGS+= -D_FILE_OFFSET_BITS=64 -DWXUSINGDLL -D__WXGTK__ -pthread

LFLAGS+=-L$(ROOT)\\lib -lasound

BIN = pisample

SRC = main.cpp

OBJ = $(patsubst %.cpp,%.o,$(SRC))

$(BIN): $(OBJ)

$(BIN):
	$(CXX) $(CXXFLAGS) $(CFLAGS)  -o $(BIN) $(OBJ) $(LFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CFLAGS) -c -o $@ $^

clean:
	rm $(BIN)
	rm -rf *.o
