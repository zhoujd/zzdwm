AI
==

## Hermes Agent

```
## Setup 
$ docker run -it --rm 
	-v ~/.hermes:/opt/data \
	nousresearch/hermes-agent setup

## Gateway
$ docker run -d --name hermes --restart unless-stopped \
	-v ~/.hermes:/opt/data -p 8642:8642 \
	nousresearch/hermes-agent gateway run
```
