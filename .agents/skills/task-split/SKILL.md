---
name: task-split
description: Break multi-step tasks into a checklist tracked with the plan tool before executing. Use when a task needs three or more steps, touches multiple files, or will span many command rounds.
---

# Task Split

Work from a checklist held in the plan tool so long tasks stay on track
and reviewable by the human.

1. Before executing anything multi-step, call the `plan` tool with the
   full checklist:

   ```text
   goal: <one line>
   1. [ ] step one
   2. [ ] step two
   3. [ ] step three
   ```

   Send the complete checklist every time, never a delta. The human sees
   each update as `[Plan updated]:`.

2. Execute one step at a time: one command per tool call, then re-send
   the plan with the finished step ticked `[x]`. Only mark `[x]` after
   command output confirms the step; never pre-mark.

3. The pinned plan reappears in every later request even when the
   conversation drops old turns, so there is no need to re-read it.

4. If a step proves wrong, rewrite the plan with corrected steps instead
   of drifting silently.

5. Finish with the final all-`[x]` checklist, a one-line outcome per
   step, then clear the plan by sending an empty checklist.

Keep steps small enough that each is a single verifiable command or
edit. When a step needs sub-steps, split it while rewriting the plan.
