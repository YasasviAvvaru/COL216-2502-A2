# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall
SRC = Processor.cpp BranchPredictor.cpp ExecutionUnit.cpp LoadStoreQueue.cpp

compile:
	@echo "Compiling simulator:"
	$(CXX) $(CXXFLAGS) $(FILE) $(SRC) -o main
	@echo "Build successful, 'main' created."

run:
	@echo "Preprocessing $(FILE)..."
	$(CXX) $(CXXFLAGS) compiler.cpp -o compiler
	./compiler $(FILE)
	@echo "Preprocessing complete."
