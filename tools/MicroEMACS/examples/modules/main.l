; Run from tools/MicroEMACS with:
;   me --script examples/modules/main.l
(require "demo" "examples/modules/demo.l")
(message "%s" (module-greeting))
