# Compiler and flags
CXX = g++
CXXFLAGS = -g -MMD

INCLUDE_PATHS = -I ../Inc \
				-I $(HOME)/Data/github/can/libcanard/libcanard \
				-I $(HOME)/Data/github/can/libudpard/libudpard \
				-I $(HOME)/Data/github/can/libserard_ck/libserard \
				-I $(HOME)/Data/github/can/o1heap/o1heap \
				-I /usr/include/eigen3 \
				-I /usr/include/doctest

LIBS_PATH =			-L $(HOME)/Data/github/can/lib_x86_64 
LIBS =				-lcanardd -lserardd -ludpardd -lo1heapd

# Source and object files
TEST_FILES = TestArrayList.cpp \
             TestCircularBuffer.cpp \
             TestBoxSet.cpp \
             TestBoxSetIterator.cpp \
			 TestCyphal.cpp \
			 TestMockHal.cpp \
			 TestO1HeapAllocator.cpp \
			 TestLogger.cpp
			 
TEST_NAMES = $(TEST_FILES:.cpp=)

# Build directories
BIN_DIR = bin

# Object files
OBJECTS = $(addprefix $(BIN_DIR)/,$(TEST_NAMES:=.o))

# Executables
EXECUTABLES = $(addprefix $(BIN_DIR)/,$(TEST_NAMES))

MOCK_HAL_EXTRA_SOURCES =  ../Src/mock_hal.c

# Extra defines for logger
LOGGER_EXTRA_DEFINES = -DLOGGER_ENABLED -DLOGGER_OUTPUT_STDERR -DLOGGER_OUTPUT_UART -DLOGGER_OUTPUT_USB

# Default target
all: create_bin_dir $(EXECUTABLES)

# Create the bin directory
create_bin_dir:
	@mkdir -p $(BIN_DIR)


# Rule for TestMockHal with extra sources
$(BIN_DIR)/TestMockHal.o : TestMockHal.cpp $(MOCK_HAL_EXTRA_SOURCES)
	@echo "Compiling $< with extra sources"
	$(CXX) -c TestMockHal.cpp $(CXXFLAGS) $(INCLUDE_PATHS) -o $(BIN_DIR)/TestMockHal.o

$(BIN_DIR)/TestMockHal : $(BIN_DIR)/TestMockHal.o $(MOCK_HAL_EXTRA_SOURCES:.c=.o)
	$(CXX) $(BIN_DIR)/TestMockHal.o $(MOCK_HAL_EXTRA_SOURCES:.c=.o) $(CXXFLAGS) $(INCLUDE_PATHS) $(LIBS_PATH) $(LIBS) -o $@

../Src/mock_hal.o : ../Src/mock_hal.c
	$(CXX) -c ../Src/mock_hal.c $(CXXFLAGS) $(INCLUDE_PATHS) -o $@

# Rule for TestLogger with extra sources
$(BIN_DIR)/TestLogger.o : TestLogger.cpp
	@echo "Compiling $< with extra sources"
	$(CXX) $(LOGGER_EXTRA_DEFINES) -c TestLogger.cpp $(CXXFLAGS) $(INCLUDE_PATHS) -o $(BIN_DIR)/TestLogger.o

$(BIN_DIR)/TestLogger : $(BIN_DIR)/TestLogger.o ../Src/Logger.o ../Src/mock_hal.o
	$(CXX) $(BIN_DIR)/TestLogger.o ../Src/Logger.o ../Src/mock_hal.o $(CXXFLAGS) $(INCLUDE_PATHS) $(LIBS_PATH) $(LIBS) -o $@

../Src/Logger.o : ../Src/Logger.cpp
	$(CXX) $(LOGGER_EXTRA_DEFINES) -c ../Src/Logger.cpp $(CXXFLAGS) $(INCLUDE_PATHS) -o $@


# Generic rule for all other tests
$(BIN_DIR)/%.o : %.cpp
	@echo "Compiling $<"
	$(CXX) -c $< $(CXXFLAGS) $(INCLUDE_PATHS) -o $@

$(BIN_DIR)/% : $(BIN_DIR)/%.o
	$(CXX) $<  $(CXXFLAGS) $(INCLUDE_PATHS) $(LIBS_PATH) $(LIBS) -o $@
	
# Run all tests target
run_all: all
	@echo "Running all tests..."
	@for exec in $(EXECUTABLES); do \
		echo "Running $$exec..."; \
		$$exec; \
	done

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -rf $(BIN_DIR) *.d ../Src/*.o

-include $(OBJECTS:.o=.d)