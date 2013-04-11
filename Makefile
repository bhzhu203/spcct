
SUBDIRS = src

.PHONY : $(SUBDIRS) all

all:	$(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@


.PHONY : install clean uninstall

install clean uninstall: 
	$(MAKE) -C $(SUBDIRS) $@
