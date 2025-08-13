package cli

import "base:runtime"

import "core:io"
import "core:os"
import "core:log"
import "core:fmt"
import "core:mem"
import "core:slice"
import "core:bytes"
import "core:strings"
import "core:sys/posix"
import "core:path/filepath"
import "core:unicode/utf8"

import "../config"

History :: struct {
    data: [dynamic][]rune,
    file: string,
    maybe_idx: Maybe(i32),
    maybe_cached_input: Maybe([]rune),
}

Input :: struct {
    buffer: [dynamic]rune,
    prompt_size: i32,
    cursor_offset: i32,
}

Term :: struct {
    orig_mode: posix.termios,
    input: Input,
    history: History,
}

term := Term{}

init_cli :: proc () {
    mem_err: mem.Allocator_Error
    term.input.buffer, mem_err = make([dynamic]rune, context.allocator) 
    log.assertf(mem_err == nil, "Memory allocation failed")

    term.history.data, mem_err = make([dynamic][]rune, context.allocator) 
    log.assertf(mem_err == nil, "Memory allocation failed")

    home_path := string(posix.getenv("HOME"))
    log.assertf(home_path != "", "Home path not found")

    term.history.file = filepath.join({home_path, config.HISTORY_FILE}, context.allocator) 
    log.assertf(mem_err == nil, "Memory allocation failed")

    USER_PERMISSIONS :: 0o600
    if !os.exists(term.history.file) {
        handle, ferr := os.open(term.history.file, os.O_CREATE | os.O_RDWR, USER_PERMISSIONS) 
        log.assertf(ferr == nil, "Failed to create history file")
        os.close(handle)
    }

    history_content, err := os.read_entire_file_from_filename_or_err(term.history.file, context.allocator)
    log.assertf(err == nil, "Failed to read history file")
    defer delete(history_content)

    history_content_it := string(history_content)
    for line in strings.split_lines_iterator(&history_content_it) {
        append(&term.history.data, utf8.string_to_runes(line))
    }

}

deinit_cli :: proc () {
   delete(term.input.buffer) 
   delete(term.history.data)
   delete(term.history.file)
}

disable_raw_mode :: proc "c" () {
	posix.tcsetattr(posix.STDIN_FILENO, .TCSANOW, &term.orig_mode)
}

enable_raw_mode :: proc() {
	status := posix.tcgetattr(posix.STDIN_FILENO, &term.orig_mode)
	log.assertf(status == .OK, "Couldn't enable terminal in raw mode")

	posix.atexit(disable_raw_mode)

	raw := term.orig_mode
	raw.c_lflag -= {.ECHO, .ICANON}
	status = posix.tcsetattr(posix.STDIN_FILENO, .TCSANOW, &raw)
	log.assertf(status == .OK, "Couldn't enable terminal in raw mode")
}

get_maybe_git_prompt :: proc(dir: string) -> Maybe(string) {
    GIT_FOLDER :: ".git"
    git_path := filepath.join({dir, GIT_FOLDER}, context.temp_allocator) 
    uses_git := os.exists(git_path)

    if !uses_git do return nil

    HEAD_FILE_NAME :: "HEAD"
    head_file_path := filepath.join({git_path, HEAD_FILE_NAME}, context.temp_allocator)
    head_data, ferr := os.read_entire_file_or_err(head_file_path, context.temp_allocator)
    if ferr != nil do return nil

    // e.g. ref: refs/heads/main, last part indicates branch
    ref_parts, merr := strings.split(string(head_data), "/", context.temp_allocator)
    log.assertf(merr == nil, "Memory allocation failed")
   
    branch, prompt_part: string
    ok: bool
    is_raw_hash := len(ref_parts) == 1
    if is_raw_hash {
        HASH_MAX_LEN :: 9
        branch, ok = strings.substring(ref_parts[0], 0, HASH_MAX_LEN)
        if !ok do return nil
    } else {
        branch = strings.trim_right_space(ref_parts[len(ref_parts) - 1])
    }

    // todo: add if branch has changes or not
    prompt_part, merr = strings.concatenate({"‹", branch, "›"}, context.temp_allocator)
    log.assertf(merr == nil, "Memory allocation failed")
    return prompt_part
}

@(private)
get_prompt_prefix :: proc() -> string {
    dir := os.get_current_directory(context.temp_allocator)
    home := string(posix.getenv("HOME"))
   
    prompt_dir: string
    if strings.contains(dir, home) {  // Bug: might replace if later directories mirror the home path
        prompt_dir, _ = strings.replace(dir, home, "~", 1, context.temp_allocator)
    } else {
        prompt_dir = dir
    }
    
    uid := posix.getuid()
    pw := posix.getpwuid(uid)

    DEFAULT_USERNAME :: "skal"
    user: string
    if pw == nil {
        user = DEFAULT_USERNAME 
    } else {
        user = string(pw.pw_name)
    }


    prompt_parts := []string {
        config.USER_COLOR, 
        user, 
        config.DETAILS_COLOR,
        " :: ", 
        config.DIR_COLOR,
        prompt_dir,
        " ",
        config.GIT_COLOR,
        get_maybe_git_prompt(dir).? or_else "",
        config.DETAILS_COLOR,
        " » ",
        config.BASE_COLOR}

    prompt, err := strings.concatenate(prompt_parts[:], context.temp_allocator)
    log.assertf(err == nil, "Memory allocation failed")

    return prompt
}

