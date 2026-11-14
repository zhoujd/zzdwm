# AI Agent One-Shot Mode [DONE]

Goal:

- Add `ai -exec "prompt"` and the shorter `ai -e "prompt"` alias.
- Preserve command approval unless `--full-access` is also supplied.
- Send the complete argument as one prompt without interactive slash handling.
- Keep startup banners and prompts out of one-shot output.
- Cover C argument validation, model input, and full-access behavior with tests.
- Match the feature in `tools/ai/python/ai.py` for Windows users.

Status: complete and validated.
