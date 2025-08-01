package shell

import "base:runtime"

import "core:fmt"
import "core:strings"
import "core:sys/posix"
import "core:os"
import "core:log"
import "core:mem"

import "../parser"

ShellState :: enum {
    Continue,
    Stop
}

maybe_foreground_pid: Maybe(posix.pid_t) = nil
ctx: runtime.Context

handle_ctrl_c :: proc"c"(sig: posix.Signal) {
    foreground_pid, foreground_running := maybe_foreground_pid.?

    if foreground_running {
        posix.kill(foreground_pid, .SIGINT) 
    } else {
        // Clear prompt
    }
}

handle_backround_process :: proc"c"(sig: posix.Signal) {
    context = ctx
    foreground_pid, foreground_running := maybe_foreground_pid.?

    if foreground_running do return

    CHILD_PROCESS :: -1
    background_pid := posix.waitpid(CHILD_PROCESS, nil, {.NOHANG})

    if background_pid > 0 && background_pid != foreground_pid {
        fmt.printf("skal: [%d] done", background_pid)
        // Clear prompt
    }

    if foreground_pid == background_pid {
        maybe_foreground_pid = nil
    }

}

init_shell :: proc() {
    ctx = context
    posix.signal(.SIGINT, handle_ctrl_c)
    posix.signal(.SIGCHLD, handle_backround_process)
}

get_prompt :: proc() -> string {
    dir := os.get_current_directory(context.temp_allocator)
    home := string(posix.getenv("HOME"))
    
    if strings.contains(dir, home) {
        dir, _ = strings.replace(dir, home, "~", 1, context.temp_allocator)
    } 
    
    uid := posix.getuid()
    pw := posix.getpwuid(uid)

    user: string
    if pw == nil {
        user = "Skal"
    } else {
        user = string(pw.pw_name)
    }

    prompt_parts := []string {user, " :: ", dir, " » "}
    prompt, err := strings.concatenate(prompt_parts[:], context.temp_allocator)
    log.assertf(err == nil, "Memory allocation failed")

    return prompt
}

pipe_command :: proc(pipe_seq: ^parser.PipeSequence, maybe_fd: Maybe(posix.FD), idx: int) {
    cmd := pipe_seq.commands[idx]

    fd, missing_fd := maybe_fd.?

    if missing_fd {
        dup_status := posix.dup2(fd, posix.STDOUT_FILENO)
        log.assertf(dup_status > -1, "Pipe failed")

        close_status := posix.close(fd)
        log.assertf(close_status == .OK, "Pipe failed")
    }

    if idx == 0 {
        if pipe_seq^.rstdin != "" {
            stdin_fd := posix.open(pipe_seq^.rstdin, { .RDWR }, { .IRUSR, .IWUSR, .IRGRP, .IROTH })
            log.assertf(stdin_fd > -1, "Pipe failed")

            dup_status := posix.dup2(stdin_fd, posix.STDIN_FILENO)
            log.assertf(dup_status > -1, "Pipe failed")

            close_status := posix.close(stdin_fd)
            log.assertf(close_status == .OK, "Pipe failed")
        }

        if pipe_seq^.rstdout != "" {
            stdout_fd := posix.open(pipe_seq^.rstdout, { .WRONLY, .CREAT, .TRUNC }, { .IRUSR, .IWUSR, .IRGRP, .IROTH })
            log.assertf(stdout_fd > -1, "Pipe failed")

            dup_status := posix.dup2(stdout_fd, posix.STDOUT_FILENO)
            log.assertf(dup_status > -1, "Pipe failed")

            close_status := posix.close(stdout_fd)
            log.assertf(close_status == .OK, "Pipe failed")
        }

        if pipe_seq^.rstderr != "" {
            stderr_fd := posix.open(pipe_seq^.rstderr, { .WRONLY, .CREAT, .TRUNC }, { .IRUSR, .IWUSR, .IRGRP, .IROTH })
            log.assertf(stderr_fd > -1, "Pipe failed")

            dup_status := posix.dup2(stderr_fd, posix.STDERR_FILENO)
            log.assertf(dup_status > -1, "Pipe failed")

            close_status := posix.close(stderr_fd)
            log.assertf(close_status == .OK, "Pipe failed")
        }

    } else {
        pipe: [2]posix.FD 
        log.assertf(posix.pipe(&pipe) == .OK, "Pipe failed")

        pid := posix.fork()
        switch pid {
        case -1: // Error
            log.assertf(posix.pipe(&pipe) == .OK, "Fork failed")
        case 0: // Child
            close_status := posix.close(pipe[0])
            log.assertf(close_status == .OK, "Pipe failed")

            pipe_command(pipe_seq, pipe[1], idx-1)
        case: // Parent
            close_status := posix.close(pipe[1])
            log.assertf(close_status == .OK, "Pipe failed")

            dup_status := posix.dup2(pipe[0], posix.STDIN_FILENO)
            log.assertf(dup_status > -1, "Pipe failed")

            close_status = posix.close(pipe[0])
            log.assertf(close_status == .OK, "Pipe failed")

            _ = posix.waitpid(pid, nil, { .UNTRACED, .CONTINUED })
        }

    }

    argv, mem_err := make([]cstring, len(cmd.args)+1, context.temp_allocator)
    log.assertf(mem_err == nil, "Memory allocation failed")

    argv[0] = cmd.name
    for i := 0; i < len(cmd.args); i += 1 do argv[i+1] = cmd.args[i]

    _ = posix.execvp(argv[0], raw_data(argv))
    fmt.printfln("skal: command not found: %s", argv[0])
    os.exit(1)
}

