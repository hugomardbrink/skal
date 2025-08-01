package main

import "core:os"
import "core:fmt"
import "core:log"
import "core:mem"

import "parser"
import "shell"

INPUT_MAX :: 4096

main :: proc() {
    track: mem.Tracking_Allocator
    mem.tracking_allocator_init(&track, context.temp_allocator)
    context.temp_allocator = mem.tracking_allocator(&track)

    defer {
        if len(track.allocation_map) > 0 {
            fmt.eprintf("=== %v allocations not freed: ===\n", len(track.allocation_map))
            for _, entry in track.allocation_map {
                fmt.eprintf("- %v bytes @ %v\n", entry.size, entry.location)
            }
        }
        mem.tracking_allocator_destroy(&track)
    }

    buf: [INPUT_MAX]byte
    shell.init_shell()

    for true {
        prompt := shell.get_prompt()

        fmt.print(prompt)

        buf_len, err := os.read(os.stdin, buf[:])
        log.assertf(err == nil, "Error reading input")

        input := string(buf[:buf_len]) 
        
        cmd_seq, parser_err := parser.parse(input)
        log.assertf(parser_err == nil, "Could not parse input")

        stop := shell.execute(&cmd_seq) 

        free_all(context.temp_allocator)
        if stop == .Stop do break
    }
}
