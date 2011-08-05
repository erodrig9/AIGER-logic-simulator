
OBJ = aignode.o aig.o aiger_cc.o main.o
OBJS = $(OBJ)

#PLATFORM = __APPLE_MAC_OS__
PLATFORM = __LINUX__
#PLATFORM = __SOLARIS__

CC = g++ -DDEBUG_MODE -D$(PLATFORM) -g -Wno-deprecated
#CC = g++ -DDEBUG_MODE -D$(PLATFORM) -I$(INCLUDE) -g -pg -Wno-deprecated

aig : $(OBJS)
	$(CC) -o sim $(OBJS)
	
aig.o: aig.h aig.cc aignode.h
	$(CC) -c $*.cc
	
aignode.o : aignode.h aignode.cc
	$(CC) -c $*.cc

aiger_cc.o : aiger_cc.h aiger_cc.cc
	$(CC) -c $*.cc

main.o: main.cc aig.h aiger_cc.h
	$(CC) -c $*.cc
	
clean:
	rm *.o
