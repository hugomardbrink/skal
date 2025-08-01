package parser

import "core:strings"
import "core:fmt"
import "core:mem"
import "core:slice"
import "core:log"

FORBIDDEN_TOKENS :: []string {
    "&&",
    "||",
    "|",
    ">",
    "&>",
    "<",
    "2>",
}

@private
ParseError :: enum {
    None = 0,
    MissingCommand,
    MissingRedirect,
    ForbiddenToken,
}

SequenceType :: enum {
    Head,
    Or,
    And
}

Command :: struct {
    name: cstring,
    args: [dynamic]cstring,
}

PipeSequence :: struct {
    rstdin: cstring,
    rstdout: cstring,
    rstderr: cstring,

    is_background: bool,

    commands: [dynamic]Command,
    sequence_type: SequenceType,
}

CommandSequence :: struct {
    pipe_sequences: [dynamic]PipeSequence,
}

ParserState :: struct {
    cmd_seq: CommandSequence,
    cur_pipe_seq: PipeSequence,
    cur_cmd: Command,

    tokens: []cstring,
    token_idx: int,
}

@private
new_command :: proc() -> Command {
    cmd := Command{ name = "" }
    err: mem.Allocator_Error
    cmd.args, err = make([dynamic]cstring, 0, 1, context.temp_allocator)
    log.assertf(err == nil, "Memory allocation failed")

    return cmd
}

@private
new_pipe_sequence :: proc(seq_type: SequenceType) -> PipeSequence {
    pipe_seq := PipeSequence{ sequence_type = seq_type }
    err: mem.Allocator_Error
    pipe_seq.commands, err = make([dynamic]Command, 0, 1, context.temp_allocator)
    log.assertf(err == nil, "Memory allocation failed")

    return pipe_seq
}

@private
parse_token :: proc(parser_state: ^ParserState) -> ParseError { 
    token := parser_state.tokens[parser_state.token_idx]

    switch(token) {
    case "&": 
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand
        

        parser_state^.cur_pipe_seq.is_background = true
        parser_state^.token_idx += 1
    case "&&": 
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand

        _, mem_err := append(&parser_state^.cur_pipe_seq.commands, parser_state^.cur_cmd)
        log.assertf(mem_err == nil, "Memory allocation failed")
        _, mem_err = append(&parser_state^.cmd_seq.pipe_sequences, parser_state^.cur_pipe_seq)
        log.assertf(mem_err == nil, "Memory allocation failed")

        parser_state^.cur_pipe_seq = new_pipe_sequence(SequenceType.And) 
        parser_state^.cur_cmd = new_command()
        parser_state^.token_idx += 1

    case "||": 
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand

        _, mem_err := append(&parser_state^.cur_pipe_seq.commands, parser_state^.cur_cmd)
        log.assertf(mem_err == nil, "Memory allocation failed")
        _, mem_err = append(&parser_state^.cmd_seq.pipe_sequences, parser_state^.cur_pipe_seq)
        log.assertf(mem_err == nil, "Memory allocation failed")

        parser_state^.cur_pipe_seq = new_pipe_sequence(SequenceType.Or)
        parser_state^.cur_cmd = new_command()
        parser_state^.token_idx += 1

    case "|": 
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand 

        _, mem_err := append(&parser_state^.cur_pipe_seq.commands, parser_state^.cur_cmd)
        log.assertf(mem_err == nil, "Memory allocation failed")

        parser_state^.cur_cmd = new_command()
        parser_state^.token_idx += 1
    case ">": 
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand
        if len(parser_state^.cur_pipe_seq.commands) >= parser_state^.token_idx + 1 do return ParseError.MissingRedirect 
        
        next_token := parser_state.tokens[parser_state.token_idx + 1]
        if slice.contains(FORBIDDEN_TOKENS, string(next_token)) do return ParseError.ForbiddenToken 

        parser_state^.cur_pipe_seq.rstdout = next_token
        parser_state^.token_idx += 2
    case "&>":
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand
        if len(parser_state^.cur_pipe_seq.commands) >= parser_state^.token_idx + 1 do return ParseError.MissingRedirect 

        next_token := parser_state.tokens[parser_state.token_idx + 1]
        if slice.contains(FORBIDDEN_TOKENS, string(next_token)) do return ParseError.ForbiddenToken 

        parser_state^.cur_pipe_seq.rstdout = next_token
        parser_state^.cur_pipe_seq.rstderr = next_token
        parser_state^.token_idx += 2
    case "<":
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand
        if len(parser_state^.cur_pipe_seq.commands) >= parser_state^.token_idx + 1 do return ParseError.MissingRedirect 

        next_token := parser_state.tokens[parser_state.token_idx + 1]
        if slice.contains(FORBIDDEN_TOKENS, string(next_token)) do return ParseError.ForbiddenToken 

        parser_state^.cur_pipe_seq.rstdin = next_token
        parser_state^.token_idx += 2
    case "2>":
        if parser_state^.cur_cmd.name == "" do return ParseError.MissingCommand
        if len(parser_state^.cur_pipe_seq.commands) >= parser_state^.token_idx + 1 do return ParseError.MissingRedirect 

        next_token := parser_state.tokens[parser_state.token_idx + 1]
        if slice.contains(FORBIDDEN_TOKENS, string(next_token)) do return ParseError.ForbiddenToken

        parser_state^.cur_pipe_seq.rstderr = next_token
        parser_state^.token_idx += 2
    case: 
        if parser_state^.cur_cmd.name != "" {
            _, mem_err := append(&parser_state^.cur_cmd.args, token)
            log.assertf(mem_err == nil, "Memory allocation failed")
        } else {
            parser_state^.cur_cmd.name = token
        }
        parser_state^.token_idx += 1
    } 

    return nil
} 

