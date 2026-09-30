# =============================================================================
#  Claude Tetris - GNU Makefile (Windows, LLVM-MinGW clang)
#
#    mingw32-make                 release build -> build/release/tetris.exe
#    mingw32-make CONFIG=debug    debug build   -> build/debug/tetris.exe + .pdb
#    mingw32-make run             build, then launch the game
#    mingw32-make test            build, then run the rules-engine tests
#    mingw32-make clean           delete build output of the current CONFIG
#    mingw32-make distclean       delete the whole build folder (incl. raylib)
#    mingw32-make icon            regenerate assets/tetris.ico (tools/make_icon.cpp)
#
#  Add -j (e.g. `mingw32-make -j12`) to compile in parallel.
# =============================================================================

CONFIG ?= release

# Toolchain (override on the command line, e.g. `mingw32-make CXX=g++`)
ifeq ($(origin CC),default)
  CC := clang
endif
ifeq ($(origin CXX),default)
  CXX := clang++
endif
ifeq ($(origin AR),default)
  AR := llvm-ar
endif
WINDRES ?= windres

# raylib backend knobs - run `mingw32-make distclean` after changing them
RAYLIB_PLATFORM ?= PLATFORM_DESKTOP_GLFW
RAYLIB_GRAPHICS ?= GRAPHICS_API_OPENGL_33

# Recipes always run in cmd.exe, whichever shell started make
SHELL := cmd.exe
winpath = $(subst /,\,$1)

BUILD   := build
OUT     := $(BUILD)/$(CONFIG)
OBJDIR  := $(OUT)/obj
TARGET  := $(OUT)/tetris.exe
TESTBIN := $(OUT)/test_rules.exe

# --- raylib (vendored source, compiled once into a static library) ----------
RAYLIB_SRC     := third_party/raylib/src
RAYLIB_OUT     := $(BUILD)/raylib
RAYLIB_LIB     := $(RAYLIB_OUT)/libraylib.a
RAYLIB_MODULES := rcore rshapes rtextures rtext rmodels raudio
ifeq ($(RAYLIB_PLATFORM),PLATFORM_DESKTOP_GLFW)
  RAYLIB_MODULES += rglfw
endif
RAYLIB_OBJS   := $(RAYLIB_MODULES:%=$(RAYLIB_OUT)/%.o)
RAYLIB_CFLAGS := -std=c99 -O2 -w -D_GNU_SOURCE -DUNICODE \
                 -D$(RAYLIB_PLATFORM) -D$(RAYLIB_GRAPHICS) -fno-strict-aliasing \
                 -DSUPPORT_SCREEN_CAPTURE=0 \
                 -I$(RAYLIB_SRC) -I$(RAYLIB_SRC)/external/glfw/include

# --- game --------------------------------------------------------------------
SRCS      := $(wildcard src/*.cpp)
OBJS      := $(SRCS:src/%.cpp=$(OBJDIR)/%.o)
RESOURCES := $(OBJDIR)/resources.o
TEST_OBJS := $(OBJDIR)/tests/test_rules.o $(OBJDIR)/game.o $(OBJDIR)/tetromino.o
DEPS      := $(OBJS:.o=.d) $(OBJDIR)/tests/test_rules.d

CXXFLAGS := -std=c++20 -Wall -Wextra -MMD -MP -I$(RAYLIB_SRC)
LDFLAGS  := -static
LDLIBS   := -L$(RAYLIB_OUT) -lraylib -lopengl32 -lgdi32 -lwinmm
ifeq ($(RAYLIB_PLATFORM),PLATFORM_DESKTOP_WIN32)
  LDLIBS += -lshcore
endif

ifeq ($(CONFIG),release)
  CXXFLAGS    += -O2 -DNDEBUG
  APP_LDFLAGS := -mwindows -s
else ifeq ($(CONFIG),debug)
  CXXFLAGS    += -O0 -g -gcodeview
  LDFLAGS     += -g -Wl,--pdb=
  APP_LDFLAGS :=
else
  $(error CONFIG must be 'release' or 'debug', not '$(CONFIG)')
endif

.PHONY: all run test clean distclean icon

all: $(TARGET)

$(TARGET): $(OBJS) $(RESOURCES) $(RAYLIB_LIB)
	@echo Linking $@
	@$(CXX) $(LDFLAGS) $(APP_LDFLAGS) -o $@ $(OBJS) $(RESOURCES) $(LDLIBS)

# Icon and version info, compiled into the exe
$(RESOURCES): assets/tetris.rc assets/tetris.ico | $(OBJDIR)
	@echo Compiling assets/tetris.rc
	@$(WINDRES) -I assets -i assets/tetris.rc -o $@

# The icon is generated and committed; rebuild it only when changing its design
icon: $(BUILD)/tools/make_icon.exe
	$(call winpath,$<) assets/tetris.ico

$(BUILD)/tools/make_icon.exe: tools/make_icon.cpp | $(BUILD)/tools
	@echo Compiling tools/make_icon.cpp
	@$(CXX) -std=c++20 -O2 -Wall -Wextra -isystem $(RAYLIB_SRC)/external $< -o $@ -static

$(OBJDIR)/%.o: src/%.cpp | $(OBJDIR)
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/tests/%.o: tests/%.cpp | $(OBJDIR)/tests
	@echo Compiling $<
	@$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

$(TESTBIN): $(TEST_OBJS)
	@echo Linking $@
	@$(CXX) $(LDFLAGS) -o $@ $(TEST_OBJS)

$(RAYLIB_LIB): $(RAYLIB_OBJS)
	@echo Archiving $@
	@$(AR) rcs $@ $(RAYLIB_OBJS)

$(RAYLIB_OUT)/%.o: $(RAYLIB_SRC)/%.c | $(RAYLIB_OUT)
	@echo Compiling raylib/$(notdir $<)
	@$(CC) $(RAYLIB_CFLAGS) -c $< -o $@

run: $(TARGET)
	$(call winpath,$(TARGET))

test: $(TESTBIN)
	$(call winpath,$(TESTBIN))

# Output folders, created one level at a time so parallel builds never race
$(BUILD):
	@mkdir $(call winpath,$@)
$(OUT) $(RAYLIB_OUT) $(BUILD)/tools: | $(BUILD)
	@mkdir $(call winpath,$@)
$(OBJDIR): | $(OUT)
	@mkdir $(call winpath,$@)
$(OBJDIR)/tests: | $(OBJDIR)
	@mkdir $(call winpath,$@)

clean:
	@if exist $(call winpath,$(OUT)) rmdir /s /q $(call winpath,$(OUT))

distclean:
	@if exist $(BUILD) rmdir /s /q $(BUILD)

-include $(DEPS)
