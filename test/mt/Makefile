rootdir := $(shell pwd)/../../..
project := ubuntu-base

include $(rootdir)/build/common/inc.mk
ifeq ($(hostmaketype), windows)
rootdir := ../../..
endif

####
##  Application targets
####
DEP_HEADERS += $(incdir)/libonplatform $(incdir)/libontimer     \
               $(incdir)/libonmsgagent $(incdir)/libonevent

apps = mt_timer

mt_timer_SRCS = $(rootdir)/libontimer/test/mt/main.c
mt_timer_ARCHIVES = libontimer libonevent libonplatform msgagent libonutils
mt_timer_LDLIBS = -pthread -ltcmalloc_minimal_debug
mt_timer_DISTSOLIBS =

include $(rootdir)/build/utils/app.mk
