include ../test_defines.mk

MOCK_SRC_DIR = ../mocks
MOCK_SOURCES = $(wildcard $(MOCK_SRC_DIR)/*.c)

TEST_UTILS_SRC_DIR = ../test_utils
TEST_UTILS_SOURCES = $(wildcard $(TEST_UTILS_SRC_DIR)/*.c)

TARGET = run_test
SOURCES += $(wildcard ./*.c) $(MOCK_SOURCES) $(TEST_UTILS_SOURCES)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

WRAP_FUNC=-Wl,--wrap=

MOCK_WRAP = netmodel_initialize		\
            netmodel_cleanup		\
            netmodel_openQuery_getFirstParameter \
            netmodel_openQuery_getIntfs \
            netmodel_openQuery_isUp \
            netmodel_getFirstParameter \
            netmodel_closeQuery \
            netmodel_clearFlag \
            amxb_set \
            amxb_set_multiple \
            amxb_get \
            amxb_add \
            amxb_del \
            amxb_call \
            amxm_execute_function \
            amxc_llist_it_take \
            amxc_string_list_it_free

LDFLAGS += -g $(addprefix $(WRAP_FUNC),$(MOCK_WRAP))

include ../test_targets.mk
