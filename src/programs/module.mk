local_src := $(wildcard $(subdirectory)/*.c)

$(eval $(call make-program,$(subdirectory)/cpumasks,$(subdirectory)/cpumasks.c))
$(eval $(call make-program,$(subdirectory)/cachelist,$(subdirectory)/cachelist.c))
