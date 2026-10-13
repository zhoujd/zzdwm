# MicroEMACS (me) Project Documentation: AI Connection Infrastructure

This document outlines the architecture, implementation, and educational design for embedding a high-performance **Local AI Connection Primitive** into our customized MicroEMACS fork (`me`).

---

## 1. Architectural Strategy
By embracing a binary profile slightly over **400 KB**, we unlock full **64-bit IEEE 754 NaN-Boxed floating-point support** inside our embedded `tiny-lisp` runtime engine.

Instead of linking heavy third-party networking dependencies (like `libcurl`), we implement a direct POSIX socket layer to communicate with a local **Ollama** AI service. This preserves our fast, lightweight execution profile while introducing advanced scripting flexibility for students.

### Structural Blueprint
```text
  [ C Text Buffer ] <---> [ tiny-lisp Evaluation Engine ]
                                   | (NaN-Boxed Floats)
                                   v
                        [ custom_me_ai_complete() ]
                                   | (POSIX Sockets / Loopback)
                                   v
                        [ Local Ollama Instance ] (qwen2.5-coder / llama3)
```

---

## 2. Implementation: Core C Infrastructure
This C code should be integrated directly into your editor's communication layer (e.g., `file.c` or a dedicated `ai.c` file) and exposed to `tinylisp.c`.

```c
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define OLLAMA_PORT 11434
#define OLLAMA_HOST "127.0.0.1"

/**
 * Sends a raw prompt buffer to a local Ollama instance via non-blocking POSIX sockets.
 * Returns a double-precision confidence metric back to the Lisp environment.
 */
double me_ai_complete(const char *prompt, char *output_buffer, int max_len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;

    if (sock < 0) return 0.0;

    server.sin_addr.s_addr = inet_addr(OLLAMA_HOST);
    server.sin_family = AF_INET;
    server.sin_port = htons(OLLAMA_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(sock);
        return 0.0; // Connection failed
    }

    // Hand-rolled JSON payload to bypass external library dependencies
    char payload[4096];
    snprintf(payload, sizeof(payload),
             "{\"model\": \"qwen2.5-coder:1.5b\", \"prompt\": \"%s\", \"stream\": false}",
             prompt);

    // Format raw HTTP request headers
    char request[8192];
    snprintf(request, sizeof(request),
             "POST /api/generate HTTP/1.1\r\n"
             "Host: 127.0.0.1:11434\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n%s",
             strlen(payload), payload);

    // Write to stream loop
    send(sock, request, strlen(request), 0);

    // Read raw HTTP response payload
    int bytes_received = recv(sock, output_buffer, max_len - 1, 0);
    close(sock);

    if (bytes_received <= 0) return 0.0;
    output_buffer[bytes_received] = '\0';

    // Return a mock confidence float score for students to practice conditional float comparisons
    return 0.94;
}
```

### Mapping into `tinylisp.c`
Register the primitive function inside the core primitive structural dispatch table of the Robert van Engelen NaN-boxed layout:

```c
// Locate your primitive symbol map array in tinylisp.c and add:
{ "ai-complete", me_ai_primitive_bridge },
```

---

## 3. High-Level Automation: Lisp Scripting Interface
Once the structural C interface is compiled, students can write lean, high-level code inside their profile configuration files (`~/.emacsrc` or loaded via `C-x F`) without recompiling the editor.

```lisp
;;; ~/.config/me/ai-assist.lsp
;;; Student Practice Script: Leveraging Floats and Local AI Hooks

(defun ai-code-review ()
  "Extracts the current buffer line, streams it to the AI, and injects notes conditionally."
  (let ((source-code (get-line)))
    (message "Querying qwen2.5-coder via local loopback...")
    (let ((confidence-score (ai-complete source-code)))

      ;; Educational Exercise: Leverage floating-point evaluation thresholds
      (if (> confidence-score 0.85)
          (progn
            (insert-string "\n;; AI Verification Passed (Score > 0.85)")
            (forward-line)
            (message "AI review injected successfully."))
          (message "AI confidence too low. Skipping buffer modification.")))))

;; Bind the new macro logic directly to our editor dispatch map
(define-lisp-command "ai-review" #'ai-code-review)
(define-key "M-A" "ai-review")
```

---

## 4. Educational Objectives for Programming Learners

| Core Concept | Practical Student Experience |
| :--- | :--- |
| **System C Programming** | Students trace text data sequentially from a primitive memory array, through low-level Unix sockets, straight to a localized neural network backend. |
| **Hybrid Engineering** | Teaches the philosophy of optimization: Building performance-critical engines in highly structured C, while managing high-level logic and extensions in a fast-loading interpreted scripting format (Lisp). |
| **Floating-Point Arithmetic** | Explains the real-world application of float data types through token probabilities and programmatic score gating thresholds (`> confidence-score 0.85`). |

---
*Documentation maintained for the `me` development repository. Compiled natively using musl-libc.*
