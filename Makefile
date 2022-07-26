all: sim-hello sim-negate sim-extend

sim-%: sim-%.c gb-sim.h
	$(CC) -o $@ $<
