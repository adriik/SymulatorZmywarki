SUBDIRS = $(shell ls -d */)
all:
	for dir in $(SUBDIRS) ; do \
		if [ "$$dir" != "logi/" ] ; then \
			make -C  $$dir ; \
		fi \
	done