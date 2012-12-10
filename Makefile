TARGET   = inc.exe
CXXFLAGS = -std=c++11

all: $(TARGET)

inc.exe: PELib.o inc.o
	$(CXX) $(CXXFLAGS) -o $@ $^

PELib.o: PELib.cpp PELib.h
inc.o: inc.cpp PELib.h

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET)
