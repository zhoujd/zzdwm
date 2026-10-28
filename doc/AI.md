AI
==

## Hermes Agent

```
## Setup 
$ docker run -it --rm \
	-v ~/.hermes:/opt/data \
	nousresearch/hermes-agent setup

## Gateway
$ docker run -d --name hermes --restart unless-stopped \
	-v ~/.hermes:/opt/data -p 8642:8642 \
	nousresearch/hermes-agent gateway run
```

## Clause Tips

To close or exit the interactive /diff panel view in Claude Code
(which displays the "source: Current" vs turn-by-turn source selectors),
you can use the following methods:
• Press Esc: Pressing the Escape key on your keyboard will instantly close
  the diff overlay interface and return you to the main chat.
• Run /diff again: Typing and submitting /diff a second time acts as a
  toggle to turn the view off.
• Press Enter: If you have selected a specific file or view from the panel,
  Enter can drill into or toggle out of the view depending on your
  selection context.
