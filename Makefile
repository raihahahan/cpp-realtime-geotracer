CXX = g++
CXXFLAGS = -Wall -std=c++20 -Iinclude
TARGET = geotracer
LDFLAGS = -lcurl

SRC = src/main.cpp src/utils.cpp src/net_helpers.cpp src/tcp_packet.cpp src/probe.cpp src/geolocation.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET).exe