@private
change_dir :: proc(cmd: ^parser.Command) {
    if len(cmd.args) > 0 {
        ch_status := posix.chdir(cmd.args[0]) 
        if ch_status != .OK do fmt.printf("cd: No such file or directory: %s\n", cmd.args[0]) 
    } else { 
        ch_status := posix.chdir(posix.getenv("HOME"))
        if ch_status != .OK do fmt.printf("cd: No such file or directory: %s\n", posix.getenv("HOME"))
    } 
}

stop_chain :: proc(sequence_type: parser.SequenceType, exec_failed: bool) -> bool {
    stop_chain: bool

    switch sequence_type {
    case .Or:
        stop_chain = !exec_failed 
    case .And:
        stop_chain = exec_failed
    case .Head:
        stop_chain = false 
    }

    return stop_chain
}

execute_cmd_seq :: proc(cmd_seq: ^parser.CommandSequence) {
    for &pipe_seq in cmd_seq^.pipe_sequences {
        pid := posix.fork() 
        switch pid {
        case -1: // Error
            log.assertf(true, "Fork failed")
        case 0: // Child
            pipe_command(&pipe_seq, nil, len(pipe_seq.commands)-1)
        case: // Parent
            if pipe_seq.is_background {
                fmt.printf("skal: [%d] spawned\n", pid)
                posix.waitpid(pid, nil, { .NOHANG})
            } else {
                maybe_foreground_pid = pid

                status: i32 
                posix.waitpid(pid, &status, { .UNTRACED, .CONTINUED })

                exec_failed := posix.WIFEXITED(status) && posix.WEXITSTATUS(status) != 0

                if stop_chain(pipe_seq.sequence_type, exec_failed) do return
            }
        }

    }
}

execute :: proc(cmd_seq: ^parser.CommandSequence) -> ShellState {
    if len(cmd_seq^.pipe_sequences) == 0 do return .Continue 

    // if pipe sequence is above 0 it has at least one command
    first_cmd := cmd_seq^.pipe_sequences[0].commands[0]

    switch first_cmd.name {
    case "cd":
        change_dir(&first_cmd)
    case "exit":
        return .Stop 
    case:
        execute_cmd_seq(cmd_seq)   
    }

    return .Continue
}


