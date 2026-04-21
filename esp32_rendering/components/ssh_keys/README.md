# ssh_keys — managed SSH key store

Provides `KeyStore` (NVS index + LittleFS PEM blobs) and five exporters
(text / QR / SD / REPL / SSH enrollment). All PKI operations delegate to
libssh 0.11.4 via the in-tree `components/libssh/` component.

## Layout

- `include/` — public headers
- `src/key_*.cpp` — store + codec + generate
- `src/export_*.cpp` — five export paths
- `src/third_party/` — vendored [nayuki/QR-Code-generator](https://github.com/nayuki/QR-Code-generator) (MIT), pin `2c9044de6b049ca25cb3cd1649ed7e27aa055138`

## Refresh procedure (nayuki QR encoder)

1. Clone `https://github.com/nayuki/QR-Code-generator` at a fresh commit.
2. Replace `src/third_party/qrcodegen.{h,c}` with the `c/` variants from the clone.
3. Update this README's pin SHA.
4. Re-run `host_test/app/test_ssh_keys_qr_encode` and on-device `test_ssh_keys_qr_encode.py`.

## Relationship to ssh_client

`ssh_client::doAuthenticate` looks up `Config.ssh_key_id` via
`KeyStore::path_for(id, out_path, cap)` to resolve the LittleFS PEM path.
Deleting a key while an active SSH session is open does NOT affect that
session (libssh holds the parsed key in memory); only the next connect
against the same server will surface `"Private key unreadable"`.
