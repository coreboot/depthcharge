# Writing Unit Tests for Depthcharge
This document aims to guide developers through the process of creating and
running unit tests for Depthcharge.


## Setup
Before running any tests, the environment has to be prepared. Unit tests
require CMocka-1.1.5 or compatible installed in the system.
In the Chromium OS SDK it can be installed, e.g. by invoking
`sudo emerge dev-util/cmocka-1.1.5`.
Furthermore, below is the assumed source tree for the Chromium OS SDK
and required variables with their list of default values.

### Assumed source tree
```
src
├── platform
|   ├── depthcharge <- depthcharge source
|   ├── vboot_reference <- VB_SOURCE
|   └── ec
|       └── util <- EC_HEADERS
|           └── ec_commands.h
|
└──third_party
   └── coreboot
       └──payloads
          └── libpayload <- LP_SOURCE default
```

### Dependency variables
`LP_SOURCE` - libpayload source path. \
Defaults: `../libpayload` ; `$(src)/../../third_party/coreboot/payloads/libpayload`

`VB_SOURCE` - vboot reference implementation source path. \
Defaults: `../../3rdparty/vboot` ; `$(src)/../vboot_reference`

`EC_HEADERS` - ec-headers path. Has to contain ec_commands.h \
Default: `$(src)/../ec/util`


## Analysis of unit under test
As an example of unit under test (referred hereafter as UUT "Unit Under Test"),
`src/vboot/secdata_tpm.c` will be used.

### Adding unit tests
In order to keep the tree clean, the `tests/` directory should mimic the `src/`
directory, so that test harness code is placed in a location corresponding to
UUT. Furthermore, the naming convention is to add the suffix `-test` to the UUT
name when creating a new test harness file.

> Considering that UUT is `src/vboot/secdata_tpm.c`, the test file should be
> named `tests/vboot/secdata_tpm-test.c`.

Every directory under `tests/` (except _include_ and _stubs_) should contain
a `Makefile.inc`, similar to what can be seen under the `src/` directory.
New unit tests can be registered by **appending** the test name to the `tests-y`
variable.

```makefile
tests-y += secdata_tpm-test
```

The next step is to list all required source files, which should be linked
together in order to create the test binary. Usually one test requires only two
files - UUT and the test harness code, but sometimes other files are needed to
meet all dependencies. Source files are registered by **appending** them to the
`<test_name>-srcs` variable.

```makefile
secdata_tpm-test-srcs += tests/vboot/secdata_tpm-test.c
secdata_tpm-test-srcs += src/vboot/secdata_tpm.c
secdata_tpm-test-srcs += src/vboot/callbacks/tpm.c
```

### Providing config.h values
Some UUTs might require `config.h` entries. Unit testing infrastructure does
not rely on any particular dotconfig, but rather generates config.h file using
variable names and values pairs from the `<test_name>-config` variable.

```makefile
example-test-config += CONFIG_BOOL_VALUE=0
example-test-config += CONFIG_INT_VALUE=128
example-test-config += CONFIG_HEX_VALUE=0xBE60
example-test-config += CONFIG_STRING_VALUE=\"Example text value\"
```

**IMPORTANT:** Notice escaped quotes around text value.


## Makefile unit tests targets
### Help
To list all available test targets, invoke `make help-unit-tests`.

```
*** unit-tests targets ***
  unit-tests            - Run all unit-tests from tests/
  clean-unit-tests      - Remove unit-tests build artifacts
  list-unit-tests       - List all unit-tests
  <unit-test>           - Build and run single unit-test
  clean-<unit-test>     - Remove single unit-test build artifacts
```

### Running unit tests
One unit test can be compiled and run by invoking `make tests/<module>/<test>`,
for example `make tests/vboot/secdata_tpm-test`.
To run all unit-tests (whole suite) invoke `make unit-tests`.

Console output of UUT is not shown by default. Pass `TEST_PRINT=1` to `make` to
enable it.

