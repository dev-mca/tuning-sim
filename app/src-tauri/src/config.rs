//! Shell settings from environment variables (no config file: the shell has almost nothing to set).

use std::net::{Ipv4Addr, SocketAddr};
use std::time::Duration;

/// simcore's default port (docs/interface.md). The UI is built against the same port.
pub const DEFAULT_PORT: u16 = 47800;

/// Port for simcore. Must match the port the UI was built with (see scripts/prepare-ui.sh).
pub const PORT_VARIABLE: &str = "TUNING_SIM_PORT";
/// Set to `1` to start simcore with `--no-audio` (e.g. on machines without a sound device).
pub const NO_AUDIO_VARIABLE: &str = "TUNING_SIM_NO_AUDIO";

/// How long simcore may take to accept connections. It loads two JSON files and opens the
/// audio device; that takes well under a second, so 10 s only trips when something is wrong.
const STARTUP_TIMEOUT: Duration = Duration::from_secs(10);

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ShellConfig {
    pub port: u16,
    pub audio: bool,
    pub startup_timeout: Duration,
}

impl ShellConfig {
    pub fn from_env() -> Result<Self, String> {
        Self::from_lookup(|name| std::env::var(name).ok())
    }

    /// Builds the config from any variable source, so tests do not touch the process environment.
    pub fn from_lookup(lookup: impl Fn(&str) -> Option<String>) -> Result<Self, String> {
        let port = match lookup(PORT_VARIABLE) {
            None => DEFAULT_PORT,
            Some(text) => parse_port(&text)?,
        };
        let audio = lookup(NO_AUDIO_VARIABLE).as_deref() != Some("1");
        Ok(Self {
            port,
            audio,
            startup_timeout: STARTUP_TIMEOUT,
        })
    }

    /// simcore only listens on 127.0.0.1 (docs/interface.md), so that is where we wait for it.
    pub fn simcore_address(&self) -> SocketAddr {
        SocketAddr::from((Ipv4Addr::LOCALHOST, self.port))
    }
}

fn parse_port(text: &str) -> Result<u16, String> {
    match text.trim().parse::<u16>() {
        Ok(port) if port != 0 => Ok(port),
        _ => Err(format!(
            "{PORT_VARIABLE} must be a port number 1..65535, got \"{text}\""
        )),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn config_from(pairs: &[(&str, &str)]) -> Result<ShellConfig, String> {
        ShellConfig::from_lookup(|name| {
            pairs
                .iter()
                .find(|(key, _)| *key == name)
                .map(|(_, value)| (*value).to_string())
        })
    }

    #[test]
    fn defaults_to_port_47800_with_audio() {
        let config = config_from(&[]).unwrap();
        assert_eq!(config.port, 47800);
        assert!(config.audio);
        assert_eq!(config.simcore_address().to_string(), "127.0.0.1:47800");
    }

    #[test]
    fn reads_port_and_no_audio() {
        let config = config_from(&[(PORT_VARIABLE, "47801"), (NO_AUDIO_VARIABLE, "1")]).unwrap();
        assert_eq!(config.port, 47801);
        assert!(!config.audio);
    }

    #[test]
    fn rejects_invalid_ports() {
        for text in ["0", "65536", "-1", "abc", ""] {
            assert!(
                config_from(&[(PORT_VARIABLE, text)]).is_err(),
                "accepted {text:?}"
            );
        }
    }

    #[test]
    fn keeps_audio_unless_variable_is_exactly_one() {
        assert!(config_from(&[(NO_AUDIO_VARIABLE, "0")]).unwrap().audio);
    }
}
