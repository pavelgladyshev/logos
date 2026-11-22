#!/usr/bin/expect -f
#
# Test fork/exec/wait functionality in Logisim
# Usage: test_fork.sh <Logisim jar> <circuit>  (make test runs it)

set timeout 300

# Start Logisim in text mode, with the jar and the circuit that make test gives
if {$argc != 2} { puts "usage: test_fork.sh <Logisim jar> <circuit>"; exit 1 }
spawn java -jar [lindex $argv 0] [lindex $argv 1] --tty raw

# Wait for shell prompt
set send_slow {1 0.03}
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No shell prompt"; exit 1 }
}
puts "\n>>> Shell prompt received"

# Test 1: Basic echo (sanity check)
send "echo hello\r"
expect {
    -re {hello} { }
    timeout { puts "FAIL: echo test"; exit 1 }
}
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No prompt after echo"; exit 1 }
}
puts ">>> PASS: echo test"

# Test 2: Run hello (existing spawn test)
send "hello\r"
expect {
    -re {Hello} { }
    timeout { puts "FAIL: hello test"; exit 1 }
}
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No prompt after hello"; exit 1 }
}
puts ">>> PASS: hello test"

# Test 3: Run fork_demo
send "fork_demo\r"
expect {
    -re {fork_demo.*my PID} { }
    timeout { puts "FAIL: fork_demo start"; exit 1 }
}
puts ">>> fork_demo started"

# Wait for Test 1 output (fork + exec + wait)
expect {
    -re {Test 1.*fork.*exec.*wait} { }
    timeout { puts "FAIL: fork_demo test 1 header"; exit 1 }
}
puts ">>> fork_demo Test 1 header"

expect {
    -re {Parent.*child exited with code} { }
    timeout { puts "FAIL: fork_demo test 1 completion"; exit 1 }
}
puts ">>> PASS: fork_demo Test 1 (fork+exec+wait)"

# Wait for Test 2 output (fork + exit)
expect {
    -re {Test 2.*fork.*exit} { }
    timeout { puts "FAIL: fork_demo test 2 header"; exit 1 }
}
puts ">>> fork_demo Test 2 header"

expect {
    -re {Parent.*child exited with code 42} { }
    timeout { puts "FAIL: fork_demo test 2 completion"; exit 1 }
}
puts ">>> PASS: fork_demo Test 2 (fork+exit with code 42)"

# Wait for Test 3 output (wait with no children)
expect {
    -re {wait.*returned -1} { }
    timeout { puts "FAIL: fork_demo test 3"; exit 1 }
}
puts ">>> PASS: fork_demo Test 3 (wait with no children = -1)"

# Wait for fork_demo completion
expect {
    -re {all tests done} { }
    timeout { puts "FAIL: fork_demo completion"; exit 1 }
}
puts ">>> PASS: fork_demo completed"

# Wait for shell prompt after fork_demo
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No prompt after fork_demo"; exit 1 }
}
puts ">>> Shell prompt returned after fork_demo"

# Test 4: Single pipe (ls | cat)
send "ls /bin | cat\r"
expect {
    -re {hello} { }
    timeout { puts "FAIL: single pipe (ls /bin | cat)"; exit 1 }
}
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No prompt after single pipe"; exit 1 }
}
puts ">>> PASS: single pipe (ls /bin | cat)"

# Test 5: Multi-stage pipe (ls | cat | cat)
send "ls /etc | cat | cat\r"
expect {
    -re {hello.txt} { }
    timeout { puts "FAIL: multi pipe (ls /etc | cat | cat)"; exit 1 }
}
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No prompt after multi pipe"; exit 1 }
}
puts ">>> PASS: multi-stage pipe (ls /etc | cat | cat)"

# Test 6: Test exit and shell restart (backward compat)
send "exit\r"
expect {
    -re {exit.*code} { }
    timeout { puts "FAIL: exit test"; exit 1 }
}
expect {
    -re {\$\s} { }
    timeout { puts "FAIL: No prompt after exit"; exit 1 }
}
puts ">>> PASS: exit and shell restart"

# Test 7: the ten test programs and /etc/test.sh, with their PASS and FAIL lines counted
set passed 0
set failed 0
foreach cmd {fs_test pipe_test redir_test fd_test proc_test link_test shm_test sem_test script_test stress_test {sh /etc/test.sh}} {
    send -s "$cmd\r"
    expect {
        -re {PASS|FAIL|\$\s$} {
            if {$expect_out(0,string) eq "PASS"} { incr passed; exp_continue }
            if {$expect_out(0,string) eq "FAIL"} { incr failed; exp_continue }
        }
        timeout { puts "FAIL: $cmd did not finish"; exit 1 }
    }
    puts ">>> $cmd: $passed PASS, $failed FAIL so far"
}
if {$failed > 0} { puts "FAIL: $failed of the tests failed"; exit 1 }
puts ">>> PASS: the test programs and /etc/test.sh ($passed PASS, 0 FAIL)"

puts "\n=== ALL TESTS PASSED ==="
exit 0
