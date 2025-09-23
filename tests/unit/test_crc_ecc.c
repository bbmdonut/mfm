#include "unity.h"
#include "crc_ecc.h"
#include <stdint.h>

void setUp(void) {
    // Set up code here
}

void tearDown(void) {
    // Tear down code here
}

void test_crc_revbits_basic(void) {
    // Test reversing bits
    // 0b00100001 (0x21) should become 0b10000100 (0x84)
    uint64_t result = crc_revbits(0x21, 8);
    TEST_ASSERT_EQUAL_HEX64(0x84, result);
}

void test_crc_revbits_single_bit(void) {
    // Test single bit reversal
    uint64_t result = crc_revbits(0x01, 8);
    TEST_ASSERT_EQUAL_HEX64(0x80, result);

    result = crc_revbits(0x80, 8);
    TEST_ASSERT_EQUAL_HEX64(0x01, result);
}

void test_crc_revbits_zero(void) {
    // Test zero - should remain zero
    uint64_t result = crc_revbits(0x00, 8);
    TEST_ASSERT_EQUAL_HEX64(0x00, result);
}

void test_crc64_basic(void) {
    // Test basic CRC calculation
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    CRC_INFO crc_info = {
        .init_value = 0,
        .poly = 0x1021, // CRC-16-CCITT polynomial
        .length = 16,
        .ecc_max_span = 0
    };

    uint64_t result = crc64(test_data, sizeof(test_data), &crc_info);
    // Just verify it returns something consistent (not zero for non-zero data)
    TEST_ASSERT_NOT_EQUAL(0, result);
}

void test_checksum64_basic(void) {
    // Test basic checksum
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    CRC_INFO checksum_info = {
        .init_value = 0,
        .poly = 0,
        .length = 16,
        .ecc_max_span = 0
    };

    uint64_t result = checksum64(test_data, sizeof(test_data), &checksum_info);
    // Basic checksum should be sum of bytes
    TEST_ASSERT_EQUAL_HEX64(0x0A, result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_crc_revbits_basic);
    RUN_TEST(test_crc_revbits_single_bit);
    RUN_TEST(test_crc_revbits_zero);
    RUN_TEST(test_crc64_basic);
    RUN_TEST(test_checksum64_basic);

    return UNITY_END();
}