## Analysis of unit under test
First, it is necessary to precisely establish what we want to test in
a particular module. Usually this will be an externally exposed API, which can
be used by other modules.

In case of our UUT, the API consist of four methods:
```c
uint32_t secdata_firmware_write(struct vb2_context *ctx);
uint32_t secdata_kernel_write(struct vb2_context *ctx);
uint32_t secdata_kernel_lock(struct vb2_context *ctx);
uint32_t secdata_fwmp_read(struct vb2_context *ctx);
```

Let's focus on `test_secdata_kernel_write()`, as it requires many methods
of testing.

Once the API is defined, the next question is **what** this API is doing.
In other words, what outputs we are expecting from particular functions, when
providing particular input parameters.

```c
uint32_t secdata_kernel_write(struct vb2_context *ctx)
```

From comments present in the header file containing the function declaration,
we can learn that this function is "for ... manipulating ... secure data spaces
stored in the TPM NVRAM". Furthermore, comments tell us, that the function
returns `TPM_SUCCESS` on success, and non-zero value on error, so `TPM_E_*`
values.

Now let's look at the function parameters. It takes a pointer to
`struct vb2_context`. By digging further, we can see, that it contains two
fields interesting for us: `flags` and `secdata_kernel`. The comment before
`flags` tell us that this field uses `enum vb2_context_flag` as its values (and
those are bit values, so `flags` is a bitfield). The comment before the second
field tells us, that `VB2_CONTEXT_SECDATA_KERNEL_CHANGED` flag has to be set
for the function to take effect.

If we go back to the function header file, we can find `KERNEL_NV_INDEX`. This
is the index used by the function to write data to the TPM NVRAM, using
TPM Lightweight Command Library (TLCL) functions provided the vboot_reference,
especially `TlclWrite()`. So this is the first external dependency.

We can look at code of `secdata_kernel_write()` to check, if our assumptions
were correct. We can see, that the function does not call `TlclWrite()`
directly, but uses local `secdata_safe_write()` instead. This function tries to
write data to the provided TPM NVRAM index. If `TlclWrite()` returns
`TPM_E_MAXNVWRITES`, then it tries to clear and re-enable TPM NVRAM by calling
`TlclForceClear()`, `TlclSetEnable()`, `TlclSetDeactivated(0)`, and again tries
to write the data. Regardless, `secdata_kernel_write()` returns the error code.

The next step is to determine all external dependencies of UUT in order to mock
them out. Usually we want to isolate the UUT as much as possible, so that
the test result depends **only** on the behavior of UUT and not on the other
modules. While some software dependencies may be hard to mock (for example due
to complicated dependencies) and thus should be simply added to the test
sources, all hardware dependencies need to be mocked out, since
in the user-space host environment, a target's hardware is not available.

After looking again at the code, four mockable dependencies can be identified.
```c
uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length);
uint32_t TlclForceClear(void);
uint32_t TlclSetEnable(void);
uint32_t TlclSetDeactivated(uint8_t flag);
```

Why not to mock `secdata_clear_and_reenable()` instead of last three functions
listed above? The mentioned function is declared as **static**, so it is not
easy to mock it without workarounds.

**IMPORTANT**: Unit tests should rely on API and documentation, not
implementation. Latter can lead to unit tests that pass even, when UUT has bugs.

