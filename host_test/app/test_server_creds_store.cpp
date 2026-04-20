#include <catch2/catch_test_macros.hpp>
#include "config_store_nvs.hpp"
#include "nvs.h"
#include <cstring>

static void resetNvs() {
    nvs_stub::db().clear();
    nvs_stub::existing_nss().clear();
}

TEST_CASE("ServerStore: persist + load roundtrip", "[config][nvs]") {
    resetNvs();
    sdcard::ServerRuntime servers[2] = {};
    std::strncpy(servers[0].creds.name, "prod", sizeof(servers[0].creds.name));
    std::strncpy(servers[0].creds.host, "example.com", sizeof(servers[0].creds.host));
    servers[0].creds.port = 2222;
    std::strncpy(servers[0].creds.username, "dev", sizeof(servers[0].creds.username));
    std::strncpy(servers[0].creds.password, "s3cret", sizeof(servers[0].creds.password));
    servers[0].creds.use_key_auth = false;
    servers[0].valid = true;

    std::strncpy(servers[1].creds.name, "staging", sizeof(servers[1].creds.name));
    std::strncpy(servers[1].creds.host, "stg.example.com", sizeof(servers[1].creds.host));
    servers[1].creds.port = 22;
    servers[1].valid = true;

    REQUIRE(sdcard::persistServersToNvs(servers, 2));

    sdcard::ServerRuntime loaded[8] = {};
    int n = sdcard::loadServersFromNvs(loaded, 8);
    REQUIRE(n == 2);
    REQUIRE(std::strcmp(loaded[0].creds.name, "prod") == 0);
    REQUIRE(loaded[0].creds.port == 2222);
    REQUIRE(std::strcmp(loaded[0].creds.password, "s3cret") == 0);
    REQUIRE(loaded[0].valid);
    REQUIRE(std::strcmp(loaded[1].creds.name, "staging") == 0);
}

TEST_CASE("ServerStore: loadFromNvs on empty namespace returns 0",
          "[config][nvs]") {
    resetNvs();
    sdcard::ServerRuntime loaded[8] = {};
    int n = sdcard::loadServersFromNvs(loaded, 8);
    REQUIRE(n == 0);
}

TEST_CASE("ServerStore: tail-erase wipes stale entries on count decrease",
          "[config][nvs]") {
    resetNvs();
    // Persist 3 servers.
    sdcard::ServerRuntime servers[3] = {};
    for (int i = 0; i < 3; ++i) {
        snprintf(servers[i].creds.name, sizeof(servers[i].creds.name), "s%d", i);
        snprintf(servers[i].creds.host, sizeof(servers[i].creds.host), "h%d", i);
        servers[i].creds.port = 22;
        servers[i].valid = true;
    }
    REQUIRE(sdcard::persistServersToNvs(servers, 3));

    // Persist with count=1 (simulating deletion of 2).
    REQUIRE(sdcard::persistServersToNvs(servers, 1));

    sdcard::ServerRuntime loaded[8] = {};
    int n = sdcard::loadServersFromNvs(loaded, 8);
    REQUIRE(n == 1);
    REQUIRE(std::strcmp(loaded[0].creds.name, "s0") == 0);
    // Slots 1, 2 must not contain stale data from the prior persist.
    REQUIRE(loaded[1].creds.name[0] == '\0');
    REQUIRE(loaded[2].creds.name[0] == '\0');
}

TEST_CASE("ServerStore: active index persist/load roundtrip", "[config][nvs]") {
    resetNvs();
    sdcard::persistActiveIndex(3);
    REQUIRE(sdcard::loadActiveIndex(8) == 3);
    // Clamp: loading with max_valid <= index returns 0.
    REQUIRE(sdcard::loadActiveIndex(2) == 0);
    REQUIRE(sdcard::loadActiveIndex(3) == 0);
    REQUIRE(sdcard::loadActiveIndex(4) == 3);
}
