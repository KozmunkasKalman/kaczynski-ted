#Jan 1, 2026 Vani1-2 <giovannirafanan609@gmail.com>

CXX = g++
CXXFLAGS = -std=c++23 -lncursesw -g
TARGET = bin/kcz
SRC = src/main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@echo "Installing binary to /usr/bin/ (requires sudo)..."
	sudo cp $(TARGET) /usr/bin/kcz
	@echo "Installing configuration to $(HOME)/.config/kaczynski/..."
	mkdir -p $(HOME)/.config/kaczynski/
	cp unabombrc $(HOME)/.config/kaczynski/
	@echo "Installation complete."	