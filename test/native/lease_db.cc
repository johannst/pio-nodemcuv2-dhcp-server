// Copyright (c) 2022 Johannes Stoelp

#include <lease_db.h>

#include <gtest/gtest.h>

TEST(lease_db, null_client_hash) {
    lease_db<2> db;

    ASSERT_EQ(std::nullopt, db.new_lease(0, 0));
    ASSERT_EQ(std::nullopt, db.get_lease(0));
    ASSERT_EQ(0, db.active_leases());
}

TEST(lease_db, get_new_lease) {
    lease_db<2> db;

    ASSERT_EQ(std::optional(0), db.new_lease(10, 0));
    ASSERT_EQ(std::optional(1), db.new_lease(20, 0));
    ASSERT_EQ(std::nullopt, db.new_lease(30, 0));  // exhausted

    ASSERT_EQ(std::optional(0), db.get_lease(10));
    ASSERT_EQ(std::optional(1), db.get_lease(20));
    ASSERT_EQ(std::nullopt, db.get_lease(30));  // exhausted

    ASSERT_EQ(2, db.active_leases());
}

TEST(lease_db, get_new_lease_twice) {
    lease_db<2> db;

    ASSERT_EQ(std::optional(0), db.new_lease(10, 0));
    ASSERT_EQ(std::nullopt, db.new_lease(10, 0));

    ASSERT_EQ(1, db.active_leases());
}

TEST(lease_db, flush_expired) {
    lease_db<2> db;

    ASSERT_EQ(std::optional(0), db.new_lease(10, 100 /* lease end */));
    ASSERT_EQ(std::optional(1), db.new_lease(20, 200 /* lease end */));

    db.flush_expired(50 /* current time */);
    ASSERT_EQ(2, db.active_leases());
    ASSERT_EQ(std::optional(0), db.get_lease(10));
    ASSERT_EQ(std::optional(1), db.get_lease(20));

    db.flush_expired(150 /* current time */);
    ASSERT_EQ(1, db.active_leases());
    ASSERT_EQ(std::nullopt, db.get_lease(10));
    ASSERT_EQ(std::optional(1), db.get_lease(20));

    db.flush_expired(250 /* current time */);
    ASSERT_EQ(0, db.active_leases());
    ASSERT_EQ(std::nullopt, db.get_lease(10));
    ASSERT_EQ(std::nullopt, db.get_lease(20));
}

TEST(lease_db, update_lease) {
    lease_db<2> db;

    ASSERT_EQ(std::optional(0), db.new_lease(10, 100 /* lease end */));
    ASSERT_EQ(std::optional(1), db.new_lease(20, 200 /* lease end */));

    ASSERT_EQ(2, db.active_leases());

    db.flush_expired(150 /* current time */);
    ASSERT_EQ(1, db.active_leases());
    ASSERT_EQ(std::nullopt, db.get_lease(10));
    ASSERT_EQ(std::optional(1), db.get_lease(20));

    ASSERT_EQ(false, db.update_lease(10, 300 /* lease end */));
    ASSERT_EQ(true, db.update_lease(20, 300 /* lease end */));

    db.flush_expired(250 /* current time */);
    ASSERT_EQ(1, db.active_leases());
    ASSERT_EQ(std::nullopt, db.get_lease(10));
    ASSERT_EQ(std::optional(1), db.get_lease(20));

    db.flush_expired(350 /* current time */);
    ASSERT_EQ(0, db.active_leases());
    ASSERT_EQ(std::nullopt, db.get_lease(10));
    ASSERT_EQ(std::nullopt, db.get_lease(20));
}
