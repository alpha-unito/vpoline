#
# Copyright 2026, University of Turin
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

CC = gcc
CFLAGS += -fpic -shared

BIN_DIR = ./bin
BUILD_DIR = ./build
SRC_DIR = src

LD_LIBRARY_PATH = ./build:../build
LD_PRELOAD = ../build/libvpoline.so
LIBVPHOOK_DIR=./build

INCLUDES = -I../include

#TESTS += clone
TESTS += fcntl
#TESTS += fork
#TESTS += mkdirat
#TESTS += openat
#TESTS += read
#TESTS += statx
TESTS += write

#EXECUTABLES += $(BIN_DIR)/clone_test
EXECUTABLES += $(BIN_DIR)/fcntl_test
#EXECUTABLES += $(BIN_DIR)/fork_test
#EXECUTABLES += $(BIN_DIR)/mkdirat_test
#EXECUTABLES += $(BIN_DIR)/openat_test
#EXECUTABLES += $(BIN_DIR)/read_test
#EXECUTABLES += $(BIN_DIR)/statx_test
EXECUTABLES += $(BIN_DIR)/write_test

#INTERCEPT_LIBS += $(BUILD_DIR)/hook_clone_test.so
INTERCEPT_LIBS += $(BUILD_DIR)/hook_fcntl_test.so
#INTERCEPT_LIBS += $(BUILD_DIR)/hook_fork_test.so
#INTERCEPT_LIBS += $(BUILD_DIR)/hook_mkdirat_test.so
#INTERCEPT_LIBS += $(BUILD_DIR)/hook_openat_test.so
#INTERCEPT_LIBS += $(BUILD_DIR)/hook_read_test.so
#INTERCEPT_LIBS += $(BUILD_DIR)/hook_statx_test.so
INTERCEPT_LIBS += $(BUILD_DIR)/hook_write_test.so
INTERCEPT_LIBS += $(BUILD_DIR)/syscall_tracker.so
INTERCEPT_LIBS += $(BUILD_DIR)/safe_syscall_tracker.so

DEBUG += $(BIN_DIR)/clone_debug
DEBUG += $(BIN_DIR)/clone3_debug
DEBUG += $(BIN_DIR)/fork_debug
DEBUG += $(BIN_DIR)/write_debug

all: $(BIN_DIR) $(BUILD_DIR) $(EXECUTABLES) $(INTERCEPT_LIBS) $(DEBUG)

$(BUILD_DIR)/%.so: %.c
	$(CC) -o $@ $< $(CFLAGS) $(INCLUDES)

$(BIN_DIR)/%: %.c
	$(CC) $< -o $@

test: $(TESTS)

$(TESTS):
	@echo "Running test: $@"
	@LD_PRELOAD=$(LD_PRELOAD) LIBVPHOOK=$(LIBVPHOOK_DIR)/hook_$@_test.so $(BIN_DIR)/$@_test \
		&& echo "Test $@ - \033[32mPASSED\033[0m" \
		|| echo "Test $@ - \033[31mFAILED\033[0m"
	@echo

clean:
	rm -rf $(BIN_DIR)/
	rm -rf $(BUILD_DIR)/
	rm -rf testdir/
	rm -f testfile.txt testfile2.txt

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
