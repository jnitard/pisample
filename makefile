

CXXFLAGS+=-O3 -g
CXXFLAGS+=-std=c++17 -Wall -Werror -Wextra
CXXFLAGS+=-fdiagnostics-color=always


#### Cross compilation stuff ####
CXX=arm-linux-gnueabihf-g++.exe
# This needs to contain includes and binaries for:
# - alsa
# - wxwidgets
# - fmt
# The includes must be in their own folder per project (say fmt/format.h).
# The libraries must all be in the "lib" subfolder.
ROOT=../pi
#### ----------------------- ####


#### LIBRARIES ####
CXXFLAGS+=-I$(ROOT)
LFLAGS+=-L$(ROOT)\\lib
# WXWidgets, got from wx-config --cxxflags
CXXFLAGS+= -D_FILE_OFFSET_BITS=64 -DWXUSINGDLL -D__WXGTK__ -pthread
# Alsa
LFLAGS += -lasound
# FMT
LFLAGS += -lfmt
#### --------- ####

BIN = pisample

all: $(BIN)

SRC = main.cpp    \
      Device.cpp  \
      Pads.cpp    \
#

OBJ  := $(patsubst %.cpp,%.o,$(SRC))
DEPS := $(patsubst %.o,%.d,$(OBJ))

%.d: %.cpp
	$(CXX) $(CXXFLAGS) $(CFLAGS) -MM $< > $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CFLAGS) -c -o $@ $<

-include $(DEPS)
$(OBJ) : $(DEPS)

$(BIN): $(OBJ)
$(BIN):
	$(CXX) $(CXXFLAGS) $(CFLAGS)  -o $(BIN) $(OBJ) $(LFLAGS)



# run on the PI from the host via `make run`
# need SSH keys in the proper places
PI_HOST:=raspberrypi.local
PI_IP:=$(shell dig $(PI_HOST) A +noall +answer | sed -r 's/.*A[ \t]*(.*)/\1/')
PI:=pi@$(PI_IP)
run: $(BIN)
	@scp $(BIN) *.cpp *.h $(PI):pisample/ 2>/dev/null
	@echo "Synch’ed files"
	@ssh -t $(PI) "bash -c 'cd pisample && gdb --args ~/pisample/pisample --port 24'"

clean:
	rm $(BIN)
	rm -rf *.o
