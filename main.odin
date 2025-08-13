package main

import "core:os"
import "core:fmt"
import "core:log"
import "core:mem"

import "parser"
import "shell"
import "cli"

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

    shell.init_shell()
    cli.init_cli()
    defer cli.deinit_cli()

    for true {
        defer free_all(context.temp_allocator)

        maybe_input := cli.run_prompt()
        input, ok := maybe_input.?
        if !ok {
            fmt.println()
            break
        }
        
        cmd_seq, parser_err := parser.parse(input)
        log.assertf(parser_err == nil, "Could not parse input")

        stop := shell.execute(&cmd_seq) 

        if stop == .Stop do break
    }
}
