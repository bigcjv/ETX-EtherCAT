CXX ?= g++
TARGET ?= etx_6axis_csp_demo

ETX_HOME ?= /home/tpm/ETX-R4K111
PREFIX ?= /opt/etx-motion

INC_DIR := $(ETX_HOME)/include
SRC_DIR := src
BUILD_DIR := build

SRCS := \
	$(SRC_DIR)/AppConfig.cpp \
	$(SRC_DIR)/AxisController.cpp \
	$(SRC_DIR)/EcpwSession.cpp \
	$(SRC_DIR)/main.cpp

OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -Iinclude -I$(INC_DIR) -MMD -MP
LDFLAGS ?= -L/usr/lib
LDLIBS ?= -lECPW -lpthread -ldl

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)

