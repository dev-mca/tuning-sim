// Hide the console window that Windows would otherwise open next to the app in release builds.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

//! Desktop shell: starts simcore, shows the UI once simcore accepts connections, stops simcore
//! on exit (ADR 0009). The UI talks to simcore directly over WebSocket, not through Tauri.

use std::sync::{Mutex, PoisonError};
use std::thread;
use std::time::Duration;

use tauri::path::BaseDirectory;
use tauri::{AppHandle, Manager, RunEvent};
use tauri_plugin_dialog::{DialogExt, MessageDialogKind};
use tuning_sim_shell::config::{PORT_VARIABLE, ShellConfig};
use tuning_sim_shell::probe;
use tuning_sim_shell::sidecar::{self, ShutdownOutcome, Sidecar};

const MAIN_WINDOW: &str = "main";
/// Name in bundle.externalBin (tauri.conf.json), without the target-triple suffix.
const SIMCORE_BINARY: &str = "simcore";
/// Folder with engines/ and tables/, copied by bundle.resources.
const SHARED_RESOURCE_DIR: &str = "shared";
/// simcore normally stops within milliseconds after SIGTERM (it joins three threads).
const SHUTDOWN_GRACE: Duration = Duration::from_secs(3);
/// Exit code when simcore cannot be started; 2 is used for invalid settings.
const EXIT_STARTUP_FAILED: i32 = 1;
const EXIT_INVALID_CONFIG: i32 = 2;

/// The running simcore, shared between the start-up thread and the exit handler.
#[derive(Default)]
struct SimcoreState(Mutex<Option<Sidecar>>);

fn main() {
    let config = ShellConfig::from_env().unwrap_or_else(|error| {
        eprintln!("tuning-sim: {error}");
        std::process::exit(EXIT_INVALID_CONFIG);
    });

    let app = tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(SimcoreState::default())
        .setup(move |app| {
            // Runs on the main thread. That matters: simcore's parent-death signal is tied to
            // the thread that starts it (sidecar.rs, stop_with_parent).
            let launched = launch_simcore(app.handle(), &config);
            let handle = app.handle().clone();
            thread::spawn(move || match launched {
                Ok(()) => wait_and_show_ui(&handle, &config),
                Err(message) => fail_startup(&handle, &message),
            });
            Ok(())
        })
        .build(tauri::generate_context!())
        .expect("cannot build the Tauri application");

    app.run(|handle, event| {
        if let RunEvent::Exit = event {
            stop_simcore(handle);
        }
    });
}

fn launch_simcore(app: &AppHandle, config: &ShellConfig) -> Result<(), String> {
    let address = config.simcore_address();
    // Without this check we would "find" a simcore that is already running (e.g. the user's
    // own from the terminal) and our sidecar would quietly fail to bind.
    if probe::is_port_in_use(address) {
        return Err(format!(
            "Port {} is already in use, probably by another simcore.\n\
             Stop it, or start tuning-sim with another port: {PORT_VARIABLE}=<port> \
             (the UI must be built with the same port, see docs/setup.md).",
            config.port
        ));
    }
    let binary = sidecar::sidecar_path(SIMCORE_BINARY).map_err(|error| error.to_string())?;
    let shared_dir = app
        .path()
        .resolve(SHARED_RESOURCE_DIR, BaseDirectory::Resource)
        .map_err(|error| format!("cannot locate the bundled engine data: {error}"))?;
    let args = sidecar::simcore_args(&shared_dir, config.port, config.audio);
    let simcore = Sidecar::spawn(&binary, &args).map_err(|error| error.to_string())?;
    eprintln!(
        "tuning-sim: started simcore (pid {}) on {address}",
        simcore.pid()
    );
    *lock_state(app) = Some(simcore);
    Ok(())
}

fn wait_and_show_ui(app: &AppHandle, config: &ShellConfig) {
    let address = config.simcore_address();
    let ready = sidecar::wait_until_ready(address, config.startup_timeout, || {
        lock_state(app).as_mut().and_then(Sidecar::exit_error)
    });
    if let Err(error) = ready {
        fail_startup(app, &error.to_string());
        return;
    }
    eprintln!("tuning-sim: simcore accepts connections on {address}");
    match app.get_webview_window(MAIN_WINDOW) {
        Some(window) => {
            if let Err(error) = window.show().and_then(|()| window.set_focus()) {
                fail_startup(app, &format!("cannot show the main window: {error}"));
            }
        }
        None => fail_startup(app, "main window missing in tauri.conf.json"),
    }
}

/// Shows the reason in a dialog (a desktop app has no visible terminal) and quits. Quitting
/// goes through RunEvent::Exit, so a half-started simcore is stopped as well.
fn fail_startup(app: &AppHandle, message: &str) {
    eprintln!("tuning-sim: {message}");
    app.dialog()
        .message(message)
        .title("tuning-sim cannot start")
        .kind(MessageDialogKind::Error)
        .blocking_show(); // fine here: we are never on the main thread
    app.exit(EXIT_STARTUP_FAILED);
}

fn stop_simcore(app: &AppHandle) {
    let Some(mut simcore) = lock_state(app).take() else {
        return;
    };
    let pid = simcore.pid();
    match simcore.shutdown(SHUTDOWN_GRACE) {
        Ok(ShutdownOutcome::Stopped(status)) => {
            eprintln!("tuning-sim: simcore (pid {pid}) stopped ({status})");
        }
        Ok(ShutdownOutcome::Killed(status)) => {
            eprintln!("tuning-sim: simcore (pid {pid}) ignored SIGTERM, killed ({status})");
        }
        Ok(ShutdownOutcome::AlreadyExited(status)) => {
            eprintln!("tuning-sim: simcore (pid {pid}) had already exited ({status})");
        }
        Err(error) => eprintln!("tuning-sim: cannot stop simcore (pid {pid}): {error}"),
    }
}

fn lock_state(app: &AppHandle) -> std::sync::MutexGuard<'_, Option<Sidecar>> {
    // A panic while holding the lock leaves the Option intact, so recovering is safe.
    app.state::<SimcoreState>()
        .inner()
        .0
        .lock()
        .unwrap_or_else(PoisonError::into_inner)
}
