# Makefile
# FLP Project 1: ECDSA
# Author: Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
# Year: 2023

EXEC_SRC_NAME=leo-flp-proj1
EXEC_TARGET_NAME=flp22-fun

.PHONY: clean test build pack

build: $(EXEC_TARGET_NAME)

$(EXEC_TARGET_NAME):
	stack clean
	stack build
	cp "$$(stack path --local-install-root)/bin/$(EXEC_SRC_NAME)" $(EXEC_TARGET_NAME)

clean:
	stack clean
	rm -f flp22-fun flp-fun-xondry02.zip

test: build
	dotnet run --project test/testprog $(EXEC_TARGET_NAME)

pack: build
	rm -rf test/testprog/bin
	rm -rf test/testprog/obj
	zip -r flp-fun-xondry02.zip src test flp-proj1.cabal Setup.hs stack.yaml Makefile README.md