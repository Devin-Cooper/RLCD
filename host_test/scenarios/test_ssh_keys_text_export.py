"""Scenario 9: ssh-keys-pubkey returns an openssh-format line; ssh-keys-export
REPL returns the same.

The ssh-keys-export REPL is declared as an alias of ssh-keys-pubkey in
export_repl.cpp (cmd_ssh_keys_export just calls cmd_ssh_keys_pubkey).
This scenario confirms the alias matches and the line is well-formed:

    ssh-ed25519 <base64-blob> <name>
"""


def test_ssh_keys_text_export(wifi_device):
    d = wifi_device

    uuid = d.ssh_keys_generate("text_export_key")
    try:
        # pubkey returns a single-line DATA frame, joined via "\n".join.
        # For a single-line openssh pubkey, the join is a no-op.
        line = d.ssh_keys_pubkey(uuid)

        # Format: "ssh-ed25519 <b64> <name>"
        parts = line.split(" ", 2)
        assert len(parts) == 3, f"pubkey line not 3-part: {line!r}"
        assert parts[0] == "ssh-ed25519", parts
        assert parts[2] == "text_export_key", parts
        # Base64 body: non-empty, only valid b64 chars + '=' padding
        assert len(parts[1]) > 0
        assert all(c.isalnum() or c in "+/=" for c in parts[1]), parts[1]

        # ssh-keys-export REPL alias returns identical bytes
        r = d.send(f"ssh-keys-export {uuid}")
        assert r.ok, r
        exported = "\n".join(r.data_lines)
        assert exported == line, f"export != pubkey:\n  export={exported!r}\n  pubkey={line!r}"
    finally:
        d.ssh_keys_delete(uuid)
