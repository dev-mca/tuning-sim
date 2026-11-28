//! Starts, watches and stops the simcore process (ADR 0009).
//!
//! Plain `std::process` instead of tauri-plugin-shell: we need SIGTERM (simcore stops its
//! network, simulation and audio threads in order on SIGTERM, see core/src/app/main.cpp) and a
//! parent-death signal, so simcore never outlives the shell, not even when the shell crashes.

use std::collections::VecDeque;
use std::ffi::OsString;
use std::fmt;
use std::io::{self, BufRead, BufReader};
use std::net::SocketAddr;
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStderr, Command, ExitStatus, Stdio};
use std::sync::{Arc, Mutex, PoisonError};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use crate::probe;

/// How often we probe the port and check whether simcore is still alive while it starts.
const POLL_INTERVAL: Duration = Duration::from_millis(50);
/// Number of stderr lines we keep to explain a failed start.
const STDERR_TAIL_LINES: usize = 20;

/// Arguments for serving the WebSocket API (simcore --help).
pub fn simcore_args(shared_dir: &Path, port: u16, audio: bool) -> Vec<OsString> {
    let mut args: Vec<OsString> = vec![
        "--shared".into(),
        shared_dir.into(),
        "--port".into(),
        port.to_string().into(),
    ];
    if !audio {
        args.push("--no-audio".into());
    }
    args
}

#[derive(Debug)]
pub enum StartupError {
    /// simcore ended before it accepted connections (e.g. port taken, bad engine files).
    Exited { status: ExitStatus, stderr: String },
    TimedOut {
        address: SocketAddr,
        after: Duration,
    },
}

impl fmt::Display for StartupError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Exited { status, stderr } if stderr.is_empty() => {
                write!(f, "simcore stopped during start-up ({status})")
            }
            Self::Exited { status, stderr } => {
                write!(f, "simcore stopped during start-up ({status}):\n{stderr}")
            }
            Self::TimedOut { address, after } => write!(
                f,
                "simcore did not accept connections on {address} within {} s",
                after.as_secs_f64()
            ),
        }
    }
}

impl std::error::Error for StartupError {}

