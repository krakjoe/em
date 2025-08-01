--TEST--
VFS: Basic file write/read operations
--FILE--
<?php
file_put_contents(
    "/test.txt", "Hello World");
echo file_get_contents("/test.txt") . "\n";
echo file_get_contents("vfs://test.txt") . "\n";
?>
--EXPECT--
Hello World
Hello World
