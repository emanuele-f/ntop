CFLAGS=-g
LDFLAGS=

# Naming our phony targets
.PHONY: clean install all mac

# Programs
PROGRAMS=addMacAddress addVlanName gdbm_rw
OBJECTS=addMacAddress.o addVlanName.o
SOURCES=addMacAddress.c addVlanName.c

all: mac vlan gdbm_rw
mac: addMacAddress
vlan: addVlanName


gdbm_rw: gdbm_rw.c
	$(CC) $(CFLAGS) gdbm_rw.c -lgdbm -o gdbm_rw

addMacAddress: addMacAddress.c
	$(CC) $(CFLAGS) addMacAddress.c -lgdbm -o addMacAddress

addVlanName: addVlanName.c
	$(CC) $(CFLAGS) addVlanName.c -lgdbm -o addVlanName

# Installing the final product
install:
	@echo "Utility programs do not need to be installed!"

.c.o:
	@rm -f $@
	$(CC) $(CFLAGS) -c $*.c

# Removing the executable and the object files
clean:
	@rm -f $(PROGRAMS)
	@rm -f *.o
	@rm -f *~ #* core
	@echo make clean complete

