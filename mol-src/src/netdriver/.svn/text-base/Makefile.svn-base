
DEFINE_KERNEL_SOURCE	= yes
include			../../config/Makefile.top

LV			:= $(shell test -f $(KERNEL_SOURCE)/Rules.make && echo 24 || echo 26)
MP			:= $(shell test -f $(KERNEL_SOURCE)/Rules.make && echo "o" || echo "ko")
MOD_DEST		= $(mollib)/modules/`cat $(ODIR)/.kuname`

KILL_WARNING_26		= 2>&1 | sed -e '/\*\*\*.*SUBDIRS/ d' \
				     -e '/\*\*\*.*inconsistencies/ d' \
				     -e '/scripts\/Makefile.*complain if/ d' 1>&2

ABSODIR			:= $(shell test -d $(ODIR) || $(INSTALL) -d $(ODIR) ; test -d $(ODIR) ; cd $(ODIR) ; pwd )

setup-tree-common:
	@test -d $(ODIR) || $(INSTALL) -d $(ODIR)
	@rm -f $(ODIR)/dir1
	@ln -sf ../../../stree/src/netdriver $(ODIR)/dir1
	@ln -sf dir1/kuname.c dir1/sheep.c $(ODIR)/
	@ln -sf dir1/Makefile.$(LV) $(ODIR)/Makefile

setup-tree-26: setup-tree-common
	ln -sf $(KERNEL_SOURCE)/drivers/net/tun.c $(ODIR)/ || exit 1 ; \

setup-tree-24: setup-tree-common
	@for x in tun.c if_tun.h ; do \
		ln -sf dir1/$$x $(ODIR)/ || exit 1 ; \
	done

all-local: setup-tree-$(LV)
	@$(MAKE) --no-print-directory -C $(KERNEL_SOURCE) "SUBDIRS=$(ABSODIR)" \
		BUILD_SHEEP=$(CONFIG_SHEEP:y=m) BUILD_TAP=$(CONFIG_TAP:y=m) \
		BUILD_TUN=$(CONFIG_TUN:y=m) \
		modules $(KILL_WARNING_$(LV))

	@test -d $(MOD_DEST) || $(INSTALL) -d $(MOD_DEST)
	@for x in tun sheep ; do \
		[ -f $(ODIR)/$$x.$(MP) ] || continue ; \
		$(NM) $(ODIR)/$$x.$(MP) | grep -q init_module || continue ; \
		ln -f $(ODIR)/$$x.$(MP) $(MOD_DEST)/ || exit 1 ; \
	done

#@$(STRIP) -g $(MOD_DEST)/$$x.$(MP)


clean-local:
	@rm -rf $(top_odir)/build/src/netdriver

include		$(rules)/Rules.make
