# Shell

An implementation of a simple shell.

## Group Members
- **Miles Burkart**: mjb23b@fsu.edu
- **Chamee Nernginn**: cn24a@fsu.edu
- **Cole Dunlop**: cjd21d@fsu.edu

## Division of Labor

### Part 1: Prompt
- **Responsibilities**: Print the prompt, including the username and machine name
- **Assigned to**: Cole, Chamee

### Part 2: Environment Variables
- **Responsibilities**: Allow the usage of environment variables in command args
- **Assigned to**: Chamee

### Part 3: Tilde Expansion
- **Responsibilities**: Expand tildes into the user's home directory
- **Assigned to**: Chamee

### Part 4: $PATH Search
- **Responsibilities**: Search the $PATH for external executables
- **Assigned to**: Chamee, Cole

### Part 5: External Command Execution
- **Responsibilities**: Allow executing external commands from within the shell
- **Assigned to**: Miles, Chamee

### Part 6: I/O Redirection
- **Responsibilities**: Implement `<` and `>` redirection operators
- **Assigned to**: Miles, Cole

### Part 7: Piping
- **Responsibilities**: Use `|` to pipe output of one command into another command
- **Assigned to**: Miles, Cole

### Part 8: Background Processing
- **Responsibilities**: Create a background job if a command ends with `&`
- **Assigned to**: Miles, Cole

### Part 9: Internal Command Execution
- **Responsibilities**: Implement `cd`, `jobs`, and `exit` commands
- **Assigned to**: Cole, Miles

### Extra Credit
- **Responsibilities**: Unlimited number of pipes, piping and I/O redirection in a single command, execute shell within itself
- **Assigned to**: Miles

## File Listing
```
shell/
│
├── src/
│ ├── lexer.c
│ └── shell.c
│
├── include/
│ └── lexer.h
│
├── README.md
└── Makefile
```
## How to Compile & Execute

### Requirements
- **Compiler**: `gcc`

### Compilation
```bash
make
```
This will build the executable in `bin`.
### Execution
```bash
make run
```
This will run the program.

## Development Log
Each member records their contributions here.

### Miles

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-09-13 | Created the repository |
| 2026-09-28 | Implemented piping and I/O redirection |

### Chamee

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-09-26 | Organized project files |
| 2026-09-27 | Completed parts 1-5    |


## Meetings
Document in-person meetings, their purpose, and what was discussed.

| Date       | Attendees            | Topics Discussed | Outcomes / Decisions |
|------------|----------------------|------------------|-----------------------|
| 2026-09-10 | Miles, Chamee, Cole  | Responsibilities | Create repository     |
| 2026-09-24 | Miles, Chamee        | Implementation details | Start implementation |

