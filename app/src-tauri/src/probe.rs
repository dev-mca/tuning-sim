//! Checks whether simcore's WebSocket server is up, without a WebSocket library.
//!
//! A bare TCP connect is not enough: simcore's server (IXWebSocket) logs every connection that
//! sends no HTTP request ("Error reading HTTP request line"). So the readiness probe performs a
//! real WebSocket opening handshake (RFC 6455 §4) and closes the connection properly.

use std::io::{self, Read, Write};
use std::net::{SocketAddr, TcpStream};
use std::time::Duration;

/// On loopback a refused connect returns at once; the timeout only matters for a hung server.
const CONNECT_TIMEOUT: Duration = Duration::from_millis(200);
const HANDSHAKE_TIMEOUT: Duration = Duration::from_millis(500);
/// The handshake response is a few hundred bytes; anything larger is not simcore.
const MAX_RESPONSE_BYTES: usize = 4096;
/// Fixed client key: the probe does not need to verify Sec-WebSocket-Accept, only the 101.
const PROBE_KEY: &str = "dHVuaW5nLXNpbSBwcm9iZQ==";
/// Close frame, masked as required for clients (RFC 6455 §5.5.1): FIN+close, mask bit, zero key.
const CLOSE_FRAME: [u8; 6] = [0x88, 0x80, 0, 0, 0, 0];

/// True if anything accepts TCP connections at `address` (used to detect a busy port).
pub fn is_port_in_use(address: SocketAddr) -> bool {
    TcpStream::connect_timeout(&address, CONNECT_TIMEOUT).is_ok()
}

/// True if a WebSocket server at `address` answers the opening handshake with 101.
pub fn accepts_websocket(address: SocketAddr) -> bool {
    handshake(address).unwrap_or(false)
}

fn handshake(address: SocketAddr) -> io::Result<bool> {
    let mut stream = TcpStream::connect_timeout(&address, CONNECT_TIMEOUT)?;
    stream.set_read_timeout(Some(HANDSHAKE_TIMEOUT))?;
    stream.set_write_timeout(Some(HANDSHAKE_TIMEOUT))?;
    let request = format!(
        "GET / HTTP/1.1\r\nHost: {address}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n\
         Sec-WebSocket-Key: {PROBE_KEY}\r\nSec-WebSocket-Version: 13\r\n\r\n"
    );
    stream.write_all(request.as_bytes())?;
    let response = read_response_head(&mut stream)?;
    let upgraded = is_switching_protocols(&response);
    if upgraded {
        // Best effort: a missing close frame only costs simcore a log line.
        let _ = stream.write_all(&CLOSE_FRAME);
    }
    Ok(upgraded)
}

/// Reads until the blank line that ends the HTTP response head.
fn read_response_head(stream: &mut TcpStream) -> io::Result<Vec<u8>> {
    let mut response = Vec::new();
    let mut chunk = [0_u8; 512];
    while !response.windows(4).any(|window| window == b"\r\n\r\n") {
        let read = stream.read(&mut chunk)?;
        if read == 0 || response.len() + read > MAX_RESPONSE_BYTES {
            break;
        }
        response.extend_from_slice(&chunk[..read]);
    }
    Ok(response)
}

fn is_switching_protocols(response: &[u8]) -> bool {
    // Status line: "HTTP/1.1 101 Switching Protocols".
    response
        .split(|&byte| byte == b'\r')
        .next()
        .and_then(|line| std::str::from_utf8(line).ok())
        .and_then(|line| line.split_whitespace().nth(1))
        == Some("101")
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::TcpListener;
    use std::thread;

    /// Accepts one connection, reads the request and answers with `response`.
    fn serve_once(response: &'static str) -> SocketAddr {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let address = listener.local_addr().unwrap();
        thread::spawn(move || {
            let (mut stream, _) = listener.accept().unwrap();
            let mut request = [0_u8; 1024];
            let _ = stream.read(&mut request);
            let _ = stream.write_all(response.as_bytes());
        });
        address
    }

    fn unused_address() -> SocketAddr {
        TcpListener::bind("127.0.0.1:0")
            .unwrap()
            .local_addr()
            .unwrap()
    }

    #[test]
    fn accepts_a_server_that_switches_protocols() {
        let address = serve_once("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n");
        assert!(accepts_websocket(address));
    }

    #[test]
    fn rejects_a_plain_http_server() {
        let address = serve_once("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
        assert!(!accepts_websocket(address));
    }

    #[test]
    fn rejects_when_nothing_listens() {
        let address = unused_address();
        assert!(!accepts_websocket(address));
        assert!(!is_port_in_use(address));
    }

    #[test]
    fn detects_any_listener_as_port_in_use() {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        assert!(is_port_in_use(listener.local_addr().unwrap()));
    }

    #[test]
    fn parses_status_line_only() {
        assert!(is_switching_protocols(
            b"HTTP/1.1 101 Switching Protocols\r\n\r\n"
        ));
        assert!(!is_switching_protocols(
            b"HTTP/1.1 400 Bad Request\r\nX: 101\r\n\r\n"
        ));
        assert!(!is_switching_protocols(b""));
    }
}
