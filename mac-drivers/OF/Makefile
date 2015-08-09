

TOKE 		= toke
DRIVERS		= video enet blk sound irqtest misc tty

all: $(addsuffix .ow,$(DRIVERS))
	@true

dummy:

%.ow: forth/%.of dummy
	@echo "====================================================="
	@$(TOKE) -o $@ $<

clean:
	@rm -f *.ow

.PHONY: dummy
