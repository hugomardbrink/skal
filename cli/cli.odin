package cli

import "core:io"
import "core:os"
import "core:log"
import "core:fmt"
import "core:mem"
import "core:slice"
import "core:strings"
import "core:sys/posix"
import "core:path/filepath"
import "core:unicode/utf8"

INPUT_MAX :: 4096
HISTORY_MAX :: 2048
HISTORY_FILE :: "skal.history"

Term :: struct {
    input_buffer: [dynamic]rune,
    pos: i32,
    min_pos: i32,
    history: [dynamic][]rune,
    history_pos: Maybe(i32),
    written_line: Maybe([]rune),
    orig_mode: posix.termios
}

term := Term{pos = 0, min_pos = 0, history_pos = nil}

init_cli :: proc () {
    mem_err: mem.Allocator_Error
    term.input_buffer, mem_err = make([dynamic]rune, context.allocator) //todo deinit
    log.assertf(mem_err == nil, "Memory allocation failed")

    term.history, mem_err = make([dynamic][]rune, context.allocator) //todo deinit
    log.assertf(mem_err == nil, "Memory allocation failed")

    home_path := string(posix.getenv("HOME"))
    log.assertf(home_path != "", "Home path not found")

    history_file := filepath.join({home_path, HISTORY_FILE}, context.allocator) 
    log.assertf(mem_err == nil, "Memory allocation failed")
    defer delete(history_file)

    history_content, err := os.read_entire_file_from_filename_or_err(history_file, context.allocator)
    if err != nil {
        fmt.printfln("skal: Couldn't read history file, continuing without history...")
        return
    }
    defer delete(history_content)

    history_content_it := string(history_content)
    for line in strings.split_lines_iterator(&history_content_it) {
        append(&term.history, utf8.string_to_runes(line))
    }

}

deinit_cli :: proc () {
   delete(term.input_buffer) 
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

@(private)
get_prompt_prefix :: proc() -> string {
    dir := os.get_current_directory(context.temp_allocator)
    home := string(posix.getenv("HOME"))
    
    if strings.contains(dir, home) {  // Bug: might replace if later directories mirror the home path
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

@(private)
get_input :: proc() -> string {
    return utf8.runes_to_string(term.input_buffer[:])
}

@(private)
reset_prompt :: proc() {
    clear(&term.input_buffer)
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
        if term.history_pos == nil {
            term.written_line = slice.clone(term.input_buffer[:], context.temp_allocator)
        }

        for len(term.input_buffer) > 0 {
            backwards()
        }
        history_pos := term.history_pos.? or_else i32(len(term.history))
        history_pos = max(history_pos-1, 0)
        term.history_pos = history_pos

        new_input := term.history[history_pos]
        append(&term.input_buffer, ..new_input[:])
        fmt.printf("%s", term.input_buffer[:])

        term.pos = term.min_pos + i32(len(term.input_buffer))
    case DOWN: 
        history_pos, ok := term.history_pos.?
        if !ok do return

        for len(term.input_buffer) > 0 {
            backwards()
        }

        history_pos += 1 
        new_input: []rune 
        if history_pos > i32(len(term.history)-1) {
            term.history_pos = nil 
            history_pos = i32(len(term.history)-1)
            new_input, ok = term.written_line.?; assert(ok)
            term.written_line = nil
        } else {
            term.history_pos = history_pos
            new_input = term.history[history_pos]
        }

        append(&term.input_buffer, ..new_input[:])
        fmt.printf("%s", term.input_buffer[:])

        term.pos = term.min_pos + i32(len(term.input_buffer))
    case RIGHT: 
        if i32(len(term.input_buffer)) + term.min_pos > term.pos {
            fmt.print("\x1b[C")
            term.pos += 1
        }

    case LEFT:
        if term.min_pos < term.pos {
            fmt.print("\x1b[D")
            term.pos -= 1
        }
    }
}

backwards :: proc() {
    DELETE_AND_REVERSE :: "\b \b"

    if term.pos == term.min_pos do return
    
    last_rune := term.input_buffer[len(term.input_buffer)-1]
    _, _, width := utf8.grapheme_count(utf8.runes_to_string({last_rune}))
    resize(&term.input_buffer, len(term.input_buffer)-1)

    for _ in 0..<width {
        fmt.print(DELETE_AND_REVERSE)
    }

    term.pos -=1
}

skip_and_clear :: proc() {
    reset_prompt()
    prompt_prefix := get_prompt_prefix()
    fmt.printf("%s", prompt_prefix)

    term.min_pos = i32(len(prompt_prefix))
    term.pos = term.min_pos
}

clear_and_print :: proc(input: []rune) {
    skip_and_clear()
    append(&term.input_buffer, ..input[:]) 
}

run_prompt :: proc() -> string {
    enable_raw_mode() 
    
    skip_and_clear()
    
    in_stream := os.stream_from_handle(os.stdin)
    
    for {
        rn, size, err := io.read_rune(in_stream)
        log.assertf(err == nil, "Couldn't read from stdin")

        switch rn {
        case '\n': 
            fmt.println()        
            return get_input()

        case '\f':
            fmt.println()
            return "clear" // This is a terminal history wipe, usually isn't

        case '\u007f':
            backwards()

        case '\x1b':
            handle_esc_seq(&in_stream)

        case utf8.RUNE_EOF:
            fallthrough
        case '\x04':
            fmt.println()
            skip_and_clear()

        case: // Bug: if user moved to earlier character
            append(&term.input_buffer, rn)
            term.pos += 1 

            fmt.print(rn)
        }

    }
}

