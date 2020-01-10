tests_src = $(wildcard $(subdirectory)/*.c)
test_inst_prefix := $(subdirectory)/install

# fake sysfs directories created for testcases
cleanfiles += $(subst .c,.sysfs,$(tests_src))

$(foreach test,$(tests_src),$(eval $(call test-template,$(test))))
