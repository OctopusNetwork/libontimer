COMMON_INCLUDE_DIRS += $(rootdir)/$(MODULE)/include     \
                       $(incdir)/libonplatform            \
                       $(incdir)/libonevent               \
                       $(incdir)/libonutils               \
                       $(incdir)/libonmsgagent

COMMON_SRC_FILES := $(rootdir)/$(MODULE)/src/libontimer.c

COMMON_INST_HEADER_DIRS += $(rootdir)/$(MODULE)/include
