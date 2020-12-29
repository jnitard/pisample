

CXXFLAGS+=-O3 -g
CXXFLAGS+=-std=c++17 -Wall -Werror -Wextra
CXXFLAGS+=-fdiagnostics-color=always


#### Cross compilation stuff ####
ifndef X86
CXX=arm-linux-gnueabihf-g++.exe
# This needs to contain includes and binaries for:
# - alsa
# - wxwidgets
# - fmt
# The includes must be in their own folder per project (say fmt/format.h).
# The libraries must all be in the "lib" subfolder.
ROOT=../pi
endif
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

.PHONY: all
all: $(BIN)

SRC = main.cpp      \
      Device.cpp    \
      Pads.cpp      \
      Recorder.cpp  \
#

OBJ  := $(patsubst %.cpp,%.o,$(SRC))
DEPS := $(patsubst %.o,%.d,$(OBJ))
PATHOBJ = $(patsubst %.o,obj/%.o,$(OBJ))
PATHDEPS = $(patsubst %.d,obj/%.d,$(DEPS))

ifneq ($(MAKECMDGOALS), clean)
-include $(PATHDEPS)
endif

.PHONY: obj
obj:
	@mkdir -p obj

obj/%.o: %.cpp | obj
	$(CXX) $(CXXFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(OBJ) : $(DEPS)

$(BIN): $(PATHOBJ)
$(BIN):
	$(CXX) $(CXXFLAGS) $(CFLAGS)  -o $(BIN) $(PATHOBJ) $(LFLAGS)


# run on the PI from the host via `make run`
# need SSH keys in the proper places
ifndef PI_IP
PI_HOST:=raspberrypi.local
PI_IP:=$(shell dig $(PI_HOST) A +noall +answer | sed -r 's/.*A[ \t]*(.*)/\1/')
endif
PI:=pi@$(PI_IP)


.PHONY: sync run debug clean
sync: $(BIN)
	@echo "Connecting to ${PI}"
	@scp $(BIN) *.cpp *.h $(PI):pisample/ 1>/dev/null 2>/dev/null
	@echo "Synch’ed files"

run: sync
	@ssh -t $(PI) "bash -c '~/pisample/pisample --port 24'"

debug: $(BIN)
	@ssh -t $(PI) "bash -c 'cd pisample && gdb --args ~/pisample/pisample --port 24'"

clean:
	rm -rf $(BIN)
	rm -rf obj
