---
name: task-split
description: Break multi-step tasks into a written checklist before executing. Use when a task needs three or more steps, touches multiple files, or will span many command rounds.
---

# Task Split

Work from a checklist file so long tasks survive context loss and stay
reviewable by the human.

1. Before executing anything multi-step, write the plan to
   `.ai-plan.md` in the workspace root:

   ```sh
   cat > .ai-plan.md <<'EOF'
   goal: <one line>
   1. [ ] step one
   2. [ ] step two
   3. [ ] step three
   EOF
   ```

   The write passes the normal approval boundary, so the human sees the
   plan before work starts.

2. Execute one step at a time: one command per tool call, then tick the
   finished step by rewriting the file with `[x]`. Only mark `[x]` after
   command output confirms the step; never pre-mark.

3. Re-read the plan with `cat .ai-plan.md` whenever continuing after a
   long stretch of output. The conversation may have dropped earlier
   turns; the file never drops.

4. If a step proves wrong, rewrite the plan with corrected steps instead
   of drifting silently.

5. Finish by showing the final checklist, summarizing each step's
   outcome, then remove the file with `rm .ai-plan.md`.

Keep steps small enough that each is a single verifiable command or
edit. When a step needs sub-steps, split it while rewriting the plan.
