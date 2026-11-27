#!/usr/bin/env python3
"""Drive the vi editor through a pty: the basics of editing,
saving, quitting, and the read-only mode."""
import fcntl
import os
import pty
import select
import struct
import sys
import tempfile
import termios
import time

VI = os.path.join (os.path.dirname (os.path.abspath (__file__)), "..", "vi")


def drain (fd, seconds):
    out = b""
    end = time.monotonic () + seconds
    while time.monotonic () < end:
        r, _, _ = select.select ([fd], [], [], 0.1)
        if r:
            try:
                out += os.read (fd, 65536)
            except OSError:
                break
    return out


def spawn (path, args = ()):
    pid, fd = pty.fork ()
    if pid == 0:
        os.execv (VI, [VI, *args, path])
        os._exit (127)
    fcntl.ioctl (fd, termios.TIOCSWINSZ,
                 struct.pack ("HHHH", 24, 80, 0, 0))
    return pid, fd


def run (path, keys, args = ()):
    """Edit path with the given key sequences, return the whole
    terminal output and the exit status, or None if the editor
    hung."""
    pid, fd = spawn (path, args)
    out = drain (fd, 0.6)
    for k in keys:
        os.write (fd, k)
        out += drain (fd, 0.15)
    out += drain (fd, 0.8)
    st = None
    for _ in range (100):
        w, st = os.waitpid (pid, os.WNOHANG)
        if w:
            break
        time.sleep (0.1)
    else:
        os.kill (pid, 9)
        os.waitpid (pid, 0)
    try:
        os.close (fd)
    except OSError:
        pass
    return out, st


def expect (cond, what):
    print (("PASS " if cond else "FAIL ") + what)
    if not cond:
        sys.exit (1)


def main ():
    with tempfile.TemporaryDirectory () as d:
        # Insert, write, quit: the file keeps the typed line and the
        # editor exits cleanly.
        f = d + "/one.txt"
        open (f, "w").write ("")
        out, st = run (f, [b"ihello world\x1b", b":wq\r"])
        expect (open (f).read () == "hello world\n",
                "insert+write+quit wrote the line (%r)" % open (f).read ())
        expect (st is not None and os.WIFEXITED (st)
                and os.WEXITSTATUS (st) == 0,
                "clean exit after :wq (status %r)" % (st,))

        # Deleting a line and undoing it leaves the file alone when
        # quitting with :q!.
        g = d + "/two.txt"
        open (g, "w").write ("alpha\nbeta\n")
        run (g, [b"dd", b"u", b":q!\r"])
        expect (open (g).read () == "alpha\nbeta\n",
                "dd + u + :q! left the file alone (%r)" % open (g).read ())

        # Motions and edits on an existing file: $ moves to the end
        # of the line, x deletes the char under dot.
        h = d + "/three.txt"
        open (h, "w").write ("abcdef\n")
        run (h, [b"$", b"x", b"x", b":wq\r"])
        expect (open (h).read () == "abcd\n",
                "$ + x x trimmed the tail (%r)" % open (h).read ())

        # -R: the file is read-only, :w is refused.
        i = d + "/four.txt"
        open (i, "w").write ("kept\n")
        out, _ = run (i, [b"dd", b":w\r", b":q!\r"], args = ("-R",))
        expect (open (i).read () == "kept\n",
                "-R kept the file through :w (%r)" % open (i).read ())
        expect (b"read only" in out.lower (),
                "-R reported the read-only mode")

    print ("RESULT: all passed")


if __name__ == "__main__":
    main ()
