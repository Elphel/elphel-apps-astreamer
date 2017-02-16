# Runs 'make', 'make install', and 'make clean' in specified subdirectories
SUBDIRS := src
INSTALLDIRS = $(SUBDIRS:%=install-%)
CLEANDIRS =   $(SUBDIRS:%=clean-%)

#TARGETDIR=$(DESTDIR)/www/pages

all: $(SUBDIRS)
	@echo "make all top"

$(SUBDIRS):
	$(MAKE) -C $@

install: $(INSTALLDIRS)
	@echo "make install top"

$(INSTALLDIRS): 
	$(MAKE) -C $(@:install-%=%) install

clean: $(CLEANDIRS)
	@echo "make clean top"

$(CLEANDIRS): 
	$(MAKE) -C $(@:clean-%=%) clean

.PHONY: all install clean $(SUBDIRS) $(INSTALLDIRS) $(CLEANDIRS)

