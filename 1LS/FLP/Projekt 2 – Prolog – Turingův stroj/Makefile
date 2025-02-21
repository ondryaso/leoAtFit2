EXE = flp22-log
SWIPL = swipl
SWIPL_FLAGS = --goal=start -o $(EXE) -c turing.pl

all: $(EXE)

$(EXE): turing.pl
	$(SWIPL) $(SWIPL_FLAGS)

clean:
	rm -f $(EXE)