@(private)
handle_esc_seq :: proc(in_stream: ^io.Stream) {
    NAV_SEQ_START :: '['

    next_rn, _, err := io.read_rune(in_stream^)
    log.assertf(err == nil, "Couldn't read from stdin")
    
    if next_rn == NAV_SEQ_START do handle_nav(in_stream) 
}

@(private)
handle_nav :: proc(in_stream: ^io.Stream) {
    UP    :: 'A'
    DOWN  :: 'B'
    RIGHT :: 'C'
    LEFT  :: 'D'

    rn, _, err := io.read_rune(in_stream^)
    log.assertf(err == nil, "Couldn't read from stdin")

    switch rn {
    case UP:
        cur_history_idx, scrolling_history := term.history.maybe_idx.?
        if !scrolling_history {
            term.history.maybe_cached_input = slice.clone(term.input.buffer[:], context.temp_allocator)
        } else if cur_history_idx == 0 {
            return
        }

        pop_runes(i32(len(term.input.buffer)))

        history_idx := term.history.maybe_idx.? or_else i32(len(term.history.data))
        history_idx -= 1
        term.history.maybe_idx = history_idx

        new_input := term.history.data[history_idx]
        write_runes(new_input[:])

    case DOWN: 
        history_idx, scrolling_history := term.history.maybe_idx.?
        if !scrolling_history do return

        pop_runes(i32(len(term.input.buffer)))

        history_idx += 1 
        history_len := i32(len(term.history.data))
        new_input: []rune 

        if history_idx >= history_len {
            term.history.maybe_idx = nil 
            history_idx = history_len

            ok: bool
            new_input, ok = term.history.maybe_cached_input.?; assert(ok)
            term.history.maybe_cached_input = nil
        } else {
            term.history.maybe_idx = history_idx
            new_input = term.history.data[history_idx]
        }
        write_runes(new_input[:])

    case RIGHT: 
        if term.input.cursor_offset > 0 {
            fmt.print("\x1b[C")
            term.input.cursor_offset -= 1
        }

    case LEFT:
        if term.input.cursor_offset < i32(len(term.input.buffer)) {
            fmt.print("\x1b[D")
            term.input.cursor_offset += 1
        }
    }
}


@(private)
write_runes :: proc(rns: []rune) {
    append(&term.input.buffer, ..rns[:])
    fmt.print(utf8.runes_to_string(rns[:]))
}

@(private)
write_rune :: proc(rn: rune) {
    append(&term.input.buffer, rn)
    fmt.print(rn)
}

@(private)
pop_runes :: proc(amount: i32) {
    DELETE_AND_REVERSE :: "\b \b"
    log.assertf(amount <= i32(len(term.input.buffer)), "Cannot remove more runes that written in buffer")
    
    start_idx := i32(len(term.input.buffer)) - amount
    runes := term.input.buffer[start_idx:]
    _, _, width := utf8.grapheme_count(utf8.runes_to_string(runes, context.temp_allocator))
    resize(&term.input.buffer, i32(len(term.input.buffer)) - amount)

    for _ in 0..<width {
        fmt.print(DELETE_AND_REVERSE)
    }
}

@(private)
pop_rune_at_cursor :: proc() {
    DELETE_AND_REVERSE :: "\b \b"

    if len(term.input.buffer) == 0 do return
    
    last_rune := term.input.buffer[len(term.input.buffer)-1]
    _, _, width := utf8.grapheme_count(utf8.runes_to_string({last_rune}, context.temp_allocator))
    resize(&term.input.buffer, len(term.input.buffer)-1)

    for _ in 0..<width {
        fmt.print(DELETE_AND_REVERSE)
    }
}

@(private)
append_history_file :: proc(data: []rune) {
    append(&term.history.data, slice.clone(data))
    term.history.maybe_idx = nil
    term.history.maybe_cached_input = nil

    fd, err := os.open(term.history.file, os.O_WRONLY | os.O_APPEND | os.O_CREATE)
    log.assertf(err == nil, "Failed to write to history file")
    defer os.close(fd)
    
    history_data := strings.concatenate({utf8.runes_to_string(data[:]), "\n"}, context.temp_allocator)
    _, werr := os.write_string(fd, history_data)
    log.assertf(werr == nil, "Failed to write to history file")
}

clear_input :: proc() {
    clear(&term.input.buffer)
    prompt_prefix := get_prompt_prefix()
    fmt.printf("%s", prompt_prefix)

    term.input.prompt_size = i32(len(prompt_prefix))
}

skip_and_clear :: proc() {
    fmt.println()
    clear_input()
}

run_prompt :: proc() -> Maybe(string) {
    enable_raw_mode() 
    clear_input()
    
    in_stream := os.stream_from_handle(os.stdin)
    
    for {
        rn, _, err := io.read_rune(in_stream)
        log.assertf(err == nil, "Couldn't read from stdin")

        switch rn {
        case '\n': 
            fmt.println()
            if len(term.input.buffer) == 0 do return "" 

            append_history_file(term.input.buffer[:])
            input := utf8.runes_to_string(term.input.buffer[:], context.temp_allocator)

            return input

        case '\f':
            return "clear" // This is a terminal history wipe, usually isn't

        case '\u007f':
            pop_rune_at_cursor()

        case '\x1b':
            handle_esc_seq(&in_stream)

        case utf8.RUNE_EOF:
            fallthrough
        case '\x04':
            return nil

        case: // Bug: if user moved to earlier character
            write_rune(rn)
        }

    }
}

