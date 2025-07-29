--TEST--
VFS: Error handling for non-existent files
--FILE--
<?php
$result = @file_get_contents("vfs://nonexistent.txt");
var_dump($result === false);
$stat = @stat("vfs://nonexistent.txt");
var_dump($stat === false);
?>
--EXPECT--
bool(true)
bool(true)
