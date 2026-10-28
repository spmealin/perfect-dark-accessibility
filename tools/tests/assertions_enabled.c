/* Compile this alongside every fixture, including Release configurations.
 * A passing test with its assert expressions compiled out is not a test. */
#include <assert.h>

#ifdef NDEBUG
#error "Accessibility regression tests must be built with assertions enabled."
#endif
