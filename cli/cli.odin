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
    input_buffer: [dynamic]byte,
    pos: u32,
    min_pos: u32,
    history: [HISTORY_MAX]string,
    orig_mode: posix.termios
}

term := Term{pos = 0, min_pos = 0}

init_cli :: proc () {
    mem_err: mem.Allocator_Error
    term.input_buffer, mem_err = make([dynamic]byte, context.allocator) //todo deinit
    home_path := string(posix.getenv("HOME"))
    log.assertf(home_path != "", "Home path not found")

    history_file := filepath.join({home_path, HISTORY_FILE}, context.allocator) //todo deinit
    log.assertf(mem_err == nil, "Memory allocation failed")
    defer delete(history_file)

    history_content, err := os.read_entire_file_from_filename_or_err(history_file, context.allocator)
    
    if err != nil {
        fmt.printfln("skal: Couldn't read history file, continuing without history...")
        defer delete(history_content)
        return
    }

    history_content_it := string(history_content)
    for line in strings.split_lines_iterator(&history_content_it) {
        fmt.printfln(line)
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
    return string(term.input_buffer[:])
}

@(private)
reset_prompt :: proc() {
    clear(&term.input_buffer)
}

@(private)
handle_esc_seq :: proc(in_stream: ^io.Stream) {
    next_ch, _, err := io.read_rune(in_stream^)

    log.assertf(err == nil, "Couldn't read from stdin")
    
    NAV_SEQ_START :: '['
    if next_ch == NAV_SEQ_START do handle_nav(in_stream) 
}

@(private)
handle_nav :: proc(in_stream: ^io.Stream) {
    UP    :: 'A'
    DOWN  :: 'B'
    RIGHT :: 'C'
    LEFT  :: 'D'

    ch, _, err := io.read_rune(in_stream^)
    log.assertf(err == nil, "Couldn't read from stdin")

    switch ch {
    case UP:
    case DOWN: 
    case RIGHT: 
        if u32(len(term.input_buffer)) + term.min_pos > term.pos {
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

// This is not correct for multisize runes
backwards :: proc() {
    DELETE_AND_REVERSE :: "\b \b"

    _, rm_size := utf8.decode_last_rune(term.input_buffer[:])
    if rm_size > 0 && term.pos > term.min_pos {
        resize(&term.input_buffer, len(term.input_buffer)-rm_size)
        fmt.print(DELETE_AND_REVERSE)
        term.pos -= 1
    }
}

skip_and_clear :: proc() {
    reset_prompt()
    prompt_prefix := get_prompt_prefix()
    fmt.printf("%s", prompt_prefix)

    term.min_pos = u32(len(prompt_prefix))
    term.pos = term.min_pos
}

run_prompt :: proc() -> string {
    enable_raw_mode() 
    
    skip_and_clear()
    
    in_stream := os.stream_from_handle(os.stdin)

    for {
        ch, size, err := io.read_rune(in_stream)
        log.assertf(err == nil, "Couldn't read from stdin")

        switch {
        case ch == '\n': 
            fmt.println()        
            return get_input()

        case ch == '\f':
            fmt.println()
            return "clear"

        case ch == '\u007f':
            backwards()

        case ch == '\x1b':
            handle_esc_seq(&in_stream)


        case ch == utf8.RUNE_EOF:
            fallthrough
        case ch == '\x04':
            fmt.println()
            skip_and_clear()

        case:
            bytes, _ := utf8.encode_rune(ch)
            append(&term.input_buffer, ..bytes[:size])
            term.pos += u32(size)

            fmt.print(ch)
        }

    }
}

