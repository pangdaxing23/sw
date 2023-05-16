# sw - a simple, precise stopwatch

Precisely record times to the 1/100th of a second.

## Installation

`make && make install`

## Usage

```
./sw [-hsr]
```

### Options:
- `-h, --help` : Show a help message and exit.
- `-s, --save` : Save the final time to `~/.sw/savedtime` upon termination.
- `-r, --restore` : Restore time from `~/.sw/savedtime` at the start.

### Controls:

Once the program is running, you can use the following controls:

- `space` : Pause or resume the stopwatch.
- `s` : Save the current time to `~/.sw/savedtime`.
- `r` : Reset the stopwatch to zero.
- `q` : Quit.
