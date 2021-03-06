PVMINC=$(PVM_ROOT)/include
PVMLIB=$(PVM_ROOT)/lib/$(PVM_ARCH) 

all:	$(PVM_HOME)/master $(PVM_HOME)/slave

run:
	$(PVM_HOME)/master

$(PVM_HOME)/master:	master.c def.h data.h utils.h
	$(CC) -g master.c -o $(PVM_HOME)/master -L$(PVMLIB) -I$(PVMINC) -lpvm3 -lgpvm3 `pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0`

$(PVM_HOME)/slave:	slave.c def.h data.h utils.h
	$(CC) -g slave.c -o $(PVM_HOME)/slave -L$(PVMLIB) -I$(PVMINC) -lpvm3 -lgpvm3 -l crypt `pkg-config --cflags glib-2.0` `pkg-config --libs glib-2.0`

clean:
	rm $(PVM_HOME)/master $(PVM_HOME)/slave
	rm -f *.o

