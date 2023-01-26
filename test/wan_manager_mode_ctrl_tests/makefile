include ../test_defines.mk

MOCK_SRC_DIR = ../mocks
MOCK_SOURCES = $(wildcard $(MOCK_SRC_DIR)/*.c)

TEST_UTILS_SRC_DIR = ../test_utils
TEST_UTILS_SOURCES = $(wildcard $(TEST_UTILS_SRC_DIR)/*.c)

TARGET = run_test
SOURCES += $(wildcard ./*.c) $(MOCK_SOURCES) $(TEST_UTILS_SOURCES)
OBJECTS = $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

WRAP_FUNC=-Wl,--wrap=
MOCK_WRAP = dhcpc4_enable \
            dhcpc4_disable \
            dhcpc6_enable \
            dhcpc6_disable \
            ppp_enable \
            ppp_disable

LDFLAGS += -g $(addprefix $(WRAP_FUNC),$(MOCK_WRAP))

include ../test_targets.mk
