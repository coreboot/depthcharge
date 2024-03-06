# SPDX-License-Identifier: GPL-2.0

# Place the build output in one of two places depending on COV variable,
# so that code build with the code coverage never mixes with code build
# without it.
ifeq ($(COV),1)
testobj := $(obj)/coverage
covinitobj := $(obj)/coverage_init/$(BOARD)
else
testobj := $(obj)/tests
endif
testobj := $(abspath $(testobj))
objutil := $(testobj)/util
objext := $(testobj)/external

coverage-reports-dir := $(testobj)/coverage_reports

include $(src)/tests/Makefile.common

ifeq ($(COV),1)
TEST_CFLAGS += --coverage
TEST_LDFLAGS += --coverage
endif

alltests :=
subdirs := tests/arch tests/base tests/board tests/boot tests/debug \
	tests/diag tests/drivers tests/image tests/net tests/netboot tests/vboot

define tests-handler
alltests += $(1)$(2)
$(foreach attribute,$(attributes),
	$(eval $(1)$(2)-$(attribute) += $($(2)-$(attribute))))
$(foreach attribute,$(attributes),
	$(eval $(2)-$(attribute) := ))
endef

$(call add-special-class, tests)
$(call evaluate_subdirs)
$(foreach test, $(alltests), \
	$(eval $(test)-srcobjs := $(addprefix $(testobj)/$(test)/, \
		$(patsubst %.c,%.o,$(filter src/%,$($(test)-srcs))))) \
	$(eval $(test)-objs := $(addprefix $(testobj)/$(test)/, \
		$(patsubst %.c,%.o,$($(test)-srcs))))\
	$(eval $(test)-objs += $(addprefix $(testobj)/$(test)/, \
		$(patsubst %.c,%.o,$(default_mocks-srcs)))))
$(foreach test, $(alltests), \
	$(eval $(test)-bin := $(testobj)/$(test)/run))
$(foreach test, $(alltests), \
	$(eval $(call TEST_CC_template,$(test))))
$(foreach test, $(alltests), \
	$(eval all-test-objs += $($(test)-objs)))
$(foreach test, $(alltests), \
	$(eval test-bins += $($(test)-bin)))

DEPENDENCIES += $(addsuffix .d,$(basename $(all-test-objs)))
-include $(DEPENDENCIES)
.PHONY: $(alltests) $(addprefix clean-,$(alltests))
.PHONY: coverage-report coverage-report-board clean-coverage-report
.PHONY: unit-tests build-unit-tests run-unit-tests clean-unit-tests
.PHONY: list-unit-tests help-unit-tests

ifeq ($(JUNIT_OUTPUT),y)
$(alltests): export CMOCKA_MESSAGE_OUTPUT=xml
$(alltests): export CMOCKA_XML_FILE=$(testobj)/junit-$(subst /,_,$^)-%g.xml
endif

$(alltests): $$($$(@)-bin)
	rm -f $(testobj)/junit-$(subst /,_,$^)-*.xml $(testobj)/$(subst /,_,$^).failed
	$^ || echo failed > $(testobj)/$(subst /,_,$^).failed

# Build code coverage report by collecting all the gcov files into a single
# report. If COV is not set, this might be a user error, and they are trying
# to generate a coverage report without first having built and run the code
# with code coverage. Absence of COV=1 will be corrected, but only for report
# generation and cleaning.
ifeq ($(COV),1)
TEST_DOTCONFIG := $(testobj)/.config

$(TEST_DOTCONFIG):
	mkdir -p $(dir $@)
	+unset COV && $(MAKE) \
		obj=$(covinitobj) \
		EC_HEADERS=$(EC_HEADERS) \
		LIBPAYLOAD_DIR=$(LIBPAYLOAD_DIR) \
		DOTCONFIG=$(TEST_DOTCONFIG) \
		BOARD=$(BOARD) \
		defconfig

coverage-report:
	lcov -o $(testobj)/tests.info -c -d $(testobj) --exclude '$(testsrc)/*' \
		-b $(src)
	genhtml -q -o $(coverage-reports-dir) -t "depthcharge unit tests" \
		-s $(testobj)/tests.info

coverage-report-board: $(TEST_DOTCONFIG) $(LIBPAYLOAD_DIR)/lib/libpayload.a
	+$(MAKE) \
		obj=$(covinitobj) \
		EC_HEADERS=$(EC_HEADERS) \
		LIBPAYLOAD_DIR=$(LIBPAYLOAD_DIR) \
		DOTCONFIG=$(TEST_DOTCONFIG) \
		SOURCE_DEBUG=1 \
		COV=1 \
		coverage-init
	lcov -o $(testobj)/board_tests.info -c -d $(testobj) \
		--exclude '$(testsrc)/*' --exclude '$(libpayloadobj)/*' \
		--include '$(src)/*' -b $(src)
	lcov -a $(covinitobj)/init.info -a $(testobj)/board_tests.info \
		-o $(testobj)/total.info
	genhtml -q -o $(coverage-reports-dir) \
		-t "depthcharge unit tests for $(BOARD)" \
		-s $(testobj)/total.info

clean-coverage-report:
	rm -Rf $(coverage-reports-dir)
else
coverage-report:
	COV=1 V=$(V) $(MAKE) coverage-report

coverage-report-board:
	COV=1 V=$(V) $(MAKE) coverage-report-board

clean-coverage-report:
	COV=1 V=$(V) $(MAKE) clean-coverage-report
endif

unit-tests: build-unit-tests run-unit-tests

build-unit-tests: $(test-bins)

run-unit-tests: $(alltests)
	if [ `find $(testobj) -name '*.failed' | wc -l` -gt 0 ]; then \
		echo "**********************"; \
		echo "     TESTS FAILED     "; \
		echo "**********************"; \
		exit 1; \
	else \
		echo "**********************"; \
		echo "   ALL TESTS PASSED   "; \
		echo "**********************"; \
		exit 0; \
	fi

$(addprefix clean-,$(alltests)): clean-%:
	rm -rf $(testobj)/$*

clean-unit-tests:
	rm -rf $(testobj)

list-unit-tests:
	@echo "unit-tests:"
	for t in $(sort $(alltests)); do \
		echo "  $$t"; \
	done

help-unit-tests help::
	@echo  '*** unit-tests targets ***'
	@echo  '  Use "COV=1 make [targets]" to enable building unit tests with code coverage'
	@echo  '  unit-tests            - Run all unit-tests from tests/'
	@echo  '  clean-unit-tests      - Remove unit-tests build artifacts'
	@echo  '  list-unit-tests       - List all unit-tests'
	@echo  '  <unit-test>           - Build and run single unit-test'
	@echo  '  clean-<unit-test>     - Remove single unit-test build artifacts'
	@echo  '  coverage-report       - Generate code coverage report'
	@echo  '  clean-coverage-report - Remove code coverage report'
	@echo