get_cstring_tokens ::proc(input: string) -> []cstring {
    tokens, mem_err := strings.fields(input, context.temp_allocator)
    log.assertf(mem_err == nil, "Memory allocation failed")

    c_tokens: []cstring
    c_tokens, mem_err = make([]cstring, len(tokens), context.temp_allocator)
    log.assertf(mem_err == nil, "Memory allocation failed")

    for token, i in tokens {
        c_tokens[i], mem_err = strings.clone_to_cstring(token, context.temp_allocator) 
        log.assertf(mem_err == nil, "Memory allocation failed")
    }

    return c_tokens
}

parse :: proc(input: string) -> (cmd_seq: CommandSequence, err: ParseError) {
    mem_err: mem.Allocator_Error

    cur_cmd_seq := CommandSequence{}
    cur_cmd_seq.pipe_sequences, mem_err = make([dynamic]PipeSequence, 0, 1, context.temp_allocator)
    log.assertf(mem_err == nil, "Memory allocation failed")

    c_tokens := get_cstring_tokens(input)

    cur_pipe_seq := new_pipe_sequence(SequenceType.Head) 
    cur_pipe_seq.commands, mem_err = make([dynamic]Command, 0, 1, context.temp_allocator)
    log.assertf(mem_err == nil, "Memory allocation failed")

    if len(c_tokens) == 0 {
        return cmd_seq, nil 
    }

    cur_cmd := Command{ name = c_tokens[0] }
    cur_cmd.args, mem_err = make([dynamic]cstring, 0, 1, context.temp_allocator)
    log.assertf(mem_err == nil, "Memory allocation failed")

    parser_state := ParserState{ 
        cmd_seq = cur_cmd_seq, 
        cur_pipe_seq = cur_pipe_seq, 
        cur_cmd = cur_cmd, 
        tokens = c_tokens, 
        token_idx = 1 
    }

    for parser_state.token_idx < len(parser_state.tokens) {
        parse_err := parse_token(&parser_state)
        switch parse_err {
        case .MissingRedirect: 
            fmt.printf("Error: Missing redirect target after '%s'\n", parser_state.tokens[parser_state.token_idx])
            return cmd_seq, parse_err
        case .MissingCommand: 
            fmt.printf("Error: Missing command before '%s'\n", parser_state.tokens[parser_state.token_idx])
            return cmd_seq, parse_err
        case .ForbiddenToken:
            fmt.printf("Error: Forbidden '%s' after '%s'\n", 
                parser_state.tokens[parser_state.token_idx+1], 
                parser_state.tokens[parser_state.token_idx])
            return cmd_seq, parse_err
        case .None:
            continue
        }

    }

    if parser_state.cur_cmd.name != "" {
        _, mem_err = append(&parser_state.cur_pipe_seq.commands, parser_state.cur_cmd)
        log.assertf(mem_err == nil, "Memory allocation failed")

    }

    if len(parser_state.cur_pipe_seq.commands) > 0 {
        _, mem_err = append(&parser_state.cmd_seq.pipe_sequences, parser_state.cur_pipe_seq)
        log.assertf(mem_err == nil, "Memory allocation failed")
    }

    return parser_state.cmd_seq, nil 
}

