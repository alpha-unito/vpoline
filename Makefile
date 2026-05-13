CC = gcc

SRC =	$(SRC_DIR)/main.c \
		$(SRC_DIR)/utils.c \
		$(SRC_DIR)/patcher.c \
		$(SRC_DIR)/sigsegv_handler.c \

HDR =	$(SRC_DIR)/main.h \
		$(SRC_DIR)/utils.h \
		$(SRC_DIR)/patcher.h \
		$(SRC_DIR)/sigsegv_handler.h \

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

SO = ./libvpoline.so

EXEC_SRC = $(TEST_DIR)/dummy_exec.c
EXEC = $(TEST_DIR)/dummy_exec

SRC_DIR = src
#BIN_DIR = bin
BUILD_DIR = makefile_build
TEST_DIR = test



#SRCDIR ?= ./

#NO_MAN=
CFLAGS = -O3 -pipe
CFLAGS += -g -rdynamic -fPIC
CFLAGS += -Wall -Wno-unused-function -Wno-unused-result #-Werror
#CFLAGS += -Wextra
#CFLAGS += -DSUPPLEMENTAL__REWRITTEN_ADDR_CHECK
#CFLAGS += -DDEBUG

SOFLAGS = -shared -fPIC

LDFLAGS += -L/usr/local/lib
LDFLAGS += -ldl -lcapstone

#CLEANFILES = $(SO) *.o *.d
CLEANFILES = $(SO) $(BUILD_DIR) $(EXEC)

.PHONY: all
all: $(SO)

debug: CFLAGS += -DDEBUG
debug: $(SO) $(EXEC)

runtest:
	LD_PRELOAD=$(SO) $(EXEC)

$(EXEC): $(EXEC_SRC)
	$(CC) -o $@ $(EXEC_SRC)

$(SO): $(OBJS)
	$(CC) -o $@ $^ $(SOFLAGS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDR) | $(BUILD_DIR)
	$(CC) -o $@ -c $< $(CFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(CLEANFILES)
