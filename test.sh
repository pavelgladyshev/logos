# test.sh - Shell feature integration tests
# Run with: sh /etc/test.sh

echo === Shell Integration Tests ===

# Test 1: Variable expansion
set TESTVAR=hello
echo var: $TESTVAR

# Test 2: Exit code tracking
ls /bin
echo exit-ok: $?

# Test 3: If/then (true)
if ls /bin
then
  echo if-true: PASS
fi

# Test 4: If/then/else (false)
if ls /nonexistent_xyz
then
  echo if-false: FAIL
else
  echo if-false: PASS
fi

# Test 5: For loop
for item in one two three
do
  echo for: $item
done

# Test 6: Nested for with variable
for f in hello cat ls
do
  echo prog: $f
done

# Test 7: I/O redirection
echo redir-content > /tmp_test_redir.txt
cat /tmp_test_redir.txt
rm /tmp_test_redir.txt

# Test 8: Multiple set/echo
set A=first
set B=second
echo pair: $A $B

echo === Done ===
