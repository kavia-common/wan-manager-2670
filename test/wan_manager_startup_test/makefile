include ../test_defines.mk

MOCK_SRC_DIR = ../mocks
MOCK_SOURCES = $(wildcard $(MOCK_SRC_DIR)/*.c)

TEST_UTILS_SRC_DIR = ../test_utils
TEST_UTILS_SOURCES = $(wildcard $(TEST_UTILS_SRC_DIR)/*.c)

TARGET = run_test
SOURCES += $(wildcard ./*.c) $(MOCK_SOURCES) $(TEST_UTILS_SOURCES)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

WRAP_FUNC=-Wl,--wrap=
MOCK_WRAP = amxp_proc_ctrl_new 		\
            amxp_proc_ctrl_start 	\
            amxp_proc_ctrl_stop 	\
            amxp_proc_ctrl_delete	\
            netmodel_isUp	\
            netmodel_getFirstParameter \
            restart
            # uses the real implementation for amxb_get

LDFLAGS += -g $(addprefix $(WRAP_FUNC),$(MOCK_WRAP))

include ../test_targets.mk
