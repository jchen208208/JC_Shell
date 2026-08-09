// The page side: draws the terminal and forwards keystrokes.
// xterm.js only renders -- every byte typed here goes to the shell via the
// preload bridge, and everything the shell prints comes back the same way.

const term = new Terminal({
  fontFamily: 'Menlo, Monaco, monospace',
  fontSize: 14,
  cursorBlink: true,
  theme: {
    background: '#1e1e2e',
    foreground: '#cdd6f4',
    cursor: '#f5e0dc',
  },
});

const fitAddon = new FitAddon.FitAddon();
term.loadAddon(fitAddon);
term.open(document.getElementById('terminal'));

window.pty.onData((data) => term.write(data));
term.onData((data) => window.pty.send(data));

// Keep the shell's idea of the window size in sync with the actual size.
function syncSize() {
  fitAddon.fit();
  window.pty.resize(term.cols, term.rows);
}

window.addEventListener('resize', syncSize);
syncSize();
term.focus();
