// host_test/app/test_ssh_keys_qr_encode.cpp
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <string>
#include <vector>

#include "qrcodegen.h"

// Host-side: vendored encoder from components/ssh_keys/src/third_party.
// Produces the same matrix as a known-good Python reference for a fixed payload.

#ifndef FIXTURE_DIR
#error "FIXTURE_DIR must be defined by CMake to the host_test/app/fixtures path"
#endif

static std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
}

TEST_CASE("nayuki encoder matches Python qrcode reference", "[ssh_keys_qr]") {
    // FIXTURE_DIR is an absolute path baked in at compile time by CMake so the
    // test works under both `ctest` (CWD=build/app) and direct invocation.
    const std::string fixture_dir = FIXTURE_DIR;
    auto pubkey_text = slurp(fixture_dir + "/ed25519_expected_pub.txt");
    REQUIRE(!pubkey_text.empty());
    // Strip trailing newline
    while (!pubkey_text.empty() &&
           (pubkey_text.back() == '\n' || pubkey_text.back() == '\r')) {
        pubkey_text.pop_back();
    }
    std::string payload(pubkey_text.begin(), pubkey_text.end());

    auto reference = slurp(fixture_dir + "/qr_reference_ed25519.bin");
    REQUIRE(reference.size() >= 2);
    int ref_modules = reference[0] | (reference[1] << 8);

    std::vector<uint8_t> qr_buf(qrcodegen_BUFFER_LEN_FOR_VERSION(17));
    std::vector<uint8_t> tmp_buf(qrcodegen_BUFFER_LEN_FOR_VERSION(17));
    bool ok = qrcodegen_encodeText(payload.c_str(),
                                   tmp_buf.data(), qr_buf.data(),
                                   qrcodegen_Ecc_LOW,
                                   qrcodegen_VERSION_MIN, 17,
                                   qrcodegen_Mask_AUTO, true);
    REQUIRE(ok);
    int modules = qrcodegen_getSize(qr_buf.data());
    REQUIRE(modules == ref_modules);

    size_t bytes = (size_t(modules) * modules + 7) / 8;
    REQUIRE(reference.size() == 2 + bytes);
    for (int y = 0; y < modules; ++y) {
        for (int x = 0; x < modules; ++x) {
            size_t bit = size_t(y) * modules + x;
            uint8_t ref_bit = (reference[2 + bit / 8] >> (bit & 7)) & 1;
            bool got = qrcodegen_getModule(qr_buf.data(), x, y);
            REQUIRE(got == (bool)ref_bit);
        }
    }
}
