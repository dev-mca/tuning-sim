// tauri-build reads tauri.conf.json, checks the sidecar in binaries/ and embeds the config.
fn main() {
    tauri_build::build();
}
