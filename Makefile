CC            = gcc
CXX           = gcc
LINK          = gcc

CXXFLAGS      = 
CFLAGS        = 
INCPATH       =	mavlink/include/mavlink/v1.0/common

SOURCES       = main.c
OBJECTS       = main.o

TARGET        = px4_joystick_interface
PATHDIR	      = px4_joystick_interface

first: all


.SUFFIXES: .o .c .cpp .cc .cxx .C

.cpp.o:	
	$(CXX) -c $(CXXFLAGS) -I $(INCPATH) -o "$@" "$<"


.cc.o:
	$(CXX) -c $(CXXFLAGS) -I $(INCPATH) -o "$@" "$<"


.cxx.o:
	$(CXX) -c $(CXXFLAGS) -I $(INCPATH) -o "$@" "$<"


.C.o:
	$(CXX) -c $(CXXFLAGS) -I $(INCPATH) -o "$@" "$<"

.c.o:
	$(CXX) -c $(CFLAGS) -I $(INCPATH) -o "$@" "$<"


all: $(TARGET) $(TARGETARM)

$(TARGET):  $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) -lrt
	
clean: 
	rm -f $(OBJECTS) $(TARGET)





