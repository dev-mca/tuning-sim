//! Starts the real, staged simcore (scripts/stage-sidecar.sh) without a window.
//! Skipped with a note when no sidecar is staged, so `cargo test` also works on a fresh clone.
#![cfg(target_os = "linux")]

use std::io::{BufRead, BufReader};
use std::net::{SocketAddr, TcpListener};
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

use tuning_sim_shell::probe;
use tuning_sim_shell::sidecar::{self, ShutdownOutcome, Sidecar};

const STARTUP_TIMEOUT: Duration = Duration::from_secs(10);
const HELPER_VARIABLE: &str = "TUNING_SIM_TEST_PDEATHSIG_HELPER";
const PID_PREFIX: &str = "SIMCORE_PID=";

fn staged_simcore() -> Option<PathBuf> {
    let binaries = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("binaries");
    let found = std::fs::read_dir(binaries)
        .ok()?
        .filter_map(Result::ok)
        .map(|entry| entry.path())
        .find(|path| {
            path.file_name()
                .and_then(|name| name.to_str())
                .is_some_and(|name| name.starts_with("simcore-"))
        });
    if found.is_none() {
        eprintln!("skipped: no staged simcore, run scripts/stage-sidecar.sh first");
    }
    found
}

fn shared_dir() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../shared")
}

fn unused_address() -> SocketAddr {
    TcpListener::bind("127.0.0.1:0")
        .unwrap()
        .local_addr()
        .unwrap()
}

/// Tests never open the audio device (`--no-audio`), like simcore's own CI runs.
fn start_simcore(binary: &std::path::Path, address: SocketAddr) -> Sidecar {
    let args = sidecar::simcore_args(&shared_dir(), address.port(), false);
    let mut simcore = Sidecar::spawn(binary, &args).unwrap();
    sidecar::wait_until_ready(address, STARTUP_TIMEOUT, || simcore.exit_error())
        .expect("simcore did not start");
    simcore
}

/// True while the pid exists and is not a zombie (a zombie has finished, it only waits for
/// its new parent to collect the exit status).
fn is_running(pid: u32) -> bool {
    match std::fs::read_to_string(format!("/proc/{pid}/stat")) {
        // Format: "pid (comm) state ..."; comm may contain spaces, so split after ')'.
        Ok(stat) => stat
            .rsplit_once(')')
            .is_some_and(|(_, rest)| !rest.trim_start().starts_with('Z')),
        Err(_) => false,
    }
}

#[test]
fn simcore_starts_and_stops_cleanly_on_sigterm() {
    let Some(binary) = staged_simcore() else {
        return;
    };
    let address = unused_address();
    let mut simcore = start_simcore(&binary, address);
    let pid = simcore.pid();
    assert!(probe::accepts_websocket(address));

    match simcore.shutdown(Duration::from_secs(3)).unwrap() {
        ShutdownOutcome::Stopped(status) => assert!(status.success(), "exit status {status}"),
        other => panic!("expected a clean stop, got {other:?}"),
    }
    assert!(!is_running(pid));
    assert!(!probe::is_port_in_use(address), "port still open");
}

#[test]
fn simcore_reports_a_port_conflict_during_startup() {
    let Some(binary) = staged_simcore() else {
        return;
    };
    let occupied = TcpListener::bind("127.0.0.1:0").unwrap();
    let address = occupied.local_addr().unwrap();
    let args = sidecar::simcore_args(&shared_dir(), address.port(), false);
    let mut simcore = Sidecar::spawn(&binary, &args).unwrap();
    // Our own listener answers on that port, so ask the process instead of the port.
    let deadline = Instant::now() + STARTUP_TIMEOUT;
    let error = loop {
        if let Some(error) = simcore.exit_error() {
            break error;
        }
        assert!(Instant::now() < deadline, "simcore did not exit");
        thread::sleep(Duration::from_millis(50));
    };
    assert!(
        error.to_string().contains("cannot listen"),
        "unexpected error: {error}"
    );
}

/// Runs in a child process of the next test: starts simcore, then ends without any cleanup,
/// like a crashed shell. Does nothing in a normal test run.
#[test]
fn pdeathsig_helper() {
    if std::env::var_os(HELPER_VARIABLE).is_none() {
        return;
    }
    let binary = staged_simcore().expect("helper needs a staged simcore");
    let simcore = start_simcore(&binary, unused_address());
    println!("{PID_PREFIX}{}", simcore.pid());
    std::process::exit(0); // skips Drop on purpose
}

#[test]
fn simcore_stops_when_the_shell_dies_without_cleanup() {
    if staged_simcore().is_none() {
        return;
    }
    let mut helper = Command::new(std::env::current_exe().unwrap())
        .args([
            "--exact",
            "pdeathsig_helper",
            "--nocapture",
            "--test-threads=1",
        ])
        .env(HELPER_VARIABLE, "1")
        .stdout(Stdio::piped())
        .spawn()
        .unwrap();
    let stdout = BufReader::new(helper.stdout.take().unwrap());
    let pid: u32 = stdout
        .lines()
        .map_while(Result::ok)
        .find_map(|line| line.strip_prefix(PID_PREFIX).map(str::to_owned))
        .expect("helper printed no pid")
        .parse()
        .unwrap();
    helper.wait().unwrap();

    let deadline = Instant::now() + Duration::from_secs(3);
    while is_running(pid) && Instant::now() < deadline {
        thread::sleep(Duration::from_millis(50));
    }
    assert!(!is_running(pid), "simcore (pid {pid}) outlived its parent");
}
