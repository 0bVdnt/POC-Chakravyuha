# Makefile for: compile -> LLVM IR -> obfuscate with pass -> link -> run -> strings

# Tools (override on command line, e.g. "make CLANG=clang")
CLANG    ?= clang-20
OPT      ?= opt-20
PASS_SO  ?= ./build/lib/libChakravyuhaStringEncryptionPass.so
STRINGS  ?= strings

# Filenames
SRC      := test_program.c
LL       := $(SRC:.c=.ll)
OBF_LL   := obfuscated.ll
NO_OBF	 := no_obsfuscation
BIN      := obfuscated_program

# Flags
# produce LLVM IR from C (no optimizations, emit LLVM IR text)
CLANG_FLAGS := -O0 -S -emit-llvm

# opt pass flags: use -load-pass-plugin (modern LLVM) and -passes=...
OPT_PASSES := -load-pass-plugin $(PASS_SO) -passes=chakravyuha-string-encrypt

.PHONY: no_obfuscation all compile obfuscate link run strings clean

all: $(BIN)

# Step 1: compile C -> LLVM IR (.ll)
# Recreates .ll whenever .c changes
$(LL): $(SRC)
	@echo "[1/4] clang -> LLVM IR: $< -> $@"
	$(CLANG) $(CLANG_FLAGS) $< -o $@

# Step 2: run the custom opt pass to produce obfuscated.ll
$(OBF_LL): $(LL) $(PASS_SO)
	@echo "[2/4] obfuscating with plugin: $(PASS_SO)"
	$(OPT) $(OPT_PASSES) < $(LL) > $(OBF_LL) 2> report.json

# Step 3: compile/link obfuscated.ll -> binary
$(BIN): $(OBF_LL)
	@echo "[3/4] compiling LLVM IR -> binary: $< -> $@"
	$(CLANG) $(OBF_LL) -o $(BIN)

# convenience phony targets
no_obfuscation:
	$(CLANG) $(SRC) -o $(NO_OBF)

compile: $(LL)

obfuscate: $(OBF_LL)

link: $(BIN)

run: $(BIN)
	@echo "[4/4] running: ./$<"
	./$(BIN)

strings: $(BIN)
	@echo "strings output for $(BIN):"
	$(STRINGS) $(BIN)

clean:
	@echo "cleaning generated files..."
	-rm -f $(LL) $(OBF_LL) $(BIN)

