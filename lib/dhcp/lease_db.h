// Copyright (c) 2022 Johannes Stoelp

#ifndef LEASE_DB_H
#define LEASE_DB_H

#include "types.h"

#include <array>
#include <optional>

struct lease {
    u32 client_hash;
    usize lease_end;
};

// Lease database, for managing client leases, which includes
//   - allocation of new leases
//   - lookup of existing leases
//   - update of existing leases
//   - flushing of expired leases
//
// The database supports 'LEASE' number of clients.
//
// The lease APIs return an idx which represents the allocated client lease and
// should be interpreted as offset to the start address of the dhcp address
// range.
//
// For example, the dhcp server starts to assing addresses starting with
// 10.0.0.100, and the lease database returns idx=4, this would represent the
// allocated client address 10.0.0.104.
template<usize LEASES>
class lease_db {
  public:
    constexpr lease_db() = default;

    lease_db(const lease_db&) = delete;
    lease_db& operator=(const lease_db&) = delete;

    // Try to allocate a new client lease for the client represented by
    // 'client_hash'.
    //
    // If a lease could be allocated return the corresponding idx.
    //
    // If a lease for the client already exist or all leases are allocated,
    // return nullopt.
    //
    // 'lease_end' sets the expiration time of the lease (should be absolute time).
    std::optional<usize> new_lease(u32 client_hash, usize lease_end) {
        if (get_lease(client_hash)) {
            return std::nullopt;
        }

        for (usize l = 0; client_hash != 0 && l < LEASES; ++l) {
            if (leases[l].client_hash == 0) {
                leases[l].client_hash = client_hash;
                leases[l].lease_end = lease_end;
                return l;
            }
        }
        return std::nullopt;
    }

    // Try to get the lease for the client if it exists.
    std::optional<usize> get_lease(u32 client_hash) const {
        for (usize l = 0; client_hash != 0 && l < LEASES; ++l) {
            if (leases[l].client_hash == client_hash) {
                return l;
            }
        }
        return std::nullopt;
    }

    // Update expiration time for client if the client has an allocated lease.
    // Similar to 'new_lease' the 'lease_end' should be an absolute time value.
    bool update_lease(u32 client_hash, usize lease_end) {
        for (usize l = 0; client_hash != 0 && l < LEASES; ++l) {
            if (leases[l].client_hash == client_hash) {
                leases[l].lease_end = lease_end;
                return true;
            }
        }
        return false;
    }

    // Check for expired leases and free them accordingly.
    // 'curr_time' should be the current time as absolute time value.
    void flush_expired(usize curr_time) {
        for (lease& l : leases) {
            if (l.lease_end <= curr_time) {
                l.client_hash = 0;
                l.lease_end = 0;
            }
        }
    }

    // Get the number of active leases.
    usize active_leases() const {
        usize cnt = 0;
        for (const lease& l : leases) {
            if (l.client_hash != 0) {
                ++cnt;
            }
        }
        return cnt;
    }

  private:
    std::array<lease, LEASES> leases = {0, 0};
};

#endif
