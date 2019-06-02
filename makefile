
#
#fft routine
#

objs	=	main.o pt-play.o convert.o display.o fft.o lang.o req.o parallel.o sim.o
target	=	spectrum
libs	=	-lc -lreqtoolsnb

all:	$(target)

%.o: %.a
	a68k -q -r $<

%.o: %.c
	cc -sob $<

$(target): $(objs)
	ln +Q $(objs) -o $@ $(libs)
