// Copyright (c) 2022 Johannes Stoelp

#include "dhcp.h"
#include "utils.h"

std::optional<struct option_view> get_option(const u8* opt, usize len, dhcp_option search_tag) {
    const u8* end = opt + len;

    while (opt < end) {
        // Get tag of current option.
        dhcp_option tag = from_raw<dhcp_option>(*opt++);

        if (tag == dhcp_option::END) {
            break;
        }

        // Extract length of current option.
        usize len = *opt++;

        if (tag == search_tag) {
            if (static_cast<usize>(end - opt) < len) {
                // If length is malformed.
                return std::nullopt;
            } else {
                // Option found.
                return {{opt, len}};
            }
        }

        // Advance option iterator to beginning of next option.
        opt = opt + len;
    }

    // Option not found.
    return std::nullopt;
}
