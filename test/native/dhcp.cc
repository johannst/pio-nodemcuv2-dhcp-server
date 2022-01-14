// Copyright (c) 2022 Johannes Stoelp

#include <dhcp.h>
#include <utils.h>

#include <gtest/gtest.h>

TEST(option, get_available_opt) {
    u8 opts[] = {
        into_raw(dhcp_option::PAD), into_raw(dhcp_option::PAD), into_raw(dhcp_option::DHCP_MESSAGE_TYPE), 5 /* len */, 0, 1, 2, 3, 4,
        into_raw(dhcp_option::END),
    };

    auto ov = get_option(opts, sizeof(opts), dhcp_option::DHCP_MESSAGE_TYPE);

    ASSERT_EQ(true, ov.has_value());
    ASSERT_EQ(0, ov.value().data[0]);
    ASSERT_EQ(1, ov.value().data[1]);
    ASSERT_EQ(2, ov.value().data[2]);
    ASSERT_EQ(3, ov.value().data[3]);
    ASSERT_EQ(4, ov.value().data[4]);
    ASSERT_EQ(5, ov.value().len);
}

TEST(option, maleformed_len) {
    u8 opts[] = {
        into_raw(dhcp_option::PAD), into_raw(dhcp_option::PAD), into_raw(dhcp_option::DHCP_MESSAGE_TYPE), 5 /* len */, 0,
        into_raw(dhcp_option::END),
    };

    {
        auto ov = get_option(opts, sizeof(opts), dhcp_option::DHCP_MESSAGE_TYPE);

        ASSERT_EQ(false, ov.has_value());
    }
    {
        auto ov = get_option(opts, sizeof(opts), dhcp_option::CLASS_ID);

        ASSERT_EQ(false, ov.has_value());
    }
}

TEST(option, maleformed_missing_end) {
    u8 opts[] = {
        into_raw(dhcp_option::PAD),
        into_raw(dhcp_option::PAD),
    };

    auto ov = get_option(opts, sizeof(opts), dhcp_option::DHCP_MESSAGE_TYPE);

    ASSERT_EQ(false, ov.has_value());
}

TEST(option, put_opt_val) {
    u8 options[8] = {0};
    u32 val = 0xdeadbeef;

    u8* nextp = put_opt_val(options, val);

    ASSERT_EQ(options + 4, nextp);
    ASSERT_EQ(0xde, options[0]);
    ASSERT_EQ(0xad, options[1]);
    ASSERT_EQ(0xbe, options[2]);
    ASSERT_EQ(0xef, options[3]);
}