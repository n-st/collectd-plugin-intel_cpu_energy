CC = gcc
CFLAGS = -Wall -DHAVE_CONFIG_H -shared -fPIC -g
INCLUDES = -I. -I/usr/include/collectd/ -I/usr/include/
LFLAGS = -L.
LIBS = -lm
SRCS = cpuid.c intel_cpu_energy.c msr.c rapl.c
OBJS = $(SRCS:.c=.o)
MAIN = intel_cpu_energy.so
DEPS = cpuid.h msr.h rapl.h

all:    $(MAIN)

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJS) *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
