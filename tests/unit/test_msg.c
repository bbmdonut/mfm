#include "unity.h"
#include "msg.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {
    // Reset message mask to default
    msg_set_err_mask(MSG_FATAL | MSG_ERR_SERIOUS | MSG_ERR | MSG_ERR_SUMMARY);
}

void tearDown(void) {
    // Clean up
}

void test_msg_set_get_err_mask(void) {
    uint32_t original_mask = msg_get_err_mask();

    // Test setting a new mask
    uint32_t new_mask = MSG_DEBUG | MSG_INFO;
    uint32_t returned_mask = msg_set_err_mask(new_mask);

    // Should return the previous mask
    TEST_ASSERT_EQUAL_UINT32(original_mask, returned_mask);

    // Should have set the new mask
    TEST_ASSERT_EQUAL_UINT32(new_mask, msg_get_err_mask());
}

void test_msg_err_mask_individual_flags(void) {
    // Test each flag individually
    uint32_t flags[] = {
        MSG_DEBUG_DATA, MSG_DEBUG, MSG_INFO, MSG_PROGRESS,
        MSG_ERR, MSG_INFO_SUMMARY, MSG_ERR_SERIOUS,
        MSG_ERR_SUMMARY, MSG_FATAL, MSG_STATS, MSG_FORMAT
    };

    for (size_t i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
        msg_set_err_mask(flags[i]);
        TEST_ASSERT_EQUAL_UINT32(flags[i], msg_get_err_mask());
    }
}

void test_msg_err_mask_combined_flags(void) {
    // Test combining multiple flags
    uint32_t combined = MSG_ERR | MSG_FATAL | MSG_DEBUG;
    msg_set_err_mask(combined);
    TEST_ASSERT_EQUAL_UINT32(combined, msg_get_err_mask());
}

void test_msg_malloc_valid_size(void) {
    // Test allocating memory
    void *ptr = msg_malloc(100, "test allocation");
    TEST_ASSERT_NOT_NULL(ptr);

    // Clean up
    free(ptr);
}

void test_msg_malloc_zero_size(void) {
    // Test allocating zero bytes (should still return valid pointer or NULL)
    void *ptr = msg_malloc(0, "zero size test");
    // This is implementation dependent - just verify it doesn't crash
    if (ptr) {
        free(ptr);
    }
    TEST_ASSERT_TRUE(1); // If we get here, no crash occurred
}

void test_msg_set_logfile(void) {
    // Test setting log file (basic test - just verify it doesn't crash)
    FILE *tmp_file = tmpfile();
    if (tmp_file) {
        msg_set_logfile(tmp_file, MSG_ERR | MSG_FATAL);
        // Reset to no log file
        msg_set_logfile(NULL, 0);
        fclose(tmp_file);
    }
    TEST_ASSERT_TRUE(1); // If we get here, no crash occurred
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_msg_set_get_err_mask);
    RUN_TEST(test_msg_err_mask_individual_flags);
    RUN_TEST(test_msg_err_mask_combined_flags);
    RUN_TEST(test_msg_malloc_valid_size);
    RUN_TEST(test_msg_malloc_zero_size);
    RUN_TEST(test_msg_set_logfile);

    return UNITY_END();
}
