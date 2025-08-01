package tests

import "../parser"
import "../shell"
import "core:testing"

@(test)
testExecution_ok :: proc(t: ^testing.T) {
    INPUT :: "ls -a"
    cmd_seq, err := parser.parse(INPUT)
    testing.expect(t, err == nil, "Parsing of %s failed", INPUT)

    shell.execute(&cmd_seq)
    free_all(context.temp_allocator)
}

@(test)
testPipe_ok :: proc(t: ^testing.T) {
    SINGLE_PIPE :: "ls -a | wc"
    cmd_seq, err := parser.parse(SINGLE_PIPE)
    testing.expect(t, err == nil, "Parsing of %s failed", SINGLE_PIPE)
    shell.execute(&cmd_seq)

    DOUBLE_PIPE :: "ls -a | ls -b | wc"
    cmd_seq, err = parser.parse(DOUBLE_PIPE)
    testing.expect(t, err == nil, "Parsing of %s failed", DOUBLE_PIPE)

    shell.execute(&cmd_seq)
    free_all(context.temp_allocator)
}

@(test)
testOkChain_ok :: proc(t: ^testing.T) {
    SINGLE_CHAIN :: "ls -a && ls -b"
    cmd_seq, err := parser.parse(SINGLE_CHAIN)
    testing.expect(t, err == nil, "Parsing of %s failed", SINGLE_CHAIN)

    DOUBLE_CHAIN :: "ls -a && ls -b && ls -c"
    cmd_seq, err = parser.parse(DOUBLE_CHAIN)
    testing.expect(t, err == nil, "Parsing of %s failed", DOUBLE_CHAIN)

    shell.execute(&cmd_seq)
    free_all(context.temp_allocator)
}

@(test)
testBadChain_ok :: proc(t: ^testing.T) {
    SINGLE_CHAIN :: "xyz || ls -a"
    cmd_seq, err := parser.parse(SINGLE_CHAIN)
    testing.expect(t, err == nil, "Parsing of %s failed", SINGLE_CHAIN)

    DOUBLE_CHAIN :: "xyz || xyz || ls -a"
    cmd_seq, err = parser.parse(DOUBLE_CHAIN)
    testing.expect(t, err == nil, "Parsing of %s failed", DOUBLE_CHAIN)

    shell.execute(&cmd_seq)
    free_all(context.temp_allocator)
}
