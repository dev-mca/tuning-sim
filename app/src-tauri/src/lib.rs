//! tuning-sim desktop shell (ADR 0009). The logic lives in this library so it can be tested
//! without a window; `main.rs` only wires it to Tauri.

pub mod config;
pub mod probe;
pub mod sidecar;
