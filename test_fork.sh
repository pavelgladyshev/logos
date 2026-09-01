#!/usr/bin/expect -f
#
# Test fork/exec/wait functionality in Logisim

set timeout 300

# Start Logisim in headless mode
spawn java -jar /Users/pavelgladyshev/git/logisim-evolution-ucd/build/libs/logisim-evolution-ucd-4.0.6-all.jar computer.circ -t tty

# Wait for shell prompt
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

puts "\n=== ALL TESTS PASSED ==="
exit 0
