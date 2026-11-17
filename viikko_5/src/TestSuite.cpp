#include <gtest/gtest.h>
#include "TimeParser.h"

// Test suite: TimeParserTest
TEST(TimeParserTest, TestCaseCorrectTime) {
    char time_min[] = "000000";
    ASSERT_EQ(time_parse(time_min), 0);

    char time_test[] = "000010";
    ASSERT_EQ(time_parse(time_test), 10);

    char time_test2[] = "000101";
    ASSERT_EQ(time_parse(time_test2), 61);

    char time_max[] = "235959";
    ASSERT_EQ(time_parse(time_max), 86399);
}

TEST(TimeParserTest, TestCaseIncorrectTime) {

    char time_neg[] = "-000001";
    ASSERT_EQ(time_parse(time_neg), TIME_VALUE_ERROR);

    char time_sec_overflow[] = "000060";
    ASSERT_EQ(time_parse(time_sec_overflow), TIME_VALUE_ERROR);

    char time_min_overflow[] = "006000";
    ASSERT_EQ(time_parse(time_min_overflow), TIME_VALUE_ERROR);

    char time_hour_overflow[] = "240000";
    ASSERT_EQ(time_parse(time_hour_overflow), TIME_VALUE_ERROR);
}

// https://google.github.io/googletest/reference/testing.html
// https://google.github.io/googletest/reference/assertions.html
