#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"

/**
* This function should:
*   1) Call the my_username() function in Test_assignment_validate.c to get your hard coded username.
*   2) Obtain the value returned from function malloc_username_from_conf_file() in username-from-conf-file.h within
*       the assignment autotest submodule at assignment-autotest/test/assignment1/
*   3) Use unity assertion TEST_ASSERT_EQUAL_STRING_MESSAGE the two strings are equal.  See
*       the [unity assertion reference](https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md)
*/
void test_validate_my_username()
{
    // 1) Call my_username() to get the hardcoded username
    const char* expected_username = my_username();

    // 2) Call malloc_username_from_conf_file() to get the username from conf file
    char* actual_username = malloc_username_from_conf_file();

    // 3) Assert that they are equal
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        expected_username,
        actual_username,
        "Username from my_username() does not match username from conf file"
    );

    // Free the malloc'd string to avoid memory leak
    free(actual_username);
}

