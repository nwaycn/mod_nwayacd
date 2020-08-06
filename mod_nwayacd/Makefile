BASE=/usr/local/freeswitch

SOURCES = mod_nwayacd.c database.c

OBJS = $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCES)))
PQROOT=/usr/local/pgsql
PQINCLUDE=-I${PQROOT}/include
PQLIBRARY=-L${PQROOT}/lib 
#-lnlsCppOpu

CXXFLAGS = -fpermissive

%.o: %.c
	g++ -I${BASE}/include -I./ ${CXXFLAGS} ${PQINCLUDE} ${PQLIBRARY}  -fPIC -c $< -o $@  

%.o: %.cpp
	g++ -I${BASE}/include  ${PQINCLUDE} ${PQLIBRARY}  -fPIC -c $< -o $@ ${CXXFLAGS} -I./  


mod_nwayacd.so: $(OBJS)
	g++ -L${BASE}/lib/  ${PQINCLUDE} ${PQLIBRARY} ${CXXFLAGS} -lfreeswitch -L./ -lm -shared -fPIC -o mod_nwayacd.so $(OBJS) -lstdc++ -lpq


	
clean : 
	rm -rf $(OBJS) mod_nwayacd.so