/// Waits until the WebSocket server at `address` completes a handshake. `check_exited` is asked
/// between attempts so a crashed simcore fails fast instead of running into the timeout.
pub fn wait_until_ready(
    address: SocketAddr,
    timeout: Duration,
    mut check_exited: impl FnMut() -> Option<StartupError>,
) -> Result<(), StartupError> {
    let deadline = Instant::now() + timeout;
    loop {
        if let Some(error) = check_exited() {
            return Err(error);
        }
        if probe::accepts_websocket(address) {
            return Ok(());
        }
        if Instant::now() >= deadline {
            return Err(StartupError::TimedOut {
                address,
                after: timeout,
            });
        }
        thread::sleep(POLL_INTERVAL);
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum ShutdownOutcome {
    /// Ended within the grace period after SIGTERM.
    Stopped(ExitStatus),
    /// Ignored SIGTERM and was killed with SIGKILL.
    Killed(ExitStatus),
    /// Was not running any more when shutdown began.
    AlreadyExited(ExitStatus),
}

/// A running simcore. Dropping it stops the process, like a try-with-resources in Java.
pub struct Sidecar {
    child: Child,
    stderr_tail: Arc<Mutex<VecDeque<String>>>,
    stderr_reader: Option<JoinHandle<()>>,
    shutdown_done: bool,
}

/// Grace period used when a `Sidecar` is dropped without an explicit shutdown.
const DROP_GRACE: Duration = Duration::from_secs(3);

impl Sidecar {
    /// Starts `binary`. stdout is inherited (simcore's banner shows up in the terminal);
    /// stderr is forwarded and its last lines are kept for error messages.
    pub fn spawn(binary: &Path, args: &[OsString]) -> io::Result<Self> {
        let mut command = Command::new(binary);
        command
            .args(args)
            .stdin(Stdio::null())
            .stdout(Stdio::inherit())
            .stderr(Stdio::piped());
        stop_with_parent(&mut command);
        let mut child = command.spawn().map_err(|error| {
            io::Error::new(
                error.kind(),
                format!("cannot start {}: {error}", binary.display()),
            )
        })?;
        let stderr_tail = Arc::new(Mutex::new(VecDeque::new()));
        let stderr_reader = child
            .stderr
            .take()
            .map(|stderr| forward_stderr(stderr, Arc::clone(&stderr_tail)));
        Ok(Self {
            child,
            stderr_tail,
            stderr_reader,
            shutdown_done: false,
        })
    }

    pub fn pid(&self) -> u32 {
        self.child.id()
    }

    /// Returns why simcore is gone, or `None` while it still runs.
    pub fn exit_error(&mut self) -> Option<StartupError> {
        match self.child.try_wait() {
            Ok(Some(status)) => Some(StartupError::Exited {
                status,
                stderr: self.collected_stderr(),
            }),
            Ok(None) => None,
            // try_wait only fails if the child was already reaped; treat that as "still known".
            Err(_) => None,
        }
    }

    /// Asks simcore to stop (SIGTERM), waits up to `grace`, then kills it (SIGKILL).
    pub fn shutdown(&mut self, grace: Duration) -> io::Result<ShutdownOutcome> {
        self.shutdown_done = true;
        if let Some(status) = self.child.try_wait()? {
            return Ok(ShutdownOutcome::AlreadyExited(status));
        }
        send_terminate(self.child.id())?;
        let deadline = Instant::now() + grace;
        while Instant::now() < deadline {
            if let Some(status) = self.child.try_wait()? {
                return Ok(ShutdownOutcome::Stopped(status));
            }
            thread::sleep(POLL_INTERVAL);
        }
        self.child.kill()?;
        Ok(ShutdownOutcome::Killed(self.child.wait()?))
    }

    /// The last stderr lines. Waits for the reader to drain the pipe first, so nothing printed
    /// right before exit is lost. Only call this once the process has exited.
    fn collected_stderr(&mut self) -> String {
        if let Some(reader) = self.stderr_reader.take() {
            let _ = reader.join(); // a panicking reader only loses log lines
        }
        let tail = self
            .stderr_tail
            .lock()
            .unwrap_or_else(PoisonError::into_inner);
        tail.iter().cloned().collect::<Vec<_>>().join("\n")
    }
}

impl Drop for Sidecar {
    fn drop(&mut self) {
        if !self.shutdown_done {
            let _ = self.shutdown(DROP_GRACE);
        }
    }
}

/// Path of the sidecar next to the shell executable. Tauri copies `externalBin` there without
/// the target-triple suffix, both for `tauri dev` (target/debug) and in the installed bundle.
pub fn sidecar_path(name: &str) -> io::Result<PathBuf> {
    let executable = std::env::current_exe()?;
    let directory = executable
        .parent()
        .ok_or_else(|| io::Error::other("executable has no parent directory"))?;
    Ok(directory.join(name))
}

fn forward_stderr(stderr: ChildStderr, tail: Arc<Mutex<VecDeque<String>>>) -> JoinHandle<()> {
    thread::spawn(move || {
        for line in BufReader::new(stderr).lines().map_while(Result::ok) {
            eprintln!("simcore: {line}");
            let mut tail = tail.lock().unwrap_or_else(PoisonError::into_inner);
            if tail.len() == STDERR_TAIL_LINES {
                tail.pop_front();
            }
            tail.push_back(line);
        }
    })
}

#[cfg(target_os = "linux")]
fn stop_with_parent(command: &mut Command) {
    use std::os::unix::process::CommandExt;
    // SAFETY: the closure runs in the forked child before exec and only calls the
    // async-signal-safe functions prctl, getppid and kill.
    let parent_pid = std::process::id();
    unsafe {
        command.pre_exec(move || {
            // The kernel sends SIGTERM to simcore when the thread that started it ends. The shell
            // starts it from the main thread, so this means: when the shell process ends.
            if libc::prctl(libc::PR_SET_PDEATHSIG, libc::SIGTERM) != 0 {
                return Err(io::Error::last_os_error());
            }
            // The parent may have died between fork and prctl; then no signal would ever come.
            if libc::getppid() != parent_pid as libc::pid_t {
                libc::kill(libc::getpid(), libc::SIGTERM);
            }
            Ok(())
        });
    }
}

#[cfg(not(target_os = "linux"))]
fn stop_with_parent(_command: &mut Command) {
    // macOS/Windows have no PR_SET_PDEATHSIG; the shell's exit handler still stops simcore.
    // A crash of the shell can leave simcore running there (ADR 0009).
}

#[cfg(unix)]
fn send_terminate(pid: u32) -> io::Result<()> {
    let pid = libc::pid_t::try_from(pid).map_err(io::Error::other)?;
    // SAFETY: plain syscall; `pid` is our own child, which we have not reaped yet.
    if unsafe { libc::kill(pid, libc::SIGTERM) } == 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

#[cfg(not(unix))]
fn send_terminate(_pid: u32) -> io::Result<()> {
    // No SIGTERM on Windows; shutdown() falls back to kill() after the grace period.
    Ok(())
}

#[cfg(all(test, unix))]
mod tests {
    use super::*;
    use std::net::TcpListener;
    use std::os::unix::process::ExitStatusExt;

    fn shell(script: &str) -> Sidecar {
        Sidecar::spawn(Path::new("/bin/sh"), &["-c".into(), script.into()]).unwrap()
    }

    fn unused_address() -> SocketAddr {
        // Bind to port 0, read the port the OS picked, release it again.
        TcpListener::bind("127.0.0.1:0")
            .unwrap()
            .local_addr()
            .unwrap()
    }

    #[test]
    fn builds_serve_arguments() {
        let args = simcore_args(Path::new("/opt/shared"), 47801, false);
        assert_eq!(
            args,
            ["--shared", "/opt/shared", "--port", "47801", "--no-audio"].map(OsString::from)
        );
        assert!(!simcore_args(Path::new("s"), 1, true).contains(&OsString::from("--no-audio")));
    }

    #[test]
    fn wait_is_not_fooled_by_a_listener_that_is_no_websocket_server() {
        // The success case runs against the real simcore in tests/simcore_sidecar.rs.
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        let result = wait_until_ready(address, Duration::from_millis(300), || None);
        assert!(matches!(result, Err(StartupError::TimedOut { .. })));
    }

    #[test]
    fn wait_times_out_when_nothing_listens() {
        let started = Instant::now();
        let result = wait_until_ready(unused_address(), Duration::from_millis(300), || None);
        assert!(matches!(result, Err(StartupError::TimedOut { .. })));
        assert!(started.elapsed() < Duration::from_secs(2));
    }

    #[test]
    fn wait_fails_fast_with_stderr_when_process_exits() {
        let mut sidecar = shell("echo 'cannot listen on 127.0.0.1' >&2; exit 3");
        let result = wait_until_ready(unused_address(), Duration::from_secs(5), || {
            sidecar.exit_error()
        });
        match result {
            Err(StartupError::Exited { status, stderr }) => {
                assert_eq!(status.code(), Some(3));
                assert!(stderr.contains("cannot listen"), "stderr: {stderr:?}");
            }
            other => panic!("expected Exited, got {other:?}"),
        }
    }

    #[test]
    fn shutdown_stops_process_with_sigterm() {
        let mut sidecar = shell("exec sleep 30");
        let outcome = sidecar.shutdown(Duration::from_secs(2)).unwrap();
        match outcome {
            ShutdownOutcome::Stopped(status) => assert_eq!(status.signal(), Some(libc::SIGTERM)),
            other => panic!("expected Stopped, got {other:?}"),
        }
    }

    #[test]
    fn shutdown_kills_process_that_ignores_sigterm() {
        // Busy loop of shell builtins: no grandchild that could outlive the test.
        let mut sidecar = shell("trap '' TERM; while :; do :; done");
        thread::sleep(Duration::from_millis(100)); // let the shell install the trap
        let outcome = sidecar.shutdown(Duration::from_millis(300)).unwrap();
        match outcome {
            ShutdownOutcome::Killed(status) => assert_eq!(status.signal(), Some(libc::SIGKILL)),
            other => panic!("expected Killed, got {other:?}"),
        }
    }

    #[test]
    fn shutdown_reports_process_that_already_exited() {
        let mut sidecar = shell("exit 0");
        thread::sleep(Duration::from_millis(200));
        assert!(matches!(
            sidecar.shutdown(Duration::from_secs(1)).unwrap(),
            ShutdownOutcome::AlreadyExited(_)
        ));
    }

    #[test]
    fn dropping_sidecar_stops_process() {
        let sidecar = shell("exec sleep 30");
        let pid = libc::pid_t::try_from(sidecar.pid()).unwrap();
        drop(sidecar);
        // SAFETY: signal 0 only checks whether the pid exists. It was reaped by drop.
        let alive = unsafe { libc::kill(pid, 0) } == 0;
        assert!(!alive, "process {pid} still exists after drop");
    }
}
