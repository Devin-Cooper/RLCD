#include <catch2/catch_test_macros.hpp>
#include "config_store_nvs.hpp"
#include "nvs.h"
#include <cstring>

static void resetNvs() {
    nvs_stub::db().clear();
    nvs_stub::existing_nss().clear();
}

TEST_CASE("Migration: empty NVS + empty input → None",
          "[config][migration]") {
    resetNvs();
    sdcard::ServerRuntime s[8] = {};
    int cnt = 0;
    auto r = sdcard::runLegacyMigration(s, &cnt, 8);
    REQUIRE(r == sdcard::MigrationResult::None);
    REQUIRE(cnt == 0);
}

TEST_CASE("Migration: ssh_creds + SD identities → PathA",
          "[config][migration]") {
    resetNvs();
    nvs_handle_t h;
    nvs_open("ssh_creds", NVS_READWRITE, &h);
    nvs_set_str(h, "srv_p_0", "mypassword");
    nvs_commit(h);
    nvs_close(h);

    sdcard::ServerRuntime s[8] = {};
    std::strncpy(s[0].creds.name, "alpha", sizeof(s[0].creds.name));
    std::strncpy(s[0].creds.host, "h1", sizeof(s[0].creds.host));
    int cnt = 1;

    auto r = sdcard::runLegacyMigration(s, &cnt, 8);
    REQUIRE(r == sdcard::MigrationResult::PathA);
    REQUIRE(std::strcmp(s[0].creds.password, "mypassword") == 0);

    // ssh_creds should be erased
    if (nvs_open("ssh_creds", NVS_READONLY, &h) == ESP_OK) {
        char pw[64] = {}; size_t len = sizeof(pw);
        esp_err_t e = nvs_get_str(h, "srv_p_0", pw, &len);
        REQUIRE(e != ESP_OK);
        nvs_close(h);
    }
}

TEST_CASE("Migration: ssh_creds present but no SD → PathAHole",
          "[config][migration]") {
    resetNvs();
    nvs_handle_t h;
    nvs_open("ssh_creds", NVS_READWRITE, &h);
    nvs_set_str(h, "srv_p_0", "mypassword");
    nvs_commit(h);
    nvs_close(h);

    sdcard::ServerRuntime s[8] = {};
    int cnt = 0;
    auto r = sdcard::runLegacyMigration(s, &cnt, 8);
    REQUIRE(r == sdcard::MigrationResult::PathAHole);
    REQUIRE(cnt == 0);

    // ssh_creds must still be intact
    nvs_open("ssh_creds", NVS_READONLY, &h);
    char pw[64] = {}; size_t len = sizeof(pw);
    REQUIRE(nvs_get_str(h, "srv_p_0", pw, &len) == ESP_OK);
    REQUIRE(std::strcmp(pw, "mypassword") == 0);
    nvs_close(h);
}

TEST_CASE("Migration: ssh_host only → PathB seeds 'default' server",
          "[config][migration]") {
    resetNvs();
    nvs_handle_t h;
    nvs_open("app_settings", NVS_READWRITE, &h);
    nvs_set_str(h, "ssh_host", "example.com");
    nvs_set_u16(h, "ssh_port", 2222);
    nvs_set_str(h, "ssh_user", "dev");
    nvs_commit(h);
    nvs_close(h);

    sdcard::ServerRuntime s[8] = {};
    int cnt = 0;
    auto r = sdcard::runLegacyMigration(s, &cnt, 8);
    REQUIRE(r == sdcard::MigrationResult::PathB);
    REQUIRE(cnt == 1);
    REQUIRE(std::strcmp(s[0].creds.name, "default") == 0);
    REQUIRE(std::strcmp(s[0].creds.host, "example.com") == 0);
    REQUIRE(s[0].creds.port == 2222);
    REQUIRE(std::strcmp(s[0].creds.username, "dev") == 0);
}

TEST_CASE("Migration: ssh_creds + SD + ssh_host → BeltAndSuspenders",
          "[config][migration]") {
    resetNvs();
    nvs_handle_t h;
    nvs_open("ssh_creds", NVS_READWRITE, &h);
    nvs_set_str(h, "srv_p_0", "pw");
    nvs_commit(h);
    nvs_close(h);

    nvs_open("app_settings", NVS_READWRITE, &h);
    nvs_set_str(h, "ssh_host", "ghost.example.com");
    nvs_commit(h);
    nvs_close(h);

    sdcard::ServerRuntime s[8] = {};
    std::strncpy(s[0].creds.name, "alpha", sizeof(s[0].creds.name));
    std::strncpy(s[0].creds.host, "h1", sizeof(s[0].creds.host));
    int cnt = 1;

    auto r = sdcard::runLegacyMigration(s, &cnt, 8);
    REQUIRE(r == sdcard::MigrationResult::BeltAndSuspenders);

    // ssh_host fields should be erased
    nvs_open("app_settings", NVS_READONLY, &h);
    char host[64] = {}; size_t len = sizeof(host);
    esp_err_t e = nvs_get_str(h, "ssh_host", host, &len);
    REQUIRE(e != ESP_OK);
    nvs_close(h);
}
