# Builds libqualcomm_proto.a — Qualcomm DIAG protocol shared utilities.
# Used by both monitor_c and the decoder. No Python dependency.
#
# Usage:
#   make          build the static library
#   make clean    remove built files

# ── Compiler ─────────────────────────────────────────────────────────────────

CXX      := g++

# -std=c++17   use C++17 (std::variant, std::optional, if constexpr, etc.)
#              C++17 is a superset of C++14; existing C++14 code compiles
#              unchanged. Required for FieldValue in decoder/field_value.h.
#              GCC 9+ (Ubuntu 20.04 default) has full C++17 support.
# -Wall        enable all common warnings
# -fPIC        position-independent code; required when this .a is eventually
#              linked into a .so (needed for Python extension and Android .so)
# -I.          look for headers in this directory (utils.h, consts.h, etc.)
CXXFLAGS := -std=c++17 -Wall -fPIC -I.

# ── Sources and derived names ─────────────────────────────────────────────────

SRCS := utils.cpp log_config.cpp consts.cpp hdlc.cpp

# $(SRCS:.cpp=.o) is a substitution reference:
#   replace every ".cpp" suffix in SRCS with ".o"
#   result: utils.o log_config.o consts.o hdlc.o
OBJS := $(SRCS:.cpp=.o)

LIB := libqualcomm_proto.a

# ── Targets ───────────────────────────────────────────────────────────────────

# Default target: build the library.
all: $(LIB)

# Archive rule: pack all object files into a static library.
#   ar    the archiver (like zip for .o files)
#   r     insert/replace object files in the archive
#   c     create the archive if it does not already exist
#   s     write an index (symbol table) into the archive so the linker
#         can find symbols without scanning every object file
#   $@    expands to the rule's target name  →  libqualcomm_proto.a
#   $^    expands to all prerequisites       →  utils.o log_config.o consts.o
$(LIB): $(OBJS)
	ar rcs $@ $^

# Pattern rule: compile any .cpp into a .o.
#   %     wildcard that matches the same stem on both sides
#   $<    expands to the first prerequisite (the .cpp file)
#   $@    expands to the target (the .o file)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Explicit dependencies: tell make which headers each .o depends on so it
# recompiles when a header changes.
utils.o:      utils.cpp utils.h
log_config.o: log_config.cpp log_config.h consts.h utils.h
consts.o:     consts.cpp consts.h utils.h
hdlc.o:       hdlc.cpp hdlc.h

# Remove all build outputs. The leading '-' tells make to ignore errors
# (e.g. if the files don't exist yet).
clean:
	rm -f $(OBJS) $(LIB)

# Declare targets that are not real files so make doesn't confuse them with
# files named "all" or "clean" on disk.
.PHONY: all clean