
# This avoids garbled GCC output but prevents running an interactive
# command efficiently :(
ifeq ($(filter run debug,$(MAKECMDGOALS)), )
MAKEFLAGS=-Oline
endif

MAKEFLAGS+=-j

#CXXFLAGS+=-O3 -g
CXXFLAGS+= -g
# Cross-compiler I found is too old for c++20.
CXXFLAGS+=-std=c++2a -Wall -Werror -Wextra
CXXFLAGS+=-fdiagnostics-color=always
# We compile everything with a recent enough GCC hopefully ...
# wxwidgets is the only C++ dependency.
CXXFLAGS+=-Wno-psabi

LFLAGS += -lstdc++fs


#### Cross compilation stuff ####
ifndef X86
CXX=arm-linux-gnueabihf-g++.exe
# This needs to contain includes and binaries for:
# - alsa
# - wxwidgets
# - fmt
# - flac (C APIs only)
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
# others
LFLAGS += -lasound -lfmt -lFLAC -lavcodec -lavutil -lavformat
#### --------- ####

BIN = pisample

.PHONY: all
all: $(BIN)

SRC = main.cpp      \
      Alsa.cpp      \
      Device.cpp    \
      Pads.cpp      \
      PiSample.cpp  \
      Player.cpp    \
      Recorder.cpp  \
      Strings.cpp   \
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


# ---- run ----

# run on the PI from the host via `make run`
# need SSH keys in the proper places
ifndef PI_IP
ifneq ($(filter sync run debug,$(MAKECMDGOALS)), )
PI_HOST?=raspberrypi.local
PI_IP:=$(shell dig $(PI_HOST) A +noall +answer | sed -r 's/.*A[ \t]*(.*)/\1/')
endif
endif

PI:=pi@$(PI_IP)

.PHONY: sync run debug clean

sync: $(BIN)
	@echo "Connecting to ${PI}"
	@scp $(BIN) *.cpp *.h *.sh *.ini $(PI):pisample/ 1>/dev/null 2>/dev/null
	@echo "Synch’ed files"

### !! The command lines when running debugging
ARGS = --config ~/pisample/config.ini \
			 --record \
#
			 #--audio-in-card pulse \

run: sync
	$(info ${ARGS})
	@echo ssh -t $(PI) "bash -c '~/pisample/pisample ${ARGS}'"
	@ssh -t $(PI) "bash -c '~/pisample/pisample ${ARGS}'"

debug: sync
	@echo ssh -t $(PI) bash -c 'cd pisample && gdb --args ~/pisample/pisample ${ARGS}'
	@ssh -t $(PI) bash -c 'cd pisample && gdb --args ~/pisample/pisample ${ARGS}'

# ---- /run ---

clean:
	rm -rf $(BIN)
	rm -rf obj
