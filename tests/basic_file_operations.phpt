--TEST--
VFS: Basic file write/read operations
--FILE--
<?php
file_put_contents("vfs://test.txt", "Hello World");
echo file_get_contents("vfs://test.txt");
?>
--EXPECT--
Hello World
