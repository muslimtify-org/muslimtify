# AI Usage Policy

Muslimtify calculates prayer times that real Muslims rely on to perform their daily prayers. A wrong angle, a misplaced timezone offset, or a subtle bug in a calculation can cause someone to pray at the wrong time. That is not an ordinary software defect — it touches an act of worship (`ibadah`). Because of this, we hold contributions to a higher standard than most projects, and that includes how AI tools are used.

**Using AI to contribute to this project is discouraged.** It is not forbidden, but if you reach for an AI tool you are taking on extra responsibility, not less.

## The Rules

These rules apply to outside contributions (issues, discussions, and pull requests). Maintainers may use AI tools at their discretion; they have proven they apply good judgment.

- **All AI usage must be disclosed.** State the tool you used (e.g. Claude Code, Cursor, Copilot, ChatGPT) and how much of the work was AI-assisted. Put this in the pull request or issue description.

- **You must fully understand every line you submit.** If you cannot explain what your change does, how it interacts with the rest of the system, and *why it is correct* — without the AI in front of you — do not submit it. This is especially true for any code touching prayer time calculation, timezones, madhab/method selection, or reminder scheduling.

- **You must review and fix the output.** AI produces plausible-looking code that is often subtly wrong. It is your job to read it critically, find the bad parts, and fix them before submission. Submitting raw AI output and hoping a maintainer will catch the problems is not acceptable.

- **You take full responsibility for the code.** "The AI wrote it" is never an excuse. Once you open the PR, the work is yours. If it ships a bug that makes someone miss a prayer, that is on you.

- **NEVER TOUCH `prayertimes.h` or any of docs/<METHOD.md>** if you are not a maintainer working with this file is forbidden, instead create an issues with tag `prayertime.h` with human in the loop.

- **Issues and discussions may use AI, but a human must be in the loop.** Any AI-generated text must be reviewed *and edited* by a human before submission. AI tends to be verbose and noisy, trim it down to the actual point.

- **No AI-generated media** (images, audio, video, art). Text and code are the only AI-generated content allowed, subject to the rules above.

## There Are Humans Here

Every issue, discussion, and pull request is read and reviewed by a human maintainer. It is disrespectful to hand a maintainer low-effort, unverified work and leave them to do the validation you should have done yourself. The burden of proof is on the contributor, not the reviewer.

## AI Is Not the Enemy

This policy is not anti-AI. AI is a legitimate tool and good developers use it well. The problem is not the tool — it is people who submit unverified AI output and treat the maintainers' time and the users' worship as cheap.

If you are early in your journey and want to learn, you are welcome here — but then do the work yourself, ask questions, and we will gladly help you grow. If you only want to forward AI output without understanding it, this is not the project for that.

We build this for the sake of helping Muslims pray on time. Please contribute with that same care.