## Writing new tests
In Depthcharge, [Cmocka](https://cmocka.org/) is used as unit test framework.
The project has exhaustive [API documentation](https://api.cmocka.org/).
Let’s see how we may incorporate it when writing tests.

### Assertions
Testing the UUT consists of calling the functions in the UUT and comparing
the returned values to the expected values. Cmocka implements
[a set of assert macros](https://api.cmocka.org/group__cmocka__asserts.html)
to compare a value with an expected value. If the two values do not match, the
test fails with an error message.

```c
/* Write to unchanged kernel should be successful */
ctx->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
assert_int_equal(secdata_kernel_write(ctx), TPM_SUCCESS);
```

In this example, the simplest test is to call UUT with
`VB2_CONTEXT_SECDATA_KERNEL_CHANGED` unset. This way UUT should return
TPM_SUCCESS. Return value can be compared with the expected one using
`assert_int_equal()`.

### Mocks

#### Overview
Many Depthcharge modules might require access to hardware or to specific memory
addresses. Because of this, one of the most important and challenging part of
writing tests is to design and implement mocks. Mock is a software component
which implements the API of another component, so that the test can verify that
certain functions are or are not called, verify the parameters passed to those
functions, and specify the return values from those functions. Mocks are
especially useful when the API to be implemented is one that accesses hardware
components.

When creating a mock, the developer implements the same API as the module being
mocked. Such mock may, for example, mimic a memory writer to simulate access to
a specific memory.

For the purpose of this test, we have to mock `TlclWrite()` function.

```c
/* This mock requires two calls to will_return(). First sets read buffer,
   second sets return value. */
uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length)
{
	check_expected(index);
	check_expected(length);
	check_expected_ptr(data);

	memcpy(mock_ptr_type(void *), data, length);

	return mock_type(uint32_t);
}
```

This is quite complex mock of function, but it allows to check all parameters
and return required value. First, all parameters are checked by
[`check_expected*()`](https://api.cmocka.org/group__cmocka__param.html)
functions for expected values previously set using `expect_*()`.
Next, data from the provided buffer is written to the buffer returned by
[`mock_ptr_type(void *)`](https://api.cmocka.org/group__cmocka__mock.html).
At the end, the function returns the value previously set using
`will_return*()`.

You probably noticed two calls to `mock*()` functions. As previously mentioned,
values returned by them, are set using `will_return*()` functions. Those
functions can be used multiple times to provide data to mocks. Subsequent calls
to `will_return*()` will add provided values to a FIFO list, which values are
popped by `mock*()`. This way we can provide data to mocks without using any
additional variables.

Example:
```c
/* Check params provided to mock by UUT */
expect_value(TlclWrite, index, KERNEL_NV_INDEX);
expect_value(TlclWrite, length, VB2_SECDATA_KERNEL_MIN_SIZE);
expect_not_value(TlclWrite, data, (uintptr_t)NULL);
will_return(TlclWrite, (uintptr_t)buf);
will_return(TlclWrite, 0);
```

**IMPORTANT:** Calling `will_return_always()` and `will_return_maybe()` might
not work correctly with approach above.

UUT requires three more mocks. All are almost the same, so we will take a look
at `TlslForceClear()`.

```c
uint32_t TlclForceClear(void)
{
	function_called();

	return mock_type(uint32_t);
}
```

The only difference from previous mock is call to `function_called()`. This
function expects `expect_function_call*()` to be called earlier. If it was not,
then it will report an error. This is a great way to ensure function was called.
It can also help with tests development, as it will tell the developer, that
there was an unexpected call to the function.

#### Registering mocks
In order to replace original functions with their mocks, it is necessary to
make all of its occurrences within UUT, and its dependencies weak (weak symbol).
Unit tests in Depthcharge get all names from `<test_name>-mocks` and use objcopy
to weaken those corresponding symbols in all object files produced from source
files from `src/` and in archives (static libraries). This way, the developer
can mock functions like `TlclWrite()` from `vboot_reference`, mentioned above.

Example:
```makefile
secdata_tpm-test-mocks += TlclWrite TlclRead TlclForceClear \
	TlclSetEnable TlclSetDeactivated
```

### Test runner
Finally, the developer needs t implement the `main()` function. All tests should
be registered there, and invoked by
[CMocka test runner](https://api.cmocka.org/group__cmocka__exec.html).

```c
int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_secdata_kernel_write,
							   setup_kernel_test),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
```

This test requires extra setup function, which can be passed to
`cmocka_unit_test_setup()`.

```c
static int setup_kernel_test(void **state)
{
	memset(workbuf_kernel, 0, sizeof(workbuf_kernel));
	return 0;
}
```
