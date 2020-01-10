local_src := $(wildcard $(subdirectory)/*.c)

$(eval $(call shared-lib-template, $(subdirectory)/libtopology.so, \
	$(local_src), 0, 3))