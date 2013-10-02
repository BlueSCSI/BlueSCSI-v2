JUNK =\
	scsi2sd.pcb.bak \
	scsi2sd.pcb.bak0 \
	scsi2sd.pcb.old \
	scsi2sd.sch~ \


SYMBOLS =\
	symbols/7406.sym \
	symbols/CY8C53.sym \
	symbols/FCI-10067847.sym \
	symbols/LD1117.sym \
	symbols/MOLEX8981.sym \
	symbols/SCDA7A0101.sym \
	symbols/wurth-microsd.sym \


all: $(SYMBOLS) scsi2sd.bom scsi2sd.pcb

symbols/%.sym : symbols/%.tragesym
	tragesym $< $@

scsi2sd.bom : scsi2sd.sch
	gnetlist -g partslist3 -o scsi2sd.bom scsi2sd.sch

scsi2sd.pcb : scsi2sd.sch
	gsch2pcb -d symbols scsi2sd.sch

clean:
	rm $(JUNK)

realclean:
	rm $(JUNK)
	rm $(SYMBOLS)